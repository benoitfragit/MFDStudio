/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the public reusable runtime session API.
 */

#include "mfd/runtime_api/RuntimeSession.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "internal/RuntimeSessionInternalAccess.hpp"
#include "control/CommandProcessorInternalAccess.hpp"
#include "mfd/control/CommandProcessor.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/control/WindowFeedback.h"
#include "mfd/control/internal/CommandWorkBudget.h"

namespace mfd::runtime_api
{
namespace
{
constexpr std::size_t kMaxCommandBatchesPerFrame = 4U;
constexpr float kMinimumIntervalSeconds = 0.001f;

mfd::CommandBatchTransactionMode ToCommandProcessorTransactionMode(
    const RuntimeCommandTransactionMode mode) noexcept
{
    return mode == RuntimeCommandTransactionMode::NonTransactional
               ? mfd::CommandBatchTransactionMode::NonTransactional
               : mfd::CommandBatchTransactionMode::Transactional;
}

class RuntimeFeedbackThrottle
{
public:
    void Reset() noexcept
    {
        fastElapsedSeconds_ = 0.0f;
        heartbeatElapsedSeconds_ = 0.0f;
        firstFeedbackPending_ = true;
    }

    void Advance(const float deltaSeconds) noexcept
    {
        if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
        {
            return;
        }

        fastElapsedSeconds_ += deltaSeconds;
        heartbeatElapsedSeconds_ += deltaSeconds;
    }

    [[nodiscard]] bool ShouldSendChanged(const float fastIntervalSeconds) const noexcept
    {
        return firstFeedbackPending_ || fastElapsedSeconds_ >= SanitizeInterval(fastIntervalSeconds);
    }

    [[nodiscard]] bool ShouldSendHeartbeat(const float heartbeatIntervalSeconds) const noexcept
    {
        return !firstFeedbackPending_ && heartbeatElapsedSeconds_ >= SanitizeInterval(heartbeatIntervalSeconds);
    }

    void MarkChangedSent(const float fastIntervalSeconds) noexcept
    {
        firstFeedbackPending_ = false;
        fastElapsedSeconds_ = RetainElapsedRemainder(fastElapsedSeconds_, fastIntervalSeconds);
        heartbeatElapsedSeconds_ = 0.0f;
    }

    void MarkHeartbeatSent(const float heartbeatIntervalSeconds) noexcept
    {
        firstFeedbackPending_ = false;
        heartbeatElapsedSeconds_ = RetainElapsedRemainder(heartbeatElapsedSeconds_, heartbeatIntervalSeconds);
    }

private:
    [[nodiscard]] static float SanitizeInterval(const float intervalSeconds) noexcept
    {
        if (!std::isfinite(intervalSeconds) || intervalSeconds < kMinimumIntervalSeconds)
        {
            return kMinimumIntervalSeconds;
        }

        return intervalSeconds;
    }

    [[nodiscard]] static float RetainElapsedRemainder(const float elapsedSeconds,
                                                      const float intervalSeconds) noexcept
    {
        const float interval = SanitizeInterval(intervalSeconds);
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds < interval)
        {
            return 0.0f;
        }

        return std::fmod(elapsedSeconds, interval);
    }

