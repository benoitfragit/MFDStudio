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
    lastCommandSucceeded_ = true;
    lastError_.clear();

    return SubmitResolved(command, {});
}

bool CommandProcessor::Submit(const CommandBatch& batch)
{
    lastCommandSucceeded_ = true;
    lastError_.clear();

    if (!batch.mappingHash.empty() && !scene_.HasMatchingTransportMap(batch.mappingHash))
    {
        if (!scene_.HasTransportMap())
        {
            SetFailure("ID-based command batch requires a generated transport map loaded by the runtime");
        }
        else
        {
            SetFailure("Generated transport map hash mismatch between the client batch and the runtime window");
        }

        return false;
    }

    bool success = true;
    for (const UserCommand& command : batch.commands)
    {
        success = SubmitResolved(command, batch.mappingHash) && success;
    }

    return success;
}

bool CommandProcessor::SubmitResolved(UserCommand command, const std::string_view mappingHash)
{
    if (!ResolveCommandIdentifiers(command, mappingHash))
    {
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
        SetFailure(exception.what());
    }
    catch (...)
    {
        SetFailure("Unknown exception while dispatching a command");
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
    const auto batch = DeserializeCommandBatch(payload, &error);
    if (!batch.has_value())
    {
        SetFailure(std::move(error));
        return false;
    }

    return Submit(*batch);
}

bool CommandProcessor::Submit(const ByteView payload)
{
    if (payload.empty())
    {
        SetFailure("Command payload is empty");
        return false;
    }

    const auto* raw = reinterpret_cast<const char*>(payload.data());
    return Submit(std::string_view(raw, payload.size()));
}

bool CommandProcessor::Poll(IExchangeChannel& channel)
{
    bool processedAny = false;
    std::size_t processedCount = 0;
    bool dispatchFailed = false;

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
            if (!Submit(*payload))
            {
                dispatchFailed = true;
            }
        }
        catch (const std::exception& exception)
        {
            SetFailure(exception.what());
            dispatchFailed = true;
            break;
        }
        catch (...)
        {
            SetFailure("Unknown exception while polling a command transport");
            dispatchFailed = true;
            break;
        }
    }

    if (processedCount == kMaxCommandsPerPoll && !dispatchFailed)
    {
        lastError_ = "Command polling was throttled to keep the frame responsive";
    }

    if (!processedAny && !dispatchFailed && !channel.LastError().empty())
    {
        lastError_ = channel.LastError();
    }

    return processedAny;
}

std::string CommandProcessor::LastError() const
{
    return lastError_;
}

entt::dispatcher& CommandProcessor::Dispatcher() noexcept
{
    return dispatcher_;
}

const entt::dispatcher& CommandProcessor::Dispatcher() const noexcept
{
    return dispatcher_;
}

