/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for LatestBatchPublisher.
 */

#include "mfd/client/LatestBatchPublisher.h"

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <cstddef>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mfd/control/CommandClient.h"
#include "mfd/core/internal/CompositeKey.h"

namespace mfd::client
{
struct LatestBatchPublisher::Impl
{
    std::unique_ptr<mfd::CommandClient> ownedClient {};
    SendFunction sendFunction {};
    ErrorFunction errorFunction {};
    std::thread worker {};
    mutable std::mutex mutex {};
    std::condition_variable wakeCondition {};
    std::condition_variable idleCondition {};
    std::optional<mfd::CommandBatch> pendingBatch {};
    std::string lastError {};
    bool ready = false;
    bool stopRequested = false;
    bool sending = false;
    bool retryBlocked = false;
};

namespace
{
struct DynamicLifecycleOperation
{
    std::string key;
    mfd::UserCommand command;
};

using DynamicOperationMap = std::unordered_map<std::string, std::size_t>;

bool IsResetWindowCommand(const mfd::UserCommand& command) noexcept
{
    return std::holds_alternative<mfd::ResetWindowCommand>(command);
}

bool ContainsResetWindowCommand(const std::vector<mfd::UserCommand>& commands) noexcept
{
    return std::any_of(commands.begin(), commands.end(), IsResetWindowCommand);
}

void PrependResetWindowCommand(std::vector<mfd::UserCommand>& commands)
{
    if (ContainsResetWindowCommand(commands))
    {
        return;
    }

    commands.insert(commands.begin(), mfd::UserCommand {mfd::ResetWindowCommand {}});
}

std::string MakeDynamicReticleKey(const std::string& page,
                                  const std::string& reticleId,
                                  const mfd::RuntimeDynamicId runtimeReticleId = 0)
{
    std::string key;
    key.reserve(1U + page.size() + mfd::detail::kCompositeKeyFieldOverhead +
                (runtimeReticleId != 0
                     ? mfd::detail::kCompositeKeyFieldOverhead
                     : reticleId.size() + mfd::detail::kCompositeKeyFieldOverhead));
    key.push_back('D');
    mfd::detail::AppendCompositeStringField(key, 'P', page);
    if (runtimeReticleId != 0)
    {
        mfd::detail::AppendCompositeUnsignedField(key, 'R', runtimeReticleId);
        return key;
    }

    mfd::detail::AppendCompositeStringField(key, 'N', reticleId);
    return key;
}

std::string MakeDynamicTemplateKey(const std::string& page, const std::string& templateId)
{
    std::string key;
    key.reserve(1U + page.size() + templateId.size() +
                2U * mfd::detail::kCompositeKeyFieldOverhead);
    key.push_back('T');
    mfd::detail::AppendCompositeStringField(key, 'P', page);
    mfd::detail::AppendCompositeStringField(key, 'T', templateId);
    return key;
}

std::string MakeDynamicLifecycleKey(const mfd::UpsertDynamicReticleCommand& command)
{
    return MakeDynamicReticleKey(command.target.page, command.target.reticleId, command.target.runtimeReticleId);
}

std::string MakeDynamicLifecycleKey(const mfd::RemoveDynamicReticleCommand& command)
{
    return MakeDynamicReticleKey(command.target.page, command.target.reticleId, command.target.runtimeReticleId);
}

std::string MakeDynamicLifecycleKey(const mfd::SetDynamicReticleSetVisibilityCommand& command)
{
    return MakeDynamicTemplateKey(command.page, command.templateId);
}

std::string MakeDynamicLifecycleKey(const mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand& command)
{
    std::string key = MakeDynamicTemplateKey(command.page, command.templateId);
    mfd::detail::AppendCompositeStringField(key, 'F', "strobe_magnet");
    return key;
}

void PutDynamicLifecycleCommand(std::vector<DynamicLifecycleOperation>& operations,
                                DynamicOperationMap& operationIndexes,
                                std::string key,
                                mfd::UserCommand command);

template <typename Command>
void PutDynamicLifecycleValue(std::vector<DynamicLifecycleOperation>& operations,
                              DynamicOperationMap& operationIndexes,
                              const Command& command)
{
    PutDynamicLifecycleCommand(operations, operationIndexes, MakeDynamicLifecycleKey(command), command);
}

void PutExpandedDynamicLifecycleValues(const mfd::UpsertDynamicReticlesCommand& command,
                                       std::vector<DynamicLifecycleOperation>& operations,
                                       DynamicOperationMap& operationIndexes)
{
    operations.reserve(operations.size() + command.reticles.size());
    operationIndexes.reserve(operationIndexes.size() + command.reticles.size());

    for (const mfd::DynamicReticleState& state : command.reticles)
    {
        mfd::UpsertDynamicReticleCommand expandedCommand;
        expandedCommand.target =
            mfd::DynamicReticleHandle {command.page, state.reticleId, command.pageId, state.runtimeReticleId};
        expandedCommand.templateId = command.templateId;
        expandedCommand.templateTransportId = command.templateTransportId;
        expandedCommand.patch = state.patch;

        const std::string lifecycleKey = MakeDynamicLifecycleKey(expandedCommand);
        PutDynamicLifecycleCommand(
            operations,
            operationIndexes,
            lifecycleKey,
            std::move(expandedCommand));
    }
}

void PutDynamicLifecycleCommand(std::vector<DynamicLifecycleOperation>& operations,
                                DynamicOperationMap& operationIndexes,
                                std::string key,
                                mfd::UserCommand command)
{
    if (key.empty())
    {
        return;
    }

    const auto [indexIt, inserted] = operationIndexes.try_emplace(key, operations.size());
    if (inserted)
    {
        operations.push_back(DynamicLifecycleOperation {std::move(key), std::move(command)});
        return;
    }

    operations[indexIt->second].command = std::move(command);
}

void CollectDynamicLifecycleCommands(const std::vector<mfd::UserCommand>& source,
                                     std::vector<DynamicLifecycleOperation>& operations,
                                     DynamicOperationMap& operationIndexes)
{
    operations.reserve(operations.size() + source.size());
    operationIndexes.reserve(operationIndexes.size() + source.size());

    for (const mfd::UserCommand& command : source)
    {
        std::visit(
            [&operations, &operationIndexes](const auto& value)
            {
                using Command = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Command, mfd::UpsertDynamicReticleCommand> ||
                              std::is_same_v<Command, mfd::RemoveDynamicReticleCommand> ||
                              std::is_same_v<Command, mfd::SetDynamicReticleSetVisibilityCommand> ||
                              std::is_same_v<Command, mfd::SetDynamicReticleSetStrobeMagnetEnabledCommand>)
                {
                    PutDynamicLifecycleValue(operations, operationIndexes, value);
                }
                else if constexpr (std::is_same_v<Command, mfd::UpsertDynamicReticlesCommand>)
                {
                    PutExpandedDynamicLifecycleValues(value, operations, operationIndexes);
                }
            },
            command);
    }
}

using StaticUpdateKeyToIndex = std::unordered_map<std::string, std::size_t>;

std::string MakeStaticReticleKey(const mfd::StaticReticleHandle& target)
{
    std::string key;
    if (target.pageId != 0 && target.reticleId != 0)
    {
        key.reserve(1U + 2U * mfd::detail::kCompositeKeyFieldOverhead);
        key.push_back('S');
        mfd::detail::AppendCompositeUnsignedField(key, 'P', target.pageId);
        mfd::detail::AppendCompositeUnsignedField(key, 'R', target.reticleId);
        return key;
    }

    if (!target.page.empty() && !target.reticle.empty())
    {
        key.reserve(1U + target.page.size() + target.reticle.size() +
                    2U * mfd::detail::kCompositeKeyFieldOverhead);
        key.push_back('S');
        mfd::detail::AppendCompositeStringField(key, 'P', target.page);
        mfd::detail::AppendCompositeStringField(key, 'R', target.reticle);
        return key;
    }

    return {};
}

template <typename FieldType>
void FillOptionalIfNewestAbsent(std::optional<FieldType>& newestField, const std::optional<FieldType>& pendingField)
{
    if (!newestField.has_value() && pendingField.has_value())
    {
        newestField = pendingField;
    }
}

template <typename ScalarMap>
void CarryMapEntriesIfNewestAbsent(ScalarMap& newestMap, const ScalarMap& pendingMap)
{
    newestMap.reserve(newestMap.size() + pendingMap.size());
    for (const auto& entry : pendingMap)
    {
        newestMap.try_emplace(entry.first, entry.second);
    }
}

void MergePrimitivePatchFillingNewestAbsentFields(mfd::PrimitivePatch& newest, const mfd::PrimitivePatch& pending)
{
    FillOptionalIfNewestAbsent(newest.visible, pending.visible);
    FillOptionalIfNewestAbsent(newest.position, pending.position);
    FillOptionalIfNewestAbsent(newest.rotationDegrees, pending.rotationDegrees);
    FillOptionalIfNewestAbsent(newest.scale, pending.scale);
    FillOptionalIfNewestAbsent(newest.color, pending.color);
    FillOptionalIfNewestAbsent(newest.fillColor, pending.fillColor);
    FillOptionalIfNewestAbsent(newest.filled, pending.filled);
    FillOptionalIfNewestAbsent(newest.thickness, pending.thickness);
    FillOptionalIfNewestAbsent(newest.lineStyle, pending.lineStyle);
    FillOptionalIfNewestAbsent(newest.text, pending.text);
    FillOptionalIfNewestAbsent(newest.letterSpacing, pending.letterSpacing);
    FillOptionalIfNewestAbsent(newest.lineStart, pending.lineStart);
    FillOptionalIfNewestAbsent(newest.lineEnd, pending.lineEnd);
    FillOptionalIfNewestAbsent(newest.radius, pending.radius);
    FillOptionalIfNewestAbsent(newest.innerRadius, pending.innerRadius);
    FillOptionalIfNewestAbsent(newest.outerRadius, pending.outerRadius);
    FillOptionalIfNewestAbsent(newest.width, pending.width);
    FillOptionalIfNewestAbsent(newest.height, pending.height);
    FillOptionalIfNewestAbsent(newest.size, pending.size);
    FillOptionalIfNewestAbsent(newest.points, pending.points);
    FillOptionalIfNewestAbsent(newest.closed, pending.closed);
    FillOptionalIfNewestAbsent(newest.segments, pending.segments);
    FillOptionalIfNewestAbsent(newest.startAngleDegrees, pending.startAngleDegrees);
    FillOptionalIfNewestAbsent(newest.endAngleDegrees, pending.endAngleDegrees);
    FillOptionalIfNewestAbsent(newest.timeUtc, pending.timeUtc);
    FillOptionalIfNewestAbsent(newest.timeFields, pending.timeFields);

    // The numeric time override is exclusive: `timeValue` and the `clearTimeValue`
    // flag must never coexist. The newest patch decides the time semantics whenever
    // it already carries either signal; only an otherwise-silent newest patch may
    // inherit the pending time intent.
    const bool newestDecidesTime = newest.timeValue.has_value() || newest.clearTimeValue;
    if (!newestDecidesTime)
    {
        if (pending.timeValue.has_value())
        {
            newest.timeValue = pending.timeValue;
        }
        else if (pending.clearTimeValue)
        {
            newest.clearTimeValue = true;
        }
    }
}

template <typename PrimitivePatchMap>
void MergePrimitivePatchMap(PrimitivePatchMap& newestMap, const PrimitivePatchMap& pendingMap)
{
    newestMap.reserve(newestMap.size() + pendingMap.size());
    for (const auto& entry : pendingMap)
    {
        const auto [insertIt, inserted] = newestMap.try_emplace(entry.first, entry.second);
        if (!inserted)
        {
            MergePrimitivePatchFillingNewestAbsentFields(insertIt->second, entry.second);
        }
    }
}

void FillStaticReticleScalarFields(mfd::ReticlePatch& newest, const mfd::ReticlePatch& pending)
{
    FillOptionalIfNewestAbsent(newest.visible, pending.visible);
    FillOptionalIfNewestAbsent(newest.blinkEnabled, pending.blinkEnabled);
    FillOptionalIfNewestAbsent(newest.blinkType, pending.blinkType);
    FillOptionalIfNewestAbsent(newest.blinkTypeId, pending.blinkTypeId);
    FillOptionalIfNewestAbsent(newest.position, pending.position);
    FillOptionalIfNewestAbsent(newest.rotationDegrees, pending.rotationDegrees);
    FillOptionalIfNewestAbsent(newest.scale, pending.scale);
    FillOptionalIfNewestAbsent(newest.color, pending.color);
    FillOptionalIfNewestAbsent(newest.thickness, pending.thickness);
    FillOptionalIfNewestAbsent(newest.text, pending.text);
    FillOptionalIfNewestAbsent(newest.letterSpacing, pending.letterSpacing);
}

void MergeStaticReticleMapsWithinSameIdentificationSpace(mfd::ReticlePatch& newest, const mfd::ReticlePatch& pending)
{
    CarryMapEntriesIfNewestAbsent(newest.texts, pending.texts);
    CarryMapEntriesIfNewestAbsent(newest.textsById, pending.textsById);
    CarryMapEntriesIfNewestAbsent(newest.letterSpacings, pending.letterSpacings);
    CarryMapEntriesIfNewestAbsent(newest.letterSpacingsById, pending.letterSpacingsById);

    MergePrimitivePatchMap(newest.primitivePatches, pending.primitivePatches);
    MergePrimitivePatchMap(newest.primitivePatchesById, pending.primitivePatchesById);
}

// True when folding `pending` into `newest` would place a name-addressed primitive
// entry and a generated-id primitive entry in the same command. Transport
// normalization resolves each named entry to its generated id inside a single
// command (see NormalizePatchForTransport / MoveNamedPrimitiveFields, which does an
// insert_or_assign), so a stale pending named write could overwrite the newest
// generated-id write for the same primitive. Such pairs must stay in separate
// commands so ordering alone keeps "latest wins" true.
bool StaticReticlePatchesHaveNameIdConflict(const mfd::ReticlePatch& newest, const mfd::ReticlePatch& pending)
{
    const bool textsConflict = (!pending.texts.empty() && !newest.textsById.empty()) ||
                               (!pending.textsById.empty() && !newest.texts.empty());
    const bool letterSpacingsConflict =
        (!pending.letterSpacings.empty() && !newest.letterSpacingsById.empty()) ||
        (!pending.letterSpacingsById.empty() && !newest.letterSpacings.empty());
    const bool primitivePatchesConflict =
        (!pending.primitivePatches.empty() && !newest.primitivePatchesById.empty()) ||
        (!pending.primitivePatchesById.empty() && !newest.primitivePatches.empty());

    return textsConflict || letterSpacingsConflict || primitivePatchesConflict;
}

void MergeStaticReticlePatchFillingNewestAbsentFields(mfd::ReticlePatch& newest, const mfd::ReticlePatch& pending)
{
    FillStaticReticleScalarFields(newest, pending);
    MergeStaticReticleMapsWithinSameIdentificationSpace(newest, pending);
}

// Folds a pending static delta into the newest command targeting the same reticle.
// Simple top-level fields always merge field by field (newest wins). Map families are
// folded in place only when pending and newest share the same identification space;
// when a name/id conflict exists the pending maps are moved into `carried` so the
// caller can apply them *before* the newest command, keeping "latest wins" true after
// transport normalization. Returns true when `carried` received maps to apply.
bool FoldPendingStaticPatchIntoNewestKeepingLatestWins(mfd::ReticlePatch& newest,
                                                       mfd::ReticlePatch& pending,
                                                       mfd::ReticlePatch& carried)
{
    FillStaticReticleScalarFields(newest, pending);

    if (!StaticReticlePatchesHaveNameIdConflict(newest, pending))
    {
        MergeStaticReticleMapsWithinSameIdentificationSpace(newest, pending);
        return false;
    }

    carried.texts = std::move(pending.texts);
    carried.textsById = std::move(pending.textsById);
    carried.letterSpacings = std::move(pending.letterSpacings);
    carried.letterSpacingsById = std::move(pending.letterSpacingsById);
    carried.primitivePatches = std::move(pending.primitivePatches);
    carried.primitivePatchesById = std::move(pending.primitivePatchesById);
    return true;
}

// Collects the pending static reticle deltas, coalescing repeated updates for the
// same reticle into a single "latest pending state" per identity (the later update
// wins field by field). Two updates for the same reticle that would mix the named and
// the generated-id primitive spaces are kept as separate ordered entries instead of
// being coalesced, so their order alone keeps the later write winning. Updates whose
// identity cannot be demonstrated are gathered separately so they can be carried
// without being merged into an ambiguous target.
void CollectStaticReticleUpdates(const std::vector<mfd::UserCommand>& source,
                                 std::vector<mfd::UpdateReticleCommand>& identifiedUpdates,
                                 StaticUpdateKeyToIndex& identifiedIndexes,
                                 std::vector<mfd::UpdateReticleCommand>& unidentifiableUpdates)
{
    identifiedUpdates.reserve(source.size());
    identifiedIndexes.reserve(source.size());

    for (const mfd::UserCommand& command : source)
    {
        const mfd::UpdateReticleCommand* update = std::get_if<mfd::UpdateReticleCommand>(&command);
        if (update == nullptr)
        {
            continue;
        }

        const std::string key = MakeStaticReticleKey(update->target);
        if (key.empty())
        {
            unidentifiableUpdates.push_back(*update);
            continue;
        }

        const auto [indexIt, inserted] = identifiedIndexes.try_emplace(key, identifiedUpdates.size());
        if (inserted)
        {
            identifiedUpdates.push_back(*update);
            continue;
        }

        const std::size_t storedIndex = indexIt->second;
        if (StaticReticlePatchesHaveNameIdConflict(identifiedUpdates[storedIndex].patch, update->patch))
        {
            indexIt->second = identifiedUpdates.size();
            identifiedUpdates.push_back(*update);
            continue;
        }

        mfd::UpdateReticleCommand laterUpdate = *update;
        MergeStaticReticlePatchFillingNewestAbsentFields(laterUpdate.patch, identifiedUpdates[storedIndex].patch);
        identifiedUpdates[storedIndex] = std::move(laterUpdate);
    }
}

void IndexStaticReticleUpdates(const std::vector<mfd::UserCommand>& commands, StaticUpdateKeyToIndex& indexes)
{
    indexes.reserve(commands.size());

    for (std::size_t index = 0; index < commands.size(); ++index)
    {
        const mfd::UpdateReticleCommand* update = std::get_if<mfd::UpdateReticleCommand>(&commands[index]);
        if (update == nullptr)
        {
            continue;
        }

        const std::string key = MakeStaticReticleKey(update->target);
        if (key.empty())
        {
            continue;
        }

        indexes.try_emplace(key, index);
    }
}

void MergePendingBatchPreservingUndeliveredDeltas(std::optional<mfd::CommandBatch>& pendingBatch,
                                                  mfd::CommandBatch&& newestBatch)
{
    if (!pendingBatch.has_value())
    {
        pendingBatch = std::move(newestBatch);
        return;
    }

    if (pendingBatch->mappingHash != newestBatch.mappingHash)
    {
        pendingBatch = std::move(newestBatch);
        return;
    }

    const bool pendingHasReset = ContainsResetWindowCommand(pendingBatch->commands);
    const bool newestHasReset = ContainsResetWindowCommand(newestBatch.commands);

    if (newestHasReset)
    {
        pendingBatch = std::move(newestBatch);
        return;
    }

    std::vector<DynamicLifecycleOperation> pendingDynamicOperations;
    DynamicOperationMap pendingDynamicIndexes;
    std::vector<mfd::UpdateReticleCommand> pendingStaticUpdates;
    StaticUpdateKeyToIndex pendingStaticIndexes;
    std::vector<mfd::UpdateReticleCommand> unidentifiablePendingStaticUpdates;
    if (!pendingHasReset)
    {
        CollectDynamicLifecycleCommands(pendingBatch->commands, pendingDynamicOperations, pendingDynamicIndexes);
        CollectStaticReticleUpdates(pendingBatch->commands,
                                    pendingStaticUpdates,
                                    pendingStaticIndexes,
                                    unidentifiablePendingStaticUpdates);
    }

    std::vector<DynamicLifecycleOperation> newestDynamicOperations;
    DynamicOperationMap newestDynamicIndexes;
    CollectDynamicLifecycleCommands(newestBatch.commands, newestDynamicOperations, newestDynamicIndexes);

    mfd::CommandBatch mergedBatch = std::move(newestBatch);

    StaticUpdateKeyToIndex newestStaticIndexes;
    IndexStaticReticleUpdates(mergedBatch.commands, newestStaticIndexes);

    std::vector<mfd::UserCommand> carriedOperations;
    carriedOperations.reserve(pendingDynamicOperations.size() + pendingStaticUpdates.size() +
                              unidentifiablePendingStaticUpdates.size());

    for (DynamicLifecycleOperation& operation : pendingDynamicOperations)
    {
        if (newestDynamicIndexes.find(operation.key) == newestDynamicIndexes.end())
        {
            carriedOperations.push_back(std::move(operation.command));
        }
    }

    for (mfd::UpdateReticleCommand& pendingUpdate : pendingStaticUpdates)
    {
        const std::string key = MakeStaticReticleKey(pendingUpdate.target);
        const auto newestIt = newestStaticIndexes.find(key);
        if (newestIt == newestStaticIndexes.end())
        {
            carriedOperations.push_back(mfd::UserCommand {std::move(pendingUpdate)});
            continue;
        }

        mfd::UpdateReticleCommand& newestUpdate =
            std::get<mfd::UpdateReticleCommand>(mergedBatch.commands[newestIt->second]);

        mfd::UpdateReticleCommand carriedUpdate;
        carriedUpdate.target = pendingUpdate.target;
        if (FoldPendingStaticPatchIntoNewestKeepingLatestWins(newestUpdate.patch,
                                                              pendingUpdate.patch,
                                                              carriedUpdate.patch))
        {
            carriedOperations.push_back(mfd::UserCommand {std::move(carriedUpdate)});
        }
    }

    for (mfd::UpdateReticleCommand& pendingUpdate : unidentifiablePendingStaticUpdates)
    {
        carriedOperations.push_back(mfd::UserCommand {std::move(pendingUpdate)});
    }

    if (carriedOperations.empty())
    {
        if (pendingHasReset)
        {
            PrependResetWindowCommand(mergedBatch.commands);
        }
        pendingBatch = std::move(mergedBatch);
        return;
    }

    mergedBatch.commands.insert(mergedBatch.commands.begin(),
                                std::make_move_iterator(carriedOperations.begin()),
                                std::make_move_iterator(carriedOperations.end()));
    if (pendingHasReset)
    {
        PrependResetWindowCommand(mergedBatch.commands);
    }
    pendingBatch = std::move(mergedBatch);
}

template<typename ImplType>
void StartWorker(ImplType& impl)
{
    if (!impl.ready || !impl.sendFunction)
    {
        return;
    }

    impl.worker = std::thread(
        [&impl]()
        {
            for (;;)
            {
                mfd::CommandBatch batch;

                {
                    std::unique_lock<std::mutex> lock(impl.mutex);
                    impl.wakeCondition.wait(
                        lock,
                        [&impl]()
                        {
                            return impl.stopRequested ||
                                   (impl.pendingBatch.has_value() && !impl.retryBlocked);
                        });

                    if (impl.stopRequested && !impl.pendingBatch.has_value())
                    {
                        break;
                    }

                    batch = std::move(*impl.pendingBatch);
                    impl.pendingBatch.reset();
                    impl.sending = true;
                }

                bool success = false;
                std::string error;

                try
                {
                    success = impl.sendFunction(batch);
                    if (!success)
                    {
                        error = impl.errorFunction ? impl.errorFunction() : "Unable to send the latest command batch";
                        if (error.empty())
                        {
                            error = "Unable to send the latest command batch";
                        }
                    }
                }
                catch (const std::exception& exception)
                {
                    error = exception.what();
                }
                catch (...)
                {
                    error = "Unknown exception while sending the latest command batch";
                }

                {
                    std::lock_guard<std::mutex> lock(impl.mutex);
                    impl.sending = false;
                    if (success)
                    {
                        impl.lastError.clear();
                    }
                    else
                    {
                        std::optional<mfd::CommandBatch> retryBatch {std::move(batch)};
                        if (impl.pendingBatch.has_value())
                        {
                            MergePendingBatchPreservingUndeliveredDeltas(
                                retryBatch,
                                std::move(*impl.pendingBatch));
                        }
                        impl.pendingBatch = std::move(retryBatch);
                        impl.retryBlocked = true;
                        impl.lastError = std::move(error);
                    }
                }

                impl.idleCondition.notify_all();
            }

            impl.idleCondition.notify_all();
        });
}

} // namespace

