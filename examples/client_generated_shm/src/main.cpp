/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Minimal SHM client example using generated UI and generated SHM publisher.
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <thread>

#include "CockpitShmGenerated.h"
#include "CockpitUiGenerated.h"
#include "mfd/io/JsonLoader.h"

namespace
{
constexpr std::chrono::milliseconds kTick {20};

void FillLabel(char* destination, const char* source)
{
    std::memset(destination, 0, 16U);
    const std::size_t sourceLen = std::strlen(source);
    const std::size_t copyLen = std::min<std::size_t>(15U, sourceLen);
    std::memcpy(destination, source, copyLen);
}
} // namespace

int main()
{
    try
    {
        mfd::JsonLoader loader;
        const auto loaded = loader.LoadWindowConfiguration(std::string(cockpit_shm_generated::CockpitShmUi::WindowFile()));
        if (!loaded.window.commandTransports.shm.has_value() || !loaded.window.commandTransports.shm->enabled)
        {
            std::cerr << "Window JSON must expose an enabled commands.shm transport.\n";
            return 1;
        }

        cockpit_shm_generated::ShmClientPublisher publisher;
        if (!publisher.Initialize(*loaded.window.commandTransports.shm))
        {
            std::cerr << "Unable to initialize SHM publisher.\n";
            return 1;
        }

        cockpit_shm_generated::CockpitShmUi ui;
        ui.Reset();

        float elapsed = 0.0F;
        for (std::uint32_t sequence = 0U; sequence < 500U; ++sequence)
        {
            elapsed += 0.02F;

            mfd::RadarFrame frame;
            frame.count = 3U;

            frame.tracks[0].id = 1U;
            frame.tracks[0].x = std::sin(elapsed) * 8.0F;
            frame.tracks[0].y = std::cos(elapsed) * 8.0F;
            frame.tracks[0].heading = elapsed * 10.0F;
            FillLabel(frame.tracks[0].label, "TRK01");

            frame.tracks[1].id = 2U;
            frame.tracks[1].x = std::sin(elapsed * 0.7F + 1.0F) * 6.0F;
            frame.tracks[1].y = std::cos(elapsed * 0.7F + 1.0F) * 6.0F;
            frame.tracks[1].heading = elapsed * 18.0F;
            FillLabel(frame.tracks[1].label, "TRK02");

            frame.tracks[2].id = 3U;
            frame.tracks[2].x = std::sin(elapsed * 1.2F + 2.0F) * 4.0F;
            frame.tracks[2].y = std::cos(elapsed * 1.2F + 2.0F) * 4.0F;
            frame.tracks[2].heading = elapsed * 32.0F;
            FillLabel(frame.tracks[2].label, "TRK03");

            if (!publisher.PublishRadarFrame(frame))
            {
                std::cerr << "Failed to publish SHM frame.\n";
                return 1;
            }

            std::this_thread::sleep_for(kTick);
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
