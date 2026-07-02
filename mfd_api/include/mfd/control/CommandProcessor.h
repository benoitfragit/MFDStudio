/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Runtime command dispatcher applying Protocol Buffers user commands to a scene registry.
 */

#include <cstddef>
#include <cstdint>
#include "mfd/core/ArrayView.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "mfd/MfdExport.h"
#include "mfd/control/CommandTypes.h"

namespace mfd
{
class IExchangeChannel;
class SceneRegistry;

/**
 * @brief Chooses how multi-command submissions behave when one command fails.
 */
enum class CommandBatchTransactionMode
{
    /** @brief Roll back every earlier command of the same multi-command submission. */
    Transactional,
    /** @brief Keep earlier commands applied and stop at the first failing command. */
    NonTransactional
};

/**
 * @brief Dispatches user commands to a scene registry.
 *
 * @note This class is typically owned by the host application that receives
 * commands from UDP.
 *
 * @note In the recommended two-thread runtime model, this object stays on the
 * render thread. The UDP worker thread should only decode packets and queue
 * typed commands for later submission.
 */
class MFD_API CommandProcessor
{
public:
    /**
     * @brief Creates a command processor bound to a runtime scene.
     * @param scene Scene registry updated by the received commands.
     */
    explicit CommandProcessor(SceneRegistry& scene);
    ~CommandProcessor();

    CommandProcessor(const CommandProcessor&) = delete;
    CommandProcessor& operator=(const CommandProcessor&) = delete;
    CommandProcessor(CommandProcessor&&) = delete;
    CommandProcessor& operator=(CommandProcessor&&) = delete;

    /**
     * @brief Submits a typed command directly.
     * @param command Command to dispatch.
     * @return `true` if the command was accepted and applied.
     * @note Generated transport ids are rejected here because they require a
     * matching `CommandBatch::mappingHash`.
     */
    bool Submit(const UserCommand& command);

    /**
     * @brief Submits one typed command batch directly.
     * @param batch Batch to dispatch in order.
     * @return `true` if every command was accepted and applied.
     */
    bool Submit(const CommandBatch& batch);

    /**
     * @brief Submits a sequence of typed commands directly.
     * @param commands Commands to dispatch in order.
     * @return `true` if every command was accepted and applied.
     */
    bool Submit(ArrayView<const UserCommand> commands);

    /**
     * @brief Submits a serialized Protocol Buffers command payload.
     * @param payload Raw binary Protocol Buffers payload.
     * @return `true` if the payload was valid and the command was applied.
     */
    bool Submit(std::string_view payload);

    /**
     * @brief Submits a raw byte buffer containing a Protocol Buffers command payload.
     * @param payload Raw byte payload.
     * @return `true` if the payload was valid and the command was applied.
     */
    bool Submit(ByteView payload);

    /**
     * @brief Polls a transport and dispatches all commands received during this call.
     * @param channel Transport channel to poll.
     * @return `true` if at least one command was processed.
     * @note This helper is mainly intended for simple single-threaded hosts.
     * Multi-threaded hosts should prefer `UdpRuntimeBridge` and `Submit(...)`.
     */
    bool Poll(IExchangeChannel& channel);

    /**
     * @brief Chooses how multi-command submissions handle later failures.
     * @param mode Transaction mode used by `Submit(const CommandBatch&)` and
     * `Submit(ArrayView<const UserCommand>)` when more than one command is
     * submitted together.
     */
    void SetBatchTransactionMode(CommandBatchTransactionMode mode) noexcept;

    /**
     * @brief Returns the current multi-command transaction mode.
     * @return Active transaction mode.
     */
    [[nodiscard]] CommandBatchTransactionMode BatchTransactionMode() const noexcept;

    /**
     * @brief Returns the last error produced by a submission or a poll.
     * @return Error string, or an empty string if the last operation succeeded.
     */
    std::string LastError() const;

private:
    struct SequencedBatchState
    {
        std::uint32_t lastSequence = 0;
        std::unordered_set<std::size_t> acceptedFingerprints {};
    };

    /** @brief Per-command-type identifier resolution visitor (defined in the implementation file). */
    struct IdentifierResolver;

    /** @brief Per-command-type dispatch visitor (defined in the implementation file). */
    struct ResolvedCommandDispatcher;

    /**
     * @brief Outcome of one dispatched command: success, or one failure message.
     *
     * @note Command handlers report failures through this return value so command
     * dispatch has a single error path instead of a mutable side channel.
     */
    struct CommandResult
    {
        static CommandResult Success()
        {
            return CommandResult {};
        }

        static CommandResult Failure(std::string message)
        {
            return CommandResult {false, std::move(message)};
        }

        bool succeeded = true;
        std::string error {};
    };

    CommandResult OnActivatePage(const ActivatePageCommand& command);
    CommandResult OnSetPageView(const SetPageViewCommand& command);
    CommandResult OnUpdateWindowDisplay(const UpdateWindowDisplayCommand& command);
    CommandResult OnUpdateReticle(const UpdateReticleCommand& command);
    CommandResult OnUpdateStrobe(const UpdateStrobeCommand& command);
    CommandResult OnUpsertDynamicReticle(const UpsertDynamicReticleCommand& command);
    CommandResult OnUpsertDynamicReticles(const UpsertDynamicReticlesCommand& command);
    CommandResult OnSetDynamicReticleSetVisibility(const SetDynamicReticleSetVisibilityCommand& command);
    CommandResult OnSetDynamicReticleSetStrobeMagnetEnabled(const SetDynamicReticleSetStrobeMagnetEnabledCommand& command);
    CommandResult OnRemoveDynamicReticle(const RemoveDynamicReticleCommand& command);
    CommandResult OnResetWindow(const ResetWindowCommand& command);

    bool SubmitCommandsWithCurrentTransactionMode(ArrayView<const UserCommand> commands, std::string_view mappingHash);
    bool SubmitCommandsTransactional(ArrayView<const UserCommand> commands, std::string_view mappingHash);
    bool SubmitCommandsNonTransactional(ArrayView<const UserCommand> commands, std::string_view mappingHash);
    bool ResolveBatchCommands(ArrayView<const UserCommand> commands,
                              std::string_view mappingHash,
                              std::vector<UserCommand>& resolvedCommands);
    bool SubmitResolved(UserCommand command, std::string_view mappingHash);
    bool DispatchResolved(const UserCommand& command);
    bool ResolveCommandIdentifiers(UserCommand& command, std::string_view mappingHash);
    bool ResolveGeneratedPage(std::string& page, TransportId& pageId);
    bool ResolveGeneratedStrobe(TransportId pageId, std::string& strobeName, TransportId strobeId);
    bool ResolveGeneratedStaticReticle(StaticReticleHandle& target);
    bool ResolveGeneratedDynamicReticle(DynamicReticleHandle& target);
    bool ResolveGeneratedTemplate(std::string& templateId, TransportId templateTransportId);
    const std::string* ResolveGeneratedPrimitiveId(TransportId staticReticleId,
                                                   TransportId templateTransportId,
                                                   TransportId primitiveId) const noexcept;
    bool ResolveGeneratedPatchPrimitiveIds(ReticlePatch& patch,
                                           TransportId pageId,
                                           TransportId staticReticleId,
                                           TransportId templateTransportId);

    void SetFailure(std::string message);

    SceneRegistry& scene_;
    std::string lastError_ {};
    std::unordered_map<std::string, SequencedBatchState> sequencedBatchesByMappingHash_ {};
    CommandBatchTransactionMode batchTransactionMode_ = CommandBatchTransactionMode::Transactional;
};
} // namespace mfd
