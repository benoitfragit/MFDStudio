/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for AnimationTests.
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "mfd/client/Animation.h"

TEST(AnimationTests, ReticleEmitsSingleUpdateAndTracksPrimitiveSpecificFields)
{
    mfd::client::BlinkType fast("fast");
    mfd::client::TextReticle reticle("Radar", "caption", "value");

    reticle.SetVisible(true);
    reticle.SetPosition({0.25f, -0.50f});
    reticle.SetRotationDegrees(33.0f);
    reticle.SetColor({1, 2, 3, 255});
    reticle.SetThickness(0.01f);
    reticle.SetValue("LOCK");
    reticle.SetLetterSpacing("value", 0.02f);
    reticle.SetBlinkType(fast);

    std::vector<mfd::UserCommand> commands;
    EXPECT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->target.page, "Radar");
    EXPECT_EQ(update->target.reticle, "caption");
    ASSERT_TRUE(update->patch.visible.has_value());
    ASSERT_TRUE(update->patch.position.has_value());
    ASSERT_TRUE(update->patch.rotationDegrees.has_value());
    ASSERT_TRUE(update->patch.color.has_value());
    ASSERT_TRUE(update->patch.thickness.has_value());
    ASSERT_TRUE(update->patch.blinkEnabled.has_value());
    ASSERT_TRUE(update->patch.blinkType.has_value());
    EXPECT_TRUE(*update->patch.visible);
    EXPECT_FLOAT_EQ(update->patch.position->x, 0.25f);
    EXPECT_FLOAT_EQ(update->patch.position->y, -0.50f);
    EXPECT_FLOAT_EQ(*update->patch.rotationDegrees, 33.0f);
    EXPECT_EQ(update->patch.color->r, 1U);
    EXPECT_FLOAT_EQ(*update->patch.thickness, 0.01f);
    EXPECT_TRUE(*update->patch.blinkEnabled);
    EXPECT_EQ(*update->patch.blinkType, "fast");
    EXPECT_EQ(update->patch.texts.at("value"), "LOCK");
    EXPECT_FLOAT_EQ(update->patch.letterSpacings.at("value"), 0.02f);

    commands.clear();
    EXPECT_FALSE(reticle.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());

    reticle.Blink = nullptr;
    EXPECT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);
    const auto* blinkDisable = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(blinkDisable, nullptr);
    ASSERT_TRUE(blinkDisable->patch.blinkEnabled.has_value());
    ASSERT_TRUE(blinkDisable->patch.blinkType.has_value());
    EXPECT_FALSE(*blinkDisable->patch.blinkEnabled);
    EXPECT_TRUE(blinkDisable->patch.blinkType->empty());
    EXPECT_FALSE(blinkDisable->patch.visible.has_value());
    EXPECT_FALSE(blinkDisable->patch.position.has_value());
    EXPECT_FALSE(blinkDisable->patch.rotationDegrees.has_value());
    EXPECT_FALSE(blinkDisable->patch.color.has_value());
    EXPECT_FALSE(blinkDisable->patch.thickness.has_value());
    EXPECT_TRUE(blinkDisable->patch.texts.empty());
    EXPECT_TRUE(blinkDisable->patch.letterSpacings.empty());
}