std::unique_ptr<mfd::CommandClient> MakeOwnedClient(const mfd::WindowCommandTransportConfig& config)
{
    return std::make_unique<mfd::CommandClient>(config);
}

std::unique_ptr<mfd::CommandClient> MakeOwnedClient(const mfd::WindowUdpCommandTransport& config)
{
    return std::make_unique<mfd::CommandClient>(config);
}

LatestBatchPublisher::LatestBatchPublisher(const mfd::WindowCommandTransportConfig& config)
    : impl_(std::make_unique<Impl>())
{
    impl_->ownedClient = MakeOwnedClient(config);
    impl_->ready = impl_->ownedClient != nullptr && impl_->ownedClient->IsReady();
    impl_->lastError =
        impl_->ready ? std::string {} :
                       (impl_->ownedClient == nullptr ? "Unable to create the realtime command client"
                                                      : impl_->ownedClient->LastError());

    if (impl_->ready)
    {
        mfd::CommandClient* client = impl_->ownedClient.get();
        impl_->sendFunction = [client](const mfd::CommandBatch& batch)
        {
            return client->SendBatch(batch);
        };
        impl_->errorFunction = [client]()
        {
            return client->LastError();
        };
    }

    StartWorker(*impl_);
}

LatestBatchPublisher::LatestBatchPublisher(const mfd::WindowUdpCommandTransport& config)
    : impl_(std::make_unique<Impl>())
{
    impl_->ownedClient = MakeOwnedClient(config);
    impl_->ready = impl_->ownedClient != nullptr && impl_->ownedClient->IsReady();
    impl_->lastError =
        impl_->ready ? std::string {} :
                       (impl_->ownedClient == nullptr ? "Unable to create the realtime UDP command client"
                                                      : impl_->ownedClient->LastError());

    if (impl_->ready)
    {
        mfd::CommandClient* client = impl_->ownedClient.get();
        impl_->sendFunction = [client](const mfd::CommandBatch& batch)
        {
            return client->SendBatch(batch);
        };
        impl_->errorFunction = [client]()
        {
            return client->LastError();
        };
    }

    StartWorker(*impl_);
}

