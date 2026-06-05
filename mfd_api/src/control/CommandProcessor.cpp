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
#include <vector>
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
constexpr std::uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

bool PatchUsesGeneratedIdentifiers(const ReticlePatch& patch) noexcept
{
    return patch.blinkTypeId.has_value() ||
           !patch.textsById.empty() ||
           !patch.letterSpacingsById.empty() ||
           !patch.primitivePatchesById.empty();
}

bool StaticHandleUsesGeneratedIdentifiers(const StaticReticleHandle& handle) noexcept
{
    return handle.pageId != 0 || handle.reticleId != 0;
}

bool DynamicHandleUsesGeneratedIdentifiers(const DynamicReticleHandle& handle) noexcept
{
    return handle.pageId != 0;
}

std::string MakeRuntimeDynamicReticleAlias(const RuntimeDynamicId runtimeReticleId)
{
    return "__runtime_dynamic_" + std::to_string(runtimeReticleId);
}

bool CommandUsesGeneratedIdentifiers(const UserCommand& command) noexcept
{
    return std::visit(
        [](const auto& value) noexcept -> bool
        {
            using Command = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Command, ActivatePageCommand> ||
                          std::is_same_v<Command, SetPageViewCommand>)
            {
                return value.pageId != 0;
            }
            else if constexpr (std::is_same_v<Command, UpdateStrobeCommand>)
            {
                return value.pageId != 0 || value.strobeId != 0;
            }
            else if constexpr (std::is_same_v<Command, UpdateReticleCommand>)
            {
                return StaticHandleUsesGeneratedIdentifiers(value.target) ||
                       PatchUsesGeneratedIdentifiers(value.patch);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticleCommand>)
            {
                return DynamicHandleUsesGeneratedIdentifiers(value.target) ||
                       value.templateTransportId != 0 ||
                       PatchUsesGeneratedIdentifiers(value.patch);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
            {
                if (value.pageId != 0 || value.templateTransportId != 0)
                {
                    return true;
                }

                for (const DynamicReticleState& state : value.reticles)
                {
                    if (PatchUsesGeneratedIdentifiers(state.patch))
                    {
                        return true;
                    }
                }

                return false;
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetVisibilityCommand>)
            {
                return value.pageId != 0 || value.templateTransportId != 0;
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetStrobeMagnetEnabledCommand>)
            {
                return value.pageId != 0 || value.templateTransportId != 0;
            }
            else if constexpr (std::is_same_v<Command, RemoveDynamicReticleCommand>)
            {
                return DynamicHandleUsesGeneratedIdentifiers(value.target);
            }
            else
            {
                return false;
            }
        },
        command);
}

std::size_t HashBatchFingerprintPayload(const std::string_view payload) noexcept
{
    std::uint64_t hash = kFnvOffsetBasis;
    for (const unsigned char byte : payload)
    {
        hash ^= static_cast<std::uint64_t>(byte);
        hash *= kFnvPrime;
    }

    return static_cast<std::size_t>(hash);
}

std::size_t BuildSequencedBatchFingerprint(const CommandBatch& batch)
{
    return HashBatchFingerprintPayload(SerializeCommandBatch(batch));
}

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
    dispatcher_.sink<SetDynamicReticleSetStrobeMagnetEnabledCommand>()
        .connect<&CommandProcessor::OnSetDynamicReticleSetStrobeMagnetEnabled>(*this);
    dispatcher_.sink<RemoveDynamicReticleCommand>().connect<&CommandProcessor::OnRemoveDynamicReticle>(*this);
    dispatcher_.sink<ResetWindowCommand>().connect<&CommandProcessor::OnResetWindow>(*this);
}