    float fastElapsedSeconds_ = 0.0f;
    float heartbeatElapsedSeconds_ = 0.0f;
    bool firstFeedbackPending_ = true;
};

StrobeFeedbackCapture ToFeedbackCapture(const StrobeCaptureResult& capture)
{
    return StrobeFeedbackCapture {
        capture.runtimeReticleId,
        capture.sourceTemplateTransportId,
        capture.label,
        capture.category,
        capture.position,
        capture.distance,
        capture.metadata};
}

StrobeFeedbackMagnet ToFeedbackMagnet(const std::optional<StrobeMagnetSummary>& magnetSummary)
{
    if (!magnetSummary.has_value())
    {
        return {};
    }

    return StrobeFeedbackMagnet {
        magnetSummary->enabled,
        magnetSummary->radius,
        magnetSummary->strength,
        magnetSummary->magnetized,
        magnetSummary->runtimeReticleId,
        magnetSummary->targetPosition,
        magnetSummary->distance};
}

bool SameVec2(const Vec2& lhs, const Vec2& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool SameCaptureConfig(const StrobeCaptureConfig& lhs, const StrobeCaptureConfig& rhs) noexcept
{
    return lhs.shape == rhs.shape &&
           lhs.radius == rhs.radius &&
           SameVec2(lhs.size, rhs.size);
}

bool SameFeedbackMagnet(const StrobeFeedbackMagnet& lhs, const StrobeFeedbackMagnet& rhs)
{
    return lhs.enabled == rhs.enabled &&
           lhs.radius == rhs.radius &&
           lhs.strength == rhs.strength &&
           lhs.magnetized == rhs.magnetized &&
           lhs.runtimeReticleId == rhs.runtimeReticleId &&
           SameVec2(lhs.targetPosition, rhs.targetPosition) &&
           lhs.distance == rhs.distance;
}

bool SameFeedbackCapture(const StrobeFeedbackCapture& lhs, const StrobeFeedbackCapture& rhs)
{
    return lhs.runtimeReticleId == rhs.runtimeReticleId &&
           lhs.sourceTemplateTransportId == rhs.sourceTemplateTransportId &&
           lhs.label == rhs.label &&
           lhs.category == rhs.category &&
           SameVec2(lhs.position, rhs.position) &&
           lhs.distance == rhs.distance &&
           lhs.metadata == rhs.metadata;
}

bool SameOptionalFeedbackCapture(const std::optional<StrobeFeedbackCapture>& lhs,
                                 const std::optional<StrobeFeedbackCapture>& rhs)
{
    if (lhs.has_value() != rhs.has_value())
    {
        return false;
    }

    return !lhs.has_value() || SameFeedbackCapture(*lhs, *rhs);
}

bool SameStrobeFeedbackState(const StrobeStatusFeedback& lhs, const StrobeStatusFeedback& rhs)
{
    return lhs.pageId == rhs.pageId &&
           lhs.strobeId == rhs.strobeId &&
           lhs.active == rhs.active &&
           SameVec2(lhs.position, rhs.position) &&
           SameCaptureConfig(lhs.capture, rhs.capture) &&
           SameFeedbackMagnet(lhs.magnet, rhs.magnet) &&
           SameOptionalFeedbackCapture(lhs.captureResult, rhs.captureResult);
}

/** @brief Returns the first configured transport that failed to become ready. */
std::string ConfiguredBridgeFailure(const UdpRuntimeBridge& bridge)
{
    if (bridge.HasCommandReceiver() && !bridge.CommandTransportReady())
    {
        const std::string status = bridge.LastCommandStatus();
        return status.empty() ? "The configured command transport is not ready." : status;
    }

    if (bridge.HasFeedbackSender() && !bridge.FeedbackTransportReady())
    {
        const std::string status = bridge.LastFeedbackStatus();
        return status.empty() ? "The configured feedback transport is not ready." : status;
    }

    return {};
}

/** @brief Starts a bridge only when every configured transport becomes ready. */
bool StartConfiguredBridge(UdpRuntimeBridge& bridge, std::string& error)
{
    try
    {
        const bool started = bridge.Start();
        const std::string failure = ConfiguredBridgeFailure(bridge);
        if (!started || !failure.empty())
        {
            error = failure.empty() ? "The runtime transport bridge could not start." : failure;
            bridge.Stop();
            return false;
        }
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        bridge.Stop();
        return false;
    }
    catch (...)
    {
        error = "Unknown exception while starting the runtime transport bridge.";
        bridge.Stop();
        return false;
    }

    error.clear();
    return true;
}
} // namespace

class RuntimeSession::Impl
{
public:
    bool LoadWindowFile(const std::filesystem::path& requestedWindowFile, std::string& error)
    {
        LoadedWindowConfiguration loaded;
        try
        {
            loaded = loader_.LoadWindowConfiguration(requestedWindowFile);
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }

        return ApplyLoadedWindowConfiguration(std::move(loaded), error);
    }

