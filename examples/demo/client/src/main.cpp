/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Small DX11 demo client that automatically drives the simplified demo asset.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <imgui.h>

#include "Win32Dx11ImGuiHost.h"
#include "mfd/client/ClientSdk.h"
#include "mfd/control/CommandClient.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/io/JsonLoader.h"

namespace
{
constexpr std::string_view kWindowFile = "assets/windows/demo_window.json";
constexpr std::string_view kBlinkPage = "BlinkStrobe";
constexpr std::string_view kClippingPage = "Clipping";
constexpr float kPublishIntervalSeconds = 0.05f;
constexpr float kPageDurationSeconds = 5.0f;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kRadiansToDegrees = 57.2957795131f;

const std::array<std::string_view, 3> kPages {{
    "Primitives",
    kBlinkPage,
    kClippingPage,
}};

/**
 * @brief Runtime configuration loaded from the demo window JSON.
 */
struct DemoConfig
{
    /** @brief UDP command transport consumed by the demo client. */
    mfd::WindowUdpCommandTransport commandTransport {};
    /** @brief Generated transport map used to normalize authored identifiers. */
    mfd::GeneratedTransportMap generatedTransportMap {};
};

/**
 * @brief Runtime state of the automated demo loop.
 */
struct DemoState
{
    /** @brief Elapsed demo time in seconds. */
    float elapsedSeconds = 0.0f;
    /** @brief Accumulated time since the previous UDP publish. */
    float publishAccumulator = 0.0f;
    /** @brief Index of the currently requested demo page. */
    std::size_t pageIndex = 0;
    /** @brief Monotonic sequence copied into each command batch. */
    std::uint32_t sequence = 1;
    /** @brief Last client status displayed in the DX11 shell. */
    std::string status = "Ready.";
    /** @brief True when the last publish failed. */
    bool statusIsError = false;
};

DemoConfig LoadDemoConfig()
{
    mfd::JsonLoader loader;
    const mfd::LoadedWindowConfiguration loaded =
        loader.LoadWindowConfiguration(std::string(kWindowFile));

    if (!loaded.window.commandTransports.udp.has_value() || !loaded.window.commandTransports.udp->enabled)
    {
        throw std::runtime_error("The demo window JSON does not expose an enabled UDP command transport");
    }

    if (!loaded.generatedTransportMap.has_value())
    {
        throw std::runtime_error("The demo window JSON does not expose a generated transport map sidecar");
    }

    DemoConfig config;
    config.commandTransport = *loaded.window.commandTransports.udp;
    config.generatedTransportMap = *loaded.generatedTransportMap;
    return config;
}

mfd::ColorRgba PulseColor(const float elapsedSeconds)
{
    const float pulse = 0.5f + 0.5f * std::sin(elapsedSeconds * 2.1f);
    return mfd::ColorRgba {
        static_cast<std::uint8_t>(80.0f + pulse * 64.0f),
        static_cast<std::uint8_t>(220.0f + pulse * 35.0f),
        static_cast<std::uint8_t>(190.0f + pulse * 48.0f),
        255};
}

mfd::ReticlePatch BuildTrackPatch(const float elapsedSeconds)
{
    const float angle = elapsedSeconds * 1.3f;
    const float radius = 0.30f + 0.06f * std::sin(elapsedSeconds * 0.7f);

    mfd::ReticlePatch patch;
    patch.position = mfd::Vec2 {std::cos(angle) * radius, std::sin(angle) * radius};
    patch.rotationDegrees = angle * kRadiansToDegrees + 90.0f;
    patch.color = PulseColor(elapsedSeconds);
    patch.texts.emplace("track_label", "AUTO");
    return patch;
}

mfd::ReticlePatch BuildAttitudeBallPatch(const float elapsedSeconds)
{
    const float pitchOffset = std::sin(elapsedSeconds * 0.75f) * 0.22f;
    const float bankDegrees = std::sin(elapsedSeconds * 0.55f) * 38.0f;

    mfd::ReticlePatch patch;
    patch.position = mfd::Vec2 {0.0f, pitchOffset};
    patch.rotationDegrees = bankDegrees;
    return patch;
}

void AppendPageActivation(std::vector<mfd::UserCommand>& commands, const std::string_view pageName)
{
    mfd::ActivatePageCommand command;
    command.page = std::string(pageName);
    commands.push_back(std::move(command));
}

void AppendBlinkStrobeCommands(std::vector<mfd::UserCommand>& commands, const float elapsedSeconds)
{
    mfd::UpdateReticleCommand trackCommand;
    trackCommand.target.page = std::string(kBlinkPage);
    trackCommand.target.reticle = "track_fast";
    trackCommand.patch = BuildTrackPatch(elapsedSeconds);
    commands.push_back(std::move(trackCommand));

    const float strobeAngle = elapsedSeconds * 0.9f;
    mfd::UpdateStrobeCommand strobeCommand;
    strobeCommand.page = std::string(kBlinkPage);
    strobeCommand.strobe = "Default";
    strobeCommand.active = true;
    strobeCommand.position = mfd::Vec2 {std::cos(strobeAngle) * 0.48f, std::sin(strobeAngle) * 0.34f};
    commands.push_back(std::move(strobeCommand));
}

void AppendClippingCommands(std::vector<mfd::UserCommand>& commands, const float elapsedSeconds)
{
    const mfd::ReticlePatch attitudeBallPatch = BuildAttitudeBallPatch(elapsedSeconds);
    for (const std::string_view reticle : {"ball_sky", "ball_ground", "ball_horizon", "ball_ladder"})
    {
        mfd::UpdateReticleCommand command;
        command.target.page = std::string(kClippingPage);
        command.target.reticle = std::string(reticle);
        command.patch = attitudeBallPatch;
        commands.push_back(std::move(command));
    }
}

std::vector<mfd::UserCommand> BuildDemoCommands(DemoState& state)
{
    const std::size_t nextPageIndex =
        static_cast<std::size_t>(std::floor(state.elapsedSeconds / kPageDurationSeconds)) % kPages.size();

    std::vector<mfd::UserCommand> commands;
    commands.reserve(8);
    if (nextPageIndex != state.pageIndex || state.sequence == 1U)
    {
        state.pageIndex = nextPageIndex;
        AppendPageActivation(commands, kPages[state.pageIndex]);
    }

    AppendBlinkStrobeCommands(commands, state.elapsedSeconds);
    AppendClippingCommands(commands, state.elapsedSeconds);
    return commands;
}

void PublishDemoFrame(mfd::CommandClient& client, DemoState& state)
{
    std::vector<mfd::UserCommand> commands = BuildDemoCommands(state);
    if (commands.empty())
    {
        return;
    }

    if (!client.SendBatch(commands, state.sequence++))
    {
        state.status = client.LastError();
        state.statusIsError = true;
        return;
    }

    state.status = "Streaming " + std::string(kPages[state.pageIndex]);
    state.statusIsError = false;
}

void DrawStatusPanel(const DemoConfig& config, const DemoState& state)
{
    ImGui::SetNextWindowSize(ImVec2(430.0f, 170.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("MFD demo client");
    ImGui::Text("Window asset: %s", std::string(kWindowFile).c_str());
    ImGui::Text("UDP target: %s:%u", config.commandTransport.address.c_str(), config.commandTransport.port);
    ImGui::Text("Active page: %s", std::string(kPages[state.pageIndex]).c_str());
    ImGui::Text("Client FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::TextColored(
        state.statusIsError ? ImVec4(1.0f, 0.45f, 0.35f, 1.0f) : ImVec4(0.45f, 0.95f, 0.62f, 1.0f),
        "%s",
        state.status.c_str());
    ImGui::End();
}
} // namespace

int main()
{
    try
    {
        const DemoConfig config = LoadDemoConfig();
        mfd::CommandClient client(config.commandTransport, config.generatedTransportMap);
        if (!client.IsReady())
        {
            throw std::runtime_error("Unable to initialize the demo command client: " + client.LastError());
        }

        Win32Dx11ImGuiHost host;
        Win32Dx11ImGuiHost::Config hostConfig;
        hostConfig.width = 720;
        hostConfig.height = 300;
        hostConfig.minWidth = 520;
        hostConfig.minHeight = 240;
        hostConfig.title = "MFD Demo Client";

        std::string error;
        if (!host.Initialize(hostConfig, error))
        {
            throw std::runtime_error(error);
        }

        DemoState state;
        host.SetClearColor(5, 10, 14, 255);

        float deltaSeconds = 0.0f;
        while (host.BeginFrame(deltaSeconds))
        {
            const float boundedDelta = std::clamp(deltaSeconds, 0.0f, 0.25f);
            state.elapsedSeconds += boundedDelta;
            state.publishAccumulator += boundedDelta;
            if (state.publishAccumulator >= kPublishIntervalSeconds)
            {
                state.publishAccumulator = 0.0f;
                PublishDemoFrame(client, state);
            }

            DrawStatusPanel(config, state);
            host.EndFrame();
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "demo_client error: " << exception.what() << '\n';
        return 1;
    }
}
