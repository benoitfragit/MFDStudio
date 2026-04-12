/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "mfd/window/WindowLauncher.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <raylib.h>
#include <rlgl.h>

#include "mfd/control/CommandProcessor.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/control/UdpRuntimeBridge.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/render/MfdRenderer.h"
#include "mfd/render/OpenGlFramebufferReader.h"
#include "mfd/runtime/SceneRegistry.h"

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#endif

// NOTE:
// CI Visual Studio builds can expose a raylib graphics backend where low-level
// OpenGL PBO/sync symbols (GLsync, glBindBuffer, glFenceSync, etc.) are not
// declared at compile time. To keep builds portable/reliable, keep async PBO
// capture disabled and use the synchronous framebuffer fallback path.
#define MFD_HAS_GL_PBO_API 0

namespace
{
constexpr float kStrobeFeedbackIntervalSeconds = 0.020f;
constexpr unsigned int kCaptureRingSize = 2;

class AsyncFramebufferCapture
{
public:
    ~AsyncFramebufferCapture()
    {
        Release();
    }

    [[nodiscard]] std::optional<mfd::Rgba32Framebuffer> Capture()
    {
        const int renderWidth = GetRenderWidth();
        const int renderHeight = GetRenderHeight();
        if (renderWidth <= 0 || renderHeight <= 0)
        {
            return std::nullopt;
        }

        if (!pboSupportChecked_)
        {
            pboSupportChecked_ = true;
            pboAvailable_ = (MFD_HAS_GL_PBO_API == 1) && (rlGetVersion() >= RL_OPENGL_33);
        }

        if (!EnsureResources(renderWidth, renderHeight))
        {
            pboAvailable_ = false;
            latestFramebuffer_ = mfd::OpenGlFramebufferReader::ReadRgba32();
            hasLatestFramebuffer_ = !latestFramebuffer_.Empty();
            return latestFramebuffer_;
        }

        rlDrawRenderBatchActive();

        const unsigned int writeIndex = frameIndex_ % kCaptureRingSize;
        SubmitReadback(writeIndex, renderWidth, renderHeight);

        std::optional<mfd::Rgba32Framebuffer> readyFrame;
        if (frameIndex_ + 1 >= kCaptureRingSize)
        {
            const unsigned int readIndex = (frameIndex_ + 1) % kCaptureRingSize;
            readyFrame = TryConsume(readIndex);
        }

        ++frameIndex_;
        if (readyFrame.has_value())
        {
            hasLatestFramebuffer_ = true;
            return readyFrame;
        }

        if (hasLatestFramebuffer_)
        {
            return latestFramebuffer_;
        }

        return readyFrame;
    }

private:
    struct CaptureSlot
    {
        unsigned int pboId = 0;
        std::size_t capacityBytes = 0;
        int width = 0;
        int height = 0;
    #if MFD_HAS_GL_PBO_API == 1
        GLsync fence = nullptr;
    #endif
    };

    static void FlipRows(std::vector<mfd::Rgba8Pixel>& pixels, const int width, const int height)
    {
        if (width <= 0 || height <= 1)
        {
            return;
        }

        const std::size_t stride = static_cast<std::size_t>(width) * sizeof(mfd::Rgba8Pixel);
        std::vector<mfd::Rgba8Pixel> scratchRow(static_cast<std::size_t>(width));

        for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom)
        {
            mfd::Rgba8Pixel* topRow = pixels.data() + static_cast<std::size_t>(top) * static_cast<std::size_t>(width);
            mfd::Rgba8Pixel* bottomRow = pixels.data() + static_cast<std::size_t>(bottom) * static_cast<std::size_t>(width);
            std::memcpy(scratchRow.data(), topRow, stride);
            std::memcpy(topRow, bottomRow, stride);
            std::memcpy(bottomRow, scratchRow.data(), stride);
        }
    }