    bool Reload(std::string& error)
    {
        if (windowFile_.empty())
        {
            error = "No window JSON is loaded.";
            return false;
        }

        LoadedWindowConfiguration loaded;
        try
        {
            loaded = loader_.LoadWindowConfiguration(windowFile_);
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }

        return ApplyLoadedWindowConfiguration(std::move(loaded), error);
    }

    void Advance(const float deltaSeconds)
    {
        appliedCommandBatches_.clear();
        mfd::detail::CommandProcessorInternalAccess::MaintainTransportState(commandProcessor_);

        if (runtimeBridge_ != nullptr)
        {
            pendingCommandBatches_.clear();
            const std::size_t drainedBatches =
                runtimeBridge_->DrainReceivedBatchesForCommandBudget(
                    pendingCommandBatches_,
                    mfd::detail::kMaxAtomicCommandWorkUnits,
                    kMaxCommandBatchesPerFrame);
            if (drainedBatches > 0U)
            {
                receivedFirstClientCommand_ = true;
                bool success = true;
                std::size_t appliedCommands = 0U;
                std::size_t skippedCommands = 0U;

                for (const CommandBatch& batch : pendingCommandBatches_)
                {
                    const std::size_t batchCommandCount = batch.commands.size();
                    const bool batchApplied = commandProcessor_.Submit(batch);
                    success = batchApplied && success;
                    if (batchApplied)
                    {
                        appliedCommands += batchCommandCount;
                        if (debugTelemetryEnabled_)
                        {
                            appliedCommandBatches_.push_back(batch);
                        }
                    }
                    else
                    {
                        skippedCommands += batchCommandCount;
                    }
                }

                runtimeBridge_->RecordCommandProcessingResult(appliedCommands, skippedCommands, 0U);

                if (success)
                {
                    lastCommandStatus_ = "Applied " + std::to_string(appliedCommands) +
                                         " command(s) from the UDP I/O thread.";
                    const UdpRuntimeBridgeMetrics metrics = runtimeBridge_->MetricsSnapshot();
                    if (metrics.inboundQueueDepth > 0U)
                    {
                        lastCommandStatus_ += " " + std::to_string(metrics.inboundQueueDepth) +
                                              " batch(es) remain queued by the frame budget.";
                    }
                }
                else if (!commandProcessor_.LastError().empty())
                {
                    lastCommandStatus_ = commandProcessor_.LastError();
                }
            }
            else
            {
                const UdpRuntimeBridgeMetrics metrics = runtimeBridge_->MetricsSnapshot();
                if (metrics.inboundQueueDepth > 0U)
                {
                    lastCommandStatus_ = "UDP command queue is frame-budget limited; " +
                                         std::to_string(metrics.inboundQueueDepth) +
                                         " batch(es) remain queued.";
                }
                else if (!runtimeBridge_->LastCommandStatus().empty())
                {
                    lastCommandStatus_ = runtimeBridge_->LastCommandStatus();
                }
            }
        }

        PublishWindowLifecycleHeartbeat(deltaSeconds);
        PublishRuntimeFeedback(deltaSeconds);
    }

    [[nodiscard]] bool ApplyLoadedWindowConfiguration(LoadedWindowConfiguration loaded, std::string& error)
    {
        if (loaded.document.pages.empty())
        {
            error = "The window JSON does not contain any page.";
            return false;
        }

        const std::string previousPage = scene_.ActivePageName();
        SceneRegistry candidateScene;
        std::unique_ptr<UdpRuntimeBridge> candidateBridge;
        WindowAssetDefinition candidateWindow;
        std::filesystem::path candidateWindowFile;
        try
        {
            candidateScene.LoadDocument(std::move(loaded.document), std::move(loaded.generatedTransportMap));
            if (candidateScene.HasPage(previousPage))
            {
                candidateScene.SetActivePage(previousPage);
            }

            candidateWindow = std::move(loaded.window);
            candidateWindowFile = candidateWindow.sourceFile;
            candidateBridge = std::make_unique<UdpRuntimeBridge>(
                candidateWindow.commandTransports,
                candidateWindow.feedbackTransports);
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            return false;
        }

        const bool replacesActiveBridge = runtimeBridge_ != nullptr;
        if (replacesActiveBridge)
        {
            runtimeBridge_->Stop();
        }

        std::string candidateBridgeError;
        if (!StartConfiguredBridge(*candidateBridge, candidateBridgeError))
        {
            std::string restoreError;
            if (replacesActiveBridge && !StartConfiguredBridge(*runtimeBridge_, restoreError))
            {
                error = "Reload transport failed: " + candidateBridgeError +
                        " The previous transport could not be restored: " + restoreError;
                return false;
            }

            error = std::string(replacesActiveBridge ? "Reload transport failed: " : "Runtime transport failed: ") +
                    candidateBridgeError;
            return false;
        }

        scene_ = std::move(candidateScene);
        windowDefinition_ = std::move(candidateWindow);
        runtimeBridge_ = std::move(candidateBridge);
        windowFile_ = std::move(candidateWindowFile);
        ResetLoadedState();

        error.clear();
        return true;
    }

