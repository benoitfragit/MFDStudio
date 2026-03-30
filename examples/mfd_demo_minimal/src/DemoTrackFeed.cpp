/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "DemoTrackFeed.h"

#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace mfd
{
namespace
{
constexpr std::string_view kRadarPageName = "Radar";
}

void DemoTrackFeed::Update(const float elapsedSeconds, SceneRegistry& scene)
{
    if (!scene.HasPage(kRadarPageName))
    {
        return;
    }

    const auto templateIterator = scene.Library().find("radar_track");
    if (templateIterator == scene.Library().end())
    {
        return;
    }

    static const std::array<ColorRgba, 5> trackColors {
        ColorRgba {77, 224, 255, 255},
        ColorRgba {255, 214, 102, 255},
        ColorRgba {120, 255, 154, 255},
        ColorRgba {255, 144, 112, 255},
        ColorRgba {215, 182, 255, 255}};

    const int trackCount = 3 + static_cast<int>((std::sin(elapsedSeconds * 0.45f) + 1.0f) * 1.5f);

    for (int index = 0; index < trackCount; ++index)
    {
        const float angle = elapsedSeconds * 0.55f + static_cast<float>(index) * 1.18f;
        const float radius = 0.1979f + static_cast<float>(index) * 0.1f;

        Transform2D transform;
        transform.position = {
            std::cos(angle) * radius,
            std::sin(angle) * radius};
        transform.rotationDegrees = -angle * 57.2957795f;

        ReticleStyleOverride overrides;
        overrides.color = trackColors[static_cast<std::size_t>(index) % trackColors.size()];
        overrides.thickness = 0.0042f;

        ReticleGroup track = InstantiateReticle(templateIterator->second,
                                                "dynamic_track_" + std::to_string(index + 1),
                                                transform,
                                                overrides);

        const std::string trackCode = "T" + std::to_string(11 + index);
        track.info.label = "Track " + trackCode;
        track.info.category = "track";
        track.info.metadata["trackId"] = trackCode;
        track.info.metadata["bearingDegrees"] = std::to_string(static_cast<int>(std::lround(angle * 57.2957795f)));
        track.info.metadata["rangeNormalized"] = std::to_string(radius);
        track.blink.enabled = true;
        track.blink.typeName = (index % 3 == 0) ? "fast" : "caution";

        SetTextPrimitive(track, "track_label", trackCode);
        scene.UpsertDynamicReticle(kRadarPageName, std::move(track));
    }

    for (int index = trackCount; index < previousTrackCount_; ++index)
    {
        scene.RemoveDynamicReticle(kRadarPageName, "dynamic_track_" + std::to_string(index + 1));
    }

    previousTrackCount_ = trackCount;
}
} // namespace mfd
