/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation for UdpRuntimeBridge.
 */

#include "mfd/control/UdpRuntimeBridge.h"

#include "UdpRuntimeBridgeInternal.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

#include "mfd/control/CommandTransport.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/control/WindowFeedback.h"
#include "mfd/control/internal/CommandTraits.h"
#include "mfd/ipc/ExchangeChannel.h"

namespace mfd
{
namespace
{
constexpr std::size_t kMaxPacketsPerPump = 64;
constexpr std::size_t kMaxQueuedBatches = 8192;
constexpr std::size_t kMaxQueuedCommands = 65536;
constexpr std::size_t kMaxQueuedCommandWorkUnits = 65536;
constexpr std::size_t kMaxQueuedWireBytes = 16U * 1024U * 1024U;
constexpr std::size_t kMaxQueuedEstimatedMemoryBytes = 32U * 1024U * 1024U;
constexpr std::size_t kMaxQueuedFeedback = 256;
// Idle backoff of the worker loop. Incoming command packets are polled (TryReceive is non-blocking) and
// the wait is only interrupted by stop requests and outbound feedback, so this delay bounds the extra
// latency before idle incoming commands are pumped. 5 ms keeps that well under one frame at 60 fps while
// more than halving the at-rest wake-up rate versus the previous 2 ms busy poll.
constexpr auto kIdleWait = std::chrono::milliseconds(5);

enum class CommandCoalescingKind
{
    SetPageView,
    UpdateWindowDisplay,
    UpdateReticle,
    UpdateStrobe,
    UpsertDynamicReticle,
    SetDynamicReticleSetVisibility,
    SetDynamicReticleSetStrobeMagnetEnabled
};

struct CoalescingIdentifier
{
    std::uint64_t numericId = 0U;
    std::string text {};
};

struct CommandCoalescingKey
{
    CommandCoalescingKind kind = CommandCoalescingKind::SetPageView;
    CoalescingIdentifier page {};
    CoalescingIdentifier reticle {};
    CoalescingIdentifier dynamicTemplate {};
    CoalescingIdentifier strobe {};
};

struct UdpRuntimeCounters
{
    std::atomic<std::uint64_t> receivedPackets {0};
    std::atomic<std::uint64_t> receivedBytes {0};
    std::atomic<std::uint64_t> decodedBatches {0};
    std::atomic<std::uint64_t> decodeErrors {0};
    std::atomic<std::uint64_t> queuedBatches {0};
    std::atomic<std::uint64_t> droppedBatches {0};
    std::atomic<std::uint64_t> drainedBatches {0};
    std::atomic<std::uint64_t> drainedCommands {0};
    std::atomic<std::uint64_t> appliedCommands {0};
    std::atomic<std::uint64_t> skippedCommands {0};
    std::atomic<std::uint64_t> coalescedCommands {0};
    std::atomic<std::uint64_t> truncatedBatches {0};
    std::atomic<std::uint64_t> feedbackQueued {0};
    std::atomic<std::uint64_t> feedbackSent {0};
    std::atomic<std::uint64_t> feedbackDropped {0};
};

struct QueuedCommandBatch
{
    CommandBatch batch;
    std::size_t commandCount = 0;
    std::size_t commandWorkUnits = 0;
    std::size_t wireBytes = 0;
    std::size_t estimatedMemoryBytes = 0;
};

struct CommandWorkUnitEstimator
{
    std::size_t operator()(const ActivatePageCommand&) const noexcept { return 1U; }
    std::size_t operator()(const SetPageViewCommand&) const noexcept { return 1U; }
    std::size_t operator()(const UpdateWindowDisplayCommand&) const noexcept { return 1U; }
    std::size_t operator()(const UpdateReticleCommand&) const noexcept { return 1U; }
    std::size_t operator()(const UpdateStrobeCommand&) const noexcept { return 1U; }
    std::size_t operator()(const UpsertDynamicReticleCommand&) const noexcept { return 1U; }

    std::size_t operator()(const UpsertDynamicReticlesCommand& command) const noexcept
    {
        return std::max<std::size_t>(1U, command.reticles.size());
    }

