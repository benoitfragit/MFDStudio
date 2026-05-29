/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Compile-time and runtime integration coverage for the actual generated client API fixture.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "GeneratedUiFixture.h"
#include "mfd/client/LatestBatchPublisher.h"
#include "mfd/control/CommandTypes.h"

namespace
{
template <typename Command>
const Command* FindCommand(const std::vector<mfd::UserCommand>& commands)
{
    const auto iterator = std::find_if(
        commands.begin(),
        commands.end(),
        [](const mfd::UserCommand& command)
        {
            return std::get_if<Command>(&command) != nullptr;
        });
    return iterator == commands.end() ? nullptr : std::get_if<Command>(&(*iterator));
}

bool HasLinePatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.lineStart.has_value() && patch.lineEnd.has_value();
}

bool HasCirclePatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.radius.has_value();
}

bool HasRingPatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.innerRadius.has_value() && patch.outerRadius.has_value();
}

bool HasRectanglePatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.width.has_value() && patch.height.has_value() && patch.size.has_value();
}

bool HasEllipsePatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.width.has_value() && patch.height.has_value() && !patch.size.has_value();
}

bool HasSquarePatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.size.has_value() && !patch.width.has_value() && !patch.height.has_value();
}

bool HasDiamondPatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.width.has_value() && patch.height.has_value() && !patch.size.has_value();
}

bool HasTrianglePatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.points.has_value() && patch.points->size() == 3U;
}

bool HasPolylinePatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.points.has_value() && patch.points->size() >= 2U && patch.closed.has_value();
}

bool HasBezierPatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.points.has_value() && patch.points->size() >= 2U && patch.segments.has_value() &&
           !patch.startAngleDegrees.has_value() && !patch.endAngleDegrees.has_value();
}

bool HasArcPatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.radius.has_value() && patch.startAngleDegrees.has_value() &&
           patch.endAngleDegrees.has_value() && patch.segments.has_value();
}

bool HasImagePatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.visible.has_value() && patch.position.has_value() &&
           patch.rotationDegrees.has_value() && patch.scale.has_value();
}

bool HasFillPatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.fillColor.has_value() && patch.filled.has_value();
}

bool HasTimePatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.letterSpacing.has_value();
}

template <typename Handle, typename = void>
struct HasSetFillColor : std::false_type
{
};

template <typename Handle>
struct HasSetFillColor<Handle, std::void_t<decltype(std::declval<Handle&>().SetFillColor(mfd::ColorRgba {}))>> :
    std::true_type
{
};

template <typename Handle, typename = void>
struct HasSetFilled : std::false_type
{
};

template <typename Handle>
struct HasSetFilled<Handle, std::void_t<decltype(std::declval<Handle&>().SetFilled(true))>> : std::true_type
{
};
} // namespace

