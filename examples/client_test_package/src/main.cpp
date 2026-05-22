/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Minimal delivery-package consumer driving one bouncing circle.
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include "PackageTestUi.h"
#include "mfd/client/ClientSdk.h"

namespace
{
constexpr std::chrono::milliseconds kSimulationTick {20};
constexpr float kSimulationTickSeconds = 0.020f;
constexpr float kCircleRadius = 0.12f;
constexpr float kMinPosition = -0.82f;
constexpr float kMaxPosition = 0.82f;

volatile std::sig_atomic_t gStopRequested = 0;

/**
 * @brief Runtime configuration loaded from the lightweight package-test window JSON.
 */
struct ExampleConfig
{
    /** @brief UDP command transport exposed by the window JSON. */
    mfd::WindowUdpCommandTransport transport {};
};

/**
 * @brief Mutable state of the bouncing-circle simulation.
 */
struct BounceState
{
    /** @brief Current logical page position of the circle center. */
    mfd::Vec2 position {-0.46f, 0.32f};
    /** @brief Current logical page velocity in page units per second. */
    mfd::Vec2 velocity {0.78f, -0.66f};
    /** @brief Elapsed simulation time in seconds. */
    float elapsedSeconds = 0.0f;
    /** @brief Monotonic sequence copied in each generated command batch. */
    std::uint32_t sequence = 1;
};

void HandleSignal(int)
{
    gStopRequested = 1;
}

void Require(const bool success, const mfd::CommandClient& client, const std::string_view action)
{
    if (!success)
    {
        throw std::runtime_error(std::string(action) + ": " + client.LastError());
    }
}

std::uint8_t ToChannel(const float value)
{
    const long rounded = std::lround(std::clamp(value, 0.0f, 255.0f));
    return static_cast<std::uint8_t>(rounded);
}

/**
 * @brief Loads the package-test window transport from the authored JSON.
 * @return Example configuration required by the client.
 */
ExampleConfig LoadExampleConfig()
{
    mfd::JsonLoader loader;
    const mfd::LoadedWindowConfiguration loaded =
        loader.LoadWindowConfiguration(std::string(package_test_ui::PackageTestUi::WindowFile()));

    if (!loaded.window.commandTransports.udp.has_value() || !loaded.window.commandTransports.udp->enabled)
    {
        throw std::runtime_error("The package-test window JSON does not expose an enabled UDP command transport");
    }

    ExampleConfig config;
    config.transport = *loaded.window.commandTransports.udp;
    return config;
}

void ReflectOnBounds(float& position, float& velocity)
{
    if (position < kMinPosition)
    {
        position = kMinPosition + (kMinPosition - position);
        velocity = std::abs(velocity);
    }
    else if (position > kMaxPosition)
    {
        position = kMaxPosition - (position - kMaxPosition);
        velocity = -std::abs(velocity);
    }
}

/**
 * @brief Advances the bouncing circle by one fixed simulation step.
 * @param state Simulation state updated in place.
 */
void Advance(BounceState& state)
{
    state.elapsedSeconds += kSimulationTickSeconds;
    state.position.x += state.velocity.x * kSimulationTickSeconds;
    state.position.y += state.velocity.y * kSimulationTickSeconds;
    ReflectOnBounds(state.position.x, state.velocity.x);
    ReflectOnBounds(state.position.y, state.velocity.y);
}

mfd::ColorRgba MakeStrokeColor(const float elapsedSeconds)
{
    const float pulse = 0.5f + 0.5f * std::sin(elapsedSeconds * 1.9f);
    return mfd::ColorRgba {
        ToChannel(112.0f + pulse * 92.0f),
        ToChannel(214.0f + pulse * 30.0f),
        ToChannel(232.0f + pulse * 18.0f),
        255};
}

mfd::ColorRgba MakeFillColor(const float elapsedSeconds)
{
    const float pulse = 0.5f + 0.5f * std::sin(elapsedSeconds * 1.9f + 1.2f);
    return mfd::ColorRgba {
        ToChannel(18.0f + pulse * 22.0f),
        ToChannel(62.0f + pulse * 44.0f),
        ToChannel(74.0f + pulse * 54.0f),
        ToChannel(168.0f + pulse * 48.0f)};
}

int mainImpl()
{
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    const ExampleConfig config = LoadExampleConfig();
    mfd::CommandClient client(config.transport);
    if (!client.IsReady())
    {
        throw std::runtime_error("Unable to initialize command client: " + client.LastError());
    }

    package_test_ui::PackageTestUi ui;
    auto& page = ui.PackageTest();
    auto& bouncingCircle = page.bouncingCircle;
    auto& orb = bouncingCircle.Orb();

    BounceState state;

    bouncingCircle.SetVisible(true);
    bouncingCircle.SetPosition(state.position);
    bouncingCircle.SetThickness(0.008f);
    orb.SetRadius(kCircleRadius);
    orb.SetFilled(true);
    orb.SetFillColor(MakeFillColor(state.elapsedSeconds));
    bouncingCircle.SetColor(MakeStrokeColor(state.elapsedSeconds));

    Require(client.ActivatePage(page), client, "Unable to activate PackageTest page");
    Require(client.SendBatch(ui.BuildCommandBatch(state.sequence++)), client, "Unable to send package-test startup batch");

    std::cout << "client_test_package\n";
    std::cout << "Window asset: " << package_test_ui::PackageTestUi::WindowFile() << '\n';
    std::cout << "Launch Scripts\\Start-MfdPackageTest.bat or the staged Start-MfdPackageTest.bat separately.\n";
    std::cout << "Press Ctrl+C here to stop.\n";

    while (!gStopRequested)
    {
        Advance(state);

        bouncingCircle.SetPosition(state.position);
        bouncingCircle.SetThickness(0.0065f + 0.0015f * (0.5f + 0.5f * std::sin(state.elapsedSeconds * 3.2f)));
        bouncingCircle.SetColor(MakeStrokeColor(state.elapsedSeconds));
        orb.SetFillColor(MakeFillColor(state.elapsedSeconds));

        Require(client.SendBatch(ui.BuildCommandBatch(state.sequence++)),
                client,
                "Unable to send package-test animation batch");

        std::this_thread::sleep_for(kSimulationTick);
    }

    std::cout << "client_test_package stopped.\n";
    return 0;
}
} // namespace

int main()
{
    try
    {
        return mainImpl();
    }
    catch (const std::exception& exception)
    {
        std::cerr << "client_test_package error: " << exception.what() << '\n';
        return 1;
    }
}