    std::size_t operator()(const RemoveDynamicReticleCommand&) const noexcept { return 1U; }
    std::size_t operator()(const SetDynamicReticleSetVisibilityCommand&) const noexcept { return 1U; }
    std::size_t operator()(const ResetWindowCommand&) const noexcept { return 1U; }
    std::size_t operator()(const SetDynamicReticleSetStrobeMagnetEnabledCommand&) const noexcept { return 1U; }
};

std::size_t EstimateCommandWorkUnits(const CommandBatch& batch) noexcept
{
    std::size_t workUnits = 0U;
    for (const UserCommand& command : batch.commands)
    {
        const std::size_t commandWorkUnits = std::visit(CommandWorkUnitEstimator {}, command);
        if (workUnits > std::numeric_limits<std::size_t>::max() - commandWorkUnits)
        {
            return std::numeric_limits<std::size_t>::max();
        }

        workUnits += commandWorkUnits;
    }

    return workUnits;
}

std::size_t EstimateBatchMemoryBytes(const CommandBatch& batch, const std::size_t wireBytes) noexcept
{
    return sizeof(CommandBatch) + batch.commands.capacity() * sizeof(UserCommand) + wireBytes;
}

CoalescingIdentifier MakeCoalescingIdentifier(const std::uint64_t numericId, const std::string_view fallbackText)
{
    CoalescingIdentifier identifier;
    identifier.numericId = numericId;
    if (numericId == 0U && !fallbackText.empty())
    {
        identifier.text = std::string(fallbackText);
    }

    return identifier;
}

CoalescingIdentifier MakeCoalescingIdentifier(const detail::CommandPageBinding& binding)
{
    return MakeCoalescingIdentifier(binding.pageId,
                                    binding.pageName == nullptr ? std::string_view {} : *binding.pageName);
}

CoalescingIdentifier MakeCoalescingIdentifier(const detail::CommandTemplateBinding& binding)
{
    return MakeCoalescingIdentifier(binding.templateTransportId,
                                    binding.templateId == nullptr ? std::string_view {} : *binding.templateId);
}

bool operator==(const CoalescingIdentifier& lhs, const CoalescingIdentifier& rhs) noexcept
{
    return lhs.numericId == rhs.numericId && lhs.text == rhs.text;
}

bool operator==(const CommandCoalescingKey& lhs, const CommandCoalescingKey& rhs) noexcept
{
    return lhs.kind == rhs.kind &&
           lhs.page == rhs.page &&
           lhs.reticle == rhs.reticle &&
           lhs.dynamicTemplate == rhs.dynamicTemplate &&
           lhs.strobe == rhs.strobe;
}

std::uint64_t LoadCounter(const std::atomic<std::uint64_t>& counter) noexcept
{
    return counter.load(std::memory_order_relaxed);
}

void AddCounter(std::atomic<std::uint64_t>& counter, const std::uint64_t amount) noexcept
{
    counter.fetch_add(amount, std::memory_order_relaxed);
}

void IncrementCounter(std::atomic<std::uint64_t>& counter) noexcept
{
    AddCounter(counter, 1U);
}

void MergePrimitivePatch(PrimitivePatch& target, const PrimitivePatch& source)
{
    if (source.visible.has_value())
    {
        target.visible = source.visible;
    }
    if (source.position.has_value())
    {
        target.position = source.position;
    }
    if (source.rotationDegrees.has_value())
    {
        target.rotationDegrees = source.rotationDegrees;
    }
    if (source.scale.has_value())
    {
        target.scale = source.scale;
    }
    if (source.color.has_value())
    {
        target.color = source.color;
    }
    if (source.fillColor.has_value())
    {
        target.fillColor = source.fillColor;
    }
    if (source.filled.has_value())
    {
        target.filled = source.filled;
    }
    if (source.thickness.has_value())
    {
        target.thickness = source.thickness;
    }
    if (source.lineStyle.has_value())
    {
        target.lineStyle = source.lineStyle;
    }
    if (source.text.has_value())
    {
        target.text = source.text;
    }
    if (source.letterSpacing.has_value())
    {
        target.letterSpacing = source.letterSpacing;
    }
    if (source.lineStart.has_value())
    {
        target.lineStart = source.lineStart;
    }
    if (source.lineEnd.has_value())
    {
        target.lineEnd = source.lineEnd;
    }
    if (source.radius.has_value())
    {
        target.radius = source.radius;
    }
    if (source.innerRadius.has_value())
    {
        target.innerRadius = source.innerRadius;
    }
    if (source.outerRadius.has_value())
    {
        target.outerRadius = source.outerRadius;
    }
    if (source.width.has_value())
    {
        target.width = source.width;
    }
    if (source.height.has_value())
    {
        target.height = source.height;
    }
    if (source.size.has_value())
    {
        target.size = source.size;
    }
    if (source.points.has_value())
    {
        target.points = source.points;
    }
    if (source.closed.has_value())
    {
        target.closed = source.closed;
    }
    if (source.segments.has_value())
    {
        target.segments = source.segments;
    }
    if (source.startAngleDegrees.has_value())
    {
        target.startAngleDegrees = source.startAngleDegrees;
    }
    if (source.endAngleDegrees.has_value())
    {
        target.endAngleDegrees = source.endAngleDegrees;
    }
    if (source.timeValue.has_value())
    {
        target.timeValue = source.timeValue;
        target.clearTimeValue = false;
    }
    if (source.clearTimeValue)
    {
        target.timeValue.reset();
        target.clearTimeValue = true;
    }
    if (source.timeUtc.has_value())
    {
        target.timeUtc = source.timeUtc;
    }
    if (source.timeFields.has_value())
    {
        target.timeFields = source.timeFields;
    }
}

void MergeReticlePatch(ReticlePatch& target, const ReticlePatch& source)
{
    if (source.visible.has_value())
    {
        target.visible = source.visible;
    }
    if (source.blinkEnabled.has_value())
    {
        target.blinkEnabled = source.blinkEnabled;
    }
    if (source.blinkType.has_value())
    {
        target.blinkType = source.blinkType;
    }
    if (source.blinkTypeId.has_value())
    {
        target.blinkTypeId = source.blinkTypeId;
    }
    if (source.position.has_value())
    {
        target.position = source.position;
    }
    if (source.rotationDegrees.has_value())
    {
        target.rotationDegrees = source.rotationDegrees;
    }
    if (source.scale.has_value())
    {
        target.scale = source.scale;
    }
    if (source.color.has_value())
    {
        target.color = source.color;
    }
    if (source.thickness.has_value())
    {
        target.thickness = source.thickness;
    }
    if (source.text.has_value())
    {
        target.text = source.text;
    }
    if (source.letterSpacing.has_value())
    {
        target.letterSpacing = source.letterSpacing;
    }

    for (const auto& entry : source.texts)
    {
        target.texts[entry.first] = entry.second;
    }

    for (const auto& entry : source.textsById)
    {
        target.textsById[entry.first] = entry.second;
    }

    for (const auto& entry : source.letterSpacings)
    {
        target.letterSpacings[entry.first] = entry.second;
    }

    for (const auto& entry : source.letterSpacingsById)
    {
        target.letterSpacingsById[entry.first] = entry.second;
    }

    for (const auto& entry : source.primitivePatches)
    {
        PrimitivePatch& targetPatch = target.primitivePatches[entry.first];
        MergePrimitivePatch(targetPatch, entry.second);
    }

    for (const auto& entry : source.primitivePatchesById)
    {
        PrimitivePatch& targetPatch = target.primitivePatchesById[entry.first];
        MergePrimitivePatch(targetPatch, entry.second);
    }
}

void MergeWindowDisplayPatch(WindowDisplayPatch& target, const WindowDisplayPatch& source)
{
    if (source.invertColors.has_value())
    {
        target.invertColors = source.invertColors;
    }
    if (source.brightness.has_value())
    {
        target.brightness = source.brightness;
    }
    if (source.disabled.has_value())
    {
        target.disabled = source.disabled;
    }
}

void MergeStrobeCommand(UpdateStrobeCommand& target, const UpdateStrobeCommand& source)
{
    if (source.active.has_value())
    {
        target.active = source.active;
    }
    if (source.position.has_value())
    {
        target.position = source.position;
    }
}

// Maps one typed command to its coalescing key with one explicit overload per command type.
// Returning std::nullopt marks the command as a coalescing barrier: it is never merged and
// stops earlier commands from being merged across it. This visitor intentionally has no
// generic fallback: adding a new UserCommand alternative must fail to compile here until
// its coalescing rule is written.
struct CommandCoalescingKeyVisitor
{
    std::optional<CommandCoalescingKey> operator()(const ActivatePageCommand&) const
    {
        // Page activation changes the context in which following commands are observed.
        return std::nullopt;
    }

