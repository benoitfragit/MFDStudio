/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "MfdApplication.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <stdexcept>
#include <string>

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#include "mfd/control/StrobeFeedback.h"

namespace mfd
{
namespace
{
Color ToRayColor(const ColorRgba& color)
{
    return Color {color.r, color.g, color.b, color.a};
}

ImVec2 FitInside(const ImVec2 available, const int width, const int height)
{
    if (width <= 0 || height <= 0 || available.x <= 0.0f || available.y <= 0.0f)
    {
        return ImVec2(0.0f, 0.0f);
    }

    const float scale = std::min(available.x / static_cast<float>(width),
                                 available.y / static_cast<float>(height));
    return ImVec2(static_cast<float>(width) * scale,
                  static_cast<float>(height) * scale);
}

constexpr float kPictureInPictureRefreshIntervalSeconds = 0.20f;
constexpr float kRemoteCommandFeedSuspendSeconds = 0.25f;
constexpr float kStrobeFeedbackIntervalSeconds = 0.020f;

StrobeFeedbackCapture ToFeedbackCapture(const StrobeCaptureResult& capture)
{
    return StrobeFeedbackCapture {
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
        magnetSummary->reticleId,
        magnetSummary->targetPosition,
        magnetSummary->distance};
}
} // namespace

struct MfdApplication::PictureInPictureState
{
    Rgba32Framebuffer framebuffer;
    Texture2D texture {};
    bool textureReady = false;
};

MfdApplication::~MfdApplication() = default;

MfdApplication::MfdApplication(ApplicationConfig config)
    : config_(std::move(config))
    , commandProcessor_(scene_)
    , pictureInPicture_(std::make_unique<PictureInPictureState>())
{
}

int MfdApplication::Run()
{
    if (!ReloadConfiguration())
    {
        throw std::runtime_error("Unable to load MFD window JSON: " + lastReloadError_);
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(windowDefinition_.width, windowDefinition_.height, windowDefinition_.title.c_str());
    SetWindowPosition(windowDefinition_.positionX, windowDefinition_.positionY);
    SetTargetFPS(windowDefinition_.targetFps);

    rlImGuiSetup(true);

    while (!WindowShouldClose())
    {
        const float deltaSeconds = GetFrameTime();
        elapsedSeconds_ += deltaSeconds;

        try
        {
            HandleShortcuts();
            UpdateDemo(deltaSeconds);
        }
        catch (const std::exception& exception)
        {
            lastRuntimeError_ = exception.what();
        }
        catch (...)
        {
            lastRuntimeError_ = "Unknown exception during update";
        }

        BeginDrawing();
        bool imguiFrameBegun = false;

        try
        {
            ClearBackground(ToRayColor(scene_.ActiveBackgroundColor()));
            renderer_.DrawActivePage(scene_);
            UpdatePictureInPicture();
            if (framebufferCaptureRequested_)
            {
                CaptureFramebufferRgba32();
            }

            rlImGuiBegin();
            imguiFrameBegun = true;
            DrawUi();
            DrawPictureInPictureRegion();
        }
        catch (const std::exception& exception)
        {
            lastRuntimeError_ = exception.what();
        }
        catch (...)
        {
            lastRuntimeError_ = "Unknown exception during rendering";
        }

        if (imguiFrameBegun)
        {
            rlImGuiEnd();
        }

        EndDrawing();
    }

    ReleasePictureInPictureTexture();
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}

void MfdApplication::RequestFramebufferCapture() noexcept
{
    framebufferCaptureRequested_ = true;
}

void MfdApplication::CaptureFramebufferRgba32()
{
    framebufferCaptureRequested_ = false;

    try
    {
        lastFramebufferCapture_ = OpenGlFramebufferReader::ReadRgba32();
        lastFramebufferCaptureStatus_ =
            std::to_string(lastFramebufferCapture_.width) + "x" +
            std::to_string(lastFramebufferCapture_.height) + ", " +
            std::to_string(lastFramebufferCapture_.PixelCount()) + " pixels, " +
            std::to_string(lastFramebufferCapture_.ByteSize()) + " bytes RGBA32";
    }
    catch (const std::exception& exception)
    {
        lastFramebufferCapture_ = {};
        lastFramebufferCaptureStatus_ = exception.what();
    }
}

void MfdApplication::UpdatePictureInPicture()
{
    if (pictureInPicture_ == nullptr || pictureInPictureAccumulator_ < kPictureInPictureRefreshIntervalSeconds)
    {
        return;
    }

    pictureInPictureAccumulator_ = 0.0f;

    try
    {
        pictureInPicture_->framebuffer = OpenGlFramebufferReader::ReadRgba32();
        if (pictureInPicture_->framebuffer.Empty())
        {
            lastPictureInPictureStatus_ = "empty framebuffer";
            return;
        }

        const bool sizeChanged =
            !pictureInPicture_->textureReady ||
            pictureInPicture_->texture.width != pictureInPicture_->framebuffer.width ||
            pictureInPicture_->texture.height != pictureInPicture_->framebuffer.height;

        if (sizeChanged)
        {
            ReleasePictureInPictureTexture();
            std::span<std::byte> pixelBytes = pictureInPicture_->framebuffer.Bytes();

            Image image {};
            image.data = pixelBytes.data();
            image.width = pictureInPicture_->framebuffer.width;
            image.height = pictureInPicture_->framebuffer.height;
            image.mipmaps = 1;
            image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

            pictureInPicture_->texture = LoadTextureFromImage(image);
            pictureInPicture_->textureReady = pictureInPicture_->texture.id != 0;
        }
        else
        {
            std::span<std::byte> pixelBytes = pictureInPicture_->framebuffer.Bytes();
            UpdateTexture(pictureInPicture_->texture, pixelBytes.data());
        }

        if (!pictureInPicture_->textureReady)
        {
            lastPictureInPictureStatus_ = "unable to upload PiP texture";
            return;
        }

        lastPictureInPictureStatus_ =
            std::to_string(pictureInPicture_->framebuffer.width) + "x" +
            std::to_string(pictureInPicture_->framebuffer.height) +
            " refreshed from OpenGL RGBA32";
    }
    catch (const std::exception& exception)
    {
        lastPictureInPictureStatus_ = exception.what();
    }
}

void MfdApplication::RequestStrobeCapture() noexcept
{
    strobeCaptureRequested_ = true;
}

bool MfdApplication::ReloadConfiguration()
{
    try
    {
        const std::string previousPage = scene_.ActivePageName();
        LoadedWindowConfiguration loaded = loader_.LoadWindowConfiguration(config_.windowFile);
        windowDefinition_ = loaded.window;
        renderer_.SetTextFontFile(windowDefinition_.fontFile);
        scene_.LoadDocument(std::move(loaded.document));
        udpRuntimeBridge_ = std::make_unique<UdpRuntimeBridge>(windowDefinition_.commandTransports,
                                                               windowDefinition_.feedbackTransports);
        (void)udpRuntimeBridge_->Start();

        if (!udpRuntimeBridge_->HasCommandReceiver())
        {
            lastCommandStatus_ = "No UDP command transport configured in the window JSON";
        }
        else
        {
            lastCommandStatus_ = udpRuntimeBridge_->LastCommandStatus();
        }

        strobeFeedbackAccumulator_ = 0.0f;
        nextStrobeFeedbackSequence_ = 1;
        if (!udpRuntimeBridge_->HasFeedbackSender())
        {
            lastStrobeFeedbackStatus_ = "No UDP feedback transport configured in the window JSON";
        }
        else
        {
            lastStrobeFeedbackStatus_ = udpRuntimeBridge_->LastFeedbackStatus();
        }

        if (IsWindowReady())
        {
            SetWindowTitle(windowDefinition_.title.c_str());
            SetWindowSize(windowDefinition_.width, windowDefinition_.height);
            SetWindowPosition(windowDefinition_.positionX, windowDefinition_.positionY);
            SetTargetFPS(windowDefinition_.targetFps);
        }

        if (scene_.HasPage(previousPage))
        {
            scene_.SetActivePage(previousPage);
        }

        lastReloadError_.clear();
        return true;
    }
    catch (const std::exception& exception)
    {
        lastReloadError_ = exception.what();
        return false;
    }
}

void MfdApplication::HandleShortcuts()
{
    if (IsKeyPressed(KEY_R))
    {
        ReloadConfiguration();
    }

    if (IsKeyPressed(KEY_F8))
    {
        RequestFramebufferCapture();
    }

    if (IsKeyPressed(KEY_C))
    {
        RequestStrobeCapture();
    }

    static constexpr std::array<int, 9> keyBindings {
        KEY_ONE,
        KEY_TWO,
        KEY_THREE,
        KEY_FOUR,
        KEY_FIVE,
        KEY_SIX,
        KEY_SEVEN,
        KEY_EIGHT,
        KEY_NINE};

    const auto pages = scene_.Pages();

    for (std::size_t index = 0; index < pages.size() && index < keyBindings.size(); ++index)
    {
        if (IsKeyPressed(keyBindings[index]))
        {
            scene_.SetActivePage(pages[index].name);
        }
    }
}

void MfdApplication::UpdateDemo(const float deltaSeconds)
{
    pictureInPictureAccumulator_ += deltaSeconds;
    remoteCommandHoldSeconds_ = std::max(0.0f, remoteCommandHoldSeconds_ - deltaSeconds);

    if (udpRuntimeBridge_ != nullptr)
    {
        pendingCommands_.clear();
        const std::size_t drainedCommands = udpRuntimeBridge_->DrainReceivedCommands(pendingCommands_);
        if (drainedCommands > 0)
        {
            if (commandProcessor_.Submit(std::span<const UserCommand>(pendingCommands_.data(), pendingCommands_.size())))
            {
                remoteCommandHoldSeconds_ = kRemoteCommandFeedSuspendSeconds;
                lastCommandStatus_ =
                    "Applied " + std::to_string(drainedCommands) + " command(s) from the UDP I/O thread";
            }
            else if (!commandProcessor_.LastError().empty())
            {
                lastCommandStatus_ = commandProcessor_.LastError();
            }
        }
        else if (!udpRuntimeBridge_->LastCommandStatus().empty())
        {
            lastCommandStatus_ = udpRuntimeBridge_->LastCommandStatus();
        }
    }

    if (remoteCommandHoldSeconds_ <= 0.0f)
    {
        demoTrackFeed_.Update(elapsedSeconds_, scene_);
    }

    if (scene_.ActivePageHasStrobe())
    {
        Vec2 strobeDelta;
        const float moveStep = (IsKeyDown(KEY_LEFT_SHIFT) ? 0.54f : 0.30f) * deltaSeconds;

        if (IsKeyDown(KEY_LEFT))
        {
            strobeDelta.x -= moveStep;
        }

        if (IsKeyDown(KEY_RIGHT))
        {
            strobeDelta.x += moveStep;
        }

        if (IsKeyDown(KEY_UP))
        {
            strobeDelta.y += moveStep;
        }

        if (IsKeyDown(KEY_DOWN))
        {
            strobeDelta.y -= moveStep;
        }

        if (strobeDelta.x != 0.0f || strobeDelta.y != 0.0f)
        {
            scene_.OffsetStrobe(scene_.ActivePageName(), strobeDelta);
        }

        if (strobeCaptureRequested_)
        {
            strobeCaptureRequested_ = false;
            lastStrobeCapture_ = scene_.CaptureActivePageStrobe();

            if (lastStrobeCapture_.has_value())
            {
                lastStrobeCaptureStatus_ =
                    "captured " + lastStrobeCapture_->reticleId +
                    " at distance " + std::to_string(lastStrobeCapture_->distance);
            }
            else
            {
                lastStrobeCaptureStatus_ = "no dynamic reticle captured";
            }
        }
    }
    else
    {
        strobeCaptureRequested_ = false;
    }

    PublishStrobeFeedbacks(deltaSeconds);
}

void MfdApplication::PublishStrobeFeedbacks(const float deltaSeconds)
{
    if (udpRuntimeBridge_ == nullptr || !udpRuntimeBridge_->FeedbackTransportReady())
    {
        return;
    }

    strobeFeedbackAccumulator_ += std::max(deltaSeconds, 0.0f);
    if (strobeFeedbackAccumulator_ < kStrobeFeedbackIntervalSeconds)
    {
        return;
    }

    strobeFeedbackAccumulator_ = std::fmod(strobeFeedbackAccumulator_, kStrobeFeedbackIntervalSeconds);

    for (const PageSummary& page : scene_.Pages())
    {
        if (!page.hasStrobe)
        {
            continue;
        }

        const auto strobe = scene_.StrobeForPage(page.name);
        if (!strobe.has_value())
        {
            continue;
        }

        StrobeStatusFeedback feedback;
        feedback.sequence = nextStrobeFeedbackSequence_++;
        feedback.pageName = strobe->pageName;
        feedback.strobeId = strobe->reticleId;
        feedback.active = strobe->visible;
        feedback.position = strobe->position;
        feedback.capture = strobe->capture;
        feedback.magnet = ToFeedbackMagnet(scene_.StrobeMagnetForPage(page.name));

        if (const auto capture = scene_.CaptureWithStrobe(page.name); capture.has_value())
        {
            feedback.captureResult = ToFeedbackCapture(*capture);
        }

        udpRuntimeBridge_->EnqueueStrobeFeedback(std::move(feedback));
        lastStrobeFeedbackStatus_ =
            "Queued strobe feedback for page '" + page.name + "' to the UDP I/O thread";
    }

    if (!udpRuntimeBridge_->LastFeedbackStatus().empty())
    {
        lastStrobeFeedbackStatus_ = udpRuntimeBridge_->LastFeedbackStatus();
    }
}

void MfdApplication::DrawUi()
{
    ImGui::SetNextWindowPos(ImVec2(18.0f, 56.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(390.0f, 0.0f), ImGuiCond_Once);

    ImGui::Begin("MFD Control");

    ImGui::Text("Window: %s", windowDefinition_.title.c_str());
    ImGui::Text("Window JSON: %s", config_.windowFile.string().c_str());
    ImGui::Text("Size: %d x %d px", windowDefinition_.width, windowDefinition_.height);
    ImGui::Text("Screen Pos: %d / %d px", windowDefinition_.positionX, windowDefinition_.positionY);
    ImGui::Text("R reloads JSON, 1..9 switches pages, F8 captures RGBA32");
    ImGui::TextWrapped("User coordinates are normalized in [-1, 1] with (0, 0) at screen center.");
    ImGui::TextWrapped("A live Picture in Picture is refreshed from the framebuffer readback API.");

    if (ImGui::Button("Reload Window JSON"))
    {
        ReloadConfiguration();
    }

    ImGui::SameLine();

    if (ImGui::Button("Capture RGBA32"))
    {
        RequestFramebufferCapture();
    }

    ImGui::Separator();
    ImGui::Text("Page JSON Files");

    for (const auto& pageFile : windowDefinition_.pageFiles)
    {
        ImGui::TextWrapped("%s", pageFile.string().c_str());
    }

    ImGui::Separator();
    ImGui::Text("Loaded Pages");

    for (const auto& page : scene_.Pages())
    {
        const std::string label =
            page.title == page.name
                ? page.title
                : page.title + " [" + page.name + "]";
        if (ImGui::Selectable(label.c_str(), page.active))
        {
            scene_.SetActivePage(page.name);
        }
    }

    ImGui::Separator();
    ImGui::TextWrapped("Only the active page is rendered.");
    ImGui::TextWrapped("The radar page also receives dynamic track reticles instantiated from the reticle library through EnTT.");

    if (const auto strobe = scene_.ActiveStrobeSummary(); strobe.has_value())
    {
        ImGui::Separator();
        ImGui::Text("Strobe");
        ImGui::TextWrapped("Arrow keys move the strobe. C captures the nearest dynamic reticle in range.");
        ImGui::Text("Position: %.3f / %.3f", strobe->position.x, strobe->position.y);

        if (ImGui::Button("Capture Strobe"))
        {
            RequestStrobeCapture();
        }

        ImGui::TextWrapped("Last result: %s",
                           lastStrobeCaptureStatus_.empty()
                               ? "<none>"
                               : lastStrobeCaptureStatus_.c_str());

        if (lastStrobeCapture_.has_value())
        {
            ImGui::Text("Captured: %s", lastStrobeCapture_->reticleId.c_str());

            if (!lastStrobeCapture_->label.empty())
            {
                ImGui::Text("Label: %s", lastStrobeCapture_->label.c_str());
            }

            if (!lastStrobeCapture_->category.empty())
            {
                ImGui::Text("Category: %s", lastStrobeCapture_->category.c_str());
            }

            ImGui::Text("Position: %.3f / %.3f",
                        lastStrobeCapture_->position.x,
                        lastStrobeCapture_->position.y);
            ImGui::Text("Distance: %.3f", lastStrobeCapture_->distance);

            for (const auto& [key, value] : lastStrobeCapture_->metadata)
            {
                const std::string line = key + ": " + value;
                ImGui::TextWrapped("%s", line.c_str());
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Remote Commands");
    ImGui::TextWrapped("The UDP listener is configured from the window JSON and handled by a background I/O thread.");
    ImGui::TextWrapped("Status: %s",
                       lastCommandStatus_.empty()
                           ? "<waiting for commands>"
                           : lastCommandStatus_.c_str());

    if (udpRuntimeBridge_ != nullptr)
    {
        ImGui::Text("UDP worker thread: %s", udpRuntimeBridge_->IsRunning() ? "running" : "stopped");
    }

    ImGui::TextWrapped("Strobe feedback: %s",
                       lastStrobeFeedbackStatus_.empty()
                           ? "<disabled>"
                           : lastStrobeFeedbackStatus_.c_str());

    ImGui::Separator();
    ImGui::Text("OpenGL Readback");
    ImGui::TextWrapped("Framebuffer API available through OpenGlFramebufferReader::ReadRgba32() and capture.Bytes().");
    ImGui::TextWrapped("Last capture: %s",
                       lastFramebufferCaptureStatus_.empty()
                           ? "<none>"
                           : lastFramebufferCaptureStatus_.c_str());

    if (!lastReloadError_.empty())
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.55f, 1.0f), "Reload error: %s", lastReloadError_.c_str());
    }

    if (!lastRuntimeError_.empty())
    {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.55f, 1.0f), "Runtime error: %s", lastRuntimeError_.c_str());
    }

    ImGui::End();
}

void MfdApplication::DrawPictureInPictureRegion()
{
    if (pictureInPicture_ == nullptr)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(GetScreenWidth()) - 410.0f, 56.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 265.0f), ImGuiCond_Once);

    ImGui::Begin("Picture in Picture");
    ImGui::TextWrapped("Secondary view fed by OpenGlFramebufferReader::ReadRgba32() plus the raw std::byte buffer.");
    ImGui::TextWrapped("Status: %s",
                       lastPictureInPictureStatus_.empty()
                           ? "<waiting for first capture>"
                           : lastPictureInPictureStatus_.c_str());

    if (pictureInPicture_->textureReady)
    {
        const ImVec2 size =
            FitInside(ImGui::GetContentRegionAvail(),
                      pictureInPicture_->framebuffer.width,
                      pictureInPicture_->framebuffer.height);

        if (size.x > 0.0f && size.y > 0.0f)
        {
            ImGui::Image(static_cast<ImTextureID>(pictureInPicture_->texture.id),
                         size,
                         ImVec2(0.0f, 1.0f),
                         ImVec2(1.0f, 0.0f));
        }
    }
    else
    {
        ImGui::TextWrapped("No PiP frame available yet.");
    }

    ImGui::End();
}

void MfdApplication::ReleasePictureInPictureTexture() noexcept
{
    if (pictureInPicture_ == nullptr || !pictureInPicture_->textureReady)
    {
        return;
    }

    UnloadTexture(pictureInPicture_->texture);
    pictureInPicture_->texture = {};
    pictureInPicture_->textureReady = false;
}
} // namespace mfd