TEST(GeneratedUiCompiledApiTests, GeneratedFixtureBuildsIdBasedCommandsFromRealGeneratedClasses)
{
    static_assert(std::is_same_v<generated_ui_fixture::RadarMockupPage::MfdGeneratedPageTag,
                                 mfd::CommandClient::GeneratedPageTag>);
    static_assert(generated_ui_fixture::RadarMockupPage::GeneratedId() != 0U);
    static_assert(generated_ui_fixture::RadarMockupPage::MappingHash() ==
                  generated_ui_fixture::GeneratedUiFixture::MappingHash());
    static_assert(!std::is_convertible_v<generated_ui_fixture::RadarGeometryPanelReticle*, mfd::client::Reticle*>);
    static_assert(!std::is_convertible_v<generated_ui_fixture::RadarStrobe1StrobeReticle*, mfd::client::Reticle*>);
    static_assert(
        !std::is_convertible_v<generated_ui_fixture::GeometryTemplateDynamicReticle*, mfd::client::DynamicReticle*>);
    static_assert(!std::is_convertible_v<generated_ui_fixture::GeometryTemplateDynamicReticleSet*,
                                         mfd::client::GeneratedDynamicReticleSet*>);

    generated_ui_fixture::GeneratedUiFixture ui;
    ui.Window().SetBrightness(0.55f);

    auto& radar = ui.Radar();
    EXPECT_EQ(radar.GeneratedId(), generated_ui_fixture::RadarMockupPage::GeneratedId());
    radar.strobe = radar.strobe1;
    radar.strobe.SetActive(true);
    radar.strobe.SetPosition({0.25f, -0.10f});
    radar.defaultReticle.CursorLine().SetLineStyle(generated_ui_fixture::LineStyle::Dotted);
    radar.strobe1Reticle.SetRotationDegrees(18.0f);
    radar.strobe1Reticle.SetScale({1.15f, 0.90f});
    radar.strobe1Reticle.StrobeLabel().SetText("ALT-1");
    radar.radarStatus.SetValue("LOCK");
    radar.geometryPanel.HeadingLine().SetLineStyle(generated_ui_fixture::LineStyle::Dashed);
    radar.geometryPanel.HeadingLine().SetStart({-0.5f, 0.0f});
    radar.geometryPanel.HeadingLine().SetEnd({0.5f, 0.0f});
    radar.geometryPanel.CursorCircle().SetRadius(0.07f);
    radar.geometryPanel.ScopeRing().SetInnerRadius(0.15f);
    radar.geometryPanel.ScopeRing().SetOuterRadius(0.21f);
    radar.geometryPanel.ScopeRing().SetSegments(40);
    radar.geometryPanel.LockBox().SetSize({0.32f, 0.14f});
    radar.geometryPanel.LockBox().SetWidth(0.30f);
    radar.geometryPanel.LockBox().SetHeight(0.12f);
    radar.geometryPanel.LockBox().SetFillColor({1, 2, 3, 180});
    radar.geometryPanel.LockBox().SetFilled(true);
    radar.geometryPanel.WarningTriangle().SetPoints(
        std::array<mfd::Vec2, 3> {{{-0.22f, -0.08f}, {0.0f, 0.22f}, {0.20f, -0.08f}}});
    radar.geometryPanel.RoutePolyline().SetPoints({{-0.24f, -0.06f}, {-0.08f, 0.14f}, {0.12f, 0.10f}, {0.24f, -0.02f}});
    radar.geometryPanel.RoutePolyline().SetClosed(true);
    radar.geometryPanel.UncertaintyEllipse().SetWidth(0.40f);
    radar.geometryPanel.UncertaintyEllipse().SetHeight(0.20f);
    radar.geometryPanel.TargetSquare().SetSize({0.11f, 0.11f});
    radar.geometryPanel.SteerDiamond().SetWidth(0.18f);
    radar.geometryPanel.SteerDiamond().SetHeight(0.24f);
    radar.geometryPanel.GuideBezier().SetControlPoints({{-0.22f, -0.12f}, {-0.06f, 0.18f}, {0.08f, 0.18f}, {0.22f, -0.02f}});
    radar.geometryPanel.GuideBezier().SetSegments(18);
    radar.geometryPanel.ScanArc().SetRadius(0.19f);
    radar.geometryPanel.ScanArc().SetStartAngleDegrees(-60.0f);
    radar.geometryPanel.ScanArc().SetEndAngleDegrees(120.0f);
    radar.geometryPanel.ScanArc().SetSegments(28);
    radar.geometryPanel.PanelImage().SetVisible(true);
    radar.geometryPanel.PanelImage().SetPosition({0.28f, -0.14f});
    radar.geometryPanel.PanelImage().SetScale({1.25f, 0.85f});
    radar.geometryPanel.PanelImage().SetRotationDegrees(17.0f);
    radar.geometryPanel.MissionTime().SetLetterSpacing(0.02f);

    auto& track = radar.DynamicGeometryTemplate().Create();
    track.SetPosition({0.10f, -0.20f});
    track.SetBlinkType(radar.slow);
    track.TrackLabel().SetText("T01");
    track.HeadingLine().SetLineStyle(generated_ui_fixture::LineStyle::Dotted);
    track.HeadingLine().SetStart({-0.25f, 0.15f});
    track.HeadingLine().SetEnd({0.25f, 0.15f});
    track.CursorCircle().SetRadius(0.05f);
    track.ScopeRing().SetInnerRadius(0.08f);
    track.ScopeRing().SetOuterRadius(0.14f);
    track.ScopeRing().SetSegments(32);
    track.LockBox().SetSize({0.16f, 0.10f});
    track.LockBox().SetWidth(0.15f);
    track.LockBox().SetHeight(0.09f);
    track.WarningTriangle().SetPoints(
        std::array<mfd::Vec2, 3> {{{-0.12f, -0.06f}, {0.0f, 0.12f}, {0.11f, -0.05f}}});
    track.RoutePolyline().SetPoints({{-0.16f, -0.04f}, {-0.04f, 0.08f}, {0.12f, 0.02f}});
    track.RoutePolyline().SetClosed(true);
    track.UncertaintyEllipse().SetWidth(0.20f);
    track.UncertaintyEllipse().SetHeight(0.12f);
    track.TargetSquare().SetSize({0.07f, 0.07f});
    track.SteerDiamond().SetWidth(0.09f);
    track.SteerDiamond().SetHeight(0.13f);
    track.GuideBezier().SetControlPoints({{-0.18f, -0.08f}, {-0.05f, 0.12f}, {0.05f, 0.12f}, {0.18f, -0.02f}});
    track.GuideBezier().SetSegments(16);
    track.ScanArc().SetRadius(0.11f);
    track.ScanArc().SetStartAngleDegrees(-45.0f);
    track.ScanArc().SetEndAngleDegrees(90.0f);
    track.ScanArc().SetSegments(20);
    track.OverlayImage().SetVisible(true);
    track.OverlayImage().SetPosition({0.06f, -0.03f});
    track.OverlayImage().SetScale({0.75f, 1.10f});
    track.OverlayImage().SetRotationDegrees(-12.0f);
    track.MissionTime().SetLetterSpacing(0.03f);

    const mfd::CommandBatch batch = ui.BuildCommandBatch(9U);
    EXPECT_EQ(batch.sequence, 9U);
    EXPECT_EQ(batch.mappingHash, generated_ui_fixture::GeneratedUiFixture::MappingHash());
    ASSERT_EQ(batch.commands.size(), 6U);

    const auto* window = FindCommand<mfd::UpdateWindowDisplayCommand>(batch.commands);
    ASSERT_NE(window, nullptr);
    ASSERT_TRUE(window->patch.brightness.has_value());
    EXPECT_FLOAT_EQ(*window->patch.brightness, 0.55f);

    const auto* strobe = FindCommand<mfd::UpdateStrobeCommand>(batch.commands);
    ASSERT_NE(strobe, nullptr);
    EXPECT_EQ(strobe->page, "Radar");
    EXPECT_NE(strobe->pageId, 0U);
    EXPECT_EQ(strobe->strobeId, radar.strobe1.GeneratedId());
    ASSERT_TRUE(strobe->active.has_value());
    ASSERT_TRUE(strobe->position.has_value());
    EXPECT_TRUE(*strobe->active);
    EXPECT_FLOAT_EQ(strobe->position->x, 0.25f);
    EXPECT_FLOAT_EQ(strobe->position->y, -0.10f);

    std::vector<const mfd::UpdateReticleCommand*> pageReticleUpdates;
    for (const mfd::UserCommand& command : batch.commands)
    {
        if (const auto* update = std::get_if<mfd::UpdateReticleCommand>(&command); update != nullptr)
        {
            pageReticleUpdates.push_back(update);
        }
    }

    ASSERT_EQ(pageReticleUpdates.size(), 3U);
    const auto statusUpdateIt = std::find_if(
        pageReticleUpdates.begin(),
        pageReticleUpdates.end(),
        [](const mfd::UpdateReticleCommand* update)
        {
            return update->target.reticle == "radar_status";
        });
    ASSERT_NE(statusUpdateIt, pageReticleUpdates.end());
    const mfd::UpdateReticleCommand& statusUpdate = **statusUpdateIt;
    EXPECT_EQ(statusUpdate.target.page, "Radar");
    EXPECT_NE(statusUpdate.target.pageId, 0U);
    EXPECT_NE(statusUpdate.target.reticleId, 0U);
    EXPECT_TRUE(statusUpdate.patch.primitivePatches.empty());
    ASSERT_EQ(statusUpdate.patch.primitivePatchesById.size(), 1U);
    EXPECT_EQ(statusUpdate.patch.primitivePatchesById.begin()->second.text, std::optional<std::string> {"LOCK"});

    const auto geometryUpdateIt = std::find_if(
        pageReticleUpdates.begin(),
        pageReticleUpdates.end(),
        [](const mfd::UpdateReticleCommand* update)
        {
            return update->target.reticle == "geometry_panel";
        });
    ASSERT_NE(geometryUpdateIt, pageReticleUpdates.end());
    const mfd::UpdateReticleCommand& geometryUpdate = **geometryUpdateIt;
    EXPECT_TRUE(geometryUpdate.patch.primitivePatches.empty());
    ASSERT_EQ(geometryUpdate.patch.primitivePatchesById.size(), 13U);
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasLinePatch(entry.second) &&
                   entry.second.lineStyle == std::optional<mfd::LineStyle> {mfd::LineStyle::Dashed};
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasCirclePatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasRingPatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasRectanglePatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasFillPatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasEllipsePatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasSquarePatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasDiamondPatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasTrianglePatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasPolylinePatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasBezierPatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasArcPatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasImagePatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasTimePatch(entry.second);
        }));

    const auto activeStrobeReticleUpdateIt = std::find_if(
        pageReticleUpdates.begin(),
        pageReticleUpdates.end(),
        [](const mfd::UpdateReticleCommand* update)
        {
            return update->target.reticle == "radar_strobe_1";
        });
    ASSERT_NE(activeStrobeReticleUpdateIt, pageReticleUpdates.end());
    const mfd::UpdateReticleCommand& activeStrobeReticleUpdate = **activeStrobeReticleUpdateIt;
    EXPECT_EQ(activeStrobeReticleUpdate.target.page, "Radar");
    EXPECT_TRUE(activeStrobeReticleUpdate.patch.rotationDegrees.has_value());
    EXPECT_TRUE(activeStrobeReticleUpdate.patch.scale.has_value());
    ASSERT_EQ(activeStrobeReticleUpdate.patch.primitivePatchesById.size(), 1U);
    EXPECT_EQ(activeStrobeReticleUpdate.patch.primitivePatchesById.begin()->second.text,
              std::optional<std::string> {"ALT-1"});
    EXPECT_TRUE(std::none_of(
        pageReticleUpdates.begin(),
        pageReticleUpdates.end(),
        [](const mfd::UpdateReticleCommand* update)
        {
            return update->target.reticle == "radar_strobe_default";
        }));

    const auto* upsert = FindCommand<mfd::UpsertDynamicReticlesCommand>(batch.commands);
    ASSERT_NE(upsert, nullptr);
    EXPECT_EQ(upsert->page, "Radar");
    EXPECT_NE(upsert->pageId, 0U);
    EXPECT_EQ(upsert->templateId, "geometry_template");
    EXPECT_NE(upsert->templateTransportId, 0U);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    EXPECT_FALSE(upsert->reticles.front().reticleId.empty());
    EXPECT_NE(upsert->reticles.front().runtimeReticleId, 0U);
    ASSERT_TRUE(upsert->reticles.front().patch.position.has_value());
    EXPECT_TRUE(upsert->reticles.front().patch.blinkEnabled.has_value());
    EXPECT_TRUE(upsert->reticles.front().patch.blinkTypeId.has_value());
    EXPECT_FALSE(upsert->reticles.front().patch.blinkType.has_value());
    EXPECT_TRUE(*upsert->reticles.front().patch.blinkEnabled);
    EXPECT_TRUE(upsert->reticles.front().patch.primitivePatches.empty());
    ASSERT_EQ(upsert->reticles.front().patch.primitivePatchesById.size(), 14U);
    EXPECT_TRUE(std::any_of(
        upsert->reticles.front().patch.primitivePatchesById.begin(),
        upsert->reticles.front().patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return entry.second.text == std::optional<std::string> {"T01"};
        }));
    EXPECT_TRUE(std::any_of(
        upsert->reticles.front().patch.primitivePatchesById.begin(),
        upsert->reticles.front().patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasLinePatch(entry.second) &&
                   entry.second.lineStyle == std::optional<mfd::LineStyle> {mfd::LineStyle::Dotted};
        }));
    EXPECT_TRUE(std::any_of(
        upsert->reticles.front().patch.primitivePatchesById.begin(),
        upsert->reticles.front().patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasTrianglePatch(entry.second) || HasPolylinePatch(entry.second) ||
                   HasBezierPatch(entry.second) || HasArcPatch(entry.second);
        }));
    EXPECT_TRUE(std::any_of(
        upsert->reticles.front().patch.primitivePatchesById.begin(),
        upsert->reticles.front().patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasImagePatch(entry.second);
        }));
}