    std::optional<CommandCoalescingKey> operator()(const SetPageViewCommand& command) const
    {
        CommandCoalescingKey key;
        key.kind = CommandCoalescingKind::SetPageView;
        key.page = MakeCoalescingIdentifier(detail::PageBindingOf(command));
        return key;
    }

    std::optional<CommandCoalescingKey> operator()(const UpdateWindowDisplayCommand&) const
    {
        return CommandCoalescingKey {CommandCoalescingKind::UpdateWindowDisplay};
    }

    std::optional<CommandCoalescingKey> operator()(const UpdateReticleCommand& command) const
    {
        CommandCoalescingKey key;
        key.kind = CommandCoalescingKind::UpdateReticle;
        key.page = MakeCoalescingIdentifier(detail::PageBindingOf(command));
        key.reticle = MakeCoalescingIdentifier(command.target.reticleId, command.target.reticle);
        return key;
    }

    std::optional<CommandCoalescingKey> operator()(const UpdateStrobeCommand& command) const
    {
        CommandCoalescingKey key;
        key.kind = CommandCoalescingKind::UpdateStrobe;
        key.page = MakeCoalescingIdentifier(detail::PageBindingOf(command));
        key.strobe = MakeCoalescingIdentifier(command.strobeId, command.strobe);
        return key;
    }

    std::optional<CommandCoalescingKey> operator()(const UpsertDynamicReticleCommand& command) const
    {
        CommandCoalescingKey key;
        key.kind = CommandCoalescingKind::UpsertDynamicReticle;
        key.page = MakeCoalescingIdentifier(detail::PageBindingOf(command));
        key.reticle = MakeCoalescingIdentifier(command.target.runtimeReticleId, command.target.reticleId);
        key.dynamicTemplate = MakeCoalescingIdentifier(detail::TemplateBindingOf(command));
        return key;
    }

    std::optional<CommandCoalescingKey> operator()(const SetDynamicReticleSetVisibilityCommand& command) const
    {
        CommandCoalescingKey key;
        key.kind = CommandCoalescingKind::SetDynamicReticleSetVisibility;
        key.page = MakeCoalescingIdentifier(detail::PageBindingOf(command));
        key.dynamicTemplate = MakeCoalescingIdentifier(detail::TemplateBindingOf(command));
        return key;
    }

    std::optional<CommandCoalescingKey> operator()(const SetDynamicReticleSetStrobeMagnetEnabledCommand& command) const
    {
        CommandCoalescingKey key;
        key.kind = CommandCoalescingKind::SetDynamicReticleSetStrobeMagnetEnabled;
        key.page = MakeCoalescingIdentifier(detail::PageBindingOf(command));
        key.dynamicTemplate = MakeCoalescingIdentifier(detail::TemplateBindingOf(command));
        return key;
    }

    std::optional<CommandCoalescingKey> operator()(const UpsertDynamicReticlesCommand&) const
    {
        // Bulk upserts stay coalescing barriers: merging two bulk updates would require
        // per-reticle reconciliation that the latest-batch publisher already performs.
        return std::nullopt;
    }

