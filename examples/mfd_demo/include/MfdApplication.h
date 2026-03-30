/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Interactive example window used to exercise rendering, UDP commands and feedback.
 */

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "DemoTrackFeed.h"
#include "mfd/control/CommandProcessor.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/control/UdpRuntimeBridge.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/render/MfdRenderer.h"
#include "mfd/render/OpenGlFramebufferReader.h"
#include "mfd/runtime/SceneRegistry.h"

namespace mfd
{
/**
 * @brief Startup configuration of the demo application.
 *
 * @note Swapping the root window JSON is enough to reuse the same executable
 * against another authored window.
 */
struct ApplicationConfig
{
    /** @brief Root JSON file describing the window, its pages and its transports. */
    std::filesystem::path windowFile = "assets/windows/demo_pages.json";
};

/**
 * @brief End-to-end sample window embedding a live scene, a renderer and optional UDP runtime bridging.
 *
 * @details `MfdApplication` is the reference "real window" used by the sample
 * programs. It loads authored JSON, owns a live `SceneRegistry`, renders the
 * active page, applies remote commands and exposes optional tools such as
 * framebuffer capture, picture-in-picture preview and strobe feedback.
 */
class MfdApplication
{
public:
    /**
     * @brief Builds the demo application from its startup configuration.
     * @param config Root window file and related startup options.
     */
    explicit MfdApplication(ApplicationConfig config = {});
    /** @brief Releases runtime resources such as preview textures and UDP bridges. */
    ~MfdApplication();

    /**
     * @brief Runs the full window lifecycle until the user closes the application.
     * @return Process exit code.
     */
    int Run();

private:
    /** @brief Cached OpenGL framebuffer preview uploaded as an ImGui texture. */
    struct PictureInPictureState;

    /** @brief Requests a raw RGBA32 framebuffer capture on the next frame. */
    void RequestFramebufferCapture() noexcept;
    /** @brief Captures the current OpenGL framebuffer into CPU-owned RGBA32 storage. */
    void CaptureFramebufferRgba32();
    /** @brief Refreshes the picture-in-picture texture from the live framebuffer. */
    void UpdatePictureInPicture();
    /** @brief Arms one strobe capture request to be resolved during the next update cycle. */
    void RequestStrobeCapture() noexcept;
    /** @brief Reloads the authored window configuration and recreates the runtime scene. */
    bool ReloadConfiguration();
    /** @brief Handles local keyboard shortcuts used by the demo shell. */
    void HandleShortcuts();
    /** @brief Advances the local sample logic and applies queued remote commands. */
    void UpdateDemo(float deltaSeconds);
    /** @brief Publishes strobe feedback snapshots back to external clients. */
    void PublishStrobeFeedbacks(float deltaSeconds);
    /** @brief Draws the debug/operator UI layered on top of the rendered page. */
    void DrawUi();
    /** @brief Draws the framebuffer picture-in-picture region when preview data is available. */
    void DrawPictureInPictureRegion();
    /** @brief Releases the GPU texture used by the picture-in-picture preview. */
    void ReleasePictureInPictureTexture() noexcept;

    /** @brief Startup parameters used to bootstrap the demo window. */
    ApplicationConfig config_ {};
    /** @brief JSON loader used to resolve the root window file and referenced assets. */
    JsonLoader loader_ {};
    /** @brief Live runtime scene consumed by the renderer and the command processor. */
    SceneRegistry scene_ {};
    /** @brief Applies typed commands onto the live scene registry. */
    CommandProcessor commandProcessor_;
    /** @brief Small local feed animating sample demo tracks when no external client drives the window. */
    DemoTrackFeed demoTrackFeed_ {};
    /** @brief Renderer drawing the active page from the current scene state. */
    MfdRenderer renderer_ {};
    /** @brief Cached authored window definition currently loaded in the application. */
    WindowAssetDefinition windowDefinition_ {};

    /** @brief Background UDP bridge receiving commands and sending optional feedback. */
    std::unique_ptr<UdpRuntimeBridge> udpRuntimeBridge_ {};
    /** @brief Optional picture-in-picture framebuffer snapshot state. */
    std::unique_ptr<PictureInPictureState> pictureInPicture_ {};
    /** @brief Temporary queue of remote commands drained on the render thread. */
    std::vector<UserCommand> pendingCommands_ {};

    /** @brief Last manual framebuffer capture requested from the UI. */
    Rgba32Framebuffer lastFramebufferCapture_ {};
    /** @brief Last reticle captured through the active page strobe. */
    std::optional<StrobeCaptureResult> lastStrobeCapture_ {};
    /** @brief Monotonic elapsed time since the window started. */
    float elapsedSeconds_ = 0.0f;
    /** @brief Accumulator driving the lower-rate picture-in-picture refresh. */
    float pictureInPictureAccumulator_ = 0.0f;
    /** @brief Cooldown keeping local demo traffic paused briefly after remote commands arrive. */
    float remoteCommandHoldSeconds_ = 0.0f;
    /** @brief Accumulator driving the outgoing strobe feedback cadence. */
    float strobeFeedbackAccumulator_ = 0.0f;
    /** @brief Sequence number assigned to the next published strobe feedback snapshot. */
    std::uint32_t nextStrobeFeedbackSequence_ = 1;
    /** @brief Indicates that a framebuffer capture must happen on the next frame. */
    bool framebufferCaptureRequested_ = false;
    /** @brief Indicates that one strobe capture should be attempted on the next update. */
    bool strobeCaptureRequested_ = false;
    /** @brief Human-readable status of the last framebuffer capture attempt. */
    std::string lastFramebufferCaptureStatus_;
    /** @brief Human-readable status of the picture-in-picture refresh path. */
    std::string lastPictureInPictureStatus_;
    /** @brief Human-readable status of the last local strobe capture. */
    std::string lastStrobeCaptureStatus_;
    /** @brief Human-readable status of the feedback publication path. */
    std::string lastStrobeFeedbackStatus_;
    /** @brief Human-readable status of the last remote command batch processed by the demo. */
    std::string lastCommandStatus_;
    /** @brief Last configuration reload error surfaced to the shell UI. */
    std::string lastReloadError_;
    /** @brief Last runtime exception surfaced to the shell UI. */
    std::string lastRuntimeError_;
};
} // namespace mfd
