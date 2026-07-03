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
#include <utility>

#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/model/Reticle.h"
#include "mfd/runtime/SceneRegistry.h"
#include "mfd/control/internal/CommandIdentifierHelpers.h"
#include "mfd/control/internal/CommandTraits.h"

namespace mfd
{
namespace
{
constexpr std::size_t kMaxCommandsPerPoll = 64;
// Upper bound on the number of distinct chunked batches retained for a single sequence value. A client
// may legitimately send several distinct batches under the same sequence (chunked payloads), so the
// fingerprint history must grow, but it is cleared only when the sequence advances. Without this cap a
// client that keeps the same sequence and varies the payload could grow the history without bound on the
// UDP boundary. The cap is generous for legitimate chunking while keeping the per-sequence history finite.
constexpr std::size_t kMaxFingerprintsPerSequence = 256;
constexpr std::uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::string MakeRuntimeDynamicReticleAlias(const RuntimeDynamicId runtimeReticleId)
{
    return "__runtime_dynamic_" + std::to_string(runtimeReticleId);
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

// Collects the pages a command can mutate, with one explicit overload per command type, so a
// transactional batch only snapshots the touched pages instead of the whole scene. This
// visitor intentionally has no generic fallback: adding a new UserCommand alternative must
// fail to compile here until its rollback footprint is written.
struct CommandRollbackFootprintCollector
{
    std::vector<std::string>& touchedPages;
    bool& touchesWholeScene;
    bool& touchesActivePageSelection;

    void operator()(const ActivatePageCommand& command) const
    {
        // Switching pages also mutates the previously active page (active flag, sticky
        // strobe refresh), so the current active page joins the footprint at capture time.
        touchedPages.push_back(*detail::PageBindingOf(command).pageName);
        touchesActivePageSelection = true;
    }

    void operator()(const SetPageViewCommand& command) const
    {
        touchedPages.push_back(*detail::PageBindingOf(command).pageName);
    }

    void operator()(const UpdateWindowDisplayCommand&) const noexcept
    {
        // Whole-window display state is scene-wide and always captured with the snapshot.
    }

    void operator()(const UpdateReticleCommand& command) const
    {
        touchedPages.push_back(*detail::PageBindingOf(command).pageName);
    }

    void operator()(const UpdateStrobeCommand& command) const
    {
        touchedPages.push_back(*detail::PageBindingOf(command).pageName);
    }

    void operator()(const UpsertDynamicReticleCommand& command) const
    {
        touchedPages.push_back(*detail::PageBindingOf(command).pageName);
    }

    void operator()(const UpsertDynamicReticlesCommand& command) const
    {
        touchedPages.push_back(*detail::PageBindingOf(command).pageName);
    }

    void operator()(const SetDynamicReticleSetVisibilityCommand& command) const
    {
        touchedPages.push_back(*detail::PageBindingOf(command).pageName);
    }

    void operator()(const SetDynamicReticleSetStrobeMagnetEnabledCommand& command) const
    {
        touchedPages.push_back(*detail::PageBindingOf(command).pageName);
    }

    void operator()(const RemoveDynamicReticleCommand& command) const
    {
        touchedPages.push_back(*detail::PageBindingOf(command).pageName);
    }

    void operator()(const ResetWindowCommand&) const noexcept
    {
        touchesWholeScene = true;
    }
};

std::vector<std::string> BuildRollbackPageKeys(const std::vector<std::string>& touchedPages,
                                               const std::string& activePageName)
{
    std::vector<std::string> normalizedPageKeys;
    normalizedPageKeys.reserve(touchedPages.size() + 1U);
    for (const std::string& pageName : touchedPages)
    {
        std::string pageKey = NormalizePageName(pageName);
        if (!pageKey.empty())
        {
            normalizedPageKeys.push_back(std::move(pageKey));
        }
    }

    if (!activePageName.empty())
    {
        normalizedPageKeys.push_back(NormalizePageName(activePageName));
    }

    return normalizedPageKeys;
}

} // namespace

CommandProcessor::CommandProcessor(SceneRegistry& scene)
    : scene_(scene)
{
}

CommandProcessor::~CommandProcessor() = default;

bool CommandProcessor::Submit(const UserCommand& command)
{
    lastError_.clear();

    return SubmitResolved(command, {});
}

bool CommandProcessor::Submit(const CommandBatch& batch)
{
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

        if (batch.sequence == sequenceState.lastSequence &&
            sequenceState.acceptedFingerprints.size() >= kMaxFingerprintsPerSequence)
        {
            SetFailure("Too many distinct command batches retained for the same sequence");
            return false;
        }
    }

