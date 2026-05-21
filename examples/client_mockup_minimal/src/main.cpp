/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Minimal headless client driving the cockpit demo from one plain `main` loop.
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
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "MockupUi.h"
#include "mfd/client/ClientSdk.h"

namespace
{
constexpr std::chrono::milliseconds kSimulationTick {20};
constexpr float kSimulationTickSeconds = 0.020f;
constexpr float kDegreesToRadians = 0.01745329252f;
constexpr float kRadiansToDegrees = 57.2957795f;
constexpr float kKnotsPerMach = 661.0f;
constexpr float kCockpitAdiCenterX = -1.03f;
constexpr float kCockpitAdiCenterY = 0.00f;
constexpr float kCockpitHudCenterX = 0.00f;
constexpr float kCockpitHudCenterY = 0.00f;
constexpr float kCockpitRadarCenterX = 1.03f;
constexpr float kCockpitRadarCenterY = -0.02f;
constexpr std::string_view kCockpitRadarTemplateId = "cockpit_radar_contact";

volatile std::sig_atomic_t gStopRequested = 0;

/**
 * @brief Runtime configuration loaded from the cockpit demo window JSON.
 */
struct ExampleConfig
{
    /** @brief UDP command transport consumed by the headless client. */
    mfd::WindowUdpCommandTransport transport {};
    /** @brief Generated transport map used by the raw command helpers. */
    mfd::GeneratedTransportMap generatedTransportMap {};
    /** @brief Authored page view re-applied during startup. */
    mfd::PageViewState cockpitView {};
};

/**
 * @brief Simple aircraft state advanced every 20 ms by the example loop.
 */
struct CockpitSimulationState
{
    bool radarEnabled = true;
    float elapsedSeconds = 0.0f;
    float pitchDegrees = 2.0f;
    float bankDegrees = 0.0f;
    float headingDegrees = 32.0f;
    float selectedHeadingDegrees = 32.0f;
    float flightPathAngleDegrees = 1.5f;
    float throttle = 0.48f;
    float speedKts = 320.0f;
    float ownshipX = 0.0f;
    float ownshipY = 0.0f;
    std::uint32_t sequence = 1;
    std::vector<mockup_ui::CockpitRadarContactDynamicReticle*> radarContacts {};
};

/**
 * @brief Immutable radar-contact seed used to synthesize dynamic tracks.
 */
struct CockpitTargetSeed
{
    const char* id = "";
    const char* label = "";
    float baseX = 0.0f;
    float baseY = 0.0f;
    float orbitRadius = 0.0f;
    float orbitRate = 0.0f;
    float orbitPhase = 0.0f;
    mfd::ColorRgba color {};
    bool blink = false;
};

const std::array<CockpitTargetSeed, 6> kCockpitTargets {{
    {"cockpit_contact_01", "B21", 1.8f, 9.4f, 0.5f, 0.30f, 0.2f, mfd::ColorRgba {94, 244, 162, 255}, false},
    {"cockpit_contact_02", "M42", -4.8f, 7.6f, 0.7f, -0.25f, 1.0f, mfd::ColorRgba {255, 198, 109, 255}, true},
    {"cockpit_contact_03", "A17", 6.1f, 5.7f, 0.6f, 0.42f, 2.2f, mfd::ColorRgba {88, 214, 255, 255}, false},
    {"cockpit_contact_04", "F03", -7.0f, 3.0f, 0.8f, 0.36f, 0.6f, mfd::ColorRgba {255, 132, 92, 255}, true},
    {"cockpit_contact_05", "L08", 2.5f, -3.4f, 0.5f, -0.44f, 2.8f, mfd::ColorRgba {166, 255, 206, 255}, false},
    {"cockpit_contact_06", "T55", -3.2f, -7.6f, 0.9f, 0.28f, 1.9f, mfd::ColorRgba {255, 176, 98, 255}, true},
}};

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

float WrapDegrees(float value)
{
    value = std::fmod(value, 360.0f);
    if (value < 0.0f)
    {
        value += 360.0f;
    }

    return value;
}

float Approach(const float current, const float target, const float maxDelta)
{
    if (current < target)
    {
        return std::min(current + maxDelta, target);
    }

    return std::max(current - maxDelta, target);
}

std::string FormatHeadingValue(const float headingDegrees)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%03d", static_cast<int>(std::lround(WrapDegrees(headingDegrees))) % 360);
    return buffer;
}