    std::optional<CommandCoalescingKey> operator()(const RemoveDynamicReticleCommand&) const
    {
        // Removals stay coalescing barriers so an upsert-remove-upsert sequence keeps its
        // observable lifecycle ordering.
        return std::nullopt;
    }

    std::optional<CommandCoalescingKey> operator()(const ResetWindowCommand&) const
    {
        // A window reset invalidates every earlier command and must never be merged away.
        return std::nullopt;
    }
};

std::optional<CommandCoalescingKey> MakeCommandCoalescingKey(const UserCommand& command)
{
    return std::visit(CommandCoalescingKeyVisitor {}, command);
}

void MergeCoalescedCommand(UserCommand& target, const UserCommand& source)
{
    if (const auto* sourceCommand = std::get_if<UpdateWindowDisplayCommand>(&source))
    {
        auto* targetCommand = std::get_if<UpdateWindowDisplayCommand>(&target);
        if (targetCommand != nullptr)
        {
            MergeWindowDisplayPatch(targetCommand->patch, sourceCommand->patch);
            return;
        }
    }

    if (const auto* sourceCommand = std::get_if<UpdateReticleCommand>(&source))
    {
        auto* targetCommand = std::get_if<UpdateReticleCommand>(&target);
        if (targetCommand != nullptr)
        {
            MergeReticlePatch(targetCommand->patch, sourceCommand->patch);
            return;
        }
    }

    if (const auto* sourceCommand = std::get_if<UpdateStrobeCommand>(&source))
    {
        auto* targetCommand = std::get_if<UpdateStrobeCommand>(&target);
        if (targetCommand != nullptr)
        {
            MergeStrobeCommand(*targetCommand, *sourceCommand);
            return;
        }
    }

    if (const auto* sourceCommand = std::get_if<UpsertDynamicReticleCommand>(&source))
    {
        auto* targetCommand = std::get_if<UpsertDynamicReticleCommand>(&target);
        if (targetCommand != nullptr)
        {
            MergeReticlePatch(targetCommand->patch, sourceCommand->patch);
            return;
        }
    }

    target = source;
}

class CommandCoalescer
{
public:
    std::size_t Coalesce(CommandBatch& batch) const
    {
        if (batch.commands.size() < 2U)
        {
            return 0;
        }

        std::vector<UserCommand> commands;
        commands.reserve(batch.commands.size());
        std::optional<CommandCoalescingKey> previousKey;
        std::size_t coalescedCommands = 0;

        for (const UserCommand& command : batch.commands)
        {
            const std::optional<CommandCoalescingKey> key = MakeCommandCoalescingKey(command);
            if (!key.has_value() || !previousKey.has_value() || !(*key == *previousKey))
            {
                commands.push_back(command);
                previousKey = key;
                continue;
            }

            MergeCoalescedCommand(commands.back(), command);
            ++coalescedCommands;
        }

        batch.commands = std::move(commands);
        return coalescedCommands;
    }
};

std::string DescribeFeedbackTargetImpl(const StrobeStatusFeedback& feedback)
{
    return "page '" + feedback.pageName + "' strobe";
}

std::string DescribeFeedbackTargetImpl(const ActivePageFeedback& feedback)
{
    return "active page '" + feedback.pageName + "'";
}

std::string DescribeFeedbackTargetImpl(const WindowLifecycleFeedback& feedback)
{
    return feedback.state == WindowLifecycleState::Closing ? "window lifecycle 'closing'"
                                                           : "window lifecycle 'alive'";
}

std::string DescribeFeedbackTarget(const FeedbackPayload& feedback)
{
    // Explicit named overloads keep the description per alternative. The static_assert turns any future
    // extension of FeedbackPayload into a compile error here, forcing a matching overload to be added
    // instead of silently falling through.
    static_assert(std::variant_size_v<FeedbackPayload> == 3,
                  "DescribeFeedbackTarget must cover every FeedbackPayload alternative");

    if (const auto* strobe = std::get_if<StrobeStatusFeedback>(&feedback))
    {
        return DescribeFeedbackTargetImpl(*strobe);
    }

    if (const auto* activePage = std::get_if<ActivePageFeedback>(&feedback))
    {
        return DescribeFeedbackTargetImpl(*activePage);
    }

    return DescribeFeedbackTargetImpl(std::get<WindowLifecycleFeedback>(feedback));
}

} // namespace

std::size_t mfd::detail::CoalesceCommandBatch(CommandBatch& batch)
{
    return CommandCoalescer {}.Coalesce(batch);
}

struct UdpRuntimeBridge::Impl
{
    WindowCommandTransportConfig commandConfig {};
    WindowFeedbackTransportConfig feedbackConfig {};
    ChannelFactory commandReceiverFactory {};
    ChannelFactory feedbackSenderFactory {};

    std::unique_ptr<IExchangeChannel> commandReceiver {};
    std::unique_ptr<IExchangeChannel> feedbackSender {};

    std::deque<QueuedCommandBatch> inboundBatches;
    std::size_t inboundQueuedCommands = 0;
    std::size_t inboundQueuedCommandWorkUnits = 0;
    std::size_t inboundQueuedWireBytes = 0;
    std::size_t inboundEstimatedMemoryBytes = 0;
    std::deque<FeedbackPayload> outboundFeedback;
    UdpRuntimeCounters counters;

