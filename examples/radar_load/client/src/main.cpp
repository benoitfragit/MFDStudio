/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Headless radar load example that progressively drives up to 300 dynamic tracks.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "mfd/client/ClientSdk.h"
#include "mfd/control/CommandClient.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/io/JsonLoader.h"

namespace
{
constexpr std::string_view kWindowFile = "assets/windows/radar_load_window.json";
constexpr std::string_view kPageName = "RadarLoad";
constexpr std::string_view kTrackTemplateId = "radar_track";
constexpr std::chrono::milliseconds kSimulationTick {20};
constexpr float kSimulationTickSeconds = 0.020f;
constexpr float kTrackCountStepSeconds = 4.0f;
constexpr float kTwoPi = 6.28318530718f;
constexpr float kRadiansToDegrees = 57.2957795131f;
constexpr int kMaxTrackCount = 300;

const std::array<int, 14> kTrackCountRamp {{
    10,
    20,
    30,
    40,
    50,
    60,
    70,
    80,
    90,
    100,
    150,
    200,
    250,
    300,
}};

const std::array<mfd::ColorRgba, 4> kTrackPalette {{
    mfd::ColorRgba {0, 255, 0, 255},
    mfd::ColorRgba {72, 220, 255, 255},
    mfd::ColorRgba {255, 191, 0, 255},
    mfd::ColorRgba {255, 96, 96, 255},
}};

volatile std::sig_atomic_t gStopRequested = 0;

/**
 * @brief Runtime configuration loaded from the radar load window JSON.
 */
struct RadarLoadConfig
{
    /** @brief UDP command transport consumed by the load client. */
    mfd::WindowUdpCommandTransport commandTransport {};
    /** @brief Generated transport map used to normalize authored identifiers. */
    mfd::GeneratedTransportMap generatedTransportMap {};
};

/**
 * @brief Stable ids and labels used by the radar load generator.
 */
struct TrackCatalog
{
    /** @brief Public aliases used to obtain stable runtime dynamic ids. */
    std::array<std::string, kMaxTrackCount> ids {};
    /** @brief Short labels rendered inside each track reticle. */
    std::array<std::string, kMaxTrackCount> labels {};
};

/**
 * @brief Last known transmission state for one dynamic radar track.
 */
struct TrackPublishState
{
    /** @brief Indicates whether the track was already created in the runtime. */
    bool published = false;
};

/**
 * @brief Per-tick radar track values computed by the local simulation.
 */
struct TrackFrame
{
    /** @brief Track position in page logical coordinates. */
    mfd::Vec2 position {};
    /** @brief Track heading in degrees. */
    float rotationDegrees = 0.0f;
    /** @brief Stable track stroke color. */
    mfd::ColorRgba color {};
    /** @brief Whether the authored blink behavior is enabled for this track. */
    bool blinkEnabled = false;
    /** @brief Blink type selected when blinking is enabled. */
    std::string_view blinkType {};
};

/**
 * @brief Runtime load-test state advanced on every fixed tick.
 */
struct RadarLoadState
{
    /** @brief Elapsed simulation time in seconds. */
    float elapsedSeconds = 0.0f;
    /** @brief Last one-second FPS sample. */
    float currentFps = 0.0f;
    /** @brief Command batch sequence sent to the runtime. */
    std::uint32_t sequence = 1;
    /** @brief Time point of the previous FPS sample. */
    std::chrono::steady_clock::time_point fpsSampleStart {};
    /** @brief Frames counted since the previous FPS sample. */
    int framesSinceSample = 0;
    /** @brief Last status text accepted for publication. */
    std::string lastStatusText {};
    /** @brief Tracks already created in the runtime, indexed like TrackCatalog. */
    std::array<TrackPublishState, kMaxTrackCount> tracks {};
    /** @brief Reused dynamic track update buffer to avoid per-frame vector allocation. */
    std::vector<mfd::DynamicReticleState> trackStates {};
};

void HandleSignal(int)
{
    gStopRequested = 1;
}

TrackCatalog BuildTrackCatalog()
{
    TrackCatalog catalog;
    for (int index = 0; index < kMaxTrackCount; ++index)
    {
        char idBuffer[32] {};
        std::snprintf(idBuffer, sizeof(idBuffer), "load_track_%03d", index + 1);
        catalog.ids[static_cast<std::size_t>(index)] = idBuffer;

        char labelBuffer[16] {};
        std::snprintf(labelBuffer, sizeof(labelBuffer), "T%03d", index + 1);
        catalog.labels[static_cast<std::size_t>(index)] = labelBuffer;
    }

    return catalog;
}

RadarLoadConfig LoadRadarConfig()
{
    mfd::JsonLoader loader;
    const mfd::LoadedWindowConfiguration loaded =
        loader.LoadWindowConfiguration(std::string(kWindowFile));

    if (!loaded.window.commandTransports.udp.has_value() || !loaded.window.commandTransports.udp->enabled)
    {
        throw std::runtime_error("The radar load window JSON does not expose an enabled UDP command transport");
    }

    if (!loaded.generatedTransportMap.has_value())
    {
        throw std::runtime_error("The radar load window JSON does not expose a generated transport map sidecar");
    }

    RadarLoadConfig config;
    config.commandTransport = *loaded.window.commandTransports.udp;
    config.generatedTransportMap = *loaded.generatedTransportMap;
    return config;
}

int CurrentTrackCount(const float elapsedSeconds) noexcept
{
    const int rampIndex = std::min(
        static_cast<int>(elapsedSeconds / kTrackCountStepSeconds),
        static_cast<int>(kTrackCountRamp.size() - 1U));
    return kTrackCountRamp[static_cast<std::size_t>(rampIndex)];
}

void UpdateFpsSample(RadarLoadState& state, const std::chrono::steady_clock::time_point now)
{
    ++state.framesSinceSample;
    if (state.fpsSampleStart.time_since_epoch().count() == 0)
    {
        state.fpsSampleStart = now;
        return;
    }

    const std::chrono::duration<float> elapsed = now - state.fpsSampleStart;
    if (elapsed.count() < 1.0f)
    {
        return;
    }

    state.currentFps = static_cast<float>(state.framesSinceSample) / elapsed.count();
    state.framesSinceSample = 0;
    state.fpsSampleStart = now;
}

std::string FormatStatusText(const int trackCount, const float fps)
{
    char buffer[64] {};
    std::snprintf(buffer, sizeof(buffer), "TRACKS %03d | FPS %04.1f", trackCount, fps);
    return buffer;
}

TrackFrame BuildTrackFrame(const int index, const int trackCount, const float elapsedSeconds)
{
    const float normalizedIndex =
        static_cast<float>(index) / static_cast<float>(std::max(trackCount, 1));
    const float ringIndex = static_cast<float>(index % 12) / 11.0f;
    const float speed = 0.28f + normalizedIndex * 1.35f;
    const float baseAngle = normalizedIndex * kTwoPi;
    const float angle = baseAngle + elapsedSeconds * speed;
    const float radiusPulse = 0.035f * std::sin(elapsedSeconds * 0.85f + normalizedIndex * 11.0f);
    const float radius = std::clamp(0.12f + ringIndex * 0.76f + radiusPulse, 0.10f, 0.94f);

    TrackFrame frame;
    frame.position = mfd::Vec2 {std::cos(angle) * radius, std::sin(angle) * radius};
    frame.rotationDegrees = angle * kRadiansToDegrees + 90.0f;
    frame.color = kTrackPalette[static_cast<std::size_t>(index) % kTrackPalette.size()];
    frame.blinkEnabled = index % 5 == 0;
    frame.blinkType = index % 10 == 0 ? std::string_view {"caution"} : std::string_view {"fast"};
    return frame;
}

void ApplyTrackMotionPatch(mfd::ReticlePatch& patch, const TrackFrame& frame)
{
    patch.position = frame.position;
    patch.rotationDegrees = frame.rotationDegrees;
}

void ApplyTrackCreationPatch(mfd::ReticlePatch& patch,
                             const TrackFrame& frame,
                             const TrackCatalog& catalog,
                             const int index)
{
    patch.visible = true;
    patch.color = frame.color;
    patch.blinkEnabled = frame.blinkEnabled;
    patch.blinkType = std::string(frame.blinkType);
    patch.texts.emplace("track_label", catalog.labels[static_cast<std::size_t>(index)]);
}

void BuildTrackStates(RadarLoadState& state, const TrackCatalog& catalog, const int trackCount)
{
    state.trackStates.clear();
    state.trackStates.reserve(static_cast<std::size_t>(kMaxTrackCount));

    for (int index = 0; index < trackCount; ++index)
    {
        const TrackFrame frame = BuildTrackFrame(index, trackCount, state.elapsedSeconds);

        mfd::DynamicReticleState track;
        track.reticleId = catalog.ids[static_cast<std::size_t>(index)];
        ApplyTrackMotionPatch(track.patch, frame);
        if (!state.tracks[static_cast<std::size_t>(index)].published)
        {
            ApplyTrackCreationPatch(track.patch, frame, catalog, index);
        }
        state.trackStates.push_back(std::move(track));
    }
}

void MarkTracksPublished(RadarLoadState& state, const int trackCount) noexcept
{
    for (int index = 0; index < trackCount; ++index)
    {
        state.tracks[static_cast<std::size_t>(index)].published = true;
    }
}

void AppendStatusCommand(RadarLoadState& state,
                         std::vector<mfd::UserCommand>& commands,
                         const int trackCount)
{
    std::string statusText = FormatStatusText(trackCount, state.currentFps);
    if (statusText == state.lastStatusText)
    {
        return;
    }

    mfd::UpdateReticleCommand statusCommand;
    statusCommand.target.page = std::string(kPageName);
    statusCommand.target.reticle = "load_status";
    statusCommand.patch.texts.emplace("status_text", statusText);
    commands.push_back(std::move(statusCommand));
    state.lastStatusText = std::move(statusText);
}

std::vector<mfd::UserCommand> BuildRadarCommands(RadarLoadState& state,
                                                 const TrackCatalog& catalog,
                                                 const int trackCount)
{
    BuildTrackStates(state, catalog, trackCount);

    std::vector<mfd::UserCommand> commands;
    commands.reserve(3);

    if (state.sequence == 1U)
    {
        mfd::ActivatePageCommand activatePage;
        activatePage.page = std::string(kPageName);
        commands.push_back(std::move(activatePage));
    }

    AppendStatusCommand(state, commands, trackCount);

    mfd::UpsertDynamicReticlesCommand tracksCommand;
    tracksCommand.page = std::string(kPageName);
    tracksCommand.templateId = std::string(kTrackTemplateId);
    tracksCommand.reticles = state.trackStates;
    commands.push_back(std::move(tracksCommand));

    return commands;
}

void PublishRadarFrame(mfd::CommandClient& client, RadarLoadState& state, const TrackCatalog& catalog)
{
    const int trackCount = CurrentTrackCount(state.elapsedSeconds);
    std::vector<mfd::UserCommand> commands = BuildRadarCommands(state, catalog, trackCount);
    if (!client.SendBatch(commands, state.sequence))
    {
        throw std::runtime_error("Unable to send radar load batch: " + client.LastError());
    }
    MarkTracksPublished(state, trackCount);

    if (state.sequence % 50U == 1U)
    {
        std::cout << FormatStatusText(trackCount, state.currentFps) << '\n';
    }

    ++state.sequence;
}
} // namespace

