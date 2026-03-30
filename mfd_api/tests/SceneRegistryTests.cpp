/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "mfd/control/CommandTypes.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/Reticle.h"
#include "mfd/runtime/SceneRegistry.h"

namespace
{
mfd::ReticleGroup MakeReticle(const std::string_view id)
{
    mfd::ReticleGroup reticle;
    reticle.id = std::string(id);

    mfd::Primitive primitive;
    primitive.id = "shape";
    primitive.type = mfd::PrimitiveType::Circle;
    primitive.geometry = mfd::CircleGeometry {0.05f};
    reticle.primitives.push_back(std::move(primitive));
    return reticle;
}

mfd::ReticleGroup MakeTextReticle(const std::string_view id)
{
    mfd::ReticleGroup reticle;
    reticle.id = std::string(id);

    mfd::Primitive title;
    title.id = "title";
    title.type = mfd::PrimitiveType::Text;
    title.geometry = mfd::TextGeometry {"INIT", 0.04f, 0.002f};
    reticle.primitives.push_back(std::move(title));

    mfd::Primitive value;
    value.id = "value";
    value.type = mfd::PrimitiveType::Text;
    value.transform.position = {0.15f, 0.0f};
    value.geometry = mfd::TextGeometry {"0", 0.04f, 0.002f};
    reticle.primitives.push_back(std::move(value));

    mfd::Primitive shape;
    shape.id = "shape";
    shape.type = mfd::PrimitiveType::Line;
    shape.geometry = mfd::LineGeometry {{-0.05f, -0.04f}, {0.05f, -0.04f}};
    reticle.primitives.push_back(std::move(shape));
    return reticle;
}

mfd::PageDefinition MakeBlinkPage()
{
    mfd::PageDefinition page;
    page.name = "Radar";
    page.normalizedName = "radar";
    page.title = "Radar";
    page.blinkTypes = {
        {"slow", "slow", 120},
        {"caution", "caution", 120},
        {"fast", "fast", 40}};
    page.defaultBlinkTypeName = "slow";
    page.normalizedDefaultBlinkTypeName = "slow";

    mfd::ReticleGroup defaultBlink = MakeReticle("default");
    defaultBlink.blink.enabled = true;
    defaultBlink.blink.durationMs = 120;

    mfd::ReticleGroup cautionBlink = MakeReticle("caution");
    cautionBlink.blink.enabled = true;
    cautionBlink.blink.typeName = "caution";
    cautionBlink.blink.normalizedTypeName = "caution";
    cautionBlink.blink.durationMs = 120;

    page.staticReticles = {std::move(defaultBlink), std::move(cautionBlink)};
    return page;
}

mfd::PageDefinition MakeRuntimePage()
{
    mfd::PageDefinition page = MakeBlinkPage();
    page.view.center = {0.1f, -0.2f};
    page.view.zoom = 1.25f;
    page.staticReticles.push_back(MakeTextReticle("textual"));

    mfd::PageStrobeDefinition strobe;
    strobe.reticle = MakeReticle("strobe");
    strobe.capture.shape = mfd::StrobeCaptureShape::Circle;
    strobe.capture.radius = 0.12f;
    strobe.magnet.enabled = true;
    strobe.magnet.radius = 0.15f;
    strobe.magnet.strength = 1.0f;
    page.strobe = std::move(strobe);
    return page;
}

const mfd::ReticleGroup* FindReticle(const std::vector<const mfd::ReticleGroup*>& reticles,
                                     const std::string_view id)
{
    for (const mfd::ReticleGroup* reticle : reticles)
    {
        if (reticle != nullptr && reticle->id == id)
        {
            return reticle;
        }
    }

    return nullptr;
}

const mfd::ReticleGroup* FindReticle(const std::vector<mfd::ReticleGroup>& reticles,
                                     const std::string_view id)
{
    for (const mfd::ReticleGroup& reticle : reticles)
    {
        if (reticle.id == id)
        {
            return &reticle;
        }
    }

    return nullptr;
}

const mfd::TextGeometry* FindTextGeometry(const mfd::ReticleGroup& reticle, const std::string_view primitiveId)
{
    const mfd::Primitive* primitive = mfd::FindPrimitive(reticle, primitiveId);
    if (primitive == nullptr)
    {
        return nullptr;
    }

    return std::get_if<mfd::TextGeometry>(&primitive->geometry);
}
} // namespace

