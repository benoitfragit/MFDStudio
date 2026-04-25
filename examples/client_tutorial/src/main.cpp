/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Executable entry point.
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
#include "TutorialUi.h"

namespace
{
constexpr std::string_view kWindowFile = "assets/windows/mfd_tutorial.json";
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
    auto& generatedDynamicTracks = page1.DynamicMfdTutorialRadarTrack();
    auto& page1Circle = page1.mfdTutorialCircle;
    auto& page1Strobe = page1.strobe;
    std::vector<tutorial_ui::MfdTutorialRadarTrackDynamicReticle*> generatedTracks;
    generatedTracks.reserve(kMaxTracks);
    bool generatedDeclutterVisible = true;

    bool page1Active = true;
    if (!client.ActivatePage(page1))
    {
        throw std::runtime_error("Unable to activate tutorial Page1: " + client.LastError());
    }

    auto nextTrackTime = std::chrono::steady_clock::now() + kTrackInterval;
    auto nextPageTime = std::chrono::steady_clock::now() + kPageSwitchInterval;
    std::uint32_t serial = 1;

    while (true)
    {
        const auto now = std::chrono::steady_clock::now();

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
                generatedDynamicTracks.Remove(*generatedTracks.front());
                generatedTracks.erase(generatedTracks.begin());
            }

            const std::uint32_t trackSerial = serial++;
            const mfd::Vec2 trackPosition {axis(rng), axis(rng)};
            const float trackSize = 0.18f + 0.01f * static_cast<float>(trackSerial % 3U);

            auto& generatedTrack = generatedDynamicTracks.Create();
            generatedTrack.SetPosition(trackPosition);
            generatedTrack.SetColor({80, 255, 185, 255});
            generatedTrack.SetThickness(0.0038f);
            generatedTrack.Primitive01().SetSize({trackSize, trackSize});
            generatedTrack.Primitive01().SetRotationDegrees(static_cast<float>((trackSerial % 8U) * 12U));

            if ((trackSerial % 5U) == 0U)
            {
                generatedDeclutterVisible = !generatedDeclutterVisible;
            }

            generatedDynamicTracks.SetVisible(generatedDeclutterVisible);

            page1Circle.SetVisible(true);
            page1Circle.SetColor(
                generatedDeclutterVisible ? mfd::ColorRgba {0, 255, 128, 255} : mfd::ColorRgba {0, 96, 48, 255});
            page1Circle.Primitive01().SetRadius(0.42f + 0.015f * static_cast<float>(generatedTracks.size() + 1U));
            page1Circle.Primitive01().SetThickness(0.0045f);

            if (page1Strobe.IsValid())
            {
                page1Strobe.SetActive(true);
                page1Strobe.SetPosition(trackPosition);
            }

            const auto commands = generatedUi.BuildBatch();
            if (!commands.empty())
            {
                client.SendBatch(commands);
            }
            generatedTracks.push_back(&generatedTrack);

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