    mutable std::mutex stateMutex;
    mutable std::mutex inboundMutex;
    mutable std::mutex outboundMutex;
    std::condition_variable waitCondition;

    std::thread worker;
    bool stopRequested = false;
    bool running = false;
    bool hasCommandReceiver = false;
    bool hasFeedbackSender = false;
    bool commandReady = false;
    bool feedbackReady = false;

    std::string lastCommandStatus;
    std::string lastFeedbackStatus;

    void SetCommandStatus(std::string status)
    {
        std::lock_guard lock(stateMutex);
        lastCommandStatus = std::move(status);
    }

    void SetFeedbackStatus(std::string status)
    {
        std::lock_guard lock(stateMutex);
        lastFeedbackStatus = std::move(status);
    }

    bool ShouldStop() const
    {
        std::lock_guard lock(stateMutex);
        return stopRequested;
    }

    bool IsCommandReady() const
    {
        std::lock_guard lock(stateMutex);
        return commandReady;
    }

    bool IsFeedbackReady() const
    {
        std::lock_guard lock(stateMutex);
        return feedbackReady;
    }

    QueuedCommandBatch TakeOldestQueuedBatch()
    {
        QueuedCommandBatch oldest = std::move(inboundBatches.front());
        inboundQueuedCommands -= oldest.commandCount;
        inboundQueuedCommandWorkUnits -= oldest.commandWorkUnits;
        inboundQueuedWireBytes -= oldest.wireBytes;
        inboundEstimatedMemoryBytes -= oldest.estimatedMemoryBytes;
        inboundBatches.pop_front();
        return oldest;
    }

    void PushQueuedBatch(CommandBatch&& batch, const std::size_t wireBytes)
    {
        if (batch.commands.empty())
        {
            return;
        }

        const std::size_t coalescedCommandCount = detail::CoalesceCommandBatch(batch);
        if (coalescedCommandCount > 0U)
        {
            AddCounter(counters.coalescedCommands, static_cast<std::uint64_t>(coalescedCommandCount));
        }

        QueuedCommandBatch queued;
        queued.commandCount = batch.commands.size();
        queued.commandWorkUnits = EstimateCommandWorkUnits(batch);
        queued.wireBytes = wireBytes;
        queued.estimatedMemoryBytes = EstimateBatchMemoryBytes(batch, wireBytes);
        queued.batch = std::move(batch);

        std::lock_guard lock(inboundMutex);
        if (queued.commandCount > kMaxQueuedCommands ||
            queued.commandWorkUnits > kMaxQueuedCommandWorkUnits ||
            queued.wireBytes > kMaxQueuedWireBytes ||
            queued.estimatedMemoryBytes > kMaxQueuedEstimatedMemoryBytes)
        {
            IncrementCounter(counters.droppedBatches);
            SetCommandStatus("UDP command batch exceeds inbound queue budget, dropping batch");
            return;
        }

        std::size_t droppedCount = 0;
        while (!inboundBatches.empty() &&
               (inboundBatches.size() >= kMaxQueuedBatches ||
                inboundQueuedCommands > kMaxQueuedCommands - queued.commandCount ||
                inboundQueuedCommandWorkUnits >
                    kMaxQueuedCommandWorkUnits - queued.commandWorkUnits ||
                inboundQueuedWireBytes > kMaxQueuedWireBytes - queued.wireBytes ||
                inboundEstimatedMemoryBytes >
                    kMaxQueuedEstimatedMemoryBytes - queued.estimatedMemoryBytes))
        {
            TakeOldestQueuedBatch();
            ++droppedCount;
        }

        if (droppedCount > 0U)
        {
            AddCounter(counters.droppedBatches, static_cast<std::uint64_t>(droppedCount));
            SetCommandStatus("UDP command queue budget exceeded, dropping oldest batches");
        }

        inboundQueuedCommands += queued.commandCount;
        inboundQueuedCommandWorkUnits += queued.commandWorkUnits;
        inboundQueuedWireBytes += queued.wireBytes;
        inboundEstimatedMemoryBytes += queued.estimatedMemoryBytes;
        inboundBatches.push_back(std::move(queued));
        IncrementCounter(counters.queuedBatches);
    }

    void PushQueuedFeedback(FeedbackPayload&& feedback)
    {
        {
            std::lock_guard lock(outboundMutex);
            if (outboundFeedback.size() >= kMaxQueuedFeedback)
            {
                outboundFeedback.pop_front();
                IncrementCounter(counters.feedbackDropped);
                SetFeedbackStatus("UDP feedback queue overflow, dropping oldest runtime feedback");
            }

            outboundFeedback.emplace_back(std::move(feedback));
            IncrementCounter(counters.feedbackQueued);
        }

        waitCondition.notify_all();
    }