    [[nodiscard]] std::optional<StrobeStatusFeedback> BuildStrobeFeedback(const std::string& pageName) const
    {
        const auto strobe = scene_.StrobeForPage(pageName);
        if (!strobe.has_value())
        {
            return std::nullopt;
        }

        if (strobe->pageId == 0U || strobe->strobeTransportId == 0U)
        {
            return std::nullopt;
        }

        StrobeStatusFeedback feedback;
        feedback.pageId = strobe->pageId;
        feedback.strobeId = strobe->strobeTransportId;
        feedback.active = strobe->visible;
        feedback.position = strobe->position;
        feedback.capture = strobe->capture;

        // Resolve magnetization and capture in a single dynamic-reticle pass instead of two.
        const StrobeScanResult scan = scene_.ScanStrobeForPage(pageName);
        feedback.magnet = ToFeedbackMagnet(scan.magnet);
        if (scan.capture.has_value())
        {
            feedback.captureResult = ToFeedbackCapture(*scan.capture);
        }

        return feedback;
    }

    // The window lifecycle heartbeat is a window-level liveness signal. It is
    // intentionally independent of the active page and of any strobe so that an
    // idle window with no strobe still proves it is alive to its clients.
    void PublishWindowLifecycleHeartbeat(const float deltaSeconds)
    {
        if (runtimeBridge_ == nullptr || !runtimeBridge_->FeedbackTransportReady())
        {
            return;
        }

        lifecycleThrottle_.Advance(deltaSeconds);

        const float heartbeatInterval = windowDefinition_.feedbackHeartbeatIntervalSeconds;
        if (!lifecycleHeartbeatPending_ && !lifecycleThrottle_.ShouldSendHeartbeat(heartbeatInterval))
        {
            return;
        }

        WindowLifecycleFeedback lifecycle;
        lifecycle.sequence = nextFeedbackSequence_++;
        lifecycle.state = WindowLifecycleState::Alive;
        runtimeBridge_->EnqueueWindowLifecycleFeedback(std::move(lifecycle));

        lifecycleThrottle_.MarkHeartbeatSent(heartbeatInterval);
        lifecycleHeartbeatPending_ = false;
    }

    void NotifyClosing()
    {
        if (closingNotified_ || runtimeBridge_ == nullptr || !runtimeBridge_->HasFeedbackSender())
        {
            return;
        }

        WindowLifecycleFeedback lifecycle;
        lifecycle.sequence = nextFeedbackSequence_++;
        lifecycle.state = WindowLifecycleState::Closing;
        runtimeBridge_->EnqueueWindowLifecycleFeedback(std::move(lifecycle));
        closingNotified_ = true;
    }

    void SetCommandTransactionMode(const RuntimeCommandTransactionMode mode) noexcept
    {
        commandTransactionMode_ = mode;
        commandProcessor_.SetBatchTransactionMode(ToCommandProcessorTransactionMode(mode));
    }

    void SetDebugTelemetryEnabled(const bool enabled) noexcept
    {
        debugTelemetryEnabled_ = enabled;
        if (!debugTelemetryEnabled_)
        {
            appliedCommandBatches_.clear();
        }
    }

    [[nodiscard]] RuntimeCommandTransactionMode CommandTransactionMode() const noexcept
    {
        return commandTransactionMode_;
    }