    bool EnsureResources(const int width, const int height)
    {
#if MFD_HAS_GL_PBO_API == 0
        (void) width;
        (void) height;
        return false;
#else
        if (!pboAvailable_)
        {
            return false;
        }

        const std::size_t requiredBytes =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * sizeof(mfd::Rgba8Pixel);
        if (requiredBytes == 0)
        {
            return false;
        }

        if (!initialized_)
        {
            for (CaptureSlot& slot : slots_)
            {
                glGenBuffers(1, &slot.pboId);
                if (slot.pboId == 0)
                {
                    Release();
                    return false;
                }
            }
            initialized_ = true;
            frameIndex_ = 0;
        }

        for (CaptureSlot& slot : slots_)
        {
            if (slot.capacityBytes == requiredBytes && slot.width == width && slot.height == height)
            {
                continue;
            }

            glBindBuffer(GL_PIXEL_PACK_BUFFER, slot.pboId);
            glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(requiredBytes), nullptr, GL_STREAM_READ);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            slot.capacityBytes = requiredBytes;
            slot.width = width;
            slot.height = height;
        }

        return true;
#endif
    }

    void SubmitReadback(const unsigned int slotIndex, const int width, const int height)
    {
#if MFD_HAS_GL_PBO_API == 0
        (void) slotIndex;
        (void) width;
        (void) height;
        return;
#else
        CaptureSlot& slot = slots_[slotIndex];
    #if MFD_HAS_GL_PBO_API == 1
        if (slot.fence != nullptr)
        {
            glDeleteSync(slot.fence);
            slot.fence = nullptr;
        }
    #endif

        glBindBuffer(GL_PIXEL_PACK_BUFFER, slot.pboId);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    #if MFD_HAS_GL_PBO_API == 1
        slot.fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    #endif
#endif
    }

    [[nodiscard]] std::optional<mfd::Rgba32Framebuffer> TryConsume(const unsigned int slotIndex)
    {
#if MFD_HAS_GL_PBO_API == 0
        (void) slotIndex;
        return std::nullopt;
#else
        CaptureSlot& slot = slots_[slotIndex];
        if (slot.pboId == 0 || slot.capacityBytes == 0)
        {
            return std::nullopt;
        }

    #if MFD_HAS_GL_PBO_API == 1
        if (slot.fence == nullptr)
        {
            return std::nullopt;
        }

        const GLenum waitResult = glClientWaitSync(slot.fence, 0, 0);
        if (waitResult != GL_ALREADY_SIGNALED && waitResult != GL_CONDITION_SATISFIED)
        {
            return std::nullopt;
        }
    #endif

        glBindBuffer(GL_PIXEL_PACK_BUFFER, slot.pboId);
        void* mappedBuffer = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (mappedBuffer == nullptr)
        {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            return std::nullopt;
        }

        latestFramebuffer_.width = slot.width;
        latestFramebuffer_.height = slot.height;
        latestFramebuffer_.pixels.resize(static_cast<std::size_t>(slot.width) * static_cast<std::size_t>(slot.height));
        std::memcpy(latestFramebuffer_.pixels.data(), mappedBuffer, slot.capacityBytes);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    #if MFD_HAS_GL_PBO_API == 1
        glDeleteSync(slot.fence);
        slot.fence = nullptr;
    #endif

        FlipRows(latestFramebuffer_.pixels, latestFramebuffer_.width, latestFramebuffer_.height);
        return latestFramebuffer_;
#endif
    }

    void Release()
    {
#if MFD_HAS_GL_PBO_API == 0
        initialized_ = false;
        frameIndex_ = 0;
        return;
#else
        for (CaptureSlot& slot : slots_)
        {
    #if MFD_HAS_GL_PBO_API == 1
            if (slot.fence != nullptr)
            {
                glDeleteSync(slot.fence);
                slot.fence = nullptr;
            }
    #endif
            if (slot.pboId != 0)
            {
                glDeleteBuffers(1, &slot.pboId);
                slot.pboId = 0;
            }

            slot.capacityBytes = 0;
            slot.width = 0;
            slot.height = 0;
        }

        initialized_ = false;
        frameIndex_ = 0;
#endif
    }

    std::array<CaptureSlot, kCaptureRingSize> slots_ {};
    mfd::Rgba32Framebuffer latestFramebuffer_ {};
    std::size_t frameIndex_ = 0;
    bool initialized_ = false;
    bool pboSupportChecked_ = false;
    bool pboAvailable_ = true;
    bool hasLatestFramebuffer_ = false;
};