TEST(GeneratedUiCompiledApiTests, GeneratedFixtureShutdownBuildsStatusAndDynamicRemovalCommands)
{
    generated_ui_fixture::GeneratedUiFixture ui;
    auto& track = ui.Radar().DynamicGeometryTemplate().Create();
    track.TrackLabel().SetText("A1");

    std::vector<mfd::UserCommand> publishCommands;
    ASSERT_EQ(ui.Radar().AppendCommands(publishCommands), 1U);
    ASSERT_EQ(publishCommands.size(), 1U);

    const mfd::CommandBatch shutdown = ui.BuildShutdownCommandBatch(77U, "OFF");
    EXPECT_EQ(shutdown.sequence, 77U);
    EXPECT_EQ(shutdown.mappingHash, generated_ui_fixture::GeneratedUiFixture::MappingHash());
    ASSERT_EQ(shutdown.commands.size(), 2U);

    std::vector<const mfd::UpdateReticleCommand*> statusUpdates;
    std::vector<const mfd::RemoveDynamicReticleCommand*> removals;
    for (const mfd::UserCommand& command : shutdown.commands)
    {
        if (const auto* update = std::get_if<mfd::UpdateReticleCommand>(&command); update != nullptr)
        {
            statusUpdates.push_back(update);
        }

        if (const auto* removal = std::get_if<mfd::RemoveDynamicReticleCommand>(&command); removal != nullptr)
        {
            removals.push_back(removal);
        }
    }

    ASSERT_EQ(statusUpdates.size(), 1U);
    EXPECT_EQ(statusUpdates.front()->target.reticle, "radar_status");
    ASSERT_EQ(statusUpdates.front()->patch.primitivePatchesById.size(), 1U);
    EXPECT_EQ(statusUpdates.front()->patch.primitivePatchesById.begin()->second.text, std::optional<std::string> {"OFF"});

    ASSERT_EQ(removals.size(), 1U);
    EXPECT_EQ(removals.front()->target.page, "Radar");
    EXPECT_NE(removals.front()->target.pageId, 0U);
    EXPECT_FALSE(removals.front()->target.reticleId.empty());
    EXPECT_NE(removals.front()->target.runtimeReticleId, 0U);

    std::mutex mutex;
    bool deliveredReady = false;
    mfd::CommandBatch delivered;
    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &deliveredReady, &delivered](const mfd::CommandBatch& batch)
        {
            std::lock_guard lock(mutex);
            delivered = batch;
            deliveredReady = true;
            return true;
        });

    ASSERT_TRUE(ui.SubmitShutdown(publisher, 88U, "RTB"));
    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_TRUE(deliveredReady);
    EXPECT_EQ(delivered.sequence, 88U);
    EXPECT_EQ(delivered.mappingHash, generated_ui_fixture::GeneratedUiFixture::MappingHash());
    ASSERT_EQ(delivered.commands.size(), 1U);
    ASSERT_NE(std::get_if<mfd::UpdateReticleCommand>(&delivered.commands.front()), nullptr);
}