CommandProcessor::~CommandProcessor() = default;

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
            SetFailure("Client generated API requires the matching generated transport map loaded by the runtime window");
        }
        else
        {
            SetFailure("Generated transport map hash mismatch between the client batch and the runtime window");
        }

        return false;
    }

    std::optional<std::size_t> batchFingerprint;
    if (batch.sequence != 0 && !batch.mappingHash.empty())
    {
        auto& sequenceState = sequencedBatchesByMappingHash_[batch.mappingHash];
        if (batch.sequence < sequenceState.lastSequence)
        {
            SetFailure("Dropped stale or duplicate command batch");
            return false;
        }

        try
        {
            batchFingerprint = BuildSequencedBatchFingerprint(batch);
        }
        catch (const std::exception& exception)
        {
            SetFailure(exception.what());
            return false;
        }
        catch (...)
        {
            SetFailure("Unknown exception while fingerprinting a sequenced command batch");
            return false;
        }

        if (batch.sequence == sequenceState.lastSequence &&
            sequenceState.acceptedFingerprints.find(*batchFingerprint) != sequenceState.acceptedFingerprints.end())
        {
            SetFailure("Dropped stale or duplicate command batch");
            return false;
        }
    }

    if (batch.commands.size() <= 1U)
    {
        for (const UserCommand& command : batch.commands)
        {
            if (!SubmitResolved(command, batch.mappingHash))
            {
                return false;
            }
        }
    }
    else
    {
        std::vector<UserCommand> resolvedCommands;
        resolvedCommands.reserve(batch.commands.size());

        for (const UserCommand& command : batch.commands)
        {
            UserCommand resolved = command;
            if (!ResolveCommandIdentifiers(resolved, batch.mappingHash))
            {
                return false;
            }

            resolvedCommands.push_back(std::move(resolved));
        }

        const SceneRegistry::RuntimeSnapshot snapshot = scene_.CaptureRuntimeSnapshot();
        for (const UserCommand& command : resolvedCommands)
        {
            if (DispatchResolved(command))
            {
                continue;
            }

            const std::string failure = lastError_;
            try
            {
                scene_.RestoreRuntimeSnapshot(snapshot);
            }
            catch (const std::exception& exception)
            {
                SetFailure(failure + " (runtime rollback failed: " + std::string(exception.what()) + ")");
                return false;
            }
            catch (...)
            {
                SetFailure(failure + " (runtime rollback failed: unknown exception)");
                return false;
            }

            lastCommandSucceeded_ = false;
            lastError_ = failure;
            return false;
        }
    }

    if (batch.sequence != 0 && !batch.mappingHash.empty())
    {
        auto& sequenceState = sequencedBatchesByMappingHash_[batch.mappingHash];
        if (batch.sequence > sequenceState.lastSequence)
        {
            sequenceState.lastSequence = batch.sequence;
            sequenceState.acceptedFingerprints.clear();
        }

        if (batchFingerprint.has_value())
        {
            sequenceState.acceptedFingerprints.insert(*batchFingerprint);
        }
    }

    return true;
}

bool CommandProcessor::SubmitResolved(UserCommand command, const std::string_view mappingHash)
{
    if (!ResolveCommandIdentifiers(command, mappingHash))
    {
        return false;
    }

    return DispatchResolved(command);
}