TEST(SceneRegistryTests, ActivatesFirstPageAndIgnoresUnknownPage)
{
    mfd::PageDefinition firstPage = MakeBlinkPage();
    mfd::PageDefinition secondPage;
    secondPage.name = "Navigation";
    secondPage.normalizedName = "navigation";
    secondPage.title = "Navigation";
    secondPage.staticReticles.push_back(MakeReticle("nav_symbol"));

    mfd::MfdDocument document;
    document.pages = {std::move(firstPage), std::move(secondPage)};

    mfd::SceneRegistry registry(std::move(document));

    EXPECT_EQ(registry.ActivePageName(), "Radar");
    registry.SetActivePage("Navigation");
    EXPECT_EQ(registry.ActivePageName(), "Navigation");

    registry.SetActivePage("Unknown");
    EXPECT_EQ(registry.ActivePageName(), "Navigation");
}

TEST(SceneRegistryTests, BlinkTypeChangesUpdateDurationAndClearFallsBackToPageDefault)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeBlinkPage());

    mfd::SceneRegistry registry(std::move(document));

    const auto initialReticles = registry.CollectPageReticlePointers("Radar");
    const mfd::ReticleGroup* defaultReticle = FindReticle(initialReticles, "default");
    ASSERT_NE(defaultReticle, nullptr);
    EXPECT_EQ(defaultReticle->blink.durationMs, 120U);
    EXPECT_TRUE(defaultReticle->blink.enabled);

    EXPECT_TRUE(registry.SetReticleBlinkType("Radar", "default", "fast"));
    const auto fastReticles = registry.CollectPageReticlePointers("Radar");
    defaultReticle = FindReticle(fastReticles, "default");
    ASSERT_NE(defaultReticle, nullptr);
    EXPECT_EQ(defaultReticle->blink.typeName, "fast");
    EXPECT_EQ(defaultReticle->blink.durationMs, 40U);

    EXPECT_TRUE(registry.ClearReticleBlinkType("Radar", "default"));
    const auto clearedReticles = registry.CollectPageReticlePointers("Radar");
    defaultReticle = FindReticle(clearedReticles, "default");
    ASSERT_NE(defaultReticle, nullptr);
    EXPECT_TRUE(defaultReticle->blink.enabled);
    EXPECT_TRUE(defaultReticle->blink.typeName.empty());
    EXPECT_EQ(defaultReticle->blink.durationMs, 120U);
}

TEST(SceneRegistryTests, InvalidBlinkTypePatchIsRejectedWithoutMutatingReticle)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeBlinkPage());

    mfd::SceneRegistry registry(std::move(document));
    ASSERT_TRUE(registry.SetReticleBlinkType("Radar", "default", "fast"));

    EXPECT_FALSE(registry.SetReticleBlinkType("Radar", "default", "missing"));

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    const mfd::ReticleGroup* reticle = FindReticle(reticles, "default");
    ASSERT_NE(reticle, nullptr);
    EXPECT_EQ(reticle->blink.typeName, "fast");
    EXPECT_EQ(reticle->blink.durationMs, 40U);
}

TEST(SceneRegistryTests, ReticlesWithSameEffectiveDurationStaySynchronized)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeBlinkPage());

    mfd::SceneRegistry registry(std::move(document));

    for (int sample = 0; sample < 6; ++sample)
    {
        const auto visibleReticles = registry.CollectPageReticles("Radar");
        const mfd::ReticleGroup* defaultReticle = FindReticle(visibleReticles, "default");
        const mfd::ReticleGroup* cautionReticle = FindReticle(visibleReticles, "caution");
        ASSERT_NE(defaultReticle, nullptr);
        ASSERT_NE(cautionReticle, nullptr);
        EXPECT_EQ(defaultReticle->blink.durationMs, cautionReticle->blink.durationMs);
        EXPECT_EQ(defaultReticle->visible, cautionReticle->visible);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
}

