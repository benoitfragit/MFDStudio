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
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
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

bool HasTimePatch(const mfd::PrimitivePatch& patch) noexcept
{
    return patch.letterSpacing.has_value();
}
} // namespace

TEST(GeneratedUiCompiledApiTests, GeneratedFixtureBuildsIdBasedCommandsFromRealGeneratedClasses)
{
    static_assert(std::is_same_v<generated_ui_fixture::RadarMockupPage::MfdGeneratedPageTag,
                                 mfd::CommandClient::GeneratedPageTag>);
    static_assert(generated_ui_fixture::RadarMockupPage::GeneratedId() != 0U);
    static_assert(generated_ui_fixture::RadarMockupPage::MappingHash() ==
                  generated_ui_fixture::GeneratedUiFixture::MappingHash());

    generated_ui_fixture::GeneratedUiFixture ui;
    ui.Window().SetBrightness(0.55f);

    auto& radar = ui.Radar();
    EXPECT_EQ(radar.GeneratedId(), generated_ui_fixture::RadarMockupPage::GeneratedId());
    radar.strobe.SetActive(true);
    radar.strobe.SetPosition({0.25f, -0.10f});
    radar.radarStatus.SetValue("LOCK");
    radar.geometryPanel.HeadingLine().SetStart({-0.5f, 0.0f});
    radar.geometryPanel.HeadingLine().SetEnd({0.5f, 0.0f});
    radar.geometryPanel.CursorCircle().SetRadius(0.07f);
    radar.geometryPanel.ScopeRing().SetInnerRadius(0.15f);
    radar.geometryPanel.ScopeRing().SetOuterRadius(0.21f);
    radar.geometryPanel.LockBox().SetSize({0.32f, 0.14f});
    radar.geometryPanel.LockBox().SetWidth(0.30f);
    radar.geometryPanel.LockBox().SetHeight(0.12f);
    radar.geometryPanel.UncertaintyEllipse().SetWidth(0.40f);
    radar.geometryPanel.UncertaintyEllipse().SetHeight(0.20f);
    radar.geometryPanel.TargetSquare().SetSize({0.11f, 0.11f});
    radar.geometryPanel.SteerDiamond().SetWidth(0.18f);
    radar.geometryPanel.SteerDiamond().SetHeight(0.24f);
    radar.geometryPanel.MissionTime().SetLetterSpacing(0.02f);

    auto& track = radar.DynamicGeometryTemplate().Create();
    track.SetPosition({0.10f, -0.20f});
    track.SetBlinkType(radar.slow);
    track.TrackLabel().SetText("T01");
    track.HeadingLine().SetStart({-0.25f, 0.15f});
    track.HeadingLine().SetEnd({0.25f, 0.15f});
    track.CursorCircle().SetRadius(0.05f);
    track.ScopeRing().SetInnerRadius(0.08f);
    track.ScopeRing().SetOuterRadius(0.14f);
    track.LockBox().SetSize({0.16f, 0.10f});
    track.LockBox().SetWidth(0.15f);
    track.LockBox().SetHeight(0.09f);
    track.UncertaintyEllipse().SetWidth(0.20f);
    track.UncertaintyEllipse().SetHeight(0.12f);
    track.TargetSquare().SetSize({0.07f, 0.07f});
    track.SteerDiamond().SetWidth(0.09f);
    track.SteerDiamond().SetHeight(0.13f);
    track.MissionTime().SetLetterSpacing(0.03f);

    const mfd::CommandBatch batch = ui.BuildCommandBatch(9U);
    EXPECT_EQ(batch.sequence, 9U);
    EXPECT_EQ(batch.mappingHash, generated_ui_fixture::GeneratedUiFixture::MappingHash());
    ASSERT_EQ(batch.commands.size(), 5U);

    const auto* window = FindCommand<mfd::UpdateWindowDisplayCommand>(batch.commands);
    ASSERT_NE(window, nullptr);
    ASSERT_TRUE(window->patch.brightness.has_value());
    EXPECT_FLOAT_EQ(*window->patch.brightness, 0.55f);

    const auto* strobe = FindCommand<mfd::UpdateStrobeCommand>(batch.commands);
    ASSERT_NE(strobe, nullptr);
    EXPECT_EQ(strobe->page, "Radar");
    EXPECT_NE(strobe->pageId, 0U);
    ASSERT_TRUE(strobe->active.has_value());
    ASSERT_TRUE(strobe->position.has_value());
    EXPECT_TRUE(*strobe->active);
    EXPECT_FLOAT_EQ(strobe->position->x, 0.25f);
    EXPECT_FLOAT_EQ(strobe->position->y, -0.10f);

    std::vector<const mfd::UpdateReticleCommand*> staticReticleUpdates;
    for (const mfd::UserCommand& command : batch.commands)
    {
        if (const auto* update = std::get_if<mfd::UpdateReticleCommand>(&command); update != nullptr)
        {
            staticReticleUpdates.push_back(update);
        }
    }

    ASSERT_EQ(staticReticleUpdates.size(), 2U);
    const auto statusUpdateIt = std::find_if(
        staticReticleUpdates.begin(),
        staticReticleUpdates.end(),
        [](const mfd::UpdateReticleCommand* update)
        {
            return update->target.reticle == "radar_status";
        });
    ASSERT_NE(statusUpdateIt, staticReticleUpdates.end());
    const mfd::UpdateReticleCommand& statusUpdate = **statusUpdateIt;
    EXPECT_EQ(statusUpdate.target.page, "Radar");
    EXPECT_NE(statusUpdate.target.pageId, 0U);
    EXPECT_NE(statusUpdate.target.reticleId, 0U);
    EXPECT_TRUE(statusUpdate.patch.primitivePatches.empty());
    ASSERT_EQ(statusUpdate.patch.primitivePatchesById.size(), 1U);
    EXPECT_EQ(statusUpdate.patch.primitivePatchesById.begin()->second.text, std::optional<std::string> {"LOCK"});

    const auto geometryUpdateIt = std::find_if(
        staticReticleUpdates.begin(),
        staticReticleUpdates.end(),
        [](const mfd::UpdateReticleCommand* update)
        {
            return update->target.reticle == "geometry_panel";
        });
    ASSERT_NE(geometryUpdateIt, staticReticleUpdates.end());
    const mfd::UpdateReticleCommand& geometryUpdate = **geometryUpdateIt;
    EXPECT_TRUE(geometryUpdate.patch.primitivePatches.empty());
    ASSERT_EQ(geometryUpdate.patch.primitivePatchesById.size(), 8U);
    EXPECT_TRUE(std::any_of(
        geometryUpdate.patch.primitivePatchesById.begin(),
        geometryUpdate.patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return HasLinePatch(entry.second);
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
            return HasTimePatch(entry.second);
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
    ASSERT_EQ(upsert->reticles.front().patch.primitivePatchesById.size(), 9U);
    EXPECT_TRUE(std::any_of(
        upsert->reticles.front().patch.primitivePatchesById.begin(),
        upsert->reticles.front().patch.primitivePatchesById.end(),
        [](const auto& entry)
        {
            return entry.second.text == std::optional<std::string> {"T01"};
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