    void PublishRuntimeFeedback(const float deltaSeconds)
    {
        if (runtimeBridge_ == nullptr || !runtimeBridge_->FeedbackTransportReady())
        {
            return;
        }

        feedbackThrottle_.Advance(deltaSeconds);
        const float fastInterval = windowDefinition_.feedbackFastIntervalSeconds;
        const float heartbeatInterval = windowDefinition_.feedbackHeartbeatIntervalSeconds;
        const bool changedDue = feedbackThrottle_.ShouldSendChanged(fastInterval);
        const bool heartbeatDue = feedbackThrottle_.ShouldSendHeartbeat(heartbeatInterval);
        bool sentChanged = false;
        bool sentHeartbeat = false;

        const std::string activePageName = scene_.ActivePageName();
        const TransportId activePageId = scene_.ActivePageTransportId();
        const bool activePageChanged =
            !lastPublishedActivePage_.has_value() || *lastPublishedActivePage_ != activePageName;
        if (activePageId != 0U && activePageChanged && changedDue)
        {
            ActivePageFeedback activePageFeedback;
            activePageFeedback.sequence = nextFeedbackSequence_++;
            activePageFeedback.pageId = activePageId;
            runtimeBridge_->EnqueueActivePageFeedback(std::move(activePageFeedback));
            lastPublishedActivePage_ = activePageName;
            sentChanged = true;
        }
        else if (activePageId != 0U && !activePageChanged && heartbeatDue)
        {
            ActivePageFeedback activePageFeedback;
            activePageFeedback.sequence = nextFeedbackSequence_++;
            activePageFeedback.pageId = activePageId;
            runtimeBridge_->EnqueueActivePageFeedback(std::move(activePageFeedback));
            sentHeartbeat = true;
        }

        // BuildStrobeFeedback() scans the scene's dynamic reticles; skip it entirely when
        // neither a changed nor a heartbeat feedback could be sent this frame anyway.
        const bool feedbackDue = changedDue || heartbeatDue;
        if (scene_.ActivePageHasStrobe() && feedbackDue)
        {
            std::optional<StrobeStatusFeedback> feedback = BuildStrobeFeedback(activePageName);
            if (feedback.has_value())
            {
                const auto previous = lastPublishedStrobeFeedbacks_.find(activePageName);
                const bool changed = previous == lastPublishedStrobeFeedbacks_.end() ||
                                     !SameStrobeFeedbackState(previous->second, *feedback);
                const bool shouldSendChanged = activePageChanged || changed;
                if (shouldSendChanged && changedDue)
                {
                    feedback->sequence = nextFeedbackSequence_++;
                    runtimeBridge_->EnqueueStrobeFeedback(*feedback);
                    lastPublishedStrobeFeedbacks_[activePageName] = std::move(*feedback);
                    sentChanged = true;
                }
                else if (!shouldSendChanged && heartbeatDue)
                {
                    feedback->sequence = nextFeedbackSequence_++;
                    runtimeBridge_->EnqueueStrobeFeedback(*feedback);
                    sentHeartbeat = true;
                }
            }
        }

        if (sentChanged)
        {
            feedbackThrottle_.MarkChangedSent(fastInterval);
        }
        else if (sentHeartbeat)
        {
            feedbackThrottle_.MarkHeartbeatSent(heartbeatInterval);
        }

        if (!runtimeBridge_->LastFeedbackStatus().empty())
        {
            lastFeedbackStatus_ = runtimeBridge_->LastFeedbackStatus();
        }
    }

    [[nodiscard]] RuntimeWindowInfo PublicWindowInfo() const
    {
        RuntimeWindowInfo info;
        info.title = windowDefinition_.title;
        info.width = windowDefinition_.width;
        info.height = windowDefinition_.height;
        info.targetFps = windowDefinition_.targetFps;
        return info;
    }

    [[nodiscard]] std::vector<RuntimePageInfo> PublicPages() const
    {
        std::vector<RuntimePageInfo> pages;
        const std::vector<PageSummary> scenePages = scene_.Pages();
        pages.reserve(scenePages.size());
        for (const PageSummary& page : scenePages)
        {
            RuntimePageInfo info;
            info.name = page.name;
            info.title = page.title;
            info.active = page.active;
            pages.push_back(std::move(info));
        }

        return pages;
    }