TEST(SceneRegistryTests, WindowDisplayPatchClampsBrightnessAndRejectsNonFiniteValues)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeBlinkPage());

    mfd::SceneRegistry registry(std::move(document));

    EXPECT_TRUE(registry.SetWindowColorInverted(true));
    EXPECT_TRUE(registry.SetWindowBrightness(2.0f));
    EXPECT_TRUE(registry.SetWindowDisabled(true));
    EXPECT_TRUE(registry.WindowDisplay().invertColors);
    EXPECT_FLOAT_EQ(registry.WindowDisplay().brightness, 1.0f);
    EXPECT_TRUE(registry.WindowDisplay().disabled);

    mfd::WindowDisplayPatch patch;
    patch.invertColors = false;
    patch.brightness = -0.25f;
    patch.disabled = false;
    EXPECT_TRUE(registry.ApplyWindowDisplayPatch(patch));
    EXPECT_FALSE(registry.WindowDisplay().invertColors);
    EXPECT_FLOAT_EQ(registry.WindowDisplay().brightness, 0.0f);
    EXPECT_FALSE(registry.WindowDisplay().disabled);

    patch.disabled = true;
    patch.brightness = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(registry.ApplyWindowDisplayPatch(patch));
    EXPECT_FLOAT_EQ(registry.WindowDisplay().brightness, 0.0f);
    EXPECT_FALSE(registry.WindowDisplay().disabled);
}

TEST(SceneRegistryTests, WindowDisplayPatchSerializationRoundTripsDisabledFlag)
{
    mfd::WindowDisplayPatch patch;
    patch.invertColors = true;
    patch.brightness = 0.35f;
    patch.disabled = true;

    const mfd::UserCommand command = mfd::UpdateWindowDisplayCommand {patch};
    const std::string payload = mfd::SerializeUserCommand(command);
    const auto decoded = mfd::DeserializeUserCommand(payload);

    ASSERT_TRUE(decoded.has_value());
    const auto* update = std::get_if<mfd::UpdateWindowDisplayCommand>(&*decoded);
    ASSERT_NE(update, nullptr);
    ASSERT_TRUE(update->patch.invertColors.has_value());
    ASSERT_TRUE(update->patch.brightness.has_value());
    ASSERT_TRUE(update->patch.disabled.has_value());
    EXPECT_TRUE(*update->patch.invertColors);
    EXPECT_FLOAT_EQ(*update->patch.brightness, 0.35f);
    EXPECT_TRUE(*update->patch.disabled);
}

TEST(SceneRegistryTests, PageViewAndReticleMutationsCoverCommonRuntimeSetters)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    EXPECT_TRUE(registry.HasPage("Radar"));
    EXPECT_TRUE(registry.HasStrobe("Radar"));
    EXPECT_TRUE(registry.ActivePageHasStrobe());
    EXPECT_EQ(registry.ActiveBackgroundColor().g, 14U);

    ASSERT_TRUE(registry.ViewForPage("Radar").has_value());
    EXPECT_TRUE(registry.SetPageViewCenter("Radar", {0.4f, -0.3f}));
    EXPECT_TRUE(registry.SetPageZoom("Radar", -5.0f));
    EXPECT_FALSE(registry.SetPageViewCenter("Unknown", {0.0f, 0.0f}));

    const auto view = registry.ViewForPage("Radar");
    ASSERT_TRUE(view.has_value());
    EXPECT_FLOAT_EQ(view->center.x, 0.4f);
    EXPECT_FLOAT_EQ(view->center.y, -0.3f);
    EXPECT_FLOAT_EQ(view->zoom, 1.0f);

    EXPECT_TRUE(registry.SetReticleVisible("Radar", "textual", false));
    EXPECT_TRUE(registry.SetReticlePosition("Radar", "textual", {0.25f, 0.35f}));
    EXPECT_TRUE(registry.SetReticleRotation("Radar", "textual", 18.0f));
    EXPECT_TRUE(registry.SetReticleColor("Radar", "textual", {9, 10, 11, 255}));
    EXPECT_FALSE(registry.SetReticleThickness("Radar", "textual", 0.0f));
    EXPECT_TRUE(registry.SetReticleThickness("Radar", "textual", 0.012f));
    EXPECT_TRUE(registry.SetReticleText("Radar", "textual", "PRIMARY"));
    EXPECT_TRUE(registry.SetReticleText("Radar", "textual", "value", "456"));
    EXPECT_FALSE(registry.SetReticleLetterSpacing("Radar", "textual", std::numeric_limits<float>::infinity()));
    EXPECT_TRUE(registry.SetReticleLetterSpacing("Radar", "textual", 0.015f));
    EXPECT_TRUE(registry.SetReticleLetterSpacing("Radar", "textual", "value", 0.025f));
    EXPECT_FALSE(registry.SetReticlePosition("Radar", "missing", {0.0f, 0.0f}));

    mfd::ReticlePatch patch;
    patch.visible = true;
    patch.position = mfd::Vec2 {0.5f, -0.25f};
    patch.rotationDegrees = -12.0f;
    patch.color = mfd::ColorRgba {31, 32, 33, 255};
    patch.thickness = 0.02f;
    patch.texts.emplace("title", "PATCHED");
    patch.texts.emplace("value", "789");
    patch.letterSpacings.emplace("value", 0.03f);

    EXPECT_TRUE(registry.ApplyReticlePatch("Radar", "textual", patch));

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    const mfd::ReticleGroup* textual = FindReticle(reticles, "textual");
    ASSERT_NE(textual, nullptr);
    EXPECT_TRUE(textual->visible);
    EXPECT_FLOAT_EQ(textual->transform.position.x, 0.5f);
    EXPECT_FLOAT_EQ(textual->transform.position.y, -0.25f);
    EXPECT_FLOAT_EQ(textual->transform.rotationDegrees, -12.0f);
    ASSERT_TRUE(textual->overrides.color.has_value());
    EXPECT_EQ(textual->overrides.color->r, 31U);
    EXPECT_EQ(textual->overrides.color->g, 32U);
    EXPECT_EQ(textual->overrides.color->b, 33U);
    ASSERT_TRUE(textual->overrides.thickness.has_value());
    EXPECT_FLOAT_EQ(*textual->overrides.thickness, 0.02f);

    const mfd::TextGeometry* title = FindTextGeometry(*textual, "title");
    const mfd::TextGeometry* value = FindTextGeometry(*textual, "value");
    ASSERT_NE(title, nullptr);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(title->text, "PATCHED");
    EXPECT_EQ(value->text, "789");
    EXPECT_FLOAT_EQ(title->letterSpacing, 0.015f);
    EXPECT_FLOAT_EQ(value->letterSpacing, 0.03f);
}

