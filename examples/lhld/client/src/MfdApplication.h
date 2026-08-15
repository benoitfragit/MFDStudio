/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Dear ImGui front-end for the LHLD demonstration client.
 *
 * Draws a 20-button MFD bezel in ImGui with the authored MFD page rendered
 * offscreen and composited into a DX11 texture at its center. The control
 * surface (bezel and panel) is kept separate from the deterministic radar
 * mini-simulation, exactly like the HUD client.
 */

#include <cstdint>
#include <memory>
#include <string>

#include "Dx11TextureUploader.h"
#include "LhldUi.h"
#include "MfdController.h"
#include "MfdInputSource.h"
#include "OffscreenMfdView.h"
#include "mfd/client/ClientSdk.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace lhld
{
/**
 * @brief Interactive LHLD demo application.
 */
class MfdApplication
{
public:
    MfdApplication();

    /**
     * @brief Creates the application with a caller-provided semantic source.
     * @param inputSource Replaceable LHLD input source. A null pointer falls
     * back to the bundled deterministic simulation.
     */
    explicit MfdApplication(std::unique_ptr<MfdInputSource> inputSource);

    /**
     * @brief Loads the window configuration, connects transports and the offscreen host.
     * @param device DX11 device owned by the host, used for the MFD texture.
     * @param context DX11 immediate context owned by the host.
     * @param error Human-readable failure reason populated on error.
     * @return `true` when the client can publish and render frames.
     */
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, std::string& error);

    /**
     * @brief Draws one ImGui frame, advances the simulation and publishes commands.
     * @param deltaSeconds Host frame delta in seconds.
     */
    void DrawFrame(float deltaSeconds);

    /**
     * @brief Flushes pending commands and emits a shutdown caption.
     */
    void Shutdown();

private:
    /** @brief Parsed asset/transport configuration reused across reconnects. */
    struct MfdConfig
    {
        mfd::WindowUdpCommandTransport commandTransport {};
        mfd::WindowFeedbackTransportConfig feedbackTransport {};
        mfd::GeneratedTransportMap generatedTransportMap {};
        mfd::PageViewState pageView {};
        std::string windowFile {};
    };

    bool LoadConfig(MfdConfig& config, std::string& error) const;
    bool Connect(const MfdConfig& config, std::string& error);

    void SelectPage(MfdPage page);
    void HandleKeyboardShortcuts();
    void HandleOsbPress(int osbOneBased);
    void StepSmsStation() noexcept;
    void StepAgStation() noexcept;
    void StepNavSteerpoint() noexcept;
    void CycleNavRange() noexcept;
    void ToggleHsdCenteredDisplay() noexcept;
    void ToggleHsdDeclutter() noexcept;
    void AddWaypointFromDraft() noexcept;
    void OverwriteCurrentWaypointFromDraft() noexcept;
    void CycleAgProfile() noexcept;
    void CycleAgQuantity() noexcept;
    void CycleReleaseInterval() noexcept;
    void ToggleAgPairs() noexcept;
    void SelectAgDeliveryMode(AirGroundDeliveryMode mode) noexcept;
    void ReleaseSelectedAgStore();
    void UpdateAgReleaseTimer(float deltaSeconds) noexcept;
    void JettisonStores();
    void ApplyControlsToInputSource() noexcept;
    void SelectRadarSearchSubmode(RadarSubmode submode) noexcept;
    void BugCapturedRadarTrack();
    void EnterSingleTargetTrack();
    void ClearRadarBug() noexcept;
    void SetRadarOperatingState(RadarOperatingState state) noexcept;

    void DrawPageSelectorBar();
    void DrawMfdUnit(float deltaSeconds);
    void DrawBezelButtons(float unitOriginX, float unitOriginY, float unitSide, float imageInset);
    void DrawControlPanel();
    void PublishFrame();
    void PollFeedback();
    void ResetScene();
    void SetStatus(std::string status, bool error);

    /** @brief Generated UI command wrapper for `examples/lhld/assets`. */
    lhld_ui::LhldUi ui_ {};
    /** @brief Replaceable semantic input producer used by the generated UI. */
    std::unique_ptr<MfdInputSource> inputSource_ {};
    /** @brief Stateless adapter from semantic inputs to generated UI handles. */
    MfdController controller_ {};
    /** @brief In-process runtime host rendering the active MFD page offscreen. */
    OffscreenMfdView offscreenView_ {};
    /** @brief DX11 texture holding the latest offscreen MFD frame. */
    Dx11TextureUploader uploader_ {};

    /** @brief Latest semantic sample published to the MFD pages. */
    MfdInputSample inputs_ {};
    /** @brief Operator radar controls collected from the bezel/panel. */
    RadarSettings radarControls_ {};
    /** @brief Search mode restored when STT is exited. */
    RadarSubmode radarReturnSubmode_ = RadarSubmode::Rws;
    /** @brief Operator-selected master mode. */
    MasterMode masterMode_ = MasterMode::AirToAir;
    /** @brief Operator stores-management controls. */
    StoresState stores_ {};
    /** @brief Operator-edited HSD navigation state and flight plan. */
    NavState navControls_ {};
    /** @brief Draft waypoint bearing used by the panel waypoint-creation controls. */
    float waypointDraftBearingDeg_ = 135.0f;
    /** @brief Draft waypoint range used by the panel waypoint-creation controls. */
    float waypointDraftRangeNm_ = 36.0f;
    /** @brief Page currently owning the bezel and shown offscreen. */
    MfdPage activePage_ = MfdPage::Radar;

    /** @brief Parsed asset and transport configuration. */
    MfdConfig config_ {};
    /** @brief Reliable startup client used for the initial scene submission. */
    std::unique_ptr<mfd::CommandClient> startupClient_ {};
    /** @brief Latest-batch UDP publisher used for realtime updates. */
    std::unique_ptr<mfd::client::LatestBatchPublisher> publisher_ {};
    /** @brief Optional feedback receiver for window liveness. */
    std::unique_ptr<mfd::IExchangeChannel> feedbackReceiver_ {};
    /** @brief Detects a stale or closed runtime from decoded feedback packets. */
    mfd::client::WindowLivenessMonitor livenessMonitor_ {2.0};
    /** @brief Monotonic command sequence number. */
    std::uint32_t sequence_ = 1;
    /** @brief Application uptime used as the liveness clock. */
    double elapsedSeconds_ = 0.0;
    /** @brief True while command publishing is available. */
    bool connected_ = false;
    /** @brief True while the mini-simulation should advance each frame. */
    bool running_ = true;
    /** @brief Last operator-facing status message. */
    std::string status_ {};
    /** @brief True when `status_` should be rendered as an error. */
    bool statusIsError_ = false;
};
} // namespace lhld