    bool FlushQueuedFeedback()
    {
        if (!IsFeedbackReady() || feedbackSender == nullptr)
        {
            return false;
        }

        std::deque<FeedbackPayload> localQueue;
        {
            std::lock_guard lock(outboundMutex);
            if (outboundFeedback.empty())
            {
                return false;
            }

            localQueue.swap(outboundFeedback);
        }

        bool sentAny = false;
        while (!localQueue.empty())
        {
            FeedbackPayload feedback = std::move(localQueue.front());
            localQueue.pop_front();

            try
            {
                const std::string payload = SerializeFeedbackPayload(feedback);
                const auto* payloadBytes = reinterpret_cast<const std::byte*>(payload.data());
                if (!feedbackSender->Send(ByteView(payloadBytes, payload.size())))
                {
                    IncrementCounter(counters.feedbackDropped);
                    SetFeedbackStatus(feedbackSender->LastError());
                    continue;
                }

                IncrementCounter(counters.feedbackSent);
                SetFeedbackStatus("UDP runtime feedback sent for " + DescribeFeedbackTarget(feedback));
                sentAny = true;
            }
            catch (const std::exception& exception)
            {
                IncrementCounter(counters.feedbackDropped);
                SetFeedbackStatus(exception.what());
            }
            catch (...)
            {
                IncrementCounter(counters.feedbackDropped);
                SetFeedbackStatus("Unknown exception while sending runtime feedback");
            }
        }

        return sentAny;
    }

    bool PumpQueuedCommands()
    {
        if (!IsCommandReady() || commandReceiver == nullptr)
        {
            return false;
        }

        bool receivedAny = false;
        for (std::size_t packetIndex = 0; packetIndex < kMaxPacketsPerPump; ++packetIndex)
        {
            try
            {
                const auto payload = commandReceiver->TryReceive();
                if (!payload.has_value())
                {
                    break;
                }

                receivedAny = true;
                IncrementCounter(counters.receivedPackets);
                AddCounter(counters.receivedBytes, static_cast<std::uint64_t>(payload->size()));

                const auto* raw = reinterpret_cast<const char*>(payload->data());
                std::string error;
                auto batch = DeserializeCommandBatch(std::string_view(raw, payload->size()), &error);
                if (!batch.has_value())
                {
                    IncrementCounter(counters.decodeErrors);
                    SetCommandStatus(error);
                    continue;
                }

                IncrementCounter(counters.decodedBatches);
                PushQueuedBatch(std::move(*batch), payload->size());
            }
            catch (const std::exception& exception)
            {
                SetCommandStatus(exception.what());
                break;
            }
            catch (...)
            {
                SetCommandStatus("Unknown exception while receiving UDP commands");
                break;
            }
        }

        if (commandReceiver != nullptr && !commandReceiver->LastError().empty())
        {
            SetCommandStatus(commandReceiver->LastError());
        }

        return receivedAny;
    }
};

UdpRuntimeBridge::UdpRuntimeBridge(WindowCommandTransportConfig commandConfig,
                                   WindowFeedbackTransportConfig feedbackConfig)
    : impl_(std::make_unique<Impl>())
{
    impl_->commandConfig = std::move(commandConfig);
    impl_->feedbackConfig = std::move(feedbackConfig);
    impl_->commandReceiverFactory = [config = impl_->commandConfig]()
    {
        return CreateCommandReceiverChannel(config);
    };
    impl_->feedbackSenderFactory = [config = impl_->feedbackConfig]()
    {
        return CreateFeedbackSenderChannel(config);
    };
}

UdpRuntimeBridge::UdpRuntimeBridge(ChannelFactory commandReceiverFactory, ChannelFactory feedbackSenderFactory)
    : impl_(std::make_unique<Impl>())
{
    impl_->commandReceiverFactory = std::move(commandReceiverFactory);
    impl_->feedbackSenderFactory = std::move(feedbackSenderFactory);
}

UdpRuntimeBridge::~UdpRuntimeBridge()
{
    Stop();
}

bool UdpRuntimeBridge::Start()
{
    if (impl_ == nullptr)
    {
        return false;
    }

    Stop();

    impl_->commandReceiver = impl_->commandReceiverFactory ? impl_->commandReceiverFactory() : nullptr;
    impl_->feedbackSender = impl_->feedbackSenderFactory ? impl_->feedbackSenderFactory() : nullptr;

    const bool hasCommandReceiver = impl_->commandReceiver != nullptr;
    const bool hasFeedbackSender = impl_->feedbackSender != nullptr;
    const bool commandReady = hasCommandReceiver && impl_->commandReceiver->IsReady();
    const bool feedbackReady = hasFeedbackSender && impl_->feedbackSender->IsReady();
    {
        std::lock_guard lock(impl_->stateMutex);
        impl_->hasCommandReceiver = hasCommandReceiver;
        impl_->hasFeedbackSender = hasFeedbackSender;
        impl_->commandReady = commandReady;
        impl_->feedbackReady = feedbackReady;
    }

    if (!hasCommandReceiver)
    {
        impl_->SetCommandStatus("UDP command bridge disabled in the window JSON");
    }
    else if (commandReady)
    {
        impl_->SetCommandStatus("UDP command receiver thread ready");
    }
    else
    {
        impl_->SetCommandStatus(impl_->commandReceiver->LastError());
    }

    if (!hasFeedbackSender)
    {
        impl_->SetFeedbackStatus("UDP runtime feedback sender disabled in the window JSON");
    }
    else if (feedbackReady)
    {
        impl_->SetFeedbackStatus("UDP runtime feedback sender thread ready");
    }
    else
    {
        impl_->SetFeedbackStatus(impl_->feedbackSender->LastError());
    }

    if (!commandReady && !feedbackReady)
    {
        return !hasCommandReceiver && !hasFeedbackSender;
    }

    {
        std::lock_guard lock(impl_->stateMutex);
        impl_->stopRequested = false;
        impl_->running = true;
    }
    impl_->worker = std::thread(
        [impl = impl_.get()]()
        {
            while (true)
            {
                {
                    if (impl->ShouldStop())
                    {
                        break;
                    }
                }

                const bool receivedCommands = impl->PumpQueuedCommands();
                const bool sentFeedback = impl->FlushQueuedFeedback();

                if (!receivedCommands && !sentFeedback)
                {
                    std::unique_lock lock(impl->stateMutex);
                    impl->waitCondition.wait_for(
                        lock,
                        kIdleWait,
                        [impl]()
                        {
                            return impl->stopRequested;
                        });
                }
            }

            impl->FlushQueuedFeedback();
            {
                std::lock_guard lock(impl->stateMutex);
                impl->running = false;
            }
        });

    return true;
}

void UdpRuntimeBridge::Stop() noexcept
{
    if (impl_ == nullptr)
    {
        return;
    }

    {
        std::lock_guard lock(impl_->stateMutex);
        impl_->stopRequested = true;
    }
    impl_->waitCondition.notify_all();

    if (impl_->worker.joinable())
    {
        impl_->worker.join();
    }

    {
        std::lock_guard lock(impl_->stateMutex);
        impl_->running = false;
        impl_->commandReady = false;
        impl_->feedbackReady = false;
        impl_->hasCommandReceiver = false;
        impl_->hasFeedbackSender = false;
    }
    impl_->commandReceiver.reset();
    impl_->feedbackSender.reset();

    {
        std::lock_guard lock(impl_->inboundMutex);
        impl_->inboundBatches.clear();
        impl_->inboundQueuedCommands = 0;
        impl_->inboundQueuedCommandWorkUnits = 0;
        impl_->inboundQueuedWireBytes = 0;
        impl_->inboundEstimatedMemoryBytes = 0;
    }

    {
        std::lock_guard lock(impl_->outboundMutex);
        impl_->outboundFeedback.clear();
    }
}

bool UdpRuntimeBridge::IsRunning() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->running;
}