bool CommandProcessor::DispatchResolved(const UserCommand& command)
{
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
    lastCommandSucceeded_ = true;
    lastError_.clear();

    if (commands.size() <= 1U)
    {
        for (const UserCommand& command : commands)
        {
            if (!SubmitResolved(command, {}))
            {
                return false;
            }
        }
    }
    else
    {
        std::vector<UserCommand> resolvedCommands;
        resolvedCommands.reserve(commands.size());

        for (const UserCommand& command : commands)
        {
            UserCommand resolved = command;
            if (!ResolveCommandIdentifiers(resolved, {}))
            {
                return false;
            }

            resolvedCommands.push_back(std::move(resolved));
        }

        const SceneRegistry::RuntimeSnapshot snapshot = scene_.CaptureRuntimeSnapshot();
        for (const UserCommand& command : resolvedCommands)
        {
            if (DispatchResolved(command))
            {
                continue;
            }

            const std::string failure = lastError_;
            try
            {
                scene_.RestoreRuntimeSnapshot(snapshot);
            }
            catch (const std::exception& exception)
            {
                SetFailure(failure + " (runtime rollback failed: " + std::string(exception.what()) + ")");
                return false;
            }
            catch (...)
            {
                SetFailure(failure + " (runtime rollback failed: unknown exception)");
                return false;
            }

            lastCommandSucceeded_ = false;
            lastError_ = failure;
            return false;
        }
    }

    return true;
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

bool CommandProcessor::ResolveGeneratedPage(std::string& page, TransportId& pageId)
{
    if (pageId == 0)
    {
        if (page.empty())
        {
            return false;
        }

        pageId = scene_.ResolvePageTransportId(page);
        if (pageId != 0)
        {
            if (const std::string* resolvedPage = scene_.ResolvePageName(pageId); resolvedPage != nullptr)
            {
                page = *resolvedPage;
            }
        }

        return true;
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
}

bool CommandProcessor::ResolveGeneratedStrobe(const TransportId pageId,
                                              std::string& strobeName,
                                              const TransportId strobeId)
{
    if (strobeId == 0)
    {
        return strobeName.empty() || !NormalizePageName(strobeName).empty();
    }

    const std::string* resolvedStrobe = scene_.ResolveStrobeName(pageId, strobeId);
    if (resolvedStrobe == nullptr)
    {
        SetFailure("Unknown generated strobe transport id " + std::to_string(strobeId));
        return false;
    }

    if (!strobeName.empty() && NormalizePageName(strobeName) != NormalizePageName(*resolvedStrobe))
    {
        SetFailure("Generated strobe transport id " + std::to_string(strobeId) +
                   " does not match strobe '" + strobeName + "'");
        return false;
    }

    strobeName = *resolvedStrobe;
    return true;
}

bool CommandProcessor::ResolveGeneratedStaticReticle(StaticReticleHandle& target)
{
    if (target.reticleId == 0)
    {
        return ResolveGeneratedPage(target.page, target.pageId) && !target.reticle.empty();
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
}

bool CommandProcessor::ResolveGeneratedDynamicReticle(DynamicReticleHandle& target)
{
    if (!ResolveGeneratedPage(target.page, target.pageId))
    {
        return false;
    }

    if (target.runtimeReticleId != 0)
    {
        if (target.reticleId.empty())
        {
            target.reticleId = MakeRuntimeDynamicReticleAlias(target.runtimeReticleId);
        }

        return true;
    }

    return !target.reticleId.empty();
}

bool CommandProcessor::ResolveGeneratedTemplate(std::string& templateId, const TransportId templateTransportId)
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
}

const std::string* CommandProcessor::ResolveGeneratedPrimitiveId(const TransportId staticReticleId,
                                                                 const TransportId templateTransportId,
                                                                 const TransportId primitiveId) const noexcept
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
}

bool CommandProcessor::ResolveGeneratedPatchPrimitiveIds(ReticlePatch& patch,
                                                         const TransportId pageId,
                                                         const TransportId staticReticleId,
                                                         const TransportId templateTransportId)
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

    for (const auto& [primitiveTransportId, text] : patch.textsById)
    {
        const std::string* primitiveId =
            ResolveGeneratedPrimitiveId(staticReticleId, templateTransportId, primitiveTransportId);
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
        const std::string* primitiveId =
            ResolveGeneratedPrimitiveId(staticReticleId, templateTransportId, primitiveTransportId);
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
        const std::string* primitiveId =
            ResolveGeneratedPrimitiveId(staticReticleId, templateTransportId, primitiveTransportId);
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
}

bool CommandProcessor::ResolveCommandIdentifiers(UserCommand& command, const std::string_view mappingHash)
{
    if (mappingHash.empty())
    {
        if (CommandUsesGeneratedIdentifiers(command))
        {
            SetFailure("Generated transport ids require a non-empty batch mapping hash");
            return false;
        }

        return true;
    }

    return std::visit(
        [this](auto& value) -> bool
        {
            using Command = std::decay_t<decltype(value)>;

            if constexpr (std::is_same_v<Command, ActivatePageCommand> ||
                          std::is_same_v<Command, SetPageViewCommand>)
            {
                return ResolveGeneratedPage(value.page, value.pageId);
            }
            else if constexpr (std::is_same_v<Command, UpdateWindowDisplayCommand> ||
                               std::is_same_v<Command, ResetWindowCommand>)
            {
                return true;
            }
            else if constexpr (std::is_same_v<Command, UpdateReticleCommand>)
            {
                return ResolveGeneratedStaticReticle(value.target) &&
                       ResolveGeneratedPatchPrimitiveIds(value.patch, value.target.pageId, value.target.reticleId, 0);
            }
            else if constexpr (std::is_same_v<Command, UpdateStrobeCommand>)
            {
                return ResolveGeneratedPage(value.page, value.pageId) &&
                       ResolveGeneratedStrobe(value.pageId, value.strobe, value.strobeId);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticleCommand>)
            {
                return ResolveGeneratedDynamicReticle(value.target) &&
                       ResolveGeneratedTemplate(value.templateId, value.templateTransportId) &&
                       ResolveGeneratedPatchPrimitiveIds(value.patch, value.target.pageId, 0, value.templateTransportId);
            }
            else if constexpr (std::is_same_v<Command, UpsertDynamicReticlesCommand>)
            {
                if (!ResolveGeneratedPage(value.page, value.pageId) ||
                    !ResolveGeneratedTemplate(value.templateId, value.templateTransportId))
                {
                    return false;
                }

                for (auto& state : value.reticles)
                {
                    if (state.runtimeReticleId != 0 && state.reticleId.empty())
                    {
                        state.reticleId = MakeRuntimeDynamicReticleAlias(state.runtimeReticleId);
                    }

                    if ((state.runtimeReticleId == 0 && state.reticleId.empty()) ||
                        !ResolveGeneratedPatchPrimitiveIds(state.patch, value.pageId, 0, value.templateTransportId))
                    {
                        return false;
                    }
                }

                return true;
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetVisibilityCommand>)
            {
                return ResolveGeneratedPage(value.page, value.pageId) &&
                       ResolveGeneratedTemplate(value.templateId, value.templateTransportId);
            }
            else if constexpr (std::is_same_v<Command, SetDynamicReticleSetStrobeMagnetEnabledCommand>)
            {
                return ResolveGeneratedPage(value.page, value.pageId) &&
                       ResolveGeneratedTemplate(value.templateId, value.templateTransportId);
            }
            else if constexpr (std::is_same_v<Command, RemoveDynamicReticleCommand>)
            {
                return ResolveGeneratedDynamicReticle(value.target);
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
    const SceneRegistry::RuntimeSnapshot snapshot = scene_.CaptureRuntimeSnapshot();
    bool success = true;

    if (!command.strobe.empty())
    {
        success = scene_.SelectStrobe(command.page, command.strobe);
    }

    if (success && command.active.has_value())
    {
        success = scene_.SetStrobeActive(command.page, *command.active);
    }

    if (success && command.position.has_value())
    {
        success = scene_.SetStrobePosition(command.page, *command.position);
    }

    if (success)
    {
        return;
    }

    const std::string failure = "Unable to update strobe on page '" + command.page + "'";
    try
    {
        scene_.RestoreRuntimeSnapshot(snapshot);
    }
    catch (const std::exception& exception)
    {
        SetFailure(failure + " (runtime rollback failed: " + std::string(exception.what()) + ")");
        return;
    }
    catch (...)
    {
        SetFailure(failure + " (runtime rollback failed: unknown exception)");
        return;
    }

    SetFailure(failure);
}

void CommandProcessor::OnUpsertDynamicReticle(const UpsertDynamicReticleCommand& command)
{
    const std::string normalizedPageName = mfd::NormalizePageName(command.target.page);
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

    const DynamicReticleLayerBinding* binding =
        scene_.FindDynamicReticleLayerBinding(normalizedPageName, command.templateId);
    if (binding == nullptr)
    {
        SetFailure("Dynamic reticle template '" + command.templateId +
                   "' is not bound on page '" + command.target.page + "'");
        return;
    }

    if (scene_.HasDynamicReticle(command.target.page, command.target.reticleId) &&
        !scene_.DynamicReticleUsesTemplate(command.target.page, command.target.reticleId, command.templateId))
    {
        SetFailure("Dynamic reticle '" + command.target.reticleId + "' on page '" + command.target.page +
                   "' already belongs to another template");
        return;
    }

    if (scene_.HasDynamicReticle(command.target.page, command.target.reticleId))
    {
        scene_.SetDynamicReticleRuntimeIdentifiers(
            normalizedPageName,
            command.target.reticleId,
            command.target.runtimeReticleId,
            command.templateTransportId);
        if (!scene_.ApplyDynamicReticlePatch(command.target.page, command.target.reticleId, command.patch))
        {
            SetFailure("Unable to update dynamic reticle '" + command.target.reticleId + "'");
        }

        return;
    }

    ReticleGroup reticle = InstantiateReticle(templateIterator->second, command.target.reticleId);
    reticle.layerId = binding->layerId;
    scene_.UpsertDynamicReticle(command.target.page, std::move(reticle));
    scene_.SetDynamicReticleRuntimeIdentifiers(
        normalizedPageName,
        command.target.reticleId,
        command.target.runtimeReticleId,
        command.templateTransportId);

    if (!scene_.ApplyDynamicReticlePatch(command.target.page, command.target.reticleId, command.patch))
    {
        SetFailure("Unable to initialize dynamic reticle '" + command.target.reticleId + "'");
    }
}

void CommandProcessor::OnUpsertDynamicReticles(const UpsertDynamicReticlesCommand& command)
{
    const std::string normalizedPageName = mfd::NormalizePageName(command.page);
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

    const DynamicReticleLayerBinding* binding =
        scene_.FindDynamicReticleLayerBinding(normalizedPageName, command.templateId);
    if (binding == nullptr)
    {
        SetFailure("Dynamic reticle template '" + command.templateId +
                   "' is not bound on page '" + command.page + "'");
        return;
    }

    for (const DynamicReticleState& state : command.reticles)
    {
        if (scene_.HasDynamicReticle(command.page, state.reticleId) &&
            !scene_.DynamicReticleUsesTemplate(command.page, state.reticleId, command.templateId))
        {
            SetFailure("Dynamic reticle '" + state.reticleId + "' on page '" + command.page +
                       "' already belongs to another template");
            return;
        }

        if (scene_.HasDynamicReticle(command.page, state.reticleId))
        {
            scene_.SetDynamicReticleRuntimeIdentifiers(
                normalizedPageName,
                state.reticleId,
                state.runtimeReticleId,
                command.templateTransportId);
            if (!scene_.ApplyDynamicReticlePatch(command.page, state.reticleId, state.patch))
            {
                SetFailure("Unable to update dynamic reticle '" + state.reticleId + "'");
                return;
            }

            continue;
        }

        ReticleGroup reticle = InstantiateReticle(templateIterator->second, state.reticleId);
        reticle.layerId = binding->layerId;
        scene_.UpsertDynamicReticle(command.page, std::move(reticle));
        scene_.SetDynamicReticleRuntimeIdentifiers(
            normalizedPageName,
            state.reticleId,
            state.runtimeReticleId,
            command.templateTransportId);

        if (!scene_.ApplyDynamicReticlePatch(command.page, state.reticleId, state.patch))
        {
            SetFailure("Unable to initialize dynamic reticle '" + state.reticleId + "'");
            return;
        }
    }
}

void CommandProcessor::OnRemoveDynamicReticle(const RemoveDynamicReticleCommand& command)
{
    if (!scene_.RemoveDynamicReticle(command.target.page, command.target.reticleId))
    {
        SetFailure("Unable to remove dynamic reticle '" + command.target.reticleId +
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

void CommandProcessor::OnSetDynamicReticleSetStrobeMagnetEnabled(
    const SetDynamicReticleSetStrobeMagnetEnabledCommand& command)
{
    if (!scene_.SetDynamicReticleSetStrobeMagnetEnabled(command.page, command.templateId, command.enabled))
    {
        SetFailure("Unable to update dynamic reticle set strobe-magnet eligibility for template '" +
                   command.templateId + "' on page '" + command.page + "'");
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