TEST(GeneratedUiCompiledApiTests, GeneratedFixtureResetBuildsRuntimeResetAndInvalidatesDynamicHandles)
{
    generated_ui_fixture::GeneratedUiFixture ui;
    auto& track = ui.Radar().DynamicGeometryTemplate().Create();
    track.TrackLabel().SetText("A1");

    const mfd::CommandBatch initialBatch = ui.BuildCommandBatch(1U);
    ASSERT_EQ(initialBatch.commands.size(), 1U);
    ASSERT_NE(std::get_if<mfd::UpsertDynamicReticlesCommand>(&initialBatch.commands.front()), nullptr);
    ASSERT_TRUE(track.IsAlive());

    ui.Reset();
    EXPECT_FALSE(track.IsAlive());

    const mfd::CommandBatch resetBatch = ui.BuildResetCommandBatch(2U);
    EXPECT_EQ(resetBatch.sequence, 2U);
    EXPECT_EQ(resetBatch.mappingHash, generated_ui_fixture::GeneratedUiFixture::MappingHash());
    ASSERT_EQ(resetBatch.commands.size(), 1U);
    ASSERT_NE(std::get_if<mfd::ResetWindowCommand>(&resetBatch.commands.front()), nullptr);
}

TEST(GeneratedUiCompiledApiTests, GeneratedFixtureResetCanBeCombinedWithFreshMutations)
{
    generated_ui_fixture::GeneratedUiFixture ui;
    auto& staleTrack = ui.Radar().DynamicGeometryTemplate().Create();
    staleTrack.TrackLabel().SetText("OLD");
    ASSERT_TRUE(staleTrack.IsAlive());

    ui.Reset();
    EXPECT_FALSE(staleTrack.IsAlive());

    ui.Window().SetDisabled(true);
    ui.Radar().SetStatusCaption("RST");
    auto& replacementTrack = ui.Radar().DynamicGeometryTemplate().Create();
    replacementTrack.TrackLabel().SetText("NEW");

    const mfd::CommandBatch batch = ui.BuildCommandBatch(5U);
    EXPECT_EQ(batch.sequence, 5U);
    EXPECT_EQ(batch.mappingHash, generated_ui_fixture::GeneratedUiFixture::MappingHash());
    ASSERT_EQ(batch.commands.size(), 4U);
    ASSERT_NE(std::get_if<mfd::ResetWindowCommand>(&batch.commands[0]), nullptr);

    const auto* display = std::get_if<mfd::UpdateWindowDisplayCommand>(&batch.commands[1]);
    ASSERT_NE(display, nullptr);
    ASSERT_TRUE(display->patch.disabled.has_value());
    EXPECT_TRUE(*display->patch.disabled);

    const auto* status = std::get_if<mfd::UpdateReticleCommand>(&batch.commands[2]);
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->target.reticle, "radar_status");
    ASSERT_EQ(status->patch.primitivePatchesById.size(), 1U);
    EXPECT_EQ(status->patch.primitivePatchesById.begin()->second.text, std::optional<std::string> {"RST"});

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&batch.commands[3]);
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    EXPECT_TRUE(std::any_of(
        upsert->reticles.front().patch.primitivePatchesById.begin(),
        upsert->reticles.front().patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return entry.second.text == std::optional<std::string> {"NEW"};
        }));
}

