/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Implementation of the HUD runtime publishing facade.
 */

#include "hud/HudRuntimeClient.h"

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>

#include "HudUi.h"
#include "hud/HudController.h"
#include "mfd/client/ClientSdk.h"
#include "mfd/control/CommandClient.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/PageName.h"

namespace hud
{
namespace
{
/** Feedback silence tolerated before the HUD window is considered lost. */
constexpr double kFeedbackLivenessTimeoutSeconds = 2.0;
/** Feedback packets drained per published frame. */
constexpr std::size_t kMaxFeedbackMessagesPerPublish = 16;

/**
 * @brief Transport and page configuration parsed from the HUD window JSON.
 */
struct HudRuntimeWindowConfig
{
    mfd::WindowUdpCommandTransport commandTransport {};
    mfd::WindowFeedbackTransportConfig feedbackTransport {};
    mfd::GeneratedTransportMap generatedTransportMap {};
    mfd::PageViewState pageView {};
};

/**
 * @brief Resolves the HUD window JSON path for one initialization request.
 * @param assetRoot Optional caller-provided HUD asset directory.
 * @return Path of the window JSON to load.
 *
 * With an empty @p assetRoot the staged runtime layout next to the working
 * directory wins; the in-repository path baked into the generated UI remains
 * the fallback so developers can run consumers from the repository root.
 */
std::string ResolveWindowJsonPath(const std::string& assetRoot)
{
    const std::filesystem::path windowRelativePath =
        std::filesystem::path("windows") / "hud_window.json";
    if (!assetRoot.empty())
    {
        return (std::filesystem::path(assetRoot) / windowRelativePath).lexically_normal().string();
    }

    const std::filesystem::path stagedPath = std::filesystem::path("assets") / windowRelativePath;
    std::error_code stagedPathError;
    if (std::filesystem::exists(stagedPath, stagedPathError))
    {
        return stagedPath.string();
    }

    return std::string(hud_ui::HudUi::WindowFile());
}

/**
 * @brief Loads and validates the HUD window JSON and its generated transport map.
 */
bool LoadWindowConfig(const std::string& windowJsonPath, HudRuntimeWindowConfig& config, std::string& error)
{
    try
    {
        mfd::JsonLoader loader;
        const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(windowJsonPath);

        const mfd::PageDefinition* hudPage =
            mfd::FindPageDefinition(loaded.document, hud_ui::HUDMockupPage::Name());
        if (hudPage == nullptr)
        {
            error = "The HUD asset does not expose a page named 'HUD'.";
            return false;
        }

        if (!loaded.window.commandTransports.udp.has_value() || !loaded.window.commandTransports.udp->enabled)
        {
            // The HUD runtime is driven by generated UDP command ids; without
            // this transport the client cannot publish any visible frame.
            error = "The HUD window JSON has no enabled UDP command transport.";
            return false;
        }

        if (!loaded.generatedTransportMap.has_value())
        {
            // The transport map binds generated UI handles to runtime command
            // identifiers. Failing early avoids silently sending unusable ids.
            error = "The HUD generated transport map is missing.";
            return false;
        }

        config.commandTransport = *loaded.window.commandTransports.udp;
        config.feedbackTransport = loaded.window.feedbackTransports;
        config.generatedTransportMap = *loaded.generatedTransportMap;
        config.pageView = hudPage->view;
        return true;
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
        return false;
    }
}
} // namespace

/**
 * @brief Transport, generated-UI and liveness state hidden behind the PIMPL.
 */
struct HudRuntimeClient::Impl
{
    /** Generated UI command wrapper for `examples/hud/assets`. */
    hud_ui::HudUi ui {};
    /** Stateless adapter from semantic HUD inputs to generated UI handles. */
    HudController controller {};
    /** Reliable startup client used for initial scene submission. */
    std::unique_ptr<mfd::CommandClient> startupClient {};
    /** Latest-batch UDP publisher used for realtime HUD updates. */
    std::unique_ptr<mfd::client::LatestBatchPublisher> publisher {};
    /** Optional feedback receiver for window close and packet-liveness signals. */
    std::unique_ptr<mfd::IExchangeChannel> feedbackReceiver {};
    /** Detects stale or closed runtime windows from decoded feedback packets. */
    mfd::client::WindowLivenessMonitor livenessMonitor {kFeedbackLivenessTimeoutSeconds};
    /** Wall-clock origin of the liveness observation timeline. */
    std::chrono::steady_clock::time_point livenessEpoch {};
    /** Monotonic command sequence number sent with generated command batches. */
    std::uint32_t sequence = 1;
    /** True while command publishing is available. */
    bool connected = false;

