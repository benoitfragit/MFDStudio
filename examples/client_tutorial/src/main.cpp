/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include <array>
#include <chrono>
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

#if __has_include("MfdTutorialMockupUi.h")
#include "MfdTutorialMockupUi.h"
#define MFD_HAS_GENERATED_TUTORIAL_UI 1
using GeneratedTutorialUi = mockup_ui::MfdTutorialMockupUi;
#elif __has_include("TutorialUi.h")
#include "TutorialUi.h"
#define MFD_HAS_GENERATED_TUTORIAL_UI 1
using GeneratedTutorialUi = tutorial_ui::TutorialUi;
#else
#define MFD_HAS_GENERATED_TUTORIAL_UI 0
#endif

namespace
{
constexpr std::string_view kWindowFile = "assets/windows/mfd_tutorial.json";
constexpr std::string_view kPage1 = "Page1";
constexpr std::string_view kPage2 = "Page2";
constexpr std::string_view kTrackTemplate = "mfd_tutorial_radar_track";
constexpr std::size_t kMaxTracks = 10;
constexpr auto kTrackInterval = std::chrono::seconds(2);
constexpr auto kPageSwitchInterval = std::chrono::seconds(30);

int mainImpl()
{
    mfd::JsonLoader loader;
    const mfd::LoadedWindowConfiguration loaded = loader.LoadWindowConfiguration(std::string(kWindowFile));
    if (!loaded.window.commandTransports.udp.has_value())
    {
        throw std::runtime_error("Tutorial window must expose an UDP command transport");
    }

    mfd::CommandClient client(*loaded.window.commandTransports.udp);
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

    std::vector<std::string> trackIds;
    trackIds.reserve(kMaxTracks);

#if MFD_HAS_GENERATED_TUTORIAL_UI
    GeneratedTutorialUi generatedUi;
    auto& generatedDynamicTracks = generatedUi.Page1().Dynamic(kTrackTemplate);
    bool generatedDeclutterVisible = true;
#endif

    std::string activePage(kPage1);
    client.ActivatePage(activePage);

    auto nextTrackTime = std::chrono::steady_clock::now() + kTrackInterval;
    auto nextPageTime = std::chrono::steady_clock::now() + kPageSwitchInterval;
    std::uint32_t serial = 1;

    while (true)
    {
        const auto now = std::chrono::steady_clock::now();

        if (now >= nextPageTime)
        {
            activePage = (activePage == kPage1) ? std::string(kPage2) : std::string(kPage1);
            client.ActivatePage(activePage);
            nextPageTime += kPageSwitchInterval;
        }

        if (now >= nextTrackTime)
        {
            if (trackIds.size() >= kMaxTracks)
            {
                client.RemoveDynamicReticle(kPage1, trackIds.front());
                trackIds.erase(trackIds.begin());
            }

            const std::string trackId = "tutorial_track_" + std::to_string(serial++);
            mfd::ReticlePatch patch;
            patch.position = mfd::Vec2 {axis(rng), axis(rng)};
            patch.color = mfd::ColorRgba {80, 255, 185, 255};
            patch.thickness = 0.0038f;
            patch.text = std::string("T") + std::to_string(serial);

#if MFD_HAS_GENERATED_TUTORIAL_UI
            auto& generatedTrack = generatedDynamicTracks.Upsert(trackId);
            generatedTrack.SetPosition(*patch.position);
            generatedTrack.SetColor(*patch.color);
            generatedTrack.SetThickness(*patch.thickness);
            generatedTrack.SetText(*patch.text);
            if ((serial % 5U) == 0U)
            {
                generatedDeclutterVisible = !generatedDeclutterVisible;
            }
            generatedDynamicTracks.SetVisible(generatedDeclutterVisible);
            const auto commands = generatedUi.BuildBatch();
            if (!commands.empty())
            {
                client.SendBatch(commands);
            }
#else
            client.UpsertDynamicReticle(kPage1, trackId, kTrackTemplate, patch);
#endif
            trackIds.push_back(trackId);

            // Tutorial strobe behavior: move strobe to each new track.
            client.SetStrobeActive(kPage1, true);
            client.SetStrobePosition(kPage1, *patch.position);

            nextTrackTime += kTrackInterval;
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
