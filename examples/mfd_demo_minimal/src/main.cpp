/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "DemoTrackFeed.h"

#include <exception>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <raylib.h>

#include "mfd/control/CommandProcessor.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/control/UdpRuntimeBridge.h"
#include "mfd/io/JsonLoader.h"
#include "mfd/render/MfdRenderer.h"
#include "mfd/runtime/SceneRegistry.h"

#if defined(_WIN32)
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent();
#endif

namespace
{
Color ToRayColor(const mfd::ColorRgba& color)
{
    return Color {color.r, color.g, color.b, color.a};
}

constexpr float kRemoteCommandFeedSuspendSeconds = 0.25f;
constexpr float kStrobeFeedbackIntervalSeconds = 0.020f;

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

void ReportFatalError(const std::string& message)
{
    std::cerr << "Fatal error: " << message << '\n';
}

int RunMinimalDemo()
{
    mfd::JsonLoader loader;
    mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration("assets/windows/demo_pages_minimal.json");

    mfd::WindowAssetDefinition window = loaded.window;
    mfd::SceneRegistry scene(std::move(loaded.document));
    if (!scene.HasPage("Radar"))
    {
        throw std::runtime_error("The demo pages do not contain a Radar page");
    }

    scene.SetActivePage("Radar");

    mfd::CommandProcessor commandProcessor(scene);
    mfd::UdpRuntimeBridge udpRuntimeBridge(window.commandTransports, window.feedbackTransports);
    (void)udpRuntimeBridge.Start();
    mfd::DemoTrackFeed demoTrackFeed;
    mfd::MfdRenderer renderer;
    renderer.SetTextFontFile(window.fontFile);
    std::vector<mfd::UserCommand> pendingCommands;

    InitWindow(window.width, window.height, window.title.c_str());
    SetWindowPosition(window.positionX, window.positionY);
    SetTargetFPS(window.targetFps);

    float elapsedSeconds = 0.0f;
    float remoteCommandHoldSeconds = 0.0f;
    float strobeFeedbackAccumulator = 0.0f;
    std::uint32_t nextStrobeFeedbackSequence = 1;
    std::string lastRuntimeError;

    while (!WindowShouldClose())
    {
        const float deltaSeconds = GetFrameTime();
        elapsedSeconds += deltaSeconds;

        bool frameBegun = false;

        try
        {
            remoteCommandHoldSeconds = std::max(0.0f, remoteCommandHoldSeconds - deltaSeconds);

            pendingCommands.clear();
            const std::size_t drainedCommands = udpRuntimeBridge.DrainReceivedCommands(pendingCommands);
            if (drainedCommands > 0)
            {
                if (commandProcessor.Submit(std::span<const mfd::UserCommand>(pendingCommands.data(), pendingCommands.size())))
                {
                    remoteCommandHoldSeconds = kRemoteCommandFeedSuspendSeconds;
                }
            }

            if (remoteCommandHoldSeconds <= 0.0f)
            {
                demoTrackFeed.Update(elapsedSeconds, scene);
            }

            if (udpRuntimeBridge.FeedbackTransportReady())
            {
                strobeFeedbackAccumulator += std::max(deltaSeconds, 0.0f);
                if (strobeFeedbackAccumulator >= kStrobeFeedbackIntervalSeconds)
                {
                    strobeFeedbackAccumulator = std::fmod(strobeFeedbackAccumulator, kStrobeFeedbackIntervalSeconds);

                    for (const mfd::PageSummary& page : scene.Pages())
                    {
                        if (!page.hasStrobe)
                        {
                            continue;
                        }

                        const auto strobe = scene.StrobeForPage(page.name);
                        if (!strobe.has_value())
                        {
                            continue;
                        }

                        mfd::StrobeStatusFeedback feedback;
                        feedback.sequence = nextStrobeFeedbackSequence++;
                        feedback.pageName = strobe->pageName;
                        feedback.strobeId = strobe->reticleId;
                        feedback.active = strobe->visible;
                        feedback.position = strobe->position;
                        feedback.capture = strobe->capture;
                        feedback.magnet = ToFeedbackMagnet(scene.StrobeMagnetForPage(page.name));

                        if (const auto capture = scene.CaptureWithStrobe(page.name); capture.has_value())
                        {
                            feedback.captureResult = ToFeedbackCapture(*capture);
                        }

                        udpRuntimeBridge.EnqueueStrobeFeedback(std::move(feedback));
                    }
                }
            }

            BeginDrawing();
            frameBegun = true;
            ClearBackground(ToRayColor(scene.ActiveBackgroundColor()));
            renderer.DrawActivePage(scene);
            EndDrawing();
            frameBegun = false;
            lastRuntimeError.clear();
        }
        catch (const std::exception& exception)
        {
            if (frameBegun)
            {
                EndDrawing();
            }

            if (lastRuntimeError != exception.what())
            {
                std::cerr << "Runtime error: " << exception.what() << '\n';
            }

            lastRuntimeError = exception.what();
        }
        catch (...)
        {
            if (frameBegun)
            {
                EndDrawing();
            }

            const std::string unknownError = "Unknown runtime exception";
            if (lastRuntimeError != unknownError)
            {
                std::cerr << "Runtime error: " << unknownError << '\n';
            }

            lastRuntimeError = unknownError;
        }
    }

    CloseWindow();
    return 0;
}
} // namespace

int main()
{
#if defined(_WIN32)
    if (::IsDebuggerPresent() != 0)
    {
        return RunMinimalDemo();
    }
#endif

    try
    {
        return RunMinimalDemo();
    }
    catch (const std::exception& exception)
    {
        ReportFatalError(exception.what());
        return 1;
    }
}