TEST(GeneratedUiCompiledApiTests, GeneratedFixtureResetThenShutdownDoesNotEmitStaleDynamicRemovalCommands)
{
    generated_ui_fixture::GeneratedUiFixture ui;
    auto& staleTrack = ui.Radar().DynamicGeometryTemplate().Create();
    staleTrack.TrackLabel().SetText("OLD");

    const mfd::CommandBatch initialBatch = ui.BuildCommandBatch(1U);
    ASSERT_EQ(initialBatch.commands.size(), 1U);
    ASSERT_NE(std::get_if<mfd::UpsertDynamicReticlesCommand>(&initialBatch.commands.front()), nullptr);

    ui.Reset();
    EXPECT_FALSE(staleTrack.IsAlive());

    const mfd::CommandBatch shutdown = ui.BuildShutdownCommandBatch(6U, "OFF");
    EXPECT_EQ(shutdown.sequence, 6U);
    EXPECT_EQ(shutdown.mappingHash, generated_ui_fixture::GeneratedUiFixture::MappingHash());
    ASSERT_EQ(shutdown.commands.size(), 2U);
    ASSERT_NE(std::get_if<mfd::ResetWindowCommand>(&shutdown.commands[0]), nullptr);
    ASSERT_NE(std::get_if<mfd::UpdateReticleCommand>(&shutdown.commands[1]), nullptr);

    for (const mfd::UserCommand& command : shutdown.commands)
    {
        EXPECT_EQ(std::get_if<mfd::RemoveDynamicReticleCommand>(&command), nullptr);
    }
}