bool CommandProcessor::ResolveCommandIdentifiers(UserCommand& command, const std::string_view mappingHash)
{
    (void)mappingHash;

    if (mappingHash.empty())
    {
        return true;
    }

    auto resolvePage = [this](std::string& page, const TransportId pageId) -> bool
    {
        if (pageId == 0)
        {
            return !page.empty();
        }

        const std::string* resolvedPage = scene_.ResolvePageName(pageId);
        if (resolvedPage == nullptr)
        {
            SetFailure("Unknown generated page transport id " + std::to_string(pageId));
            return false;
        }

        if (!page.empty() && NormalizePageName(page) != NormalizePageName(*resolvedPage))
        {
            SetFailure("Generated page transport id " + std::to_string(pageId) + " does not match page '" + page + "'");
            return false;
        }

        page = *resolvedPage;
        return true;
    };

    auto resolveStaticReticle = [this, &resolvePage](ReticleHandle& target) -> bool
    {
        if (target.reticleId == 0)
        {
            return resolvePage(target.page, target.pageId) && !target.reticle.empty();
        }

        const SceneRegistry::TransportReticleLookup* resolvedReticle = scene_.ResolveStaticReticle(target.reticleId);
        if (resolvedReticle == nullptr)
        {
            SetFailure("Unknown generated static reticle transport id " + std::to_string(target.reticleId));
            return false;
        }

        if (target.pageId != 0 && target.pageId != resolvedReticle->pageId)
        {
            SetFailure("Generated static reticle transport id " + std::to_string(target.reticleId) +
                       " does not belong to page transport id " + std::to_string(target.pageId));
            return false;
        }

        if (!target.page.empty() && NormalizePageName(target.page) != NormalizePageName(resolvedReticle->pageName))
        {
            SetFailure("Generated static reticle transport id " + std::to_string(target.reticleId) +
                       " does not belong to page '" + target.page + "'");
            return false;
        }

        if (!target.reticle.empty() && NormalizePageName(target.reticle) != NormalizePageName(resolvedReticle->reticleId))
        {
            SetFailure("Generated static reticle transport id " + std::to_string(target.reticleId) +
                       " does not match reticle '" + target.reticle + "'");
            return false;
        }

        target.pageId = resolvedReticle->pageId;
        target.page = resolvedReticle->pageName;
        target.reticle = resolvedReticle->reticleId;
        return true;
    };

    auto resolveTemplate = [this](std::string& templateId, const TransportId templateTransportId) -> bool
    {
        if (templateTransportId == 0)
        {
            return !templateId.empty();
        }

        const std::string* resolvedTemplate = scene_.ResolveTemplateId(templateTransportId);
        if (resolvedTemplate == nullptr)
        {
            SetFailure("Unknown generated template transport id " + std::to_string(templateTransportId));
            return false;
        }

        if (!templateId.empty() && NormalizePageName(templateId) != NormalizePageName(*resolvedTemplate))
        {
            SetFailure("Generated template transport id " + std::to_string(templateTransportId) +
                       " does not match template '" + templateId + "'");
            return false;
        }

        templateId = *resolvedTemplate;
        return true;
    };

    auto resolvePatchPrimitiveIds = [this](ReticlePatch& patch,
                                           const TransportId pageId,
                                           const TransportId staticReticleId,
                                           const TransportId templateTransportId) -> bool
    {
        if (patch.blinkTypeId.has_value())
        {
            if (*patch.blinkTypeId == 0)
            {
                if (patch.blinkType.has_value() && !patch.blinkType->empty())
                {
                    SetFailure("Generated blink transport id clears the blink type but a named blink type was also provided");
                    return false;
                }

                patch.blinkType = std::string {};
            }
            else
            {
                const std::string* resolvedBlinkType = scene_.ResolveBlinkType(pageId, *patch.blinkTypeId);
                if (resolvedBlinkType == nullptr)
                {
                    SetFailure("Unknown generated blink transport id " + std::to_string(*patch.blinkTypeId));
                    return false;
                }

                if (patch.blinkType.has_value() &&
                    NormalizePageName(*patch.blinkType) != NormalizePageName(*resolvedBlinkType))
                {
                    SetFailure("Generated blink transport id " + std::to_string(*patch.blinkTypeId) +
                               " does not match blink type '" + *patch.blinkType + "'");
                    return false;
                }

                patch.blinkType = *resolvedBlinkType;
            }
        }

        auto resolvePrimitiveId = [this, staticReticleId, templateTransportId](const TransportId primitiveId) -> const std::string*
        {
            if (staticReticleId != 0)
            {
                return scene_.ResolvePrimitiveIdForReticle(staticReticleId, primitiveId);
            }

            if (templateTransportId != 0)
            {
                return scene_.ResolvePrimitiveIdForTemplate(templateTransportId, primitiveId);
            }

            return nullptr;
        };

        for (const auto& [primitiveTransportId, text] : patch.textsById)
        {
            const std::string* primitiveId = resolvePrimitiveId(primitiveTransportId);
            if (primitiveId == nullptr)
            {
                SetFailure("Unknown generated primitive transport id " + std::to_string(primitiveTransportId));
                return false;
            }

            const auto iterator = patch.texts.find(*primitiveId);
            if (iterator != patch.texts.end() && iterator->second != text)
            {
                SetFailure("Generated primitive transport id " + std::to_string(primitiveTransportId) +
                           " conflicts with an explicit primitive text override");
                return false;
            }

            patch.texts[*primitiveId] = text;
        }

        for (const auto& [primitiveTransportId, spacing] : patch.letterSpacingsById)
        {
            const std::string* primitiveId = resolvePrimitiveId(primitiveTransportId);
            if (primitiveId == nullptr)
            {
                SetFailure("Unknown generated primitive transport id " + std::to_string(primitiveTransportId));
                return false;
            }

            const auto iterator = patch.letterSpacings.find(*primitiveId);
            if (iterator != patch.letterSpacings.end() && iterator->second != spacing)
            {
                SetFailure("Generated primitive transport id " + std::to_string(primitiveTransportId) +
                           " conflicts with an explicit primitive letter spacing override");
                return false;
            }

            patch.letterSpacings[*primitiveId] = spacing;
        }

        for (const auto& [primitiveTransportId, primitivePatch] : patch.primitivePatchesById)
        {
            const std::string* primitiveId = resolvePrimitiveId(primitiveTransportId);
            if (primitiveId == nullptr)
            {
                SetFailure("Unknown generated primitive transport id " + std::to_string(primitiveTransportId));
                return false;
            }

            const auto iterator = patch.primitivePatches.find(*primitiveId);
            if (iterator != patch.primitivePatches.end())
            {
                SetFailure("Generated primitive transport id " + std::to_string(primitiveTransportId) +
                           " conflicts with an explicit primitive patch override");
                return false;
            }

            patch.primitivePatches[*primitiveId] = primitivePatch;
        }

        return true;
    };

    return std::visit(
        [this, &resolvePage, &resolveStaticReticle, &resolveTemplate, &resolvePatchPrimitiveIds](auto& value) -> bool
        {
            using Command = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Command, ActivatePageCommand>)
            {
                return resolvePage(value.page, value.pageId);
            }
            else if constexpr (std::is_same_v<Command, SetPageViewCommand>)
            {
                return resolvePage(value.page, value.pageId);
            }
            else if constexpr (std::is_same_v<Command, UpdateWindowDisplayCommand> ||
                               std::is_same_v<Command, ResetWindowCommand>)
            {
                return true;
            }
            else if constexpr (std::is_same_v<Command, UpdateReticleCommand>)
            {
                return resolveStaticReticle(value.target) &&
                       resolvePatchPrimitiveIds(value.patch, value.target.pageId, value.target.reticleId, 0);
            }
            else if constexpr (std::is_same_v<Command, UpdateStrobeCommand>)
            {
                return resolvePage(value.page, value.pageId);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticleCommand>)
            {
                return resolvePage(value.target.page, value.target.pageId) &&
                       resolveTemplate(value.templateId, value.templateTransportId) &&
                       !value.target.reticle.empty() &&
                       resolvePatchPrimitiveIds(value.patch, value.target.pageId, 0, value.templateTransportId);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
            {
                if (!resolvePage(value.page, value.pageId) ||
                    !resolveTemplate(value.templateId, value.templateTransportId))
                {
                    return false;
                }

                for (auto& state : value.reticles)
                {
                    if (state.reticleId.empty() ||
                        !resolvePatchPrimitiveIds(state.patch, value.pageId, 0, value.templateTransportId))
                    {
                        return false;
                    }
                }

                return true;
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetVisibilityCommand>)
            {
                return resolvePage(value.page, value.pageId) &&
                       resolveTemplate(value.templateId, value.templateTransportId);
            }
            else if constexpr (std::is_same_v<Command, RemoveDynamicReticleCommand>)
            {
                return resolvePage(value.target.page, value.target.pageId) && !value.target.reticle.empty();
            }
            else
            {
                return true;
            }
        },
        command);
}