TEST(AnimationTests, DynamicReticleSetBatchesUpsertsAndEmitsRemovalsForMissingReticles)
{
    mfd::client::BlinkType caution("caution");
    mfd::client::DynamicReticleSet set("Radar", "radar_track");

    mfd::client::DynamicReticle& alpha = set.Upsert("alpha");
    alpha.SetPosition({0.10f, 0.20f});
    alpha.SetText("label", "A1");

    mfd::client::DynamicReticle& bravo = set.Upsert("bravo");
    bravo.SetColor({9, 8, 7, 255});
    bravo.SetBlinkType(caution);

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 2U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* initialBatch = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands.front());
    ASSERT_NE(initialBatch, nullptr);
    EXPECT_EQ(initialBatch->page, "Radar");
    EXPECT_EQ(initialBatch->templateId, "radar_track");
    ASSERT_EQ(initialBatch->reticles.size(), 2U);
    EXPECT_EQ(initialBatch->reticles[0].reticleId, "alpha");
    EXPECT_FLOAT_EQ(initialBatch->reticles[0].patch.position->x, 0.10f);
    EXPECT_EQ(initialBatch->reticles[0].patch.texts.at("label"), "A1");
    EXPECT_EQ(initialBatch->reticles[1].reticleId, "bravo");
    EXPECT_EQ(initialBatch->reticles[1].patch.color->r, 9U);
    EXPECT_EQ(*initialBatch->reticles[1].patch.blinkType, "caution");

    set.Reset();
    mfd::client::DynamicReticle& alphaOnly = set.Upsert("alpha");
    alphaOnly.SetPosition({0.40f, -0.15f});

    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 2U);
    ASSERT_EQ(commands.size(), 2U);

    const auto* remove = std::get_if<mfd::RemoveDynamicReticleCommand>(&commands[0]);
    ASSERT_NE(remove, nullptr);
    EXPECT_EQ(remove->target.page, "Radar");
    EXPECT_EQ(remove->target.reticle, "bravo");

    const auto* updateBatch = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands[1]);
    ASSERT_NE(updateBatch, nullptr);
    ASSERT_EQ(updateBatch->reticles.size(), 1U);
    EXPECT_EQ(updateBatch->reticles[0].reticleId, "alpha");
    EXPECT_FLOAT_EQ(updateBatch->reticles[0].patch.position->x, 0.40f);
    EXPECT_FLOAT_EQ(updateBatch->reticles[0].patch.position->y, -0.15f);
    EXPECT_FALSE(updateBatch->reticles[0].patch.color.has_value());
    EXPECT_FALSE(updateBatch->reticles[0].patch.blinkEnabled.has_value());
    EXPECT_FALSE(updateBatch->reticles[0].patch.blinkType.has_value());
    EXPECT_TRUE(updateBatch->reticles[0].patch.texts.empty());
    EXPECT_TRUE(updateBatch->reticles[0].patch.letterSpacings.empty());
}

TEST(AnimationTests, WindowDisplaySuppressesDuplicateUpdatesAndSupportsShutdownRemoval)
{
    mfd::client::DynamicReticleSet set("Radar", "radar_track");
    mfd::client::DynamicReticle& track = set.Upsert("shutdown");
    track.SetVisible(true);

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);

    commands.clear();
    EXPECT_EQ(set.AppendRemovalCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);
    const auto* remove = std::get_if<mfd::RemoveDynamicReticleCommand>(&commands.front());
    ASSERT_NE(remove, nullptr);
    EXPECT_EQ(remove->target.reticle, "shutdown");

    mfd::client::WindowDisplay display;
    display.SetColorInverted(true);
    display.SetBrightness(0.35f);
    display.SetDisabled(true);

    commands.clear();
    EXPECT_TRUE(display.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateWindowDisplayCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.invertColors.has_value());
    ASSERT_TRUE(update->patch.brightness.has_value());
    ASSERT_TRUE(update->patch.disabled.has_value());
    EXPECT_TRUE(*update->patch.invertColors);
    EXPECT_FLOAT_EQ(*update->patch.brightness, 0.35f);
    EXPECT_TRUE(*update->patch.disabled);

    commands.clear();
    EXPECT_FALSE(display.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());

    display.SetBrightness(0.60f);
    EXPECT_TRUE(display.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* brightnessOnly = std::get_if<mfd::UpdateWindowDisplayCommand>(&commands.front());
    ASSERT_NE(brightnessOnly, nullptr);
    EXPECT_FALSE(brightnessOnly->patch.invertColors.has_value());
    EXPECT_TRUE(brightnessOnly->patch.brightness.has_value());
    EXPECT_FALSE(brightnessOnly->patch.disabled.has_value());
    EXPECT_FLOAT_EQ(*brightnessOnly->patch.brightness, 0.60f);
}

TEST(AnimationTests, DynamicReticleSetVisibilityEmitsDedicatedTemplateCommand)
{
    mfd::client::DynamicReticleSet set("Radar", "radar_track");
    set.SetVisible(false);

    mfd::client::DynamicReticle& track = set.Upsert("alpha");
    track.SetPosition({0.2f, 0.3f});

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 2U);
    ASSERT_EQ(commands.size(), 2U);

    const auto* visibility = std::get_if<mfd::SetDynamicReticleSetVisibilityCommand>(&commands[0]);
    ASSERT_NE(visibility, nullptr);
    EXPECT_EQ(visibility->page, "Radar");
    EXPECT_EQ(visibility->templateId, "radar_track");
    EXPECT_FALSE(visibility->visible);

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands[1]);
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    EXPECT_EQ(upsert->reticles[0].reticleId, "alpha");

    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 0U);
    EXPECT_TRUE(commands.empty());

    set.SetVisible(true);
    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* showVisibility = std::get_if<mfd::SetDynamicReticleSetVisibilityCommand>(&commands[0]);
    ASSERT_NE(showVisibility, nullptr);
    EXPECT_TRUE(showVisibility->visible);
}