TEST(GeneratedUiCompiledApiTests, GeneratedFixtureExposesFillMutatorsOnlyForFillablePrimitiveHandles)
{
    using CircleHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().CursorCircle())>;
    using RingHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().ScopeRing())>;
    using RectangleHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().LockBox())>;
    using EllipseHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().UncertaintyEllipse())>;
    using SquareHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().TargetSquare())>;
    using DiamondHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().SteerDiamond())>;
    using TriangleHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().WarningTriangle())>;
    using PolylineHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().RoutePolyline())>;
    using ArcHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().ScanArc())>;
    using TimeHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().MissionTime())>;
    using LineHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().HeadingLine())>;
    using BezierHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().GuideBezier())>;
    using ImageHandle = std::remove_reference_t<
        decltype(std::declval<generated_ui_fixture::RadarGeometryPanelReticle&>().PanelImage())>;

    static_assert(!HasSetFillColor<mfd::client::PrimitiveHandle>::value);
    static_assert(!HasSetFilled<mfd::client::PrimitiveHandle>::value);

    static_assert(HasSetFillColor<CircleHandle>::value);
    static_assert(HasSetFilled<CircleHandle>::value);
    static_assert(HasSetFillColor<RingHandle>::value);
    static_assert(HasSetFilled<RingHandle>::value);
    static_assert(HasSetFillColor<RectangleHandle>::value);
    static_assert(HasSetFilled<RectangleHandle>::value);
    static_assert(HasSetFillColor<EllipseHandle>::value);
    static_assert(HasSetFilled<EllipseHandle>::value);
    static_assert(HasSetFillColor<SquareHandle>::value);
    static_assert(HasSetFilled<SquareHandle>::value);
    static_assert(HasSetFillColor<DiamondHandle>::value);
    static_assert(HasSetFilled<DiamondHandle>::value);
    static_assert(HasSetFillColor<TriangleHandle>::value);
    static_assert(HasSetFilled<TriangleHandle>::value);
    static_assert(HasSetFillColor<PolylineHandle>::value);
    static_assert(HasSetFilled<PolylineHandle>::value);
    static_assert(HasSetFillColor<ArcHandle>::value);
    static_assert(HasSetFilled<ArcHandle>::value);

    static_assert(!HasSetFillColor<TimeHandle>::value);
    static_assert(!HasSetFilled<TimeHandle>::value);
    static_assert(!HasSetFillColor<LineHandle>::value);
    static_assert(!HasSetFilled<LineHandle>::value);
    static_assert(!HasSetFillColor<BezierHandle>::value);
    static_assert(!HasSetFilled<BezierHandle>::value);
    static_assert(!HasSetFillColor<ImageHandle>::value);
    static_assert(!HasSetFilled<ImageHandle>::value);

    SUCCEED();
}