void CommandProcessor::OnActivatePage(const ActivatePageCommand& command)
{
    if (!scene_.HasPage(command.page))
    {
        SetFailure("Unknown page: " + command.page);
        return;
    }

    scene_.SetActivePage(command.page);
}

void CommandProcessor::OnSetPageView(const SetPageViewCommand& command)
{
    if (!scene_.SetPageView(command.page, command.view))
    {
        SetFailure("Unable to update page view for page: " + command.page);
    }
}

void CommandProcessor::OnUpdateWindowDisplay(const UpdateWindowDisplayCommand& command)
{
    if (!scene_.ApplyWindowDisplayPatch(command.patch))
    {
        SetFailure("Unable to update whole-window display properties");
    }
}

void CommandProcessor::OnUpdateReticle(const UpdateReticleCommand& command)
{
    if (!scene_.ApplyReticlePatch(command.target.page, command.target.reticle, command.patch))
    {
        SetFailure("Unable to update reticle '" + command.target.reticle + "' on page '" + command.target.page + "'");
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
        SetFailure("Unable to update strobe on page '" + command.page + "'");
    }
}

void CommandProcessor::OnUpsertDynamicReticle(const UpsertDynamicReticleCommand& command)
{
    if (!scene_.HasPage(command.target.page))
    {
        SetFailure("Unknown page: " + command.target.page);
        return;
    }

    const auto templateIterator = scene_.Library().find(command.templateId);
    if (templateIterator == scene_.Library().end())
    {
        SetFailure("Unknown reticle template: " + command.templateId);
        return;
    }

    if (scene_.HasDynamicReticle(command.target.page, command.target.reticle))
    {
        if (!scene_.ApplyDynamicReticlePatch(command.target.page, command.target.reticle, command.patch))
        {
            SetFailure("Unable to update dynamic reticle '" + command.target.reticle + "'");
        }

        return;
    }

    ReticleGroup reticle = InstantiateReticle(templateIterator->second, command.target.reticle);
    scene_.UpsertDynamicReticle(command.target.page, std::move(reticle));

    if (!scene_.ApplyDynamicReticlePatch(command.target.page, command.target.reticle, command.patch))
    {
        SetFailure("Unable to initialize dynamic reticle '" + command.target.reticle + "'");
    }
}