struct CommandLineOptions
{
    std::filesystem::path windowFile;
    bool showHelp = false;
};

Color ToRayColor(const mfd::ColorRgba& color)
{
    return Color {color.r, color.g, color.b, color.a};
}

mfd::StrobeFeedbackCapture ToFeedbackCapture(const mfd::StrobeCaptureResult& capture)
{
    return mfd::StrobeFeedbackCapture {
        capture.reticleId,
        capture.sourceTemplateId,
        capture.label,
        capture.category,
        capture.position,
        capture.distance,
        capture.metadata};
}

mfd::StrobeFeedbackMagnet ToFeedbackMagnet(const std::optional<mfd::StrobeMagnetSummary>& magnetSummary)
{
    if (!magnetSummary.has_value())
    {
        return {};
    }

    return mfd::StrobeFeedbackMagnet {
        magnetSummary->enabled,
        magnetSummary->radius,
        magnetSummary->strength,
        magnetSummary->magnetized,
        magnetSummary->reticleId,
        magnetSummary->targetPosition,
        magnetSummary->distance};
}

bool ParseCommandLine(const int argc,
                      char** argv,
                      const mfd::window::LauncherConfig& config,
                      CommandLineOptions& options,
                      std::string& error)
{
    options.windowFile =
        config.defaultWindowFile.empty() ? std::filesystem::path {"assets/windows/demo_pages.json"} : config.defaultWindowFile;
    bool positionalWindowConsumed = false;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument = argv[index] != nullptr ? std::string_view {argv[index]} : std::string_view {};

        if (argument == "--help" || argument == "-h")
        {
            options.showHelp = true;
            return true;
        }

        if (argument == "--window" || argument == "-w")
        {
            if (index + 1 >= argc || argv[index + 1] == nullptr)
            {
                error = "Missing path after '--window'.";
                return false;
            }

            options.windowFile = argv[++index];
            positionalWindowConsumed = true;
            continue;
        }

        if (!argument.empty() && argument.front() == '-')
        {
            error = "Unknown option: " + std::string(argument);
            return false;
        }

        if (positionalWindowConsumed)
        {
            error = "Only one window JSON path can be provided.";
            return false;
        }

        options.windowFile = std::filesystem::path {std::string(argument)};
        positionalWindowConsumed = true;
    }

    return true;
}

void ReportFatalError(const std::string_view applicationName, const std::string& message)
{
    std::cerr << applicationName << " fatal error: " << message << '\n';
}

class GenericWindowApplication
{
public:
    GenericWindowApplication(std::string applicationName,
                             std::filesystem::path windowFile,
                             mfd::window::LauncherFramebufferCallback framebufferCallback) :
        applicationName_(std::move(applicationName)),
        windowFile_(std::move(windowFile)),
        framebufferCallback_(std::move(framebufferCallback)),
        commandProcessor_(scene_)
    {
    }