std::string FormatSignedFloat(const float value, const char* format)
{
    char buffer[32] {};
    std::snprintf(buffer, sizeof(buffer), format, value);
    return buffer;
}

std::string FormatSpeedValue(const float speedKts)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%03d", static_cast<int>(std::lround(std::max(speedKts, 0.0f))));
    return buffer;
}

std::string FormatPercentValue(const float factor)
{
    char buffer[16] {};
    std::snprintf(buffer, sizeof(buffer), "%02d%%", static_cast<int>(std::lround(std::clamp(factor, 0.0f, 1.0f) * 100.0f)));
    return buffer;
}

std::string BuildStatusCaption(const CockpitSimulationState& simulation)
{
    char buffer[96] {};
    std::snprintf(buffer,
                  sizeof(buffer),
                  "CLI API seq %05u | %s | THR %s",
                  simulation.sequence,
                  simulation.radarEnabled ? "RADAR EMIT" : "RADAR STBY",
                  FormatPercentValue(simulation.throttle).c_str());
    return buffer;
}

/**
 * @brief Loads the cockpit demo transport and authored page view from JSON.
 * @return Example configuration required by the headless client.
 */
ExampleConfig LoadExampleConfig()
{
    mfd::JsonLoader loader;
    const mfd::LoadedWindowConfiguration loaded =
        loader.LoadWindowConfiguration(std::string(mockup_ui::CockpitMockupUi::WindowFile()));

    const mfd::PageDefinition* cockpitPage =
        mfd::FindPageDefinition(loaded.document, mockup_ui::CockpitMockupPage::Name());
    if (cockpitPage == nullptr)
    {
        throw std::runtime_error("The cockpit window JSON does not expose a page named 'Cockpit'");
    }

    if (loaded.document.reticleLibrary.find(std::string(kCockpitRadarTemplateId)) == loaded.document.reticleLibrary.end())
    {
        throw std::runtime_error("The cockpit reticle library does not expose the 'cockpit_radar_contact' template");
    }

    if (!loaded.window.commandTransports.udp.has_value() || !loaded.window.commandTransports.udp->enabled)
    {
        throw std::runtime_error("The cockpit window JSON does not expose an enabled UDP command transport");
    }
    if (!loaded.generatedTransportMap.has_value())
    {
        throw std::runtime_error("The cockpit window JSON does not expose a generated transport map sidecar");
    }

    ExampleConfig config;
    config.transport = *loaded.window.commandTransports.udp;
    config.generatedTransportMap = *loaded.generatedTransportMap;
    config.cockpitView = cockpitPage->view;
    return config;
}

/**
 * @brief Advances the toy aircraft model by one fixed simulation step.
 * @param simulation State updated in place.
 * @param deltaSeconds Fixed time step expressed in seconds.
 */
void StepCockpitSimulation(CockpitSimulationState& simulation, const float deltaSeconds)
{
    simulation.elapsedSeconds += std::max(deltaSeconds, 0.0f);

    const float time = simulation.elapsedSeconds;
    const float targetPitchDegrees = 18.0f * std::sin(time * 0.47f);
    const float targetBankDegrees = 42.0f * std::sin(time * 0.31f + 0.65f);

    simulation.pitchDegrees = Approach(
        simulation.pitchDegrees,
        targetPitchDegrees,
        26.0f * deltaSeconds);
    simulation.bankDegrees = Approach(
        simulation.bankDegrees,
        targetBankDegrees,
        88.0f * deltaSeconds);

    simulation.throttle = std::clamp(
        0.60f + 0.34f * std::sin(time * 0.19f - 0.80f),
        0.12f,
        0.98f);
    simulation.radarEnabled = std::fmod(time, 18.0f) < 13.5f;

    const float bankRadians = simulation.bankDegrees * kDegreesToRadians;
    const float turnRateDegreesPerSecond =
        std::sin(bankRadians) * std::clamp(8.0f + simulation.speedKts * 0.028f, 8.0f, 30.0f);

    simulation.headingDegrees = WrapDegrees(
        simulation.headingDegrees + turnRateDegreesPerSecond * deltaSeconds);
    simulation.selectedHeadingDegrees = WrapDegrees(
        simulation.headingDegrees + 26.0f * std::sin(time * 0.13f + 0.50f));

    const float targetSpeedKts =
        std::clamp(190.0f + simulation.throttle * 690.0f -
                       std::max(simulation.pitchDegrees, 0.0f) * 1.3f -
                       std::abs(simulation.bankDegrees) * 0.18f,
                   160.0f,
                   860.0f);
    simulation.speedKts += (targetSpeedKts - simulation.speedKts) * std::min(deltaSeconds * 1.8f, 1.0f);

    const float targetFlightPathAngle =
        std::clamp(simulation.pitchDegrees * 0.70f + (simulation.throttle - 0.52f) * 5.8f -
                       std::abs(simulation.bankDegrees) * 0.04f,
                   -18.0f,
                   20.0f);
    simulation.flightPathAngleDegrees +=
        (targetFlightPathAngle - simulation.flightPathAngleDegrees) * std::min(deltaSeconds * 2.5f, 1.0f);

    const float headingRadians = simulation.headingDegrees * kDegreesToRadians;
    const float speedWorldUnitsPerSecond = simulation.speedKts * 0.00045f;
    simulation.ownshipX += std::sin(headingRadians) * speedWorldUnitsPerSecond * deltaSeconds;
    simulation.ownshipY += std::cos(headingRadians) * speedWorldUnitsPerSecond * deltaSeconds;
}