    [[nodiscard]] bool SetActivePage(const std::string_view pageName)
    {
        if (!scene_.HasPage(pageName))
        {
            return false;
        }

        scene_.SetActivePage(pageName);
        return true;
    }

    [[nodiscard]] const std::filesystem::path& WindowFile() const noexcept
    {
        return windowFile_;
    }

    [[nodiscard]] std::string ActivePageName() const
    {
        return scene_.ActivePageName();
    }

    [[nodiscard]] const std::string& LastCommandStatus() const noexcept
    {
        return lastCommandStatus_;
    }

    [[nodiscard]] const std::string& LastFeedbackStatus() const noexcept
    {
        return lastFeedbackStatus_;
    }

    [[nodiscard]] bool ReceivedFirstClientCommand() const noexcept
    {
        return receivedFirstClientCommand_;
    }

    [[nodiscard]] SceneRegistry& Scene() noexcept
    {
        return scene_;
    }

    [[nodiscard]] const SceneRegistry& Scene() const noexcept
    {
        return scene_;
    }

    [[nodiscard]] const WindowAssetDefinition& WindowDefinition() const noexcept
    {
        return windowDefinition_;
    }

    [[nodiscard]] UdpRuntimeBridge* RuntimeBridge() noexcept
    {
        return runtimeBridge_.get();
    }

    [[nodiscard]] const UdpRuntimeBridge* RuntimeBridge() const noexcept
    {
        return runtimeBridge_.get();
    }

    [[nodiscard]] const std::vector<CommandBatch>& AppliedCommandBatches() const noexcept
    {
        return appliedCommandBatches_;
    }

private:
    void ResetLoadedState()
    {
        mfd::detail::CommandProcessorInternalAccess::ResetTransportState(commandProcessor_);
        feedbackThrottle_.Reset();
        lifecycleThrottle_.Reset();
        pendingCommandBatches_.clear();
        appliedCommandBatches_.clear();
        lastPublishedStrobeFeedbacks_.clear();
        lastPublishedActivePage_.reset();
        nextFeedbackSequence_ = 1U;
        lifecycleHeartbeatPending_ = true;
        closingNotified_ = false;
        receivedFirstClientCommand_ = false;

        lastCommandStatus_ = runtimeBridge_ == nullptr || !runtimeBridge_->HasCommandReceiver()
                                 ? "No UDP command transport configured in the window JSON."
                                 : runtimeBridge_->LastCommandStatus();
        lastFeedbackStatus_ = runtimeBridge_ == nullptr || !runtimeBridge_->HasFeedbackSender()
                                  ? "No UDP feedback transport configured in the window JSON."
                                  : runtimeBridge_->LastFeedbackStatus();
    }