    int Run()
    {
        if (!ReloadConfiguration())
        {
            throw std::runtime_error("Unable to load window JSON '" + windowFile_.string() + "': " + lastReloadError_);
        }

        SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(windowDefinition_.width, windowDefinition_.height, windowDefinition_.title.c_str());
        SetWindowPosition(windowDefinition_.positionX, windowDefinition_.positionY);

        if (windowDefinition_.targetFps > 0)
        {
            SetTargetFPS(windowDefinition_.targetFps);
        }

        PrintStartupSummary();

        while (!WindowShouldClose())
        {
            const float deltaSeconds = GetFrameTime();

            try
            {
                HandleShortcuts();
                Update(deltaSeconds);
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

            try
            {
                ClearBackground(ToRayColor(scene_.ActiveBackgroundColor()));
                renderer_.DrawActivePage(scene_);
                PublishFramebuffer();
            }
            catch (const std::exception& exception)
            {
                lastRuntimeError_ = exception.what();
            }
            catch (...)
            {
                lastRuntimeError_ = "Unknown exception during rendering";
            }

            EndDrawing();

            if (windowDefinition_.targetFps <= 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds {1});
            }
        }

        CloseWindow();
        return 0;
    }

private:
    bool ReloadConfiguration()
    {
        try
        {
            const std::string previousPage = scene_.ActivePageName();
            mfd::LoadedWindowConfiguration loaded = loader_.LoadWindowConfiguration(windowFile_);
            if (loaded.document.pages.empty())
            {
                throw std::runtime_error("The window JSON does not contain any page.");
            }

            windowDefinition_ = loaded.window;
            scene_.LoadDocument(std::move(loaded.document));
            renderer_.SetTextFontFile(windowDefinition_.fontFile);
            udpRuntimeBridge_ = std::make_unique<mfd::UdpRuntimeBridge>(
                windowDefinition_.commandTransports,
                windowDefinition_.feedbackTransports);
            (void)udpRuntimeBridge_->Start();

            if (scene_.HasPage(previousPage))
            {
                scene_.SetActivePage(previousPage);
            }

            strobeFeedbackAccumulator_ = 0.0f;
            nextStrobeFeedbackSequence_ = 1;
            lastRuntimeError_.clear();

            if (udpRuntimeBridge_ == nullptr || !udpRuntimeBridge_->HasCommandReceiver())
            {
                lastCommandStatus_ = "No UDP command transport configured in the window JSON.";
            }
            else
            {
                lastCommandStatus_ = udpRuntimeBridge_->LastCommandStatus();
            }

            if (udpRuntimeBridge_ == nullptr || !udpRuntimeBridge_->HasFeedbackSender())
            {
                lastFeedbackStatus_ = "No UDP feedback transport configured in the window JSON.";
            }
            else
            {
                lastFeedbackStatus_ = udpRuntimeBridge_->LastFeedbackStatus();
            }

            if (IsWindowReady())
            {
                SetWindowTitle(windowDefinition_.title.c_str());
                SetWindowSize(windowDefinition_.width, windowDefinition_.height);
                SetWindowPosition(windowDefinition_.positionX, windowDefinition_.positionY);

                if (windowDefinition_.targetFps > 0)
                {
                    SetTargetFPS(windowDefinition_.targetFps);
                }
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

    void HandleShortcuts()
    {
        if (IsKeyPressed(KEY_R))
        {
            if (!ReloadConfiguration())
            {
                throw std::runtime_error(lastReloadError_);
            }
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

    void Update(const float deltaSeconds)
    {
        if (udpRuntimeBridge_ != nullptr)
        {
            pendingCommands_.clear();
            const std::size_t drainedCommands = udpRuntimeBridge_->DrainReceivedCommands(pendingCommands_);
            if (drainedCommands > 0)
            {
                if (commandProcessor_.Submit(
                        std::span<const mfd::UserCommand>(pendingCommands_.data(), pendingCommands_.size())))
                {
                    lastCommandStatus_ =
                        "Applied " + std::to_string(drainedCommands) + " command(s) from the UDP I/O thread.";
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

        PublishStrobeFeedbacks(deltaSeconds);
    }

    void PublishStrobeFeedbacks(const float deltaSeconds)
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

        for (const mfd::PageSummary& page : scene_.Pages())
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

            mfd::StrobeStatusFeedback feedback;
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
        }

        if (!udpRuntimeBridge_->LastFeedbackStatus().empty())
        {
            lastFeedbackStatus_ = udpRuntimeBridge_->LastFeedbackStatus();
        }
    }

    void PrintStartupSummary() const
    {
        std::cout << applicationName_ << '\n';
        std::cout << "Window JSON: " << windowFile_.string() << '\n';
        std::cout << "Window title: " << windowDefinition_.title << '\n';
        std::cout << "Pages: " << scene_.Pages().size() << '\n';
        std::cout << "Shortcuts: R reloads, 1..9 switch pages\n";
    }

    void PublishFramebuffer()
    {
        if (!framebufferCallback_)
        {
            return;
        }

        const int renderWidth = GetRenderWidth();
        const int renderHeight = GetRenderHeight();
        if (renderWidth <= 0 || renderHeight <= 0)
        {
            return;
        }

        const std::optional<mfd::Rgba32Framebuffer> framebuffer = framebufferCapture_.Capture();
        if (!framebuffer.has_value())
        {
            return;
        }

        framebufferCallback_(framebuffer->width, framebuffer->height, framebuffer->Bytes());
    }

    std::string applicationName_;
    std::filesystem::path windowFile_;
    mfd::window::LauncherFramebufferCallback framebufferCallback_ {};
    mfd::JsonLoader loader_ {};
    mfd::SceneRegistry scene_ {};
    mfd::CommandProcessor commandProcessor_;
    mfd::MfdRenderer renderer_ {};
    AsyncFramebufferCapture framebufferCapture_ {};
    mfd::WindowAssetDefinition windowDefinition_ {};
    std::unique_ptr<mfd::UdpRuntimeBridge> udpRuntimeBridge_ {};
    std::vector<mfd::UserCommand> pendingCommands_ {};
    float strobeFeedbackAccumulator_ = 0.0f;
    std::uint32_t nextStrobeFeedbackSequence_ = 1;
    std::string lastCommandStatus_ {};
    std::string lastFeedbackStatus_ {};
    std::string lastReloadError_ {};
    std::string lastRuntimeError_ {};
};
} // namespace

namespace mfd::window
{
std::string BuildUsageText(const LauncherConfig& config)
{
    const std::string applicationName = config.applicationName.empty() ? std::string {"mfd_window"} : config.applicationName;
    const std::filesystem::path defaultWindowFile =
        config.defaultWindowFile.empty() ? std::filesystem::path {"assets/windows/demo_pages.json"} : config.defaultWindowFile;

    std::ostringstream output;
    output << "Usage:\n";
    output << "  " << applicationName << " <window.json>\n";
    output << "  " << applicationName << " --window <window.json>\n";
    output << "  " << applicationName << " --help\n";
    output << '\n';
    output << "If no window JSON is provided, the launcher defaults to '" << defaultWindowFile.string() << "'.\n";
    output << "Shortcuts:\n";
    output << "  R reloads the current window JSON from disk\n";
    output << "  1..9 activate the first nine authored pages\n";
    return output.str();
}

bool ParseLauncherCommandLine(const int argc,
                              char** argv,
                              const LauncherConfig& config,
                              LauncherOptions& options,
                              std::string& error)
{
    CommandLineOptions internalOptions;
    const bool parsed = ParseCommandLine(argc, argv, config, internalOptions, error);
    options.windowFile = std::move(internalOptions.windowFile);
    options.showHelp = internalOptions.showHelp;
    return parsed;
}

int RunLauncher(int argc, char** argv, const LauncherConfig& config, LauncherFramebufferCallback framebufferCallback)
{
    const std::string applicationName = config.applicationName.empty() ? std::string {"mfd_window"} : config.applicationName;
    const std::filesystem::path defaultWindowFile =
        config.defaultWindowFile.empty() ? std::filesystem::path {"assets/windows/demo_pages.json"} : config.defaultWindowFile;

    LauncherOptions options;
    std::string error;
    if (!ParseLauncherCommandLine(argc, argv, config, options, error))
    {
        std::cerr << error << '\n';
        LauncherConfig usageConfig;
        usageConfig.applicationName = applicationName;
        usageConfig.defaultWindowFile = defaultWindowFile;
        std::cerr << BuildUsageText(usageConfig);
        return 1;
    }

    if (options.showHelp)
    {
        LauncherConfig usageConfig;
        usageConfig.applicationName = applicationName;
        usageConfig.defaultWindowFile = defaultWindowFile;
        std::cout << BuildUsageText(usageConfig);
        return 0;
    }

#if defined(_WIN32)
    if (::IsDebuggerPresent() != 0)
    {
        GenericWindowApplication application(applicationName, options.windowFile, std::move(framebufferCallback));
        return application.Run();
    }
#endif

    try
    {
        GenericWindowApplication application(applicationName, options.windowFile, std::move(framebufferCallback));
        return application.Run();
    }
    catch (const std::exception& exception)
    {
        ReportFatalError(applicationName, exception.what());
        return 1;
    }
}
} // namespace mfd::window