/**
 * @brief Builds one cockpit command batch from the current simulation state.
 *
 * The function demonstrates the intended usage pattern of the minimal mockup
 * facade:
 *
 * 1. reset the typed helper tree for the new cycle
 * 2. mutate page, reticle and dynamic-reticle instances directly
 * 3. emit only the commands whose final state changed
 *
 * @param ui Typed cockpit helper tree used by the example client.
 * @param simulation Aircraft and radar state sampled for this frame.
 * @return Batched typed commands ready to be sent through `CommandClient`.
 */
void PopulateCockpitBatch(mockup_ui::CockpitMockupUi& ui, CockpitSimulationState& simulation)
{
    static constexpr mfd::ColorRgba kHudNominal {46, 255, 162, 255};
    static constexpr mfd::ColorRgba kHudWarning {255, 198, 109, 255};
    static constexpr mfd::ColorRgba kRadarSearch {86, 244, 162, 255};
    static constexpr mfd::ColorRgba kRadarStandby {255, 198, 109, 255};

    const bool overspeed = simulation.speedKts > 700.0f;
    const float adiPitchOffset = std::clamp(-simulation.pitchDegrees * 0.0063f, -0.60f, 0.60f);
    const float hudPitchOffset = std::clamp(-simulation.pitchDegrees * 0.0080f, -0.24f, 0.24f);
    const float hudFpmX = std::clamp(simulation.bankDegrees * 0.0032f, -0.14f, 0.14f);
    const float hudFpmY = std::clamp(
        (simulation.flightPathAngleDegrees - simulation.pitchDegrees) * 0.014f - 0.03f,
        -0.18f,
        0.18f);
    const float radarSweepDegrees = WrapDegrees(std::fmod(simulation.elapsedSeconds * 90.0f, 360.0f));
    const std::string headingText = FormatHeadingValue(simulation.headingDegrees);
    const std::string selectedHeadingText = FormatHeadingValue(simulation.selectedHeadingDegrees);
    const std::string speedText = FormatSpeedValue(simulation.speedKts);
    const std::string machText = FormatSignedFloat(simulation.speedKts / kKnotsPerMach, "%.2f");
    const std::string pitchText = FormatSignedFloat(simulation.pitchDegrees, "%+04.1f");
    const std::string rollText = FormatSignedFloat(simulation.bankDegrees, "%+03.0f");
    const std::string fpaText = FormatSignedFloat(simulation.flightPathAngleDegrees, "%+04.1f");
    const std::string throttleText = FormatPercentValue(simulation.throttle);
    const std::string statusCaption = BuildStatusCaption(simulation);
    const float headingBugRelativeDegrees =
        std::remainder(simulation.selectedHeadingDegrees - simulation.headingDegrees, 360.0f);

    ui.Reset();
    mockup_ui::CockpitMockupPage& cockpit = ui.Cockpit();
    auto& radarContacts = cockpit.DynamicCockpitRadarContact();

    if (simulation.radarContacts.empty())
    {
        simulation.radarContacts.reserve(kCockpitTargets.size());
        for (std::size_t index = 0; index < kCockpitTargets.size(); ++index)
        {
            simulation.radarContacts.push_back(&radarContacts.Create());
        }
    }

    // ADI updates.
    const mfd::Vec2 adiBallPosition {kCockpitAdiCenterX, kCockpitAdiCenterY + adiPitchOffset};
    cockpit.adiBallSky.SetPosition(adiBallPosition);
    cockpit.adiBallSky.SetRotationDegrees(simulation.bankDegrees);

    cockpit.adiBallGround.SetPosition(adiBallPosition);
    cockpit.adiBallGround.SetRotationDegrees(simulation.bankDegrees);

    cockpit.adiBallHorizon.SetPosition(adiBallPosition);
    cockpit.adiBallHorizon.SetRotationDegrees(simulation.bankDegrees);

    cockpit.adiBallLadder.SetPosition(adiBallPosition);
    cockpit.adiBallLadder.SetRotationDegrees(simulation.bankDegrees);

    cockpit.adiHeadingBox.HeadingValue().SetText(headingText);
    cockpit.adiHeadingBox.CommandValue().SetText(selectedHeadingText);
    cockpit.adiHeadingCard.SetRotationDegrees(-simulation.headingDegrees);
    cockpit.adiHeadingCommandBug.SetRotationDegrees(headingBugRelativeDegrees);
    cockpit.adiPitchBox.SetValue(pitchText);
    cockpit.adiRollBox.SetValue(rollText);

    // HUD updates.
    cockpit.hudPitchLadder.SetPosition(mfd::Vec2 {kCockpitHudCenterX, kCockpitHudCenterY + hudPitchOffset});
    cockpit.hudPitchLadder.SetRotationDegrees(simulation.bankDegrees * 0.88f);

    cockpit.hudVelocityVector.SetPosition(mfd::Vec2 {kCockpitHudCenterX + hudFpmX, kCockpitHudCenterY + hudFpmY});

    cockpit.hudSpeedBox.SetValue(speedText);
    cockpit.hudSpeedBox.SetColor(overspeed ? kHudWarning : kHudNominal);
    cockpit.hudMachBox.SetValue(machText);
    cockpit.hudMachBox.SetColor(overspeed ? kHudWarning : kHudNominal);

    if (overspeed)
    {
        cockpit.hudSpeedBox.Blink = cockpit.overspeed;
        cockpit.hudMachBox.Blink = cockpit.overspeed;
    }
    else
    {
        cockpit.hudSpeedBox.Blink = nullptr;
        cockpit.hudMachBox.Blink = nullptr;
    }

    cockpit.hudHeadingBox.SetValue(headingText);
    cockpit.hudFpaBox.SetValue(fpaText);
    cockpit.hudThrottleBox.SetValue(throttleText);
    cockpit.hudRadarBox.SetValue(simulation.radarEnabled ? std::string {"EMIT"} : std::string {"STBY"});
    cockpit.hudRadarBox.SetColor(simulation.radarEnabled ? kHudNominal : kHudWarning);

    // Radar panel updates.
    cockpit.radarScope.SetVisible(simulation.radarEnabled);
    cockpit.radarSweep.SetVisible(simulation.radarEnabled);
    cockpit.radarSweep.SetPosition(mfd::Vec2 {kCockpitRadarCenterX, kCockpitRadarCenterY});
    cockpit.radarSweep.SetRotationDegrees(-radarSweepDegrees);
    cockpit.radarOwnship.SetVisible(simulation.radarEnabled);
    cockpit.radarHeadingBox.SetValue(headingText);
    cockpit.radarSpeedBox.SetValue(speedText);
    cockpit.radarStatusBox.SetValue(simulation.radarEnabled ? std::string {"SEARCH"} : std::string {"STANDBY"});
    cockpit.radarStatusBox.SetColor(simulation.radarEnabled ? kRadarSearch : kRadarStandby);
    cockpit.radarOffOverlay.SetVisible(!simulation.radarEnabled);
    cockpit.SetStatusCaption(statusCaption);

    for (std::size_t index = 0; index < simulation.radarContacts.size(); ++index)
    {
        const float headingRadians = simulation.headingDegrees * kDegreesToRadians;
        const float cosine = std::cos(headingRadians);
        const float sine = std::sin(headingRadians);
        constexpr float kRadarRangeWorldUnits = 10.5f;
        constexpr float kRadarRadius = 0.33f;
        const CockpitTargetSeed& target = kCockpitTargets[index];
        mockup_ui::CockpitRadarContactDynamicReticle& contact = *simulation.radarContacts[index];

        const float orbitAngle = simulation.elapsedSeconds * target.orbitRate + target.orbitPhase;
        const float worldX = target.baseX + std::cos(orbitAngle) * target.orbitRadius;
        const float worldY = target.baseY + std::sin(orbitAngle) * target.orbitRadius;
        const float deltaX = worldX - simulation.ownshipX;
        const float deltaY = worldY - simulation.ownshipY;
        const float right = deltaX * cosine - deltaY * sine;
        const float forward = deltaX * sine + deltaY * cosine;
        const float distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
        const bool visible = simulation.radarEnabled && distance <= kRadarRangeWorldUnits;
        const float normalizedX = std::clamp(right / kRadarRangeWorldUnits, -1.0f, 1.0f) * kRadarRadius;
        const float normalizedY = std::clamp(forward / kRadarRangeWorldUnits, -1.0f, 1.0f) * kRadarRadius;
        const float contactHeadingDegrees = std::atan2(right, forward) * kRadiansToDegrees;

        contact.SetVisible(visible);
        contact.SetPosition(mfd::Vec2 {kCockpitRadarCenterX + normalizedX, kCockpitRadarCenterY + normalizedY});
        contact.SetRotationDegrees(contactHeadingDegrees);
        contact.SetColor(target.color);
        contact.ContactLabel().SetText(std::string {target.label});

        if (target.blink && simulation.radarEnabled)
        {
            contact.Blink = cockpit.threat;
        }
        else
        {
            contact.Blink = nullptr;
        }
    }

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

        const ExampleConfig config = LoadExampleConfig();
        mfd::CommandClient client(config.transport, config.generatedTransportMap);
        if (!client.IsReady())
        {
            throw std::runtime_error("Unable to create the cockpit UDP client: " + client.LastError());
        }

        mfd::client::LatestBatchPublisher publisher(config.transport);
        if (!publisher.IsReady())
        {
            throw std::runtime_error("Unable to create the realtime cockpit publisher: " + publisher.LastError());
        }

        mockup_ui::CockpitMockupUi ui;
        Require(ui.SendStartup(client,
                               config.cockpitView,
                               std::string {"CLI API booting | waiting for first dummy batch"}),
                client,
                "Unable to prime the cockpit mockup UI");

        CockpitSimulationState simulation;

        std::cout << "client_mockup_minimal\n";
        std::cout << "Window JSON: " << mockup_ui::CockpitMockupUi::WindowFile() << '\n';
        std::cout << "UDP target: " << config.transport.address << ':' << config.transport.port << '\n';
        std::cout << "Driving page '" << mockup_ui::CockpitMockupPage::Name() << "' every " << kSimulationTick.count()
                  << " ms from one plain main loop.\n";
        std::cout << "Start Scripts\\Start-MfdCockpit.bat or the staged Start-MfdCockpit.bat separately and press Ctrl+C here to stop.\n";

        using clock = std::chrono::steady_clock;
        auto nextTick = clock::now();
        auto lastReport = nextTick;

        while (gStopRequested == 0)
        {
            if (const std::string publisherError = publisher.LastError(); !publisherError.empty())
            {
                throw std::runtime_error("Unable to send the cockpit batch: " + publisherError);
            }

            StepCockpitSimulation(simulation, kSimulationTickSeconds);
            PopulateCockpitBatch(ui, simulation);
            if (!ui.SubmitLatest(publisher, simulation.sequence))
            {
                throw std::runtime_error("Unable to queue the cockpit batch: " + publisher.LastError());
            }

            const auto now = clock::now();
            if (now - lastReport >= std::chrono::seconds(1))
            {
                lastReport = now;
                std::cout << "seq=" << simulation.sequence
                          << " hdg=" << FormatHeadingValue(simulation.headingDegrees)
                          << " pitch=" << FormatSignedFloat(simulation.pitchDegrees, "%+04.1f")
                          << " bank=" << FormatSignedFloat(simulation.bankDegrees, "%+03.0f")
                          << " speed=" << FormatSpeedValue(simulation.speedKts)
                          << " radar=" << (simulation.radarEnabled ? "EMIT" : "STBY")
                          << '\n';
            }

            ++simulation.sequence;
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

        if (!ui.SubmitShutdown(
                publisher,
                simulation.sequence,
                "CLI API stopped | restart client_mockup_minimal"))
        {
            throw std::runtime_error("Unable to queue the cockpit shutdown batch: " + publisher.LastError());
        }

        publisher.Flush();
        if (const std::string publisherError = publisher.LastError(); !publisherError.empty())
        {
            throw std::runtime_error("Unable to send the cockpit shutdown batch: " + publisherError);
        }

        std::cout << "client_mockup_minimal stopped.\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    }
}