    std::filesystem::path windowFile_ {};
    JsonLoader loader_ {};
    SceneRegistry scene_ {};
    CommandProcessor commandProcessor_ {scene_};
    WindowAssetDefinition windowDefinition_ {};
    std::unique_ptr<UdpRuntimeBridge> runtimeBridge_ {};
    RuntimeFeedbackThrottle feedbackThrottle_ {};
    RuntimeFeedbackThrottle lifecycleThrottle_ {};
    std::vector<CommandBatch> pendingCommandBatches_ {};
    std::vector<CommandBatch> appliedCommandBatches_ {};
    std::unordered_map<std::string, StrobeStatusFeedback> lastPublishedStrobeFeedbacks_ {};
    std::optional<std::string> lastPublishedActivePage_ {};
    RuntimeCommandTransactionMode commandTransactionMode_ = RuntimeCommandTransactionMode::Transactional;
    bool debugTelemetryEnabled_ = false;
    std::uint32_t nextFeedbackSequence_ = 1U;
    bool lifecycleHeartbeatPending_ = true;
    bool closingNotified_ = false;
    bool receivedFirstClientCommand_ = false;
    std::string lastCommandStatus_ {};
    std::string lastFeedbackStatus_ {};
};

RuntimeSession::RuntimeSession()
    : impl_(std::make_unique<Impl>())
{
}

RuntimeSession::~RuntimeSession() = default;

bool RuntimeSession::LoadWindowFile(const std::filesystem::path& windowFile, std::string& error)
{
    return impl_ != nullptr && impl_->LoadWindowFile(windowFile, error);
}

bool RuntimeSession::Reload(std::string& error)
{
    return impl_ != nullptr && impl_->Reload(error);
}

void RuntimeSession::Advance(const float deltaSeconds)
{
    if (impl_ != nullptr)
    {
        impl_->Advance(deltaSeconds);
    }
}

void RuntimeSession::NotifyClosing()
{
    if (impl_ != nullptr)
    {
        impl_->NotifyClosing();
    }
}

void RuntimeSession::SetCommandTransactionMode(const RuntimeCommandTransactionMode mode) noexcept
{
    if (impl_ != nullptr)
    {
        impl_->SetCommandTransactionMode(mode);
    }
}

RuntimeCommandTransactionMode RuntimeSession::CommandTransactionMode() const noexcept
{
    return impl_ == nullptr ? RuntimeCommandTransactionMode::Transactional : impl_->CommandTransactionMode();
}

const std::filesystem::path& RuntimeSession::WindowFile() const noexcept
{
    static const std::filesystem::path kEmptyPath;
    return impl_ == nullptr ? kEmptyPath : impl_->WindowFile();
}

RuntimeWindowInfo RuntimeSession::WindowInfo() const
{
    return impl_ == nullptr ? RuntimeWindowInfo {} : impl_->PublicWindowInfo();
}

std::vector<RuntimePageInfo> RuntimeSession::Pages() const
{
    return impl_ == nullptr ? std::vector<RuntimePageInfo> {} : impl_->PublicPages();
}

std::string RuntimeSession::ActivePageName() const
{
    return impl_ == nullptr ? std::string {} : impl_->ActivePageName();
}

bool RuntimeSession::SetActivePage(const std::string_view pageName)
{
    return impl_ != nullptr && impl_->SetActivePage(pageName);
}

const std::string& RuntimeSession::LastCommandStatus() const noexcept
{
    static const std::string kEmptyStatus;
    return impl_ == nullptr ? kEmptyStatus : impl_->LastCommandStatus();
}

const std::string& RuntimeSession::LastFeedbackStatus() const noexcept
{
    static const std::string kEmptyStatus;
    return impl_ == nullptr ? kEmptyStatus : impl_->LastFeedbackStatus();
}

bool RuntimeSession::ReceivedFirstClientCommand() const noexcept
{
    return impl_ != nullptr && impl_->ReceivedFirstClientCommand();
}
} // namespace mfd::runtime_api

namespace mfd::runtime_api::internal
{
SceneRegistry& RuntimeSessionInternalAccess::Scene(RuntimeSession& session) noexcept
{
    return session.impl_->Scene();
}

const SceneRegistry& RuntimeSessionInternalAccess::Scene(const RuntimeSession& session) noexcept
{
    return session.impl_->Scene();
}

const WindowAssetDefinition& RuntimeSessionInternalAccess::WindowDefinition(const RuntimeSession& session) noexcept
{
    return session.impl_->WindowDefinition();
}

UdpRuntimeBridge* RuntimeSessionInternalAccess::RuntimeBridge(RuntimeSession& session) noexcept
{
    return session.impl_->RuntimeBridge();
}

const UdpRuntimeBridge* RuntimeSessionInternalAccess::RuntimeBridge(const RuntimeSession& session) noexcept
{
    return session.impl_->RuntimeBridge();
}

const std::vector<CommandBatch>& RuntimeSessionInternalAccess::AppliedCommandBatches(
    const RuntimeSession& session) noexcept
{
    static const std::vector<CommandBatch> kEmptyBatches;
    return session.impl_ == nullptr ? kEmptyBatches : session.impl_->AppliedCommandBatches();
}

void RuntimeSessionInternalAccess::SetDebugTelemetryEnabled(RuntimeSession& session, const bool enabled) noexcept
{
    if (session.impl_ != nullptr)
    {
        session.impl_->SetDebugTelemetryEnabled(enabled);
    }
}
} // namespace mfd::runtime_api::internal
