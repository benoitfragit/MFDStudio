/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Executable entry point.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "mfd/control/CommandClient.h"
#include "mfd/control/FeedbackTransport.h"
#include "mfd/control/StrobeFeedback.h"
#include "mfd/io/JsonLoader.h"
#include "TutorialUi.h"

namespace
{
constexpr std::string_view kWindowFile = "assets/windows/mfd_tutorial.json";
constexpr std::size_t kMaxTracks = 10;
constexpr auto kTrackInterval = std::chrono::seconds(2);
constexpr auto kPageSwitchInterval = std::chrono::seconds(30);
constexpr float kProgressBarMaxWidth = 0.44f;
constexpr float kProgressBarHeight = 0.06f;
constexpr float kProgressBarLeftEdge = -0.22f;
constexpr float kProgressLoopDurationSeconds = 6.0f;
constexpr float kTrackBoundsMin = -0.88f;
constexpr float kTrackBoundsMax = 0.88f;
constexpr float kPersistentTrackSize = 0.205f;
constexpr float kPersistentTrackThickness = 0.0044f;
constexpr float kPersistentLinkThickness = 0.0034f;
constexpr float kMaxFrameDeltaSeconds = 0.1f;

/**
 * @brief Runtime state for one persistent tutorial track bouncing inside the page bounds.
 */
struct PersistentTrackState
{
    tutorial_ui::MfdTutorialRadarTrackDynamicReticle* reticle = nullptr;
    mfd::Vec2 position {};
    mfd::Vec2 velocity {};
};

/**
 * @brief Mirrors one axis on the tutorial bounds to keep the track inside the visible page.
 * @param position Current axis position updated in place.
 * @param velocity Current axis velocity updated in place when a rebound occurs.
 */
void ReflectOnBounds(float& position, float& velocity)
{
    if (position < kTrackBoundsMin)
    {
        position = kTrackBoundsMin + (kTrackBoundsMin - position);
        velocity = std::abs(velocity);
    }
    else if (position > kTrackBoundsMax)
    {
        position = kTrackBoundsMax - (position - kTrackBoundsMax);
        velocity = -std::abs(velocity);
    }
}

/**
 * @brief Advances one persistent track with a constant velocity and rebound.
 * @param track Persistent track state to update.
 * @param deltaSeconds Elapsed frame time in seconds.
 */
void AdvancePersistentTrack(PersistentTrackState& track, const float deltaSeconds)
{
    track.position.x += track.velocity.x * deltaSeconds;
    track.position.y += track.velocity.y * deltaSeconds;
    ReflectOnBounds(track.position.x, track.velocity.x);
    ReflectOnBounds(track.position.y, track.velocity.y);
}

/**
 * @brief Applies one visibility state to the transient radar-track list only.
 * @param transientTracks Current FIFO list of transient generated tracks.
 * @param visible Desired visibility for the transient list.
 */
void SetTransientTrackVisibility(
    const std::vector<tutorial_ui::MfdTutorialRadarTrackDynamicReticle*>& transientTracks,
    const bool visible)
{
    for (tutorial_ui::MfdTutorialRadarTrackDynamicReticle* track : transientTracks)
    {
        if (track != nullptr)
        {
            track->SetVisible(visible);
        }
    }
}

/**
 * @brief Updates the dynamic polyline used to link the two persistent tracks.
 * @param link Dynamic polyline reticle connecting the persistent tracks.
 * @param first First persistent track.
 * @param second Second persistent track.
 */
void UpdatePersistentTrackLink(tutorial_ui::InspiredSteeringCueDynamicReticle& link,
                               const PersistentTrackState& first,
                               const PersistentTrackState& second)
{
    std::vector<mfd::Vec2> points;
    points.reserve(2);
    points.push_back(first.position);
    points.push_back(second.position);

    link.SetPosition({0.0f, 0.0f});
    link.Cue().SetClosed(false);
    link.Cue().SetPoints(std::move(points));
}

int mainImpl()
{
    mfd::JsonLoader loader;
    const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(std::string(kWindowFile));
    if (!loaded.window.commandTransports.udp.has_value())
    {
        throw std::runtime_error("Tutorial window must expose an UDP command transport");
    }
    if (!loaded.generatedTransportMap.has_value())
    {
        throw std::runtime_error("Tutorial window must expose a generated transport map next to the JSON file");
    }

    mfd::CommandClient client(*loaded.window.commandTransports.udp, loaded.generatedTransportMap);
    if (!client.IsReady())
    {
        throw std::runtime_error("Unable to initialize command client: " + client.LastError());
    }

    std::unique_ptr<mfd::IExchangeChannel> feedbackChannel;
    if (loaded.window.feedbackTransports.udp.has_value())
    {
        feedbackChannel = mfd::CreateFeedbackReceiverChannel(*loaded.window.feedbackTransports.udp);
    }

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> axis(-0.85f, 0.85f);

    tutorial_ui::TutorialUi generatedUi;
    auto& page1 = generatedUi.Page1();
    auto& page2 = generatedUi.Page2();
    auto& page1TrackSet = page1.DynamicMfdTutorialRadarTrack();
    auto& persistentTrackLinkSet = page1.DynamicInspiredSteeringCue();
    auto& page1Circle = page1.mfdTutorialCircle;
    auto& page2ProgressBar = page2.mfdTutorialProgressBar;
    auto& progressFill = page2ProgressBar.FillBar();
    auto& page1Strobe = page1.strobe;
    std::vector<tutorial_ui::MfdTutorialRadarTrackDynamicReticle*> generatedTracks;
    generatedTracks.reserve(kMaxTracks);
    bool generatedDeclutterVisible = true;
    page2ProgressBar.SetVisible(true);
    progressFill.SetVisible(true);

    bool page1Active = true;
    if (!client.ActivatePage(page1))
    {
        throw std::runtime_error("Unable to activate tutorial Page1: " + client.LastError());
    }

    std::array<PersistentTrackState, 2> persistentTracks {};
    persistentTracks[0].reticle = &page1TrackSet.Create();
    persistentTracks[0].position = {-0.58f, -0.26f};
    persistentTracks[0].velocity = {0.31f, 0.22f};
    persistentTracks[1].reticle = &page1TrackSet.Create();
    persistentTracks[1].position = {0.52f, 0.41f};
    persistentTracks[1].velocity = {-0.27f, -0.24f};

    for (PersistentTrackState& persistentTrack : persistentTracks)
    {
        persistentTrack.reticle->SetVisible(true);
        persistentTrack.reticle->SetPosition(persistentTrack.position);
        persistentTrack.reticle->SetColor({255, 224, 96, 255});
        persistentTrack.reticle->SetThickness(kPersistentTrackThickness);
        persistentTrack.reticle->Primitive01().SetSize({kPersistentTrackSize, kPersistentTrackSize});
        persistentTrack.reticle->Primitive01().SetLineStyle(mfd::LineStyle::Solid);
    }

    auto& persistentTrackLink = persistentTrackLinkSet.Create();
    persistentTrackLink.SetVisible(true);
    persistentTrackLink.SetColor({255, 192, 96, 255});
    persistentTrackLink.SetThickness(kPersistentLinkThickness);
    persistentTrackLink.Cue().SetLineStyle(mfd::LineStyle::Solid);
    UpdatePersistentTrackLink(persistentTrackLink, persistentTracks[0], persistentTracks[1]);

    auto nextTrackTime = std::chrono::steady_clock::now() + kTrackInterval;
    auto nextPageTime = std::chrono::steady_clock::now() + kPageSwitchInterval;
    const auto progressStartTime = std::chrono::steady_clock::now();
    auto previousFrameTime = progressStartTime;
    std::uint32_t serial = 1;

    while (true)
    {
        const auto now = std::chrono::steady_clock::now();
        const float deltaSeconds =
            std::min(std::chrono::duration<float>(now - previousFrameTime).count(), kMaxFrameDeltaSeconds);
        previousFrameTime = now;
        const float elapsedProgressSeconds =
            std::chrono::duration<float>(now - progressStartTime).count();
        const float progressPhase =
            std::fmod(elapsedProgressSeconds, kProgressLoopDurationSeconds) / kProgressLoopDurationSeconds;
        const float progressWidth = std::max(0.001f, kProgressBarMaxWidth * progressPhase);
        const float progressCenterX = kProgressBarLeftEdge + progressWidth * 0.5f;

        for (PersistentTrackState& persistentTrack : persistentTracks)
        {
            AdvancePersistentTrack(persistentTrack, deltaSeconds);
            persistentTrack.reticle->SetPosition(persistentTrack.position);
        }
        UpdatePersistentTrackLink(persistentTrackLink, persistentTracks[0], persistentTracks[1]);

        progressFill.SetSize({progressWidth, kProgressBarHeight});
        progressFill.SetPosition({progressCenterX, 0.0f});

        if (now >= nextPageTime)
        {
            page1Active = !page1Active;
            const bool activated = page1Active ? client.ActivatePage(page1) : client.ActivatePage(page2);
            if (!activated)
            {
                throw std::runtime_error("Unable to activate generated tutorial page: " + client.LastError());
            }
            nextPageTime += kPageSwitchInterval;
        }

        if (now >= nextTrackTime)
        {
            if (generatedTracks.size() >= kMaxTracks)
            {
                page1TrackSet.Remove(*generatedTracks.front());
                generatedTracks.erase(generatedTracks.begin());
            }

            const std::uint32_t trackSerial = serial++;
            const mfd::Vec2 trackPosition {axis(rng), axis(rng)};
            const float trackSize = 0.18f + 0.01f * static_cast<float>(trackSerial % 3U);

            auto& generatedTrack = page1TrackSet.Create();
            generatedTrack.SetPosition(trackPosition);
            generatedTrack.SetColor({80, 255, 185, 255});
            generatedTrack.SetThickness(0.0038f);
            generatedTrack.Primitive01().SetSize({trackSize, trackSize});
            generatedTrack.Primitive01().SetRotationDegrees(static_cast<float>((trackSerial % 8U) * 12U));
            generatedTrack.Primitive01().SetLineStyle(
                (trackSerial % 2U) == 0U ? mfd::LineStyle::Dashed : mfd::LineStyle::Dotted);

            if ((trackSerial % 5U) == 0U)
            {
                generatedDeclutterVisible = !generatedDeclutterVisible;
                SetTransientTrackVisibility(generatedTracks, generatedDeclutterVisible);
            }

            generatedTrack.SetVisible(generatedDeclutterVisible);

            page1Circle.SetVisible(true);
            page1Circle.SetColor(
                generatedDeclutterVisible ? mfd::ColorRgba {0, 255, 128, 255} : mfd::ColorRgba {0, 96, 48, 255});
            page1Circle.Primitive01().SetRadius(0.42f + 0.015f * static_cast<float>(generatedTracks.size() + 1U));
            page1Circle.Primitive01().SetThickness(0.0045f);
            page1Circle.Primitive01().SetLineStyle(
                generatedDeclutterVisible ? mfd::LineStyle::Solid : mfd::LineStyle::Dotted);

            if (page1Strobe.IsValid())
            {
                page1Strobe.SetActive(true);
                page1Strobe.SetPosition(trackPosition);
            }
            generatedTracks.push_back(&generatedTrack);

            nextTrackTime += kTrackInterval;
        }

        const auto commands = generatedUi.BuildBatch();
        if (!commands.empty())
        {
            client.SendBatch(commands);
        }

        if (feedbackChannel)
        {
            const auto payload = feedbackChannel->TryReceive();
            if (payload.has_value())
            {
                const std::string_view raw(reinterpret_cast<const char*>(payload->data()), payload->size());
                std::string error;
                const auto feedback = mfd::DeserializeStrobeStatusFeedback(raw, &error);
                if (feedback.has_value())
                {
                    std::cout << "Strobe feedback: page=" << feedback->pageName
                              << " active=" << (feedback->active ? "true" : "false") << '\n';
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
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
        std::cerr << "client_tutorial error: " << exception.what() << '\n';
        return 1;
    }
}