TEST(SceneRegistryTests, DynamicReticlesSupportLifecyclePatchingAndOrdering)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup dynamicOne = MakeTextReticle("track_alpha");
    dynamicOne.sourceTemplateId = "track_template";
    dynamicOne.info.label = "Track Alpha";
    dynamicOne.info.category = "track";
    dynamicOne.info.metadata["threat"] = "high";
    dynamicOne.transform.position = {0.1f, 0.2f};

    mfd::ReticleGroup dynamicTwo = MakeTextReticle("track_bravo");
    dynamicTwo.transform.position = {-0.2f, 0.1f};

    registry.UpsertDynamicReticle("Radar", dynamicOne);
    registry.UpsertDynamicReticle("Radar", dynamicTwo);

    EXPECT_TRUE(registry.HasDynamicReticle("Radar", "track_alpha"));
    EXPECT_TRUE(registry.HasDynamicReticle("Radar", "track_bravo"));

    const auto ordered = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(ordered.size(), 6U);
    EXPECT_EQ(ordered[0]->id, "default");
    EXPECT_EQ(ordered[1]->id, "caution");
    EXPECT_EQ(ordered[2]->id, "textual");
    EXPECT_EQ(ordered[3]->id, "track_alpha");
    EXPECT_EQ(ordered[4]->id, "track_bravo");
    EXPECT_EQ(ordered[5]->id, "strobe");

    mfd::ReticlePatch patch;
    patch.position = mfd::Vec2 {0.6f, -0.1f};
    patch.text = std::string {"DYN"};
    patch.blinkEnabled = true;
    EXPECT_TRUE(registry.ApplyDynamicReticlePatch("Radar", "track_alpha", patch));

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    const mfd::ReticleGroup* dynamic = FindReticle(reticles, "track_alpha");
    ASSERT_NE(dynamic, nullptr);
    EXPECT_FLOAT_EQ(dynamic->transform.position.x, 0.6f);
    EXPECT_FLOAT_EQ(dynamic->transform.position.y, -0.1f);
    EXPECT_TRUE(dynamic->blink.enabled);
    EXPECT_EQ(dynamic->blink.durationMs, 120U);

    const mfd::TextGeometry* title = FindTextGeometry(*dynamic, "title");
    ASSERT_NE(title, nullptr);
    EXPECT_EQ(title->text, "DYN");

    EXPECT_TRUE(registry.RemoveDynamicReticle("Radar", "track_alpha"));
    EXPECT_FALSE(registry.RemoveDynamicReticle("Radar", "track_alpha"));
    EXPECT_FALSE(registry.HasDynamicReticle("Radar", "track_alpha"));

    registry.ClearDynamicReticles("Radar");
    EXPECT_FALSE(registry.HasDynamicReticle("Radar", "track_bravo"));

    registry.UpsertDynamicReticle("Radar", MakeTextReticle("track_charlie"));
    mfd::PageDefinition secondaryPage;
    secondaryPage.name = "Nav";
    secondaryPage.normalizedName = "nav";
    secondaryPage.staticReticles.push_back(MakeReticle("nav_symbol"));

    mfd::MfdDocument secondDocument;
    secondDocument.pages.push_back(MakeRuntimePage());
    secondDocument.pages.push_back(std::move(secondaryPage));

    mfd::SceneRegistry secondRegistry(std::move(secondDocument));
    secondRegistry.UpsertDynamicReticle("Radar", MakeTextReticle("track_delta"));
    secondRegistry.UpsertDynamicReticle("Nav", MakeTextReticle("ghost"));
    secondRegistry.ClearAllDynamicReticles();
    EXPECT_FALSE(secondRegistry.HasDynamicReticle("Radar", "track_delta"));
    EXPECT_FALSE(secondRegistry.HasDynamicReticle("Nav", "ghost"));
}