TEST(GeneratedUiCompiledApiTests, GeneratedFixtureExposesRuntimeFeedbackQueries)
{
    generated_ui_fixture::GeneratedUiFixture ui;
    ui.Radar().strobe = ui.Radar().strobe1;
    ui.Radar().strobe.SetActive(true);
    auto& track = ui.Radar().DynamicGeometryTemplate().Create();

    std::vector<mfd::UserCommand> commands;
    ASSERT_EQ(ui.Radar().AppendCommands(commands), 2U);
    ASSERT_EQ(commands.size(), 2U);
    const auto* strobeUpdate = std::get_if<mfd::UpdateStrobeCommand>(&commands[0]);
    ASSERT_NE(strobeUpdate, nullptr);
    EXPECT_EQ(strobeUpdate->strobeId, ui.Radar().strobe1.GeneratedId());
    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands[1]);
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    EXPECT_FALSE(ui.Radar().IsActive());
    EXPECT_FALSE(track.IsStrobeCaptured());

    mfd::ActivePageFeedback activePage;
    activePage.sequence = 1U;
    activePage.pageName = "Radar";
    EXPECT_TRUE(ui.ApplyFeedback(activePage));
    EXPECT_TRUE(ui.Radar().IsActive());

    mfd::StrobeStatusFeedback strobe;
    strobe.sequence = 2U;
    strobe.pageId = upsert->pageId;
    strobe.pageName = "Radar";
    mfd::StrobeFeedbackCapture capture;
    capture.runtimeReticleId = upsert->reticles.front().runtimeReticleId;
    capture.reticleId = track.Id();
    strobe.captureResult = std::move(capture);
    EXPECT_TRUE(ui.ApplyFeedback(strobe));
    EXPECT_TRUE(track.IsStrobeCaptured());
}