/**
 * @brief Covers Reticle blink API variants and global text/letter spacing fields.
 */
TEST(AnimationTests, ReticleBlinkApiVariantsEmitExpectedDeltaFields)
{
    mfd::client::BlinkType slow("slow");
    mfd::client::Reticle reticle("HUD", "waterline");

    reticle.SetBlinkEnabled(true);
    reticle.SetBlink(false, slow);
    reticle.SetBlinkType(slow);
    reticle.ClearBlinkType();
    reticle.SetText("GLOBAL");
    reticle.SetLetterSpacing(0.11f);

    std::vector<mfd::UserCommand> commands;
    ASSERT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.blinkEnabled.has_value());
    ASSERT_TRUE(update->patch.blinkType.has_value());
    ASSERT_TRUE(update->patch.text.has_value());
    ASSERT_TRUE(update->patch.letterSpacing.has_value());
    EXPECT_TRUE(*update->patch.blinkEnabled);
    EXPECT_TRUE(update->patch.blinkType->empty());
    EXPECT_EQ(*update->patch.text, "GLOBAL");
    EXPECT_FLOAT_EQ(*update->patch.letterSpacing, 0.11f);
}

/**
 * @brief Ensures Reset() only clears dirty state and therefore suppresses redundant emissions.
 */
TEST(AnimationTests, ReticleResetSuppressesEmissionUntilANewMutation)
{
    mfd::client::Reticle reticle("HUD", "waterline");
    reticle.SetVisible(true);
    reticle.Reset();

    std::vector<mfd::UserCommand> commands;
    EXPECT_FALSE(reticle.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());

    reticle.SetVisible(false);
    EXPECT_TRUE(reticle.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);
    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.visible.has_value());
    EXPECT_FALSE(*update->patch.visible);
}

/**
 * @brief Validates dynamic reticle updates emit only field deltas after first publish.
 */
TEST(AnimationTests, DynamicReticleSetEmitsOnlyChangedFieldsOnSecondPublish)
{
    mfd::client::DynamicReticleSet set("Radar", "radar_track");
    mfd::client::DynamicReticle& track = set.Upsert("trk_01");
    track.SetPosition({0.2f, 0.1f});
    track.SetColor({10, 20, 30, 255});

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    set.Reset();
    mfd::client::DynamicReticle& sameTrack = set.Upsert("trk_01");
    sameTrack.SetPosition({0.6f, -0.4f});

    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands.front());
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    const auto& patch = upsert->reticles.front().patch;
    ASSERT_TRUE(patch.position.has_value());
    EXPECT_FLOAT_EQ(patch.position->x, 0.6f);
    EXPECT_FLOAT_EQ(patch.position->y, -0.4f);
    EXPECT_FALSE(patch.color.has_value());
}

/**
 * @brief Confirms Reset() on dynamic set does not delete published reticles until removal cycle.
 */
TEST(AnimationTests, DynamicReticleSetResetThenNoUpsertProducesRemoval)
{
    mfd::client::DynamicReticleSet set("Radar", "radar_track");
    set.Upsert("trk_09").SetVisible(true);

    std::vector<mfd::UserCommand> commands;
    EXPECT_EQ(set.AppendCommands(commands), 1U);

    set.Reset();
    commands.clear();
    EXPECT_EQ(set.AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);
    const auto* remove = std::get_if<mfd::RemoveDynamicReticleCommand>(&commands.front());
    ASSERT_NE(remove, nullptr);
    EXPECT_EQ(remove->target.reticle, "trk_09");
}

/**
 * @brief Ensures WindowDisplay Reset() suppresses an unsent mutation exactly like Reticle Reset().
 */
TEST(AnimationTests, WindowDisplayResetSuppressesEmissionUntilNextMutation)
{
    mfd::client::WindowDisplay display;
    display.SetDisabled(true);
    display.Reset();

    std::vector<mfd::UserCommand> commands;
    EXPECT_FALSE(display.AppendCommands(commands));
    EXPECT_TRUE(commands.empty());

    display.SetDisabled(false);
    EXPECT_TRUE(display.AppendCommands(commands));
    ASSERT_EQ(commands.size(), 1U);
    const auto* update = std::get_if<mfd::UpdateWindowDisplayCommand>(&commands.front());
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.disabled.has_value());
    EXPECT_FALSE(*update->patch.disabled);
}