LatestBatchPublisher::LatestBatchPublisher(SendFunction sendFunction, ErrorFunction errorFunction)
    : impl_(std::make_unique<Impl>())
{
    impl_->sendFunction = std::move(sendFunction);
    impl_->errorFunction = std::move(errorFunction);
    impl_->ready = static_cast<bool>(impl_->sendFunction);
    if (!impl_->ready)
    {
        impl_->lastError = "Latest batch publisher requires a valid send callback";
    }

    StartWorker(*impl_);
}

LatestBatchPublisher::~LatestBatchPublisher()
{
    Stop();
}

bool LatestBatchPublisher::IsReady() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->ready && !impl_->stopRequested;
}

bool LatestBatchPublisher::SubmitLatest(mfd::CommandBatch batch)
{
    if (impl_ == nullptr)
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->ready)
        {
            if (impl_->lastError.empty())
            {
                impl_->lastError = "Latest batch publisher is not ready";
            }
            return false;
        }
        if (impl_->stopRequested)
        {
            impl_->lastError = "Latest batch publisher has been stopped";
            return false;
        }

        MergePendingBatchPreservingUndeliveredDeltas(impl_->pendingBatch, std::move(batch));
        impl_->retryBlocked = false;
    }

    impl_->wakeCondition.notify_one();
    return true;
}