bool UdpRuntimeBridge::HasCommandReceiver() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->hasCommandReceiver;
}

bool UdpRuntimeBridge::HasFeedbackSender() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->hasFeedbackSender;
}

bool UdpRuntimeBridge::CommandTransportReady() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->commandReady;
}

bool UdpRuntimeBridge::FeedbackTransportReady() const noexcept
{
    if (impl_ == nullptr)
    {
        return false;
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->feedbackReady;
}

UdpRuntimeBridgeMetrics UdpRuntimeBridge::MetricsSnapshot() const
{
    UdpRuntimeBridgeMetrics snapshot;
    if (impl_ == nullptr)
    {
        return snapshot;
    }

    snapshot.receivedPackets = LoadCounter(impl_->counters.receivedPackets);
    snapshot.receivedBytes = LoadCounter(impl_->counters.receivedBytes);
    snapshot.decodedBatches = LoadCounter(impl_->counters.decodedBatches);
    snapshot.decodeErrors = LoadCounter(impl_->counters.decodeErrors);
    snapshot.queuedBatches = LoadCounter(impl_->counters.queuedBatches);
    snapshot.droppedBatches = LoadCounter(impl_->counters.droppedBatches);
    snapshot.drainedBatches = LoadCounter(impl_->counters.drainedBatches);
    snapshot.drainedCommands = LoadCounter(impl_->counters.drainedCommands);
    snapshot.appliedCommands = LoadCounter(impl_->counters.appliedCommands);
    snapshot.skippedCommands = LoadCounter(impl_->counters.skippedCommands);
    snapshot.coalescedCommands = LoadCounter(impl_->counters.coalescedCommands);
    snapshot.truncatedBatches = LoadCounter(impl_->counters.truncatedBatches);
    snapshot.feedbackQueued = LoadCounter(impl_->counters.feedbackQueued);
    snapshot.feedbackSent = LoadCounter(impl_->counters.feedbackSent);
    snapshot.feedbackDropped = LoadCounter(impl_->counters.feedbackDropped);

    {
        std::lock_guard lock(impl_->inboundMutex);
        snapshot.inboundQueueDepth = impl_->inboundBatches.size();
    }

    {
        std::lock_guard lock(impl_->outboundMutex);
        snapshot.outboundQueueDepth = impl_->outboundFeedback.size();
    }

    return snapshot;
}

std::size_t UdpRuntimeBridge::DrainReceivedBatches(std::vector<CommandBatch>& destination,
                                                   const std::size_t maxBatches)
{
    if (impl_ == nullptr || maxBatches == 0)
    {
        return 0;
    }

    std::lock_guard lock(impl_->inboundMutex);
    const std::size_t batchCount = std::min(maxBatches, impl_->inboundBatches.size());
    destination.reserve(destination.size() + batchCount);
    std::size_t commandCount = 0;

    for (std::size_t index = 0; index < batchCount; ++index)
    {
        QueuedCommandBatch queued = impl_->TakeOldestQueuedBatch();
        commandCount += queued.commandCount;
        destination.push_back(std::move(queued.batch));
    }

    AddCounter(impl_->counters.drainedBatches, static_cast<std::uint64_t>(batchCount));
    AddCounter(impl_->counters.drainedCommands, static_cast<std::uint64_t>(commandCount));
    return batchCount;
}

std::size_t UdpRuntimeBridge::DrainReceivedBatchesForCommandBudget(std::vector<CommandBatch>& destination,
                                                                   const std::size_t maxCommandWorkUnits,
                                                                   const std::size_t maxBatches)
{
    if (impl_ == nullptr || maxCommandWorkUnits == 0 || maxBatches == 0)
    {
        return 0;
    }

    std::lock_guard lock(impl_->inboundMutex);
    const std::size_t reserveCount = std::min(maxBatches, impl_->inboundBatches.size());
    destination.reserve(destination.size() + reserveCount);

    std::size_t batchCount = 0;
    std::size_t commandCount = 0;
    std::size_t commandWorkUnits = 0;
    while (!impl_->inboundBatches.empty() && batchCount < maxBatches)
    {
        const std::size_t nextCommandWorkUnits = impl_->inboundBatches.front().commandWorkUnits;
        if (nextCommandWorkUnits > maxCommandWorkUnits - commandWorkUnits)
        {
            // Always drain at least the front batch so one oversized packet
            // cannot permanently stall the inbound queue.
            if (batchCount == 0U)
            {
                commandCount += impl_->inboundBatches.front().commandCount;
                commandWorkUnits += nextCommandWorkUnits;
                ++batchCount;
                QueuedCommandBatch queued = impl_->TakeOldestQueuedBatch();
                destination.push_back(std::move(queued.batch));
            }
            break;
        }

        commandCount += impl_->inboundBatches.front().commandCount;
        commandWorkUnits += nextCommandWorkUnits;
        ++batchCount;
        QueuedCommandBatch queued = impl_->TakeOldestQueuedBatch();
        destination.push_back(std::move(queued.batch));
    }

    AddCounter(impl_->counters.drainedBatches, static_cast<std::uint64_t>(batchCount));
    AddCounter(impl_->counters.drainedCommands, static_cast<std::uint64_t>(commandCount));
    return batchCount;
}

std::size_t UdpRuntimeBridge::DrainReceivedCommands(std::vector<UserCommand>& destination,
                                                    const std::size_t maxCommands)
{
    if (impl_ == nullptr || maxCommands == 0)
    {
        return 0;
    }

    std::lock_guard lock(impl_->inboundMutex);
    std::size_t commandCount = 0;

    while (!impl_->inboundBatches.empty() && commandCount < maxCommands)
    {
        QueuedCommandBatch queued = impl_->TakeOldestQueuedBatch();
        std::size_t consumedInBatch = 0;

        for (UserCommand& command : queued.batch.commands)
        {
            if (commandCount >= maxCommands)
            {
                break;
            }

            destination.push_back(std::move(command));
            ++commandCount;
            ++consumedInBatch;
        }

        if (consumedInBatch < queued.batch.commands.size())
        {
            queued.batch.commands.erase(
                queued.batch.commands.begin(),
                queued.batch.commands.begin() + static_cast<std::ptrdiff_t>(consumedInBatch));
            queued.commandCount = queued.batch.commands.size();
            queued.estimatedMemoryBytes = EstimateBatchMemoryBytes(queued.batch, queued.wireBytes);
            impl_->inboundQueuedCommands += queued.commandCount;
            impl_->inboundQueuedWireBytes += queued.wireBytes;
            impl_->inboundEstimatedMemoryBytes += queued.estimatedMemoryBytes;
            impl_->inboundBatches.push_front(std::move(queued));
            break;
        }

        IncrementCounter(impl_->counters.drainedBatches);
    }

    AddCounter(impl_->counters.drainedCommands, static_cast<std::uint64_t>(commandCount));
    return commandCount;
}

void UdpRuntimeBridge::RecordCommandProcessingResult(const std::size_t appliedCommands,
                                                     const std::size_t skippedCommands,
                                                     const std::size_t truncatedBatches)
{
    if (impl_ == nullptr)
    {
        return;
    }

    AddCounter(impl_->counters.appliedCommands, static_cast<std::uint64_t>(appliedCommands));
    AddCounter(impl_->counters.skippedCommands, static_cast<std::uint64_t>(skippedCommands));
    AddCounter(impl_->counters.truncatedBatches, static_cast<std::uint64_t>(truncatedBatches));
}

void UdpRuntimeBridge::EnqueueStrobeFeedback(StrobeStatusFeedback feedback)
{
    if (impl_ == nullptr || !HasFeedbackSender())
    {
        return;
    }

    impl_->PushQueuedFeedback(FeedbackPayload {std::move(feedback)});
}

void UdpRuntimeBridge::EnqueueActivePageFeedback(ActivePageFeedback feedback)
{
    if (impl_ == nullptr || !HasFeedbackSender())
    {
        return;
    }

    impl_->PushQueuedFeedback(FeedbackPayload {std::move(feedback)});
}

void UdpRuntimeBridge::EnqueueWindowLifecycleFeedback(WindowLifecycleFeedback feedback)
{
    if (impl_ == nullptr || !HasFeedbackSender())
    {
        return;
    }

    impl_->PushQueuedFeedback(FeedbackPayload {std::move(feedback)});
}

std::string UdpRuntimeBridge::LastCommandStatus() const
{
    if (impl_ == nullptr)
    {
        return {};
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->lastCommandStatus;
}

std::string UdpRuntimeBridge::LastFeedbackStatus() const
{
    if (impl_ == nullptr)
    {
        return {};
    }

    std::lock_guard lock(impl_->stateMutex);
    return impl_->lastFeedbackStatus;
}
} // namespace mfd