    if (!SubmitCommandsWithCurrentTransactionMode(batch.commands, batch.mappingHash))
    {
        return false;
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

void CommandProcessor::SetBatchTransactionMode(const CommandBatchTransactionMode mode) noexcept
{
    batchTransactionMode_ = mode;
}

CommandBatchTransactionMode CommandProcessor::BatchTransactionMode() const noexcept
{
    return batchTransactionMode_;
}

bool CommandProcessor::SubmitCommandsWithCurrentTransactionMode(const ArrayView<const UserCommand> commands,
                                                               const std::string_view mappingHash)
{
    if (commands.size() <= 1U)
    {
        return SubmitCommandsNonTransactional(commands, mappingHash);
    }

    if (batchTransactionMode_ == CommandBatchTransactionMode::NonTransactional)
    {
        return SubmitCommandsNonTransactional(commands, mappingHash);
    }

    return SubmitCommandsTransactional(commands, mappingHash);
}

bool CommandProcessor::SubmitCommandsTransactional(const ArrayView<const UserCommand> commands,
                                                   const std::string_view mappingHash)
{
    std::vector<UserCommand> resolvedCommands;
    if (!ResolveBatchCommands(commands, mappingHash, resolvedCommands))
    {
        return false;
    }

    std::vector<std::string> touchedPages;
    touchedPages.reserve(resolvedCommands.size());
    bool touchesWholeScene = false;
    bool touchesActivePageSelection = false;
    for (const UserCommand& command : resolvedCommands)
    {
        std::visit(
            CommandRollbackFootprintCollector {touchedPages, touchesWholeScene, touchesActivePageSelection},
            command);
    }

    const std::string activePageName = touchesActivePageSelection ? scene_.ActivePageName() : std::string {};
    const SceneRegistry::RuntimeSnapshot snapshot =
        touchesWholeScene
            ? scene_.CaptureRuntimeSnapshot()
            : scene_.CaptureRuntimeSnapshotForPages(BuildRollbackPageKeys(touchedPages, activePageName));
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

        lastError_ = failure;
        return false;
    }

    return true;
}

bool CommandProcessor::SubmitCommandsNonTransactional(const ArrayView<const UserCommand> commands,
                                                      const std::string_view mappingHash)
{
    for (const UserCommand& command : commands)
    {
        if (!SubmitResolved(command, mappingHash))
        {
            return false;
        }
    }

    return true;
}

bool CommandProcessor::ResolveBatchCommands(const ArrayView<const UserCommand> commands,
                                            const std::string_view mappingHash,
                                            std::vector<UserCommand>& resolvedCommands)
{
    resolvedCommands.clear();
    resolvedCommands.reserve(commands.size());

    for (const UserCommand& command : commands)
    {
        UserCommand resolved = command;
        if (!ResolveCommandIdentifiers(resolved, mappingHash))
        {
            return false;
        }

        resolvedCommands.push_back(std::move(resolved));
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

/**
 * @brief Routes one resolved command to its handler with one explicit overload per command type.
 *
 * @note This visitor intentionally has no generic fallback: adding a new `UserCommand`
 * alternative must fail to compile here until its runtime handler is written.
 */
struct CommandProcessor::ResolvedCommandDispatcher
{
    CommandProcessor& processor;

    CommandResult operator()(const ActivatePageCommand& command) const
    {
        return processor.OnActivatePage(command);
    }

    CommandResult operator()(const SetPageViewCommand& command) const
    {
        return processor.OnSetPageView(command);
    }

    CommandResult operator()(const UpdateWindowDisplayCommand& command) const
    {
        return processor.OnUpdateWindowDisplay(command);
    }

    CommandResult operator()(const UpdateReticleCommand& command) const
    {
        return processor.OnUpdateReticle(command);
    }

    CommandResult operator()(const UpdateStrobeCommand& command) const
    {
        return processor.OnUpdateStrobe(command);
    }

    CommandResult operator()(const UpsertDynamicReticleCommand& command) const
    {
        return processor.OnUpsertDynamicReticle(command);
    }

    CommandResult operator()(const UpsertDynamicReticlesCommand& command) const
    {
        return processor.OnUpsertDynamicReticles(command);
    }

    CommandResult operator()(const SetDynamicReticleSetVisibilityCommand& command) const
    {
        return processor.OnSetDynamicReticleSetVisibility(command);
    }

    CommandResult operator()(const SetDynamicReticleSetStrobeMagnetEnabledCommand& command) const
    {
        return processor.OnSetDynamicReticleSetStrobeMagnetEnabled(command);
    }

    CommandResult operator()(const RemoveDynamicReticleCommand& command) const
    {
        return processor.OnRemoveDynamicReticle(command);
    }

    CommandResult operator()(const ResetWindowCommand& command) const
    {
        return processor.OnResetWindow(command);
    }
};

bool CommandProcessor::DispatchResolved(const UserCommand& command)
{
    CommandResult result = CommandResult::Success();
    try
    {
        result = std::visit(ResolvedCommandDispatcher {*this}, command);
    }
    catch (const std::exception& exception)
    {
        result = CommandResult::Failure(exception.what());
    }
    catch (...)
    {
        result = CommandResult::Failure("Unknown exception while dispatching a command");
    }

    if (!result.succeeded)
    {
        lastError_ = std::move(result.error);
        return false;
    }

    return true;
}

bool CommandProcessor::Submit(const ArrayView<const UserCommand> commands)
{
    lastError_.clear();

    return SubmitCommandsWithCurrentTransactionMode(commands, {});
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

    if (!page.empty() && !PageNamesEqual(page, *resolvedPage))
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
        return strobeName.empty() || !PageNameIsBlank(strobeName);
    }

    const std::string* resolvedStrobe = scene_.ResolveStrobeName(pageId, strobeId);
    if (resolvedStrobe == nullptr)
    {
        SetFailure("Unknown generated strobe transport id " + std::to_string(strobeId));
        return false;
    }

    if (!strobeName.empty() && !PageNamesEqual(strobeName, *resolvedStrobe))
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

    if (!target.page.empty() && !PageNamesEqual(target.page, resolvedReticle->pageName))
    {
        SetFailure("Generated static reticle transport id " + std::to_string(target.reticleId) +
                   " does not belong to page '" + target.page + "'");
        return false;
    }

    if (!target.reticle.empty() && !PageNamesEqual(target.reticle, resolvedReticle->reticleId))
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

    if (!templateId.empty() && !PageNamesEqual(templateId, *resolvedTemplate))
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
                !PageNamesEqual(*patch.blinkType, *resolvedBlinkType))
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

/**
 * @brief Resolves generated transport identifiers with one explicit overload per command type.
 *
 * @note This visitor intentionally has no generic fallback: adding a new `UserCommand`
 * alternative must fail to compile here until its runtime resolution rule is written.
 */
struct CommandProcessor::IdentifierResolver
{
    CommandProcessor& processor;

    bool operator()(ActivatePageCommand& command) const
    {
        return processor.ResolveGeneratedPage(command.page, command.pageId);
    }

    bool operator()(SetPageViewCommand& command) const
    {
        return processor.ResolveGeneratedPage(command.page, command.pageId);
    }

    bool operator()(UpdateWindowDisplayCommand&) const noexcept
    {
        return true;
    }

    bool operator()(UpdateReticleCommand& command) const
    {
        return processor.ResolveGeneratedStaticReticle(command.target) &&
               processor.ResolveGeneratedPatchPrimitiveIds(
                   command.patch, command.target.pageId, command.target.reticleId, 0);
    }

    bool operator()(UpdateStrobeCommand& command) const
    {
        return processor.ResolveGeneratedPage(command.page, command.pageId) &&
               processor.ResolveGeneratedStrobe(command.pageId, command.strobe, command.strobeId);
    }

    bool operator()(UpsertDynamicReticleCommand& command) const
    {
        return processor.ResolveGeneratedDynamicReticle(command.target) &&
               processor.ResolveGeneratedTemplate(command.templateId, command.templateTransportId) &&
               processor.ResolveGeneratedPatchPrimitiveIds(
                   command.patch, command.target.pageId, 0, command.templateTransportId);
    }

    bool operator()(UpsertDynamicReticlesCommand& command) const
    {
        if (!processor.ResolveGeneratedPage(command.page, command.pageId) ||
            !processor.ResolveGeneratedTemplate(command.templateId, command.templateTransportId))
        {
            return false;
        }

        for (DynamicReticleState& state : command.reticles)
        {
            if (state.runtimeReticleId != 0 && state.reticleId.empty())
            {
                state.reticleId = MakeRuntimeDynamicReticleAlias(state.runtimeReticleId);
            }

            if ((state.runtimeReticleId == 0 && state.reticleId.empty()) ||
                !processor.ResolveGeneratedPatchPrimitiveIds(state.patch, command.pageId, 0, command.templateTransportId))
            {
                return false;
            }
        }

        return true;
    }

    bool operator()(SetDynamicReticleSetVisibilityCommand& command) const
    {
        return processor.ResolveGeneratedPage(command.page, command.pageId) &&
               processor.ResolveGeneratedTemplate(command.templateId, command.templateTransportId);
    }

    bool operator()(SetDynamicReticleSetStrobeMagnetEnabledCommand& command) const
    {
        return processor.ResolveGeneratedPage(command.page, command.pageId) &&
               processor.ResolveGeneratedTemplate(command.templateId, command.templateTransportId);
    }

    bool operator()(RemoveDynamicReticleCommand& command) const
    {
        return processor.ResolveGeneratedDynamicReticle(command.target);
    }

    bool operator()(ResetWindowCommand&) const noexcept
    {
        return true;
    }
};

bool CommandProcessor::ResolveCommandIdentifiers(UserCommand& command, const std::string_view mappingHash)
{
    if (mappingHash.empty())
    {
        if (detail::CommandUsesGeneratedIdentifiers(command))
        {
            SetFailure("Generated transport ids require a non-empty batch mapping hash");
            return false;
        }

        return true;
    }

    return std::visit(IdentifierResolver {*this}, command);
}

CommandProcessor::CommandResult CommandProcessor::OnActivatePage(const ActivatePageCommand& command)
{
    if (!scene_.HasPage(command.page))
    {
        return CommandResult::Failure("Unknown page: " + command.page);
    }

    scene_.SetActivePage(command.page);
    return CommandResult::Success();
}

CommandProcessor::CommandResult CommandProcessor::OnSetPageView(const SetPageViewCommand& command)
{
    if (!scene_.SetPageView(command.page, command.view))
    {
        return CommandResult::Failure("Unable to update page view for page: " + command.page);
    }

    return CommandResult::Success();
}

CommandProcessor::CommandResult CommandProcessor::OnUpdateWindowDisplay(const UpdateWindowDisplayCommand& command)
{
    if (!scene_.ApplyWindowDisplayPatch(command.patch))
    {
        return CommandResult::Failure("Unable to update whole-window display properties");
    }

    return CommandResult::Success();
}

CommandProcessor::CommandResult CommandProcessor::OnUpdateReticle(const UpdateReticleCommand& command)
{
    const auto reticleRef =
        scene_.ResolveStaticReticleRef(command.target.reticleId, command.target.page, command.target.reticle);
    if (!reticleRef.has_value() || !scene_.ApplyReticlePatchByRef(*reticleRef, command.patch))
    {
        return CommandResult::Failure(
            "Unable to update reticle '" + command.target.reticle + "' on page '" + command.target.page + "'");
    }

    return CommandResult::Success();
}

CommandProcessor::CommandResult CommandProcessor::OnUpdateStrobe(const UpdateStrobeCommand& command)
{
    // The update validates all preconditions before mutating, so a failure leaves the strobe
    // untouched without any whole-scene snapshot or rollback. Genuine multi-command batches
    // still get their transactional snapshot in SubmitBatch.
    const auto pageRef = scene_.ResolvePageRef(command.pageId, command.page);
    if (!pageRef.has_value() ||
        !scene_.ApplyStrobeUpdateByKey(pageRef->normalizedName, command.strobe, command.active, command.position))
    {
        return CommandResult::Failure("Unable to update strobe on page '" + command.page + "'");
    }

    return CommandResult::Success();
}

CommandProcessor::CommandResult CommandProcessor::OnUpsertDynamicReticle(const UpsertDynamicReticleCommand& command)
{
    // The page and template identities are resolved once (generated id first, authored
    // name as fallback); every scene call below reuses the resolved refs instead of
    // renormalizing names per call.
    const auto pageRef = scene_.ResolvePageRef(command.target.pageId, command.target.page);
    if (!pageRef.has_value())
    {
        return CommandResult::Failure("Unknown page: " + command.target.page);
    }

    const auto templateRef = scene_.ResolveTemplateRef(command.templateTransportId, command.templateId);
    if (!templateRef.has_value())
    {
        return CommandResult::Failure("Unknown reticle template: " + command.templateId);
    }

    const DynamicReticleLayerBinding* binding =
        scene_.FindDynamicReticleLayerBinding(pageRef->normalizedName, *templateRef->templateId);
    if (binding == nullptr)
    {
        return CommandResult::Failure("Dynamic reticle template '" + command.templateId +
                                      "' is not bound on page '" + command.target.page + "'");
    }

    const SceneRegistry::DynamicReticleUpsertCheck check = scene_.CheckDynamicReticleUpsert(
        pageRef->normalizedName, command.target.reticleId, *templateRef->templateId, *templateRef->reticle, command.patch);
    if (check != SceneRegistry::DynamicReticleUpsertCheck::Applicable)
    {
        return CommandResult::Failure(
            SceneRegistry::DescribeDynamicReticleUpsertCheck(check, command.target.reticleId, command.target.page));
    }

    if (scene_.HasDynamicReticleByKey(pageRef->normalizedName, command.target.reticleId))
    {
        scene_.SetDynamicReticleRuntimeIdentifiers(
            pageRef->normalizedName,
            command.target.reticleId,
            command.target.runtimeReticleId,
            command.templateTransportId);
        if (!scene_.ApplyDynamicReticlePatchByKey(pageRef->normalizedName, command.target.reticleId, command.patch))
        {
            return CommandResult::Failure("Unable to update dynamic reticle '" + command.target.reticleId + "'");
        }

        return CommandResult::Success();
    }

    ReticleGroup reticle = InstantiateReticle(*templateRef->reticle, command.target.reticleId);
    reticle.layerId = binding->layerId;
    scene_.UpsertDynamicReticleByKey(pageRef->normalizedName, std::move(reticle));
    scene_.SetDynamicReticleRuntimeIdentifiers(
        pageRef->normalizedName,
        command.target.reticleId,
        command.target.runtimeReticleId,
        command.templateTransportId);

    if (!scene_.ApplyDynamicReticlePatchByKey(pageRef->normalizedName, command.target.reticleId, command.patch))
    {
        return CommandResult::Failure("Unable to initialize dynamic reticle '" + command.target.reticleId + "'");
    }

    return CommandResult::Success();
}

CommandProcessor::CommandResult CommandProcessor::OnUpsertDynamicReticles(const UpsertDynamicReticlesCommand& command)
{
    // The page and template identities are resolved once for the whole bulk command
    // (generated id first, authored name as fallback): every per-reticle scene call below
    // reuses the resolved refs through the private *ByKey entry points, so the hottest
    // dynamic update path performs no per-reticle name normalization or string rebuild.
    const auto pageRef = scene_.ResolvePageRef(command.pageId, command.page);
    if (!pageRef.has_value())
    {
        return CommandResult::Failure("Unknown page: " + command.page);
    }

    const auto templateRef = scene_.ResolveTemplateRef(command.templateTransportId, command.templateId);
    if (!templateRef.has_value())
    {
        return CommandResult::Failure("Unknown reticle template: " + command.templateId);
    }

    const DynamicReticleLayerBinding* binding =
        scene_.FindDynamicReticleLayerBinding(pageRef->normalizedName, *templateRef->templateId);
    if (binding == nullptr)
    {
        return CommandResult::Failure("Dynamic reticle template '" + command.templateId +
                                      "' is not bound on page '" + command.page + "'");
    }

    // Validation phase: prove every reticle of the bulk command will apply before mutating
    // anything, so one failing reticle cannot leave earlier reticles of the same command
    // applied. A single-command batch never takes the transactional multi-command path,
    // which makes this pre-mutation check the atomicity guarantee of the command itself.
    for (const DynamicReticleState& state : command.reticles)
    {
        const SceneRegistry::DynamicReticleUpsertCheck check = scene_.CheckDynamicReticleUpsert(
            pageRef->normalizedName, state.reticleId, *templateRef->templateId, *templateRef->reticle, state.patch);
        if (check != SceneRegistry::DynamicReticleUpsertCheck::Applicable)
        {
            return CommandResult::Failure(SceneRegistry::DescribeDynamicReticleUpsertCheck(check, state.reticleId, command.page));
        }
    }

    // Mutation phase: the checks above mirror every failure condition of the calls below,
    // so the remaining failure returns are defensive and not expected to trigger.
    for (const DynamicReticleState& state : command.reticles)
    {
        if (scene_.HasDynamicReticleByKey(pageRef->normalizedName, state.reticleId))
        {
            scene_.SetDynamicReticleRuntimeIdentifiers(
                pageRef->normalizedName,
                state.reticleId,
                state.runtimeReticleId,
                command.templateTransportId);
            if (!scene_.ApplyDynamicReticlePatchByKey(pageRef->normalizedName, state.reticleId, state.patch))
            {
                return CommandResult::Failure("Unable to update dynamic reticle '" + state.reticleId + "'");
            }

            continue;
        }

        ReticleGroup reticle = InstantiateReticle(*templateRef->reticle, state.reticleId);
        reticle.layerId = binding->layerId;
        scene_.UpsertDynamicReticleByKey(pageRef->normalizedName, std::move(reticle));
        scene_.SetDynamicReticleRuntimeIdentifiers(
            pageRef->normalizedName,
            state.reticleId,
            state.runtimeReticleId,
            command.templateTransportId);

        if (!scene_.ApplyDynamicReticlePatchByKey(pageRef->normalizedName, state.reticleId, state.patch))
        {
            return CommandResult::Failure("Unable to initialize dynamic reticle '" + state.reticleId + "'");
        }
    }

    return CommandResult::Success();
}

CommandProcessor::CommandResult CommandProcessor::OnRemoveDynamicReticle(const RemoveDynamicReticleCommand& command)
{
    const auto pageRef = scene_.ResolvePageRef(command.target.pageId, command.target.page);
    if (!pageRef.has_value() ||
        !scene_.RemoveDynamicReticleByKey(pageRef->normalizedName, command.target.reticleId))
    {
        return CommandResult::Failure("Unable to remove dynamic reticle '" + command.target.reticleId +
                                      "' from page '" + command.target.page + "'");
    }

    return CommandResult::Success();
}

CommandProcessor::CommandResult CommandProcessor::OnSetDynamicReticleSetVisibility(
    const SetDynamicReticleSetVisibilityCommand& command)
{
    const auto pageRef = scene_.ResolvePageRef(command.pageId, command.page);
    if (!pageRef.has_value() ||
        !scene_.SetDynamicReticleSetVisibleByKey(pageRef->normalizedName, command.templateId, command.visible))
    {
        return CommandResult::Failure("Unable to update dynamic reticle set visibility for template '" +
                                      command.templateId + "' on page '" + command.page + "'");
    }

    return CommandResult::Success();
}

CommandProcessor::CommandResult CommandProcessor::OnSetDynamicReticleSetStrobeMagnetEnabled(
    const SetDynamicReticleSetStrobeMagnetEnabledCommand& command)
{
    const auto pageRef = scene_.ResolvePageRef(command.pageId, command.page);
    if (!pageRef.has_value() ||
        !scene_.SetDynamicReticleSetStrobeMagnetEnabledByKey(pageRef->normalizedName, command.templateId, command.enabled))
    {
        return CommandResult::Failure("Unable to update dynamic reticle set strobe-magnet eligibility for template '" +
                                      command.templateId + "' on page '" + command.page + "'");
    }

    return CommandResult::Success();
}

CommandProcessor::CommandResult CommandProcessor::OnResetWindow(const ResetWindowCommand&)
{
    scene_.ResetToInitialState();
    return CommandResult::Success();
}

void CommandProcessor::SetFailure(std::string message)
{
    lastError_ = std::move(message);
}
} // namespace mfd