void CommandProcessor::OnUpsertDynamicReticles(const UpsertDynamicReticlesCommand& command)
{
    if (!scene_.HasPage(command.page))
    {
        SetFailure("Unknown page: " + command.page);
        return;
    }

    const auto templateIterator = scene_.Library().find(command.templateId);
    if (templateIterator == scene_.Library().end())
    {
        SetFailure("Unknown reticle template: " + command.templateId);
        return;
    }

    for (const DynamicReticleState& state : command.reticles)
    {
        if (scene_.HasDynamicReticle(command.page, state.reticleId))
        {
            if (!scene_.ApplyDynamicReticlePatch(command.page, state.reticleId, state.patch))
            {
                SetFailure("Unable to update dynamic reticle '" + state.reticleId + "'");
                return;
            }

            continue;
        }

        ReticleGroup reticle = InstantiateReticle(templateIterator->second, state.reticleId);
        scene_.UpsertDynamicReticle(command.page, std::move(reticle));

        if (!scene_.ApplyDynamicReticlePatch(command.page, state.reticleId, state.patch))
        {
            SetFailure("Unable to initialize dynamic reticle '" + state.reticleId + "'");
            return;
        }
    }
}

void CommandProcessor::OnRemoveDynamicReticle(const RemoveDynamicReticleCommand& command)
{
    if (!scene_.RemoveDynamicReticle(command.target.page, command.target.reticle))
    {
        SetFailure("Unable to remove dynamic reticle '" + command.target.reticle +
                   "' from page '" + command.target.page + "'");
    }
}

void CommandProcessor::OnSetDynamicReticleSetVisibility(const SetDynamicReticleSetVisibilityCommand& command)
{
    if (!scene_.SetDynamicReticleSetVisible(command.page, command.templateId, command.visible))
    {
        SetFailure("Unable to update dynamic reticle set visibility for template '" + command.templateId +
                   "' on page '" + command.page + "'");
    }
}

void CommandProcessor::OnResetWindow(const ResetWindowCommand&)
{
    scene_.ResetToInitialState();
}

void CommandProcessor::SetFailure(std::string message)
{
    lastCommandSucceeded_ = false;
    lastError_ = std::move(message);
}
} // namespace mfd