int main()
{
    try
    {
        std::signal(SIGINT, HandleSignal);
#if defined(SIGTERM)
        std::signal(SIGTERM, HandleSignal);
#endif

        const RadarLoadConfig config = LoadRadarConfig();
        mfd::CommandClient client(config.commandTransport, config.generatedTransportMap);
        if (!client.IsReady())
        {
            throw std::runtime_error("Unable to initialize the radar load client: " + client.LastError());
        }

        const TrackCatalog catalog = BuildTrackCatalog();
        RadarLoadState state;
        state.trackStates.reserve(static_cast<std::size_t>(kMaxTrackCount));

        std::cout << "radar_load_client\n";
        std::cout << "Window asset: " << kWindowFile << '\n';
        std::cout << "UDP target: " << config.commandTransport.address << ':' << config.commandTransport.port << '\n';
        std::cout << "Ramp: 10, 20, ... 100, 150, 200, 250, 300 tracks.\n";
        std::cout << "Start Scripts\\Start-RadarLoad.bat separately and press Ctrl+C here to stop.\n";

        using clock = std::chrono::steady_clock;
        auto nextTick = clock::now();

        while (gStopRequested == 0)
        {
            const auto now = clock::now();
            UpdateFpsSample(state, now);
            state.elapsedSeconds += kSimulationTickSeconds;
            PublishRadarFrame(client, state, catalog);

            nextTick += kSimulationTick;
            const auto sleepStart = clock::now();
            if (sleepStart < nextTick)
            {
                std::this_thread::sleep_until(nextTick);
            }
            else
            {
                nextTick = sleepStart;
            }
        }

        std::cout << "radar_load_client stopped.\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "radar_load_client error: " << exception.what() << '\n';
        return 1;
    }
}