TEST(SceneRegistryTests, StrobeMagnetizationAndCaptureTrackNearestVisibleDynamicReticle)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup nearTrack = MakeTextReticle("near_track");
    nearTrack.sourceTemplateId = "track_template";
    nearTrack.info.label = "Near";
    nearTrack.info.category = "friendly";
    nearTrack.info.metadata["callsign"] = "N1";
    nearTrack.transform.position = {0.1f, 0.0f};

    mfd::ReticleGroup farTrack = MakeTextReticle("far_track");
    farTrack.transform.position = {0.32f, 0.0f};

    mfd::ReticleGroup hiddenTrack = MakeTextReticle("hidden_track");
    hiddenTrack.transform.position = {0.08f, 0.0f};
    hiddenTrack.visible = false;

    registry.UpsertDynamicReticle("Radar", nearTrack);
    registry.UpsertDynamicReticle("Radar", farTrack);
    registry.UpsertDynamicReticle("Radar", hiddenTrack);

    ASSERT_TRUE(registry.SetStrobePosition("Radar", {0.08f, 0.02f}));

    const auto strobe = registry.ActiveStrobeSummary();
    ASSERT_TRUE(strobe.has_value());
    EXPECT_TRUE(strobe->visible);
    EXPECT_FLOAT_EQ(strobe->position.x, 0.1f);
    EXPECT_FLOAT_EQ(strobe->position.y, 0.0f);

    const auto magnet = registry.ActiveStrobeMagnetSummary();
    ASSERT_TRUE(magnet.has_value());
    EXPECT_TRUE(magnet->enabled);
    EXPECT_TRUE(magnet->magnetized);
    EXPECT_EQ(magnet->reticleId, "near_track");
    EXPECT_FLOAT_EQ(magnet->targetPosition.x, 0.1f);
    EXPECT_FLOAT_EQ(magnet->targetPosition.y, 0.0f);
    EXPECT_NEAR(magnet->distance, 0.0f, 1e-5f);

    const auto capture = registry.CaptureActivePageStrobe();
    ASSERT_TRUE(capture.has_value());
    EXPECT_EQ(capture->pageName, "Radar");
    EXPECT_EQ(capture->strobeId, "strobe");
    EXPECT_EQ(capture->reticleId, "near_track");
    EXPECT_EQ(capture->sourceTemplateId, "track_template");
    EXPECT_EQ(capture->label, "Near");
    EXPECT_EQ(capture->category, "friendly");
    EXPECT_EQ(capture->metadata.at("callsign"), "N1");

    EXPECT_TRUE(registry.OffsetStrobe("Radar", {0.01f, 0.0f}));
    ASSERT_TRUE(registry.ActiveStrobeSummary().has_value());
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.x, 0.1f);

    EXPECT_TRUE(registry.SetStrobeActive("Radar", false));
    ASSERT_TRUE(registry.ActiveStrobeSummary().has_value());
    EXPECT_FALSE(registry.ActiveStrobeSummary()->visible);
    EXPECT_FALSE(registry.CaptureActivePageStrobe().has_value());
}
