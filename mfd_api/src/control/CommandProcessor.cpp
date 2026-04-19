/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for CommandProcessor.
 */

#include "mfd/control/CommandProcessor.h"

#include <cstddef>
#include <cstring>
#include <cmath>
#include <type_traits>
#include <utility>

#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/model/Reticle.h"
#include "mfd/runtime/SceneRegistry.h"

namespace mfd
{
namespace
{
constexpr std::size_t kMaxCommandsPerPoll = 64;

} // namespace

CommandProcessor::CommandProcessor(SceneRegistry& scene)
    : scene_(scene)
{
    dispatcher_.sink<ActivatePageCommand>().connect<&CommandProcessor::OnActivatePage>(*this);
    dispatcher_.sink<SetPageViewCommand>().connect<&CommandProcessor::OnSetPageView>(*this);
    dispatcher_.sink<UpdateWindowDisplayCommand>().connect<&CommandProcessor::OnUpdateWindowDisplay>(*this);
    dispatcher_.sink<UpdateReticleCommand>().connect<&CommandProcessor::OnUpdateReticle>(*this);
    dispatcher_.sink<UpdateStrobeCommand>().connect<&CommandProcessor::OnUpdateStrobe>(*this);
    dispatcher_.sink<UpsertDynamicReticleCommand>().connect<&CommandProcessor::OnUpsertDynamicReticle>(*this);
    dispatcher_.sink<UpsertDynamicReticlesCommand>().connect<&CommandProcessor::OnUpsertDynamicReticles>(*this);
    dispatcher_.sink<SetDynamicReticleSetVisibilityCommand>().connect<&CommandProcessor::OnSetDynamicReticleSetVisibility>(*this);
    dispatcher_.sink<RemoveDynamicReticleCommand>().connect<&CommandProcessor::OnRemoveDynamicReticle>(*this);
    dispatcher_.sink<ResetWindowCommand>().connect<&CommandProcessor::OnResetWindow>(*this);
}

bool CommandProcessor::Submit(const UserCommand& command)
{
    SetSuccess();

    if (const auto validation = ValidateCommand(command); validation.has_value())
    {
        lastResult_ = *validation;
        lastCommandSucceeded_ = false;
        lastError_ = lastResult_.message;
        return false;
    }

    try
    {
        std::visit(
            [this](const auto& value)
            {
                dispatcher_.trigger(value);
            },
            command);
    }
    catch (const std::exception& exception)
    {
        SetFailure("dispatch_exception", exception.what());
    }
    catch (...)
    {
        SetFailure("dispatch_unknown_exception", "Unknown exception while dispatching a command");
    }

    return lastCommandSucceeded_;
}

bool CommandProcessor::Submit(const ArrayView<const UserCommand> commands)
{
    bool success = true;
    for (const UserCommand& command : commands)
    {
        success = Submit(command) && success;
    }

    return success;
}

bool CommandProcessor::Submit(const std::string_view payload)
{
    std::string error;
    const auto commands = DeserializeUserCommands(payload, &error);
    if (!commands.has_value())
    {
        SetFailure("deserialize_error", std::move(error));
        return false;
    }

    bool success = true;
    for (const UserCommand& command : *commands)
    {
        success = Submit(command) && success;
    }

    return success;
}

bool CommandProcessor::Submit(const ByteView payload)
{
    if (payload.empty())
    {
        SetFailure("empty_payload", "Command payload is empty");
        return false;
    }

    const auto* raw = reinterpret_cast<const char*>(payload.data());
    return Submit(std::string_view(raw, payload.size()));
}

bool CommandProcessor::Poll(IExchangeChannel& channel)
{
    bool processedAny = false;
    std::size_t processedCount = 0;

    while (processedCount < kMaxCommandsPerPoll)
    {
        try
        {
            const auto payload = channel.TryReceive();
            if (!payload.has_value())
            {
                break;
            }

            processedAny = true;
            ++processedCount;
            Submit(*payload);
        }
        catch (const std::exception& exception)
        {
            SetFailure("poll_exception", exception.what());
            break;
        }
        catch (...)
        {
            SetFailure("poll_unknown_exception", "Unknown exception while polling a command transport");
            break;
        }
    }

    if (processedCount == kMaxCommandsPerPoll)
    {
        lastError_ = "Command polling was throttled to keep the frame responsive";
    }

    if (!channel.LastError().empty())
    {
        lastError_ = channel.LastError();
    }

    return processedAny;
}

std::string CommandProcessor::LastError() const
{
    return lastError_;
}

CommandResult CommandProcessor::LastResult() const
{
    return lastResult_;
}

entt::dispatcher& CommandProcessor::Dispatcher() noexcept
{
    return dispatcher_;
}

const entt::dispatcher& CommandProcessor::Dispatcher() const noexcept
{
    return dispatcher_;
}

void CommandProcessor::OnActivatePage(const ActivatePageCommand& command)
{
    if (!scene_.HasPage(command.page))
    {
        SetFailure("unknown_page", "Unknown page: " + command.page);
        return;
    }

    scene_.SetActivePage(command.page);
}

void CommandProcessor::OnSetPageView(const SetPageViewCommand& command)
{
    if (!scene_.SetPageView(command.page, command.view))
    {
        SetFailure("page_view_update_failed", "Unable to update page view for page: " + command.page);
    }
}

void CommandProcessor::OnUpdateWindowDisplay(const UpdateWindowDisplayCommand& command)
{
    if (!scene_.ApplyWindowDisplayPatch(command.patch))
    {
        SetFailure("window_patch_failed", "Unable to update whole-window display properties");
    }
}

void CommandProcessor::OnUpdateReticle(const UpdateReticleCommand& command)
{
    if (!scene_.ApplyReticlePatch(command.target.page, command.target.reticle, command.patch))
    {
        SetFailure("reticle_patch_failed", "Unable to update reticle '" + command.target.reticle + "' on page '" + command.target.page + "'");
    }
}

void CommandProcessor::OnUpdateStrobe(const UpdateStrobeCommand& command)
{
    bool success = true;

    if (command.active.has_value())
    {
        success = scene_.SetStrobeActive(command.page, *command.active) && success;
    }

    if (command.position.has_value())
    {
        success = scene_.SetStrobePosition(command.page, *command.position) && success;
    }

    if (!success)
    {
        SetFailure("strobe_update_failed", "Unable to update strobe on page '" + command.page + "'");
    }
}

void CommandProcessor::OnUpsertDynamicReticle(const UpsertDynamicReticleCommand& command)
{
    if (!scene_.HasPage(command.target.page))
    {
        SetFailure("unknown_page", "Unknown page: " + command.target.page);
        return;
    }

    const auto templateIterator = scene_.Library().find(command.templateId);
    if (templateIterator == scene_.Library().end())
    {
        SetFailure("unknown_template", "Unknown reticle template: " + command.templateId);
        return;
    }

    if (scene_.HasDynamicReticle(command.target.page, command.target.reticle))
    {
        if (!scene_.ApplyDynamicReticlePatch(command.target.page, command.target.reticle, command.patch))
        {
            SetFailure("dynamic_reticle_update_failed", "Unable to update dynamic reticle '" + command.target.reticle + "'");
        }

        return;
    }

    ReticleGroup reticle = InstantiateReticle(templateIterator->second, command.target.reticle);
    scene_.UpsertDynamicReticle(command.target.page, std::move(reticle));

    if (!scene_.ApplyDynamicReticlePatch(command.target.page, command.target.reticle, command.patch))
    {
        SetFailure("dynamic_reticle_init_failed", "Unable to initialize dynamic reticle '" + command.target.reticle + "'");
    }
}

void CommandProcessor::OnUpsertDynamicReticles(const UpsertDynamicReticlesCommand& command)
{
    if (!scene_.HasPage(command.page))
    {
        SetFailure("unknown_page", "Unknown page: " + command.page);
        return;
    }

    const auto templateIterator = scene_.Library().find(command.templateId);
    if (templateIterator == scene_.Library().end())
    {
        SetFailure("unknown_template", "Unknown reticle template: " + command.templateId);
        return;
    }

    for (const DynamicReticleState& state : command.reticles)
    {
        if (scene_.HasDynamicReticle(command.page, state.reticleId))
        {
            if (!scene_.ApplyDynamicReticlePatch(command.page, state.reticleId, state.patch))
            {
                SetFailure("dynamic_reticle_update_failed", "Unable to update dynamic reticle '" + state.reticleId + "'");
                return;
            }

            continue;
        }

        ReticleGroup reticle = InstantiateReticle(templateIterator->second, state.reticleId);
        scene_.UpsertDynamicReticle(command.page, std::move(reticle));

        if (!scene_.ApplyDynamicReticlePatch(command.page, state.reticleId, state.patch))
        {
            SetFailure("dynamic_reticle_init_failed", "Unable to initialize dynamic reticle '" + state.reticleId + "'");
            return;
        }
    }
}

void CommandProcessor::OnRemoveDynamicReticle(const RemoveDynamicReticleCommand& command)
{
    if (!scene_.RemoveDynamicReticle(command.target.page, command.target.reticle))
    {
        SetFailure("dynamic_reticle_remove_failed", "Unable to remove dynamic reticle '" + command.target.reticle +
                   "' from page '" + command.target.page + "'");
    }
}

void CommandProcessor::OnSetDynamicReticleSetVisibility(const SetDynamicReticleSetVisibilityCommand& command)
{
    if (!scene_.SetDynamicReticleSetVisible(command.page, command.templateId, command.visible))
    {
        SetFailure("dynamic_template_visibility_failed", "Unable to update dynamic reticle set visibility for template '" + command.templateId +
                   "' on page '" + command.page + "'");
    }
}

void CommandProcessor::OnResetWindow(const ResetWindowCommand&)
{
    scene_.ResetToInitialState();
}

std::optional<CommandResult> CommandProcessor::ValidateCommand(const UserCommand& command) const
{
    auto makeFailure = [](std::string code, std::string message)
    {
        return std::optional<CommandResult> {CommandResult {false, std::move(code), std::move(message)}};
    };

    return std::visit(
        [&makeFailure](const auto& value) -> std::optional<CommandResult>
        {
            using Command = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Command, ActivatePageCommand>)
            {
                if (value.page.empty())
                {
                    return makeFailure("invalid_activate_page", "ActivatePageCommand requires a non-empty page");
                }
            }
            else if constexpr (std::is_same_v<Command, SetPageViewCommand>)
            {
                if (!std::isfinite(value.view.center.x) || !std::isfinite(value.view.center.y) ||
                    !std::isfinite(value.view.zoom))
                {
                    return makeFailure("invalid_page_view", "SetPageViewCommand contains non-finite coordinates");
                }
            }
            else if constexpr (std::is_same_v<Command, UpdateStrobeCommand>)
            {
                if (value.position.has_value() &&
                    (!std::isfinite(value.position->x) || !std::isfinite(value.position->y)))
                {
                    return makeFailure("invalid_strobe_position", "UpdateStrobeCommand contains non-finite coordinates");
                }
            }

            return std::nullopt;
        },
        command);
}

void CommandProcessor::SetFailure(std::string code, std::string message)
{
    lastCommandSucceeded_ = false;
    lastResult_ = CommandResult {false, std::move(code), message};
    lastError_ = std::move(message);
}

void CommandProcessor::SetSuccess()
{
    lastCommandSucceeded_ = true;
    lastResult_ = CommandResult {};
    lastError_.clear();
}
} // namespace mfd