bool LatestBatchPublisher::SubmitLatest(BatchBuilder batchBuilder)
{
    if (impl_ == nullptr)
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (!impl_->ready)
        {
            if (impl_->lastError.empty())
            {
                impl_->lastError = "Latest batch publisher is not ready";
            }
            return false;
        }
        if (impl_->stopRequested)
        {
            impl_->lastError = "Latest batch publisher has been stopped";
            return false;
        }
        if (!batchBuilder)
        {
            impl_->lastError = "Latest batch publisher requires a valid batch builder";
            return false;
        }

        MergePendingBatchPreservingUndeliveredDeltas(impl_->pendingBatch, batchBuilder());
        impl_->retryBlocked = false;
    }

    impl_->wakeCondition.notify_one();
    return true;
}

bool LatestBatchPublisher::SubmitLatest(std::vector<mfd::UserCommand> commands, const std::uint32_t sequence)
{
    mfd::CommandBatch batch;
    batch.sequence = sequence;
    batch.commands = std::move(commands);
    return SubmitLatest(std::move(batch));
}

void LatestBatchPublisher::Flush()
{
    if (impl_ == nullptr || !impl_->worker.joinable())
    {
        return;
    }

    std::unique_lock<std::mutex> lock(impl_->mutex);
    impl_->idleCondition.wait(
        lock,
        [this]()
        {
            return (!impl_->pendingBatch.has_value() || impl_->retryBlocked) && !impl_->sending;
        });
}

void LatestBatchPublisher::Stop()
{
    if (impl_ == nullptr)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->stopRequested = true;
        impl_->pendingBatch.reset();
        impl_->retryBlocked = false;
    }

    impl_->wakeCondition.notify_all();
    impl_->idleCondition.notify_all();

    if (impl_->worker.joinable())
    {
        impl_->worker.join();
    }
}

std::string LatestBatchPublisher::LastError() const
{
    if (impl_ == nullptr)
    {
        return {};
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->lastError;
}
} // namespace mfd::client
