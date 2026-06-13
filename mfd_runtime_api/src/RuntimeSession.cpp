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
#include "mfd/control/CommandProcessor.h"

namespace mfd::runtime_api
{
namespace
{
constexpr std::size_t kMaxCommandsPerFrame = 512U;
constexpr float kMinimumIntervalSeconds = 0.001f;

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
        capture.reticleId,
        capture.sourceTemplateId,
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
        magnetSummary->reticleId,
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
           lhs.reticleId == rhs.reticleId &&
           SameVec2(lhs.targetPosition, rhs.targetPosition) &&
           lhs.distance == rhs.distance;
}

bool SameFeedbackCapture(const StrobeFeedbackCapture& lhs, const StrobeFeedbackCapture& rhs)
{
    return lhs.runtimeReticleId == rhs.runtimeReticleId &&
           lhs.sourceTemplateTransportId == rhs.sourceTemplateTransportId &&
           lhs.reticleId == rhs.reticleId &&
           lhs.sourceTemplateId == rhs.sourceTemplateId &&
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
           lhs.pageName == rhs.pageName &&
           lhs.strobeId == rhs.strobeId &&
           lhs.active == rhs.active &&
           SameVec2(lhs.position, rhs.position) &&
           SameCaptureConfig(lhs.capture, rhs.capture) &&
           SameFeedbackMagnet(lhs.magnet, rhs.magnet) &&
           SameOptionalFeedbackCapture(lhs.captureResult, rhs.captureResult);
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

        if (runtimeBridge_ != nullptr)
        {
            pendingCommandBatches_.clear();
            const std::size_t drainedBatches =
                runtimeBridge_->DrainReceivedBatchesForCommandBudget(pendingCommandBatches_, kMaxCommandsPerFrame);
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
                        appliedCommandBatches_.push_back(batch);
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

        PublishStrobeFeedbacks(deltaSeconds);
    }

    [[nodiscard]] bool ApplyLoadedWindowConfiguration(LoadedWindowConfiguration loaded, std::string& error)
    {
        if (loaded.document.pages.empty())
        {
            error = "The window JSON does not contain any page.";
            return false;
        }

        const std::string previousPage = scene_.ActivePageName();

        windowDefinition_ = loaded.window;
        scene_.LoadDocument(std::move(loaded.document), std::move(loaded.generatedTransportMap));

        auto newBridge = std::make_unique<UdpRuntimeBridge>(
            windowDefinition_.commandTransports,
            windowDefinition_.feedbackTransports);
        newBridge->Start();

        if (scene_.HasPage(previousPage))
        {
            scene_.SetActivePage(previousPage);
        }

        runtimeBridge_ = std::move(newBridge);
        windowFile_ = windowDefinition_.sourceFile;
        feedbackThrottle_.Reset();
        pendingCommandBatches_.clear();
        appliedCommandBatches_.clear();
        lastPublishedStrobeFeedbacks_.clear();
        lastPublishedActivePage_.reset();
        nextStrobeFeedbackSequence_ = 1U;
        receivedFirstClientCommand_ = false;

        if (runtimeBridge_ == nullptr || !runtimeBridge_->HasCommandReceiver())
        {
            lastCommandStatus_ = "No UDP command transport configured in the window JSON.";
        }
        else
        {
            lastCommandStatus_ = runtimeBridge_->LastCommandStatus();
        }

        if (runtimeBridge_ == nullptr || !runtimeBridge_->HasFeedbackSender())
        {
            lastFeedbackStatus_ = "No UDP feedback transport configured in the window JSON.";
        }
        else
        {
            lastFeedbackStatus_ = runtimeBridge_->LastFeedbackStatus();
        }

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

        StrobeStatusFeedback feedback;
        feedback.pageId = strobe->pageId;
        feedback.pageName = strobe->pageName;
        feedback.strobeId = strobe->reticleId;
        feedback.active = strobe->visible;
        feedback.position = strobe->position;
        feedback.capture = strobe->capture;
        feedback.magnet = ToFeedbackMagnet(scene_.StrobeMagnetForPage(pageName));

        if (const auto capture = scene_.CaptureWithStrobe(pageName); capture.has_value())
        {
            feedback.captureResult = ToFeedbackCapture(*capture);
        }

        return feedback;
    }

    void PublishStrobeFeedbacks(const float deltaSeconds)
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
        const bool activePageChanged =
            !lastPublishedActivePage_.has_value() || *lastPublishedActivePage_ != activePageName;
        if (!activePageName.empty() && activePageChanged && changedDue)
        {
            ActivePageFeedback activePageFeedback;
            activePageFeedback.sequence = nextStrobeFeedbackSequence_++;
            activePageFeedback.pageName = activePageName;
            runtimeBridge_->EnqueueActivePageFeedback(std::move(activePageFeedback));
            lastPublishedActivePage_ = activePageName;
            sentChanged = true;
        }
        else if (!activePageName.empty() && !activePageChanged && heartbeatDue)
        {
            ActivePageFeedback activePageFeedback;
            activePageFeedback.sequence = nextStrobeFeedbackSequence_++;
            activePageFeedback.pageName = activePageName;
            runtimeBridge_->EnqueueActivePageFeedback(std::move(activePageFeedback));
            sentHeartbeat = true;
        }

        if (scene_.ActivePageHasStrobe())
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
                    feedback->sequence = nextStrobeFeedbackSequence_++;
                    runtimeBridge_->EnqueueStrobeFeedback(*feedback);
                    lastPublishedStrobeFeedbacks_[activePageName] = std::move(*feedback);
                    sentChanged = true;
                }
                else if (!shouldSendChanged && heartbeatDue)
                {
                    feedback->sequence = nextStrobeFeedbackSequence_++;
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
    std::filesystem::path windowFile_ {};
    JsonLoader loader_ {};
    SceneRegistry scene_ {};
    CommandProcessor commandProcessor_ {scene_};
    WindowAssetDefinition windowDefinition_ {};
    std::unique_ptr<UdpRuntimeBridge> runtimeBridge_ {};
    RuntimeFeedbackThrottle feedbackThrottle_ {};
    std::vector<CommandBatch> pendingCommandBatches_ {};
    std::vector<CommandBatch> appliedCommandBatches_ {};
    std::unordered_map<std::string, StrobeStatusFeedback> lastPublishedStrobeFeedbacks_ {};
    std::optional<std::string> lastPublishedActivePage_ {};
    std::uint32_t nextStrobeFeedbackSequence_ = 1U;
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
} // namespace mfd::runtime_api::internal