    bool Initialize(const HudRuntimeConfig& config, std::string& error);
    bool Publish(const HudInputSample& input, std::string& error);
    void Shutdown();

private:
    void ReleaseTransports() noexcept;
    double ElapsedLivenessSeconds() const;
    bool PollFeedback(std::string& error);
};

bool HudRuntimeClient::Impl::Initialize(const HudRuntimeConfig& config, std::string& error)
{
    // Re-initialization intentionally drops the previous transports without a
    // shutdown caption: this is the reconnect path, not a client stop.
    ReleaseTransports();

    HudRuntimeWindowConfig windowConfig;
    if (!LoadWindowConfig(ResolveWindowJsonPath(config.assetRoot), windowConfig, error))
    {
        return false;
    }

    // The startup client sends the complete initial generated scene state. The
    // realtime publisher then sends only the latest command batch each frame.
    startupClient =
        std::make_unique<mfd::CommandClient>(windowConfig.commandTransport, windowConfig.generatedTransportMap);
    if (!startupClient->IsReady())
    {
        error = startupClient->LastError();
        return false;
    }

    publisher = std::make_unique<mfd::client::LatestBatchPublisher>(windowConfig.commandTransport);
    if (!publisher->IsReady())
    {
        error = publisher->LastError();
        return false;
    }

    feedbackReceiver = mfd::CreateFeedbackReceiverChannel(windowConfig.feedbackTransport);
    livenessMonitor.Reset();
    livenessEpoch = std::chrono::steady_clock::now();
    ui.Initialize();
    // `SendStartup` also seeds generated baselines, so later `SubmitLatest`
    // calls can remain compact and only carry changed handles.
    if (!ui.SendStartup(*startupClient, windowConfig.pageView, "HUD | READY"))
    {
        error = startupClient->LastError();
        return false;
    }

    connected = true;
    return true;
}

bool HudRuntimeClient::Impl::Publish(const HudInputSample& input, std::string& error)
{
    if (!connected || publisher == nullptr || !publisher->IsReady())
    {
        error = "The HUD runtime client is not initialized.";
        return false;
    }

    // `Run` closes the previous generated-UI cycle. `Populate` then writes
    // fresh handles from the semantic sample before `SubmitLatest` builds the
    // dirty command batch for this frame.
    ui.Run();
    controller.Populate(ui, input);
    if (!ui.SubmitLatest(*publisher, sequence++))
    {
        error = publisher->LastError();
        connected = false;
        return false;
    }

    const std::string publisherError = publisher->LastError();
    if (!publisherError.empty())
    {
        // Treat asynchronous publisher errors as a lost connection so callers
        // stop assuming that realtime updates are still flowing.
        error = publisherError;
        connected = false;
        return false;
    }

    return PollFeedback(error);
}

bool HudRuntimeClient::Impl::PollFeedback(std::string& error)
{
    if (feedbackReceiver == nullptr)
    {
        return true;
    }

    std::string feedbackError;
    // Feedback is best-effort. The liveness monitor only needs packet counters
    // and the runtime close flag to detect a stale HUD window.
    ui.PollFeedback(*feedbackReceiver, kMaxFeedbackMessagesPerPublish, &feedbackError);
    livenessMonitor.Observe(
        ui.TotalDecodedFeedbackPackets(),
        ui.WindowReportedClosing(),
        ElapsedLivenessSeconds());
    if (livenessMonitor.ConsumeDisconnect())
    {
        error = "HUD window feedback lost or closed.";
        connected = false;
        return false;
    }

    return true;
}

void HudRuntimeClient::Impl::Shutdown()
{
    if (publisher != nullptr && publisher->IsReady())
    {
        // The final caption helps distinguish a clean client shutdown from a
        // lost feedback/transport failure in the HUD window.
        ui.SubmitShutdown(*publisher, sequence++, "HUD | CLIENT STOPPED");
        publisher->Flush();
    }

    ReleaseTransports();
}

void HudRuntimeClient::Impl::ReleaseTransports() noexcept
{
    feedbackReceiver.reset();
    publisher.reset();
    startupClient.reset();
    connected = false;
}

double HudRuntimeClient::Impl::ElapsedLivenessSeconds() const
{
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - livenessEpoch;
    return elapsed.count();
}

HudRuntimeClient::HudRuntimeClient()
    : impl_(std::make_unique<Impl>())
{
}

HudRuntimeClient::~HudRuntimeClient()
{
    try
    {
        Shutdown();
    }
    catch (...)
    {
        // Destruction must stay noexcept; a failed shutdown caption is dropped.
    }
}

HudRuntimeClient::HudRuntimeClient(HudRuntimeClient&&) noexcept = default;
HudRuntimeClient& HudRuntimeClient::operator=(HudRuntimeClient&&) noexcept = default;

bool HudRuntimeClient::Initialize(std::string& error)
{
    return Initialize(HudRuntimeConfig {}, error);
}

bool HudRuntimeClient::Initialize(const HudRuntimeConfig& config, std::string& error)
{
    if (impl_ == nullptr)
    {
        // A moved-from client can be revived by an explicit initialization.
        impl_ = std::make_unique<Impl>();
    }

    return impl_->Initialize(config, error);
}

bool HudRuntimeClient::Publish(const HudInputSample& input, std::string& error)
{
    if (impl_ == nullptr)
    {
        error = "The HUD runtime client is not initialized.";
        return false;
    }

    return impl_->Publish(input, error);
}

void HudRuntimeClient::Shutdown()
{
    if (impl_ != nullptr)
    {
        impl_->Shutdown();
    }
}
} // namespace hud
