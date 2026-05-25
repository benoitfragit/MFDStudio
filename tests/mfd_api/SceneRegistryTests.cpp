/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for SceneRegistryTests.
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

#include "mfd/control/CommandProcessor.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/model/Reticle.h"
#include "mfd/runtime/SceneRegistry.h"

namespace
{
constexpr std::string_view kDefaultLayerId = mfd::kDefaultPageLayerId;
constexpr std::string_view kDefaultDynamicTemplateId = "dynamic_default";

mfd::ReticleGroup MakeReticle(const std::string_view id)
{
    mfd::ReticleGroup reticle;
    reticle.id = std::string(id);
    reticle.layerId = std::string(kDefaultLayerId);

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
    reticle.layerId = std::string(kDefaultLayerId);

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

mfd::ReticleGroup MakeGeometryPatchReticle(const std::string_view id)
{
    mfd::ReticleGroup reticle;
    reticle.id = std::string(id);
    reticle.layerId = std::string(kDefaultLayerId);

    mfd::Primitive ring;
    ring.id = "ring";
    ring.type = mfd::PrimitiveType::Ring;
    ring.geometry = mfd::RingGeometry {0.08f, 0.12f, 32};
    reticle.primitives.push_back(std::move(ring));

    mfd::Primitive triangle;
    triangle.id = "triangle";
    triangle.type = mfd::PrimitiveType::Triangle;
    triangle.geometry = mfd::TriangleGeometry {{{{-0.1f, -0.1f}, {0.0f, 0.12f}, {0.1f, -0.1f}}}};
    reticle.primitives.push_back(std::move(triangle));

    mfd::Primitive polyline;
    polyline.id = "polyline";
    polyline.type = mfd::PrimitiveType::Polyline;
    polyline.geometry = mfd::PolylineGeometry {{{{-0.15f, -0.05f}, {0.0f, 0.14f}, {0.16f, -0.03f}}}, false};
    reticle.primitives.push_back(std::move(polyline));

    mfd::Primitive bezier;
    bezier.id = "bezier";
    bezier.type = mfd::PrimitiveType::Bezier;
    bezier.geometry = mfd::BezierGeometry {{{{-0.18f, -0.12f}, {-0.06f, 0.12f}, {0.06f, 0.12f}, {0.18f, -0.12f}}}, 16};
    reticle.primitives.push_back(std::move(bezier));

    mfd::Primitive arc;
    arc.id = "arc";
    arc.type = mfd::PrimitiveType::Arc;
    arc.geometry = mfd::ArcGeometry {0.18f, -30.0f, 120.0f, 24};
    reticle.primitives.push_back(std::move(arc));

    return reticle;
}

mfd::ReticleGroup MakeDynamicTextReticle(const std::string_view id,
                                         const std::string_view templateId = kDefaultDynamicTemplateId)
{
    mfd::ReticleGroup reticle = MakeTextReticle(id);
    reticle.sourceTemplateId = std::string(templateId);
    return reticle;
}

mfd::PageDefinition MakeBlinkPage()
{
    mfd::PageDefinition page;
    page.name = "Radar";
    page.normalizedName = "radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});
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
    page.dynamicReticleBindings.push_back(
        mfd::DynamicReticleLayerBinding {std::string(kDefaultDynamicTemplateId), std::string(kDefaultLayerId), 0U});
    page.dynamicReticleBindings.push_back(
        mfd::DynamicReticleLayerBinding {"track_template", std::string(kDefaultLayerId), 0U});
    page.dynamicReticleBindings.push_back(
        mfd::DynamicReticleLayerBinding {"radar_tracks", std::string(kDefaultLayerId), 0U});
    page.dynamicReticleBindings.push_back(
        mfd::DynamicReticleLayerBinding {"waypoints", std::string(kDefaultLayerId), 1U});

    mfd::PageStrobeDefinition strobe;
    strobe.name = "Default";
    strobe.normalizedName = "default";
    strobe.reticle = MakeReticle("strobe");
    strobe.capture.shape = mfd::StrobeCaptureShape::Circle;
    strobe.capture.radius = 0.12f;
    strobe.magnet.enabled = true;
    strobe.magnet.radius = 0.15f;
    strobe.magnet.strength = 1.0f;
    page.strobes.push_back(std::move(strobe));
    page.activeStrobeName = "Default";
    page.normalizedActiveStrobeName = "default";
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
    secondPage.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});
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

TEST(SceneRegistryTests, CollectPageReticleViewsDrawsTopReticlesAfterRegularOnes)
{
    mfd::PageDefinition page;
    page.name = "Images";
    page.normalizedName = "images";
    page.title = "Images";
    page.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});

    mfd::ReticleGroup overlay = MakeReticle("overlay");
    overlay.drawOnTop = true;
    page.staticReticles.push_back(std::move(overlay));
    page.staticReticles.push_back(MakeReticle("background"));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    mfd::SceneRegistry registry(std::move(document));
    const std::vector<mfd::ReticleRenderView> views = registry.CollectPageReticleViews("Images");

    ASSERT_EQ(views.size(), 2U);
    ASSERT_NE(views[0].group, nullptr);
    ASSERT_NE(views[1].group, nullptr);
    EXPECT_EQ(views[0].group->id, "background");
    EXPECT_EQ(views[1].group->id, "overlay");
}

TEST(SceneRegistryTests, CollectPageReticleViewsBufferOverloadMatchesValueOverloadAndClearsDestination)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));
    std::vector<mfd::ReticleRenderView> destination(3U, mfd::ReticleRenderView {nullptr, false});

    registry.CollectPageReticleViews("Radar", destination);
    const std::vector<mfd::ReticleRenderView> expected = registry.CollectPageReticleViews("Radar");

    ASSERT_EQ(destination.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index)
    {
        ASSERT_NE(destination[index].group, nullptr);
        ASSERT_NE(expected[index].group, nullptr);
        EXPECT_EQ(destination[index].group->id, expected[index].group->id);
        EXPECT_EQ(destination[index].visible, expected[index].visible);
    }
}

TEST(SceneRegistryTests, CollectPageReticlesUsesCaseInsensitiveLayerLookupAcrossDrawPasses)
{
    mfd::PageDefinition page;
    page.name = "Radar";
    page.normalizedName = "radar";
    page.title = "Radar";
    page.layers.push_back(mfd::PageLayerDefinition {"Background"});
    page.layers.push_back(mfd::PageLayerDefinition {"Overlay"});
    page.dynamicReticleBindings.push_back(mfd::DynamicReticleLayerBinding {"track_template", "overlay", 0});

    mfd::ReticleGroup overlayTop = MakeReticle("overlay_top");
    overlayTop.layerId = "OVERLAY";
    overlayTop.drawOnTop = true;

    mfd::ReticleGroup background = MakeReticle("background");
    background.layerId = "background";

    mfd::ReticleGroup overlayNormal = MakeReticle("overlay_normal");
    overlayNormal.layerId = "overlay";

    page.staticReticles.push_back(std::move(overlayTop));
    page.staticReticles.push_back(std::move(background));
    page.staticReticles.push_back(std::move(overlayNormal));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup dynamicNormal = MakeDynamicTextReticle("dynamic_normal", "track_template");
    dynamicNormal.layerId = "Overlay";
    registry.UpsertDynamicReticle("Radar", dynamicNormal);

    mfd::ReticleGroup dynamicTop = MakeDynamicTextReticle("dynamic_top", "track_template");
    dynamicTop.layerId = "overlay";
    dynamicTop.drawOnTop = true;
    registry.UpsertDynamicReticle("Radar", dynamicTop);

    const auto ordered = registry.CollectPageReticlePointers("Radar");
    ASSERT_EQ(ordered.size(), 5U);
    EXPECT_EQ(ordered[0]->id, "background");
    EXPECT_EQ(ordered[1]->id, "overlay_normal");
    EXPECT_EQ(ordered[2]->id, "dynamic_normal");
    EXPECT_EQ(ordered[3]->id, "overlay_top");
    EXPECT_EQ(ordered[4]->id, "dynamic_top");
}

TEST(SceneRegistryTests, ActivatesMarkedDefaultPageWhenPresent)
{
    mfd::PageDefinition firstPage = MakeBlinkPage();
    mfd::PageDefinition secondPage;
    secondPage.name = "Navigation";
    secondPage.normalizedName = "navigation";
    secondPage.title = "Navigation";
    secondPage.defaultPage = true;
    secondPage.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});
    secondPage.staticReticles.push_back(MakeReticle("nav_symbol"));

    mfd::MfdDocument document;
    document.pages = {std::move(firstPage), std::move(secondPage)};

    mfd::SceneRegistry registry(std::move(document));

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

TEST(SceneRegistryTests, ReticleViewsMatchCopyPathOrderPointersAndResolvedVisibility)
{
    mfd::PageDefinition page = MakeRuntimePage();
    page.staticReticles[0].blink = {};
    page.staticReticles[1].blink = {};

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup dynamicReticle = MakeDynamicTextReticle("track_alpha");
    dynamicReticle.transform.position = {0.15f, -0.10f};
    registry.UpsertDynamicReticle("Radar", std::move(dynamicReticle));

    const auto pointers = registry.CollectPageReticlePointers("Radar");
    const auto views = registry.CollectPageReticleViews("Radar");
    const auto copies = registry.CollectPageReticles("Radar");

    ASSERT_EQ(pointers.size(), views.size());
    ASSERT_EQ(copies.size(), views.size());

    for (std::size_t index = 0; index < views.size(); ++index)
    {
        ASSERT_NE(views[index].group, nullptr);
        EXPECT_EQ(views[index].group, pointers[index]);
        EXPECT_EQ(views[index].group->id, copies[index].id);
        EXPECT_EQ(views[index].visible, copies[index].visible);
    }
}

TEST(SceneRegistryTests, UpsertingExistingDynamicReticleKeepsSingleOrderedEntry)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup firstVersion = MakeDynamicTextReticle("track_alpha");
    firstVersion.transform.position = {0.1f, 0.2f};
    registry.UpsertDynamicReticle("Radar", firstVersion);

    mfd::ReticleGroup secondVersion = MakeDynamicTextReticle("track_alpha");
    secondVersion.transform.position = {-0.3f, 0.4f};
    registry.UpsertDynamicReticle("Radar", secondVersion);

    const auto views = registry.CollectPageReticleViews("Radar");
    std::size_t matchCount = 0;
    for (const mfd::ReticleRenderView& view : views)
    {
        if (view.group != nullptr && view.group->id == "track_alpha")
        {
            ++matchCount;
            EXPECT_FLOAT_EQ(view.group->transform.position.x, -0.3f);
            EXPECT_FLOAT_EQ(view.group->transform.position.y, 0.4f);
        }
    }

    EXPECT_EQ(matchCount, 1U);
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

TEST(SceneRegistryTests, DynamicTemplateVisibilityCommandSerializationRoundTrips)
{
    mfd::SetDynamicReticleSetVisibilityCommand original;
    original.pageId = 11U;
    original.templateTransportId = 77U;
    original.visible = false;
    const mfd::UserCommand command = original;
    const std::string payload = mfd::SerializeUserCommand(command);
    const auto decoded = mfd::DeserializeUserCommand(payload);

    ASSERT_TRUE(decoded.has_value());
    const auto* visibility = std::get_if<mfd::SetDynamicReticleSetVisibilityCommand>(&*decoded);
    ASSERT_NE(visibility, nullptr);
    EXPECT_EQ(visibility->pageId, 11U);
    EXPECT_EQ(visibility->templateTransportId, 77U);
    EXPECT_FALSE(visibility->visible);
}

TEST(SceneRegistryTests, ResetWindowCommandSerializationRoundTrips)
{
    const mfd::UserCommand command = mfd::ResetWindowCommand {};
    const std::string payload = mfd::SerializeUserCommand(command);
    const auto decoded = mfd::DeserializeUserCommand(payload);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_NE(std::get_if<mfd::ResetWindowCommand>(&*decoded), nullptr);
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
    EXPECT_FALSE(registry.SetPageZoom("Radar", -5.0f));
    EXPECT_FALSE(registry.SetPageViewCenter("Unknown", {0.0f, 0.0f}));

    const auto view = registry.ViewForPage("Radar");
    ASSERT_TRUE(view.has_value());
    EXPECT_FLOAT_EQ(view->center.x, 0.4f);
    EXPECT_FLOAT_EQ(view->center.y, -0.3f);
    EXPECT_FLOAT_EQ(view->zoom, 1.25f);

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
    patch.scale = mfd::Vec2 {1.20f, 0.85f};
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
    EXPECT_FLOAT_EQ(textual->transform.scale.x, 1.20f);
    EXPECT_FLOAT_EQ(textual->transform.scale.y, 0.85f);
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

TEST(SceneRegistryTests, ApplyReticlePatchSupportsRichPrimitiveOverrides)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    mfd::PrimitivePatch valuePatch;
    valuePatch.visible = false;
    valuePatch.position = mfd::Vec2 {0.25f, -0.10f};
    valuePatch.scale = mfd::Vec2 {1.5f, 0.75f};
    valuePatch.color = mfd::ColorRgba {10, 20, 30, 255};
    valuePatch.thickness = 0.01f;
    valuePatch.text = "LOCK";
    valuePatch.letterSpacing = 0.04f;

    mfd::PrimitivePatch linePatch;
    linePatch.lineStyle = mfd::LineStyle::Dashed;
    linePatch.lineStart = mfd::Vec2 {-0.25f, -0.02f};
    linePatch.lineEnd = mfd::Vec2 {0.30f, -0.02f};

    mfd::ReticlePatch textualPatch;
    textualPatch.primitivePatches.emplace("value", valuePatch);
    textualPatch.primitivePatches.emplace("shape", linePatch);

    EXPECT_TRUE(registry.ApplyReticlePatch("Radar", "textual", textualPatch));

    mfd::PrimitivePatch circlePatch;
    circlePatch.radius = 0.12f;

    mfd::ReticlePatch defaultPatch;
    defaultPatch.primitivePatches.emplace("shape", circlePatch);
    EXPECT_TRUE(registry.ApplyReticlePatch("Radar", "default", defaultPatch));

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    const mfd::ReticleGroup* textual = FindReticle(reticles, "textual");
    const mfd::ReticleGroup* defaultReticle = FindReticle(reticles, "default");
    ASSERT_NE(textual, nullptr);
    ASSERT_NE(defaultReticle, nullptr);

    const mfd::Primitive* valuePrimitive = mfd::FindPrimitive(*textual, "value");
    ASSERT_NE(valuePrimitive, nullptr);
    const auto* valueGeometry = std::get_if<mfd::TextGeometry>(&valuePrimitive->geometry);
    ASSERT_NE(valueGeometry, nullptr);
    EXPECT_FALSE(valuePrimitive->style.visible);
    EXPECT_FLOAT_EQ(valuePrimitive->transform.position.x, 0.25f);
    EXPECT_FLOAT_EQ(valuePrimitive->transform.position.y, -0.10f);
    EXPECT_FLOAT_EQ(valuePrimitive->transform.scale.x, 1.5f);
    EXPECT_FLOAT_EQ(valuePrimitive->transform.scale.y, 0.75f);
    EXPECT_EQ(valuePrimitive->style.color.r, 10U);
    EXPECT_EQ(valuePrimitive->style.color.g, 20U);
    EXPECT_EQ(valuePrimitive->style.color.b, 30U);
    EXPECT_FLOAT_EQ(valuePrimitive->style.thickness, 0.01f);
    EXPECT_EQ(valueGeometry->text, "LOCK");
    EXPECT_FLOAT_EQ(valueGeometry->letterSpacing, 0.04f);

    const mfd::Primitive* linePrimitive = mfd::FindPrimitive(*textual, "shape");
    ASSERT_NE(linePrimitive, nullptr);
    const auto* lineGeometry = std::get_if<mfd::LineGeometry>(&linePrimitive->geometry);
    ASSERT_NE(lineGeometry, nullptr);
    EXPECT_EQ(linePrimitive->style.lineStyle, mfd::LineStyle::Dashed);
    EXPECT_FLOAT_EQ(lineGeometry->start.x, -0.25f);
    EXPECT_FLOAT_EQ(lineGeometry->start.y, -0.02f);
    EXPECT_FLOAT_EQ(lineGeometry->end.x, 0.30f);
    EXPECT_FLOAT_EQ(lineGeometry->end.y, -0.02f);

    const mfd::Primitive* circlePrimitive = mfd::FindPrimitive(*defaultReticle, "shape");
    ASSERT_NE(circlePrimitive, nullptr);
    const auto* circleGeometry = std::get_if<mfd::CircleGeometry>(&circlePrimitive->geometry);
    ASSERT_NE(circleGeometry, nullptr);
    EXPECT_FLOAT_EQ(circleGeometry->radius, 0.12f);
}

TEST(SceneRegistryTests, ApplyReticlePatchSupportsPointListFillAndArcOverrides)
{
    mfd::MfdDocument document;
    mfd::PageDefinition page = MakeBlinkPage();
    page.staticReticles.push_back(MakeGeometryPatchReticle("geometry"));
    document.pages.push_back(std::move(page));

    mfd::SceneRegistry registry(std::move(document));

    mfd::PrimitivePatch ringPatch;
    ringPatch.fillColor = mfd::ColorRgba {1, 2, 3, 220};
    ringPatch.filled = true;
    ringPatch.segments = 48;

    mfd::PrimitivePatch trianglePatch;
    trianglePatch.points = std::vector<mfd::Vec2> {{-0.2f, -0.1f}, {0.0f, 0.25f}, {0.22f, -0.08f}};

    mfd::PrimitivePatch polylinePatch;
    polylinePatch.points = std::vector<mfd::Vec2> {{-0.2f, 0.0f}, {-0.05f, 0.18f}, {0.12f, 0.08f}, {0.20f, -0.04f}};
    polylinePatch.closed = true;
    polylinePatch.fillColor = mfd::ColorRgba {20, 30, 40, 180};
    polylinePatch.filled = true;

    mfd::PrimitivePatch bezierPatch;
    bezierPatch.points = std::vector<mfd::Vec2> {{-0.25f, -0.05f}, {-0.08f, 0.22f}, {0.08f, 0.22f}, {0.24f, -0.02f}};
    bezierPatch.segments = 28;

    mfd::PrimitivePatch arcPatch;
    arcPatch.radius = 0.26f;
    arcPatch.startAngleDegrees = -90.0f;
    arcPatch.endAngleDegrees = 135.0f;
    arcPatch.segments = 36;
    arcPatch.fillColor = mfd::ColorRgba {90, 120, 160, 200};
    arcPatch.filled = true;

    mfd::ReticlePatch patch;
    patch.primitivePatches.emplace("ring", ringPatch);
    patch.primitivePatches.emplace("triangle", trianglePatch);
    patch.primitivePatches.emplace("polyline", polylinePatch);
    patch.primitivePatches.emplace("bezier", bezierPatch);
    patch.primitivePatches.emplace("arc", arcPatch);

    EXPECT_TRUE(registry.ApplyReticlePatch("Radar", "geometry", patch));

    const auto reticles = registry.CollectPageReticlePointers("Radar");
    const mfd::ReticleGroup* geometryReticle = FindReticle(reticles, "geometry");
    ASSERT_NE(geometryReticle, nullptr);

    const auto* ringPrimitive = mfd::FindPrimitive(*geometryReticle, "ring");
    ASSERT_NE(ringPrimitive, nullptr);
    const auto* ringGeometry = std::get_if<mfd::RingGeometry>(&ringPrimitive->geometry);
    ASSERT_NE(ringGeometry, nullptr);
    EXPECT_EQ(ringGeometry->segments, 48);
    EXPECT_TRUE(ringPrimitive->style.filled);
    EXPECT_EQ(ringPrimitive->style.fillColor.r, 1U);

    const auto* trianglePrimitive = mfd::FindPrimitive(*geometryReticle, "triangle");
    ASSERT_NE(trianglePrimitive, nullptr);
    const auto* triangleGeometry = std::get_if<mfd::TriangleGeometry>(&trianglePrimitive->geometry);
    ASSERT_NE(triangleGeometry, nullptr);
    EXPECT_FLOAT_EQ(triangleGeometry->points[1].y, 0.25f);

    const auto* polylinePrimitive = mfd::FindPrimitive(*geometryReticle, "polyline");
    ASSERT_NE(polylinePrimitive, nullptr);
    const auto* polylineGeometry = std::get_if<mfd::PolylineGeometry>(&polylinePrimitive->geometry);
    ASSERT_NE(polylineGeometry, nullptr);
    EXPECT_TRUE(polylineGeometry->closed);
    ASSERT_EQ(polylineGeometry->points.size(), 4U);
    EXPECT_FLOAT_EQ(polylineGeometry->points[3].x, 0.20f);
    EXPECT_TRUE(polylinePrimitive->style.filled);
    EXPECT_EQ(polylinePrimitive->style.fillColor.g, 30U);

    const auto* bezierPrimitive = mfd::FindPrimitive(*geometryReticle, "bezier");
    ASSERT_NE(bezierPrimitive, nullptr);
    const auto* bezierGeometry = std::get_if<mfd::BezierGeometry>(&bezierPrimitive->geometry);
    ASSERT_NE(bezierGeometry, nullptr);
    EXPECT_EQ(bezierGeometry->segments, 28);
    ASSERT_EQ(bezierGeometry->controlPoints.size(), 4U);
    EXPECT_FLOAT_EQ(bezierGeometry->controlPoints[1].y, 0.22f);

    const auto* arcPrimitive = mfd::FindPrimitive(*geometryReticle, "arc");
    ASSERT_NE(arcPrimitive, nullptr);
    const auto* arcGeometry = std::get_if<mfd::ArcGeometry>(&arcPrimitive->geometry);
    ASSERT_NE(arcGeometry, nullptr);
    EXPECT_FLOAT_EQ(arcGeometry->radius, 0.26f);
    EXPECT_FLOAT_EQ(arcGeometry->startAngleDegrees, -90.0f);
    EXPECT_FLOAT_EQ(arcGeometry->endAngleDegrees, 135.0f);
    EXPECT_EQ(arcGeometry->segments, 36);
    EXPECT_TRUE(arcPrimitive->style.filled);
    EXPECT_EQ(arcPrimitive->style.fillColor.b, 160U);
}

TEST(SceneRegistryTests, DynamicReticlesSupportLifecyclePatchingAndOrdering)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup dynamicOne = MakeDynamicTextReticle("track_alpha");
    dynamicOne.sourceTemplateId = "track_template";
    dynamicOne.info.label = "Track Alpha";
    dynamicOne.info.category = "track";
    dynamicOne.info.metadata["threat"] = "high";
    dynamicOne.transform.position = {0.1f, 0.2f};

    mfd::ReticleGroup dynamicTwo = MakeDynamicTextReticle("track_bravo");
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

    registry.UpsertDynamicReticle("Radar", MakeDynamicTextReticle("track_charlie"));
    mfd::PageDefinition secondaryPage;
    secondaryPage.name = "Nav";
    secondaryPage.normalizedName = "nav";
    secondaryPage.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});
    secondaryPage.dynamicReticleBindings.push_back(
        mfd::DynamicReticleLayerBinding {std::string(kDefaultDynamicTemplateId), std::string(kDefaultLayerId), 0U});
    secondaryPage.staticReticles.push_back(MakeReticle("nav_symbol"));

    mfd::MfdDocument secondDocument;
    secondDocument.pages.push_back(MakeRuntimePage());
    secondDocument.pages.push_back(std::move(secondaryPage));

    mfd::SceneRegistry secondRegistry(std::move(secondDocument));
    secondRegistry.UpsertDynamicReticle("Radar", MakeDynamicTextReticle("track_delta"));
    secondRegistry.UpsertDynamicReticle("Nav", MakeDynamicTextReticle("ghost"));
    secondRegistry.ClearAllDynamicReticles();
    EXPECT_FALSE(secondRegistry.HasDynamicReticle("Radar", "track_delta"));
    EXPECT_FALSE(secondRegistry.HasDynamicReticle("Nav", "ghost"));
}

TEST(SceneRegistryTests, ActiveReticleViewsTrackPerPageDynamicLifecycle)
{
    mfd::PageDefinition radarPage = MakeRuntimePage();
    radarPage.staticReticles[0].blink = {};
    radarPage.staticReticles[1].blink = {};

    mfd::PageDefinition navPage;
    navPage.name = "Nav";
    navPage.normalizedName = "nav";
    navPage.title = "Navigation";
    navPage.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});
    navPage.dynamicReticleBindings.push_back(
        mfd::DynamicReticleLayerBinding {std::string(kDefaultDynamicTemplateId), std::string(kDefaultLayerId), 0U});
    navPage.staticReticles.push_back(MakeReticle("nav_symbol"));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(radarPage));
    document.pages.push_back(std::move(navPage));

    mfd::SceneRegistry registry(std::move(document));

    registry.UpsertDynamicReticle("Radar", MakeDynamicTextReticle("track_alpha"));
    registry.UpsertDynamicReticle("Nav", MakeDynamicTextReticle("ghost"));

    const auto radarViews = registry.CollectActiveReticleViews();
    ASSERT_EQ(radarViews.size(), 5U);
    EXPECT_EQ(radarViews[0].group->id, "default");
    EXPECT_EQ(radarViews[1].group->id, "caution");
    EXPECT_EQ(radarViews[2].group->id, "textual");
    EXPECT_EQ(radarViews[3].group->id, "track_alpha");
    EXPECT_EQ(radarViews[4].group->id, "strobe");

    registry.SetActivePage("Nav");

    const auto navViews = registry.CollectActiveReticleViews();
    ASSERT_EQ(navViews.size(), 2U);
    EXPECT_EQ(navViews[0].group->id, "nav_symbol");
    EXPECT_EQ(navViews[1].group->id, "ghost");

    std::vector<mfd::ReticleRenderView> navDestination;
    registry.CollectActiveReticleViews(navDestination);
    ASSERT_EQ(navDestination.size(), navViews.size());
    for (std::size_t index = 0; index < navViews.size(); ++index)
    {
        ASSERT_NE(navDestination[index].group, nullptr);
        ASSERT_NE(navViews[index].group, nullptr);
        EXPECT_EQ(navDestination[index].group->id, navViews[index].group->id);
        EXPECT_EQ(navDestination[index].visible, navViews[index].visible);
    }

    EXPECT_TRUE(registry.RemoveDynamicReticle("Nav", "ghost"));

    const auto navViewsAfterRemoval = registry.CollectActiveReticleViews();
    ASSERT_EQ(navViewsAfterRemoval.size(), 1U);
    EXPECT_EQ(navViewsAfterRemoval[0].group->id, "nav_symbol");

    registry.SetActivePage("Radar");

    const auto radarViewsAfterSwitchBack = registry.CollectActiveReticleViews();
    ASSERT_EQ(radarViewsAfterSwitchBack.size(), 5U);
    EXPECT_EQ(radarViewsAfterSwitchBack[3].group->id, "track_alpha");
    EXPECT_EQ(radarViewsAfterSwitchBack[4].group->id, "strobe");
}

TEST(SceneRegistryTests, StrobeMagnetizationAndCaptureTrackNearestVisibleDynamicReticle)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup nearTrack = MakeDynamicTextReticle("near_track", "track_template");
    nearTrack.sourceTemplateId = "track_template";
    nearTrack.info.label = "Near";
    nearTrack.info.category = "friendly";
    nearTrack.info.metadata["callsign"] = "N1";
    nearTrack.transform.position = {0.1f, 0.0f};

    mfd::ReticleGroup farTrack = MakeDynamicTextReticle("far_track", "track_template");
    farTrack.transform.position = {0.32f, 0.0f};

    mfd::ReticleGroup hiddenTrack = MakeDynamicTextReticle("hidden_track", "track_template");
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

    const mfd::ReticleGroup* strobeReticle = FindReticle(registry.CollectActiveReticlePointers(), "strobe");
    ASSERT_NE(strobeReticle, nullptr);
    ASSERT_FALSE(strobeReticle->primitives.empty());
    EXPECT_EQ(strobeReticle->primitives.front().type, mfd::PrimitiveType::Circle);
    const auto* authoredCircle = std::get_if<mfd::CircleGeometry>(&strobeReticle->primitives.front().geometry);
    ASSERT_NE(authoredCircle, nullptr);
    EXPECT_FLOAT_EQ(authoredCircle->radius, 0.05f);

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

TEST(SceneRegistryTests, InactivePageStrobeStateStaysUnavailableUntilPageBecomesActive)
{
    mfd::PageDefinition radarPage = MakeRuntimePage();
    radarPage.defaultPage = true;

    mfd::PageDefinition navPage = MakeRuntimePage();
    navPage.name = "Nav";
    navPage.normalizedName = "nav";
    navPage.title = "Navigation";
    navPage.defaultPage = false;

    mfd::MfdDocument document;
    document.pages.push_back(std::move(radarPage));
    document.pages.push_back(std::move(navPage));

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup navTrack = MakeDynamicTextReticle("nav_track", "track_template");
    navTrack.transform.position = {0.1f, 0.0f};
    registry.UpsertDynamicReticle("Nav", std::move(navTrack));

    ASSERT_TRUE(registry.SetStrobePosition("Nav", {0.08f, 0.02f}));
    EXPECT_FALSE(registry.StrobeForPage("Nav").has_value());
    EXPECT_FALSE(registry.StrobeMagnetForPage("Nav").has_value());
    EXPECT_FALSE(registry.CaptureWithStrobe("Nav").has_value());

    registry.SetActivePage("Nav");

    const auto strobe = registry.StrobeForPage("Nav");
    ASSERT_TRUE(strobe.has_value());
    EXPECT_TRUE(strobe->visible);
    EXPECT_FLOAT_EQ(strobe->position.x, 0.08f);
    EXPECT_FLOAT_EQ(strobe->position.y, 0.02f);

    const auto magnet = registry.StrobeMagnetForPage("Nav");
    ASSERT_TRUE(magnet.has_value());
    EXPECT_TRUE(magnet->enabled);

    const auto capture = registry.CaptureWithStrobe("Nav");
    ASSERT_TRUE(capture.has_value());
    EXPECT_EQ(capture->pageName, "Nav");
    EXPECT_EQ(capture->reticleId, "nav_track");
}

TEST(SceneRegistryTests, StrobeMagnetVisualShapeIsOptionalAndRestoresAuthoredReticle)
{
    mfd::PageDefinition page = MakeRuntimePage();
    mfd::PageStrobeDefinition* strobe = mfd::FindActivePageStrobeDefinition(page);
    ASSERT_NE(strobe, nullptr);
    strobe->reticle.overrides.thickness = 0.007f;
    strobe->reticle.clipping.mode = mfd::ReticleClipMode::Outer;
    strobe->reticle.clipping.primitiveId = "shape";
    strobe->magnet.visualShapeEnabled = true;
    strobe->magnet.visualShape = mfd::StrobeMagnetVisualShape::Square;
    strobe->magnet.visualShapeSize = 0.22f;

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup track = MakeDynamicTextReticle("track_alpha", "track_template");
    track.transform.position = {0.10f, 0.00f};
    registry.UpsertDynamicReticle("Radar", std::move(track));

    ASSERT_TRUE(registry.SetStrobePosition("Radar", {0.08f, 0.01f}));

    const mfd::ReticleGroup* magnetizedStrobe = FindReticle(registry.CollectActiveReticlePointers(), "strobe");
    ASSERT_NE(magnetizedStrobe, nullptr);
    ASSERT_EQ(magnetizedStrobe->primitives.size(), 1U);
    EXPECT_EQ(magnetizedStrobe->primitives.front().id, "magnet_visual_shape");
    EXPECT_EQ(magnetizedStrobe->primitives.front().type, mfd::PrimitiveType::Square);
    const auto* visualSquare = std::get_if<mfd::SquareGeometry>(&magnetizedStrobe->primitives.front().geometry);
    ASSERT_NE(visualSquare, nullptr);
    EXPECT_FLOAT_EQ(visualSquare->width, 0.22f);
    EXPECT_FLOAT_EQ(visualSquare->height, 0.22f);
    EXPECT_EQ(magnetizedStrobe->clipping.mode, mfd::ReticleClipMode::None);
    ASSERT_TRUE(magnetizedStrobe->overrides.thickness.has_value());
    EXPECT_FLOAT_EQ(*magnetizedStrobe->overrides.thickness, 0.007f);

    const auto magnet = registry.ActiveStrobeMagnetSummary();
    ASSERT_TRUE(magnet.has_value());
    EXPECT_TRUE(magnet->magnetized);
    EXPECT_EQ(magnet->reticleId, "track_alpha");

    ASSERT_TRUE(registry.SetReticleColor("Radar", "strobe", mfd::ColorRgba {220, 120, 32, 255}));
    mfd::ReticlePatch authoredPatchWhileMagnetized;
    mfd::PrimitivePatch shapePatch;
    shapePatch.radius = 0.07f;
    authoredPatchWhileMagnetized.primitivePatches.emplace("shape", shapePatch);
    ASSERT_TRUE(registry.ApplyReticlePatch("Radar", "strobe", authoredPatchWhileMagnetized));

    ASSERT_TRUE(registry.SetStrobePosition("Radar", {0.45f, 0.12f}));

    const mfd::ReticleGroup* restoredStrobe = FindReticle(registry.CollectActiveReticlePointers(), "strobe");
    ASSERT_NE(restoredStrobe, nullptr);
    ASSERT_EQ(restoredStrobe->primitives.size(), 1U);
    EXPECT_EQ(restoredStrobe->primitives.front().id, "shape");
    EXPECT_EQ(restoredStrobe->primitives.front().type, mfd::PrimitiveType::Circle);
    const auto* restoredCircle = std::get_if<mfd::CircleGeometry>(&restoredStrobe->primitives.front().geometry);
    ASSERT_NE(restoredCircle, nullptr);
    EXPECT_FLOAT_EQ(restoredCircle->radius, 0.07f);
    EXPECT_EQ(restoredStrobe->clipping.mode, mfd::ReticleClipMode::Outer);
    EXPECT_EQ(restoredStrobe->clipping.primitiveId, "shape");
    ASSERT_TRUE(restoredStrobe->overrides.color.has_value());
    EXPECT_EQ(restoredStrobe->overrides.color->r, 220);
    EXPECT_EQ(restoredStrobe->overrides.color->g, 120);
    EXPECT_EQ(restoredStrobe->overrides.color->b, 32);
    EXPECT_EQ(restoredStrobe->overrides.color->a, 255);
    ASSERT_TRUE(restoredStrobe->overrides.thickness.has_value());
    EXPECT_FLOAT_EQ(*restoredStrobe->overrides.thickness, 0.007f);

    const auto magnetAfterMove = registry.ActiveStrobeMagnetSummary();
    ASSERT_TRUE(magnetAfterMove.has_value());
    EXPECT_FALSE(magnetAfterMove->magnetized);
    EXPECT_TRUE(magnetAfterMove->reticleId.empty());
}

TEST(SceneRegistryTests, StrobeMagnetizationIgnoresNonFiniteDynamicPositions)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup invalidTrack = MakeDynamicTextReticle("invalid_track");
    invalidTrack.transform.position = {std::numeric_limits<float>::quiet_NaN(), 0.0f};
    registry.UpsertDynamicReticle("Radar", std::move(invalidTrack));

    ASSERT_TRUE(registry.SetStrobePosition("Radar", {0.0f, 0.0f}));

    const auto magnet = registry.ActiveStrobeMagnetSummary();
    ASSERT_TRUE(magnet.has_value());
    EXPECT_TRUE(magnet->enabled);
    EXPECT_FALSE(magnet->magnetized);
    EXPECT_TRUE(magnet->reticleId.empty());
    EXPECT_TRUE(std::isfinite(magnet->distance));
}

TEST(SceneRegistryTests, StrobeMagnetizationFollowsMovingDynamicReticle)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup track = MakeDynamicTextReticle("track_alpha", "track_template");
    track.sourceTemplateId = "track_template";
    track.transform.position = {0.10f, 0.00f};
    registry.UpsertDynamicReticle("Radar", std::move(track));

    ASSERT_TRUE(registry.SetStrobePosition("Radar", {0.08f, 0.01f}));
    ASSERT_TRUE(registry.ActiveStrobeSummary().has_value());
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.x, 0.10f);
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.y, 0.00f);

    mfd::ReticlePatch patch;
    patch.position = mfd::Vec2 {0.18f, -0.04f};
    EXPECT_TRUE(registry.ApplyDynamicReticlePatch("Radar", "track_alpha", patch));

    const auto strobe = registry.ActiveStrobeSummary();
    ASSERT_TRUE(strobe.has_value());
    EXPECT_TRUE(strobe->visible);
    EXPECT_FLOAT_EQ(strobe->position.x, 0.18f);
    EXPECT_FLOAT_EQ(strobe->position.y, -0.04f);

    const auto magnet = registry.ActiveStrobeMagnetSummary();
    ASSERT_TRUE(magnet.has_value());
    EXPECT_TRUE(magnet->enabled);
    EXPECT_TRUE(magnet->magnetized);
    EXPECT_EQ(magnet->reticleId, "track_alpha");
    EXPECT_FLOAT_EQ(magnet->targetPosition.x, 0.18f);
    EXPECT_FLOAT_EQ(magnet->targetPosition.y, -0.04f);
    EXPECT_NEAR(magnet->distance, 0.0f, 1e-5f);

    const auto capture = registry.CaptureActivePageStrobe();
    ASSERT_TRUE(capture.has_value());
    EXPECT_EQ(capture->reticleId, "track_alpha");
    EXPECT_EQ(capture->sourceTemplateId, "track_template");
    EXPECT_FLOAT_EQ(capture->position.x, 0.18f);
    EXPECT_FLOAT_EQ(capture->position.y, -0.04f);
}

TEST(SceneRegistryTests, ManualStrobeMoveBreaksStickyMagnetization)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup track = MakeDynamicTextReticle("track_alpha");
    track.transform.position = {0.10f, 0.00f};
    registry.UpsertDynamicReticle("Radar", std::move(track));

    ASSERT_TRUE(registry.SetStrobePosition("Radar", {0.09f, 0.01f}));
    ASSERT_TRUE(registry.ActiveStrobeSummary().has_value());
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.x, 0.10f);

    mfd::ReticlePatch patch;
    patch.position = mfd::Vec2 {0.18f, -0.04f};
    EXPECT_TRUE(registry.ApplyDynamicReticlePatch("Radar", "track_alpha", patch));
    ASSERT_TRUE(registry.ActiveStrobeSummary().has_value());
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.x, 0.18f);
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.y, -0.04f);

    ASSERT_TRUE(registry.SetStrobePosition("Radar", {0.45f, 0.12f}));
    ASSERT_TRUE(registry.ActiveStrobeSummary().has_value());
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.x, 0.45f);
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.y, 0.12f);

    const auto magnetAfterManualMove = registry.ActiveStrobeMagnetSummary();
    ASSERT_TRUE(magnetAfterManualMove.has_value());
    EXPECT_FALSE(magnetAfterManualMove->magnetized);
    EXPECT_TRUE(magnetAfterManualMove->reticleId.empty());

    patch.position = mfd::Vec2 {0.24f, -0.08f};
    EXPECT_TRUE(registry.ApplyDynamicReticlePatch("Radar", "track_alpha", patch));
    ASSERT_TRUE(registry.ActiveStrobeSummary().has_value());
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.x, 0.45f);
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.y, 0.12f);
}

TEST(SceneRegistryTests, DynamicTemplateVisibilityMasksOnlyMatchingDynamicReticles)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));

    mfd::ReticleGroup track = MakeDynamicTextReticle("track_alpha", "radar_tracks");
    track.sourceTemplateId = "radar_tracks";
    mfd::ReticleGroup waypoint = MakeDynamicTextReticle("wp_alpha", "waypoints");
    waypoint.sourceTemplateId = "waypoints";

    registry.UpsertDynamicReticle("Radar", std::move(track));
    registry.UpsertDynamicReticle("Radar", std::move(waypoint));

    auto views = registry.CollectPageReticleViews("Radar");
    ASSERT_EQ(views.size(), 6U);
    EXPECT_EQ(views[3].group->id, "track_alpha");
    EXPECT_EQ(views[4].group->id, "wp_alpha");
    EXPECT_TRUE(views[3].visible);
    EXPECT_TRUE(views[4].visible);

    EXPECT_TRUE(registry.SetDynamicReticleSetVisible("Radar", "radar_tracks", false));
    views = registry.CollectPageReticleViews("Radar");
    ASSERT_EQ(views.size(), 6U);
    EXPECT_FALSE(views[3].visible);
    EXPECT_TRUE(views[4].visible);

    EXPECT_TRUE(registry.SetDynamicReticleSetVisible("Radar", "radar_tracks", true));
    views = registry.CollectPageReticleViews("Radar");
    ASSERT_EQ(views.size(), 6U);
    EXPECT_TRUE(views[3].visible);
    EXPECT_TRUE(views[4].visible);
}

TEST(SceneRegistryTests, ResetToInitialStateRestoresDefaultPageWindowAndViewsAndClearsDynamics)
{
    mfd::PageDefinition radarPage = MakeRuntimePage();
    radarPage.defaultPage = true;

    mfd::PageDefinition navPage;
    navPage.name = "Nav";
    navPage.normalizedName = "nav";
    navPage.title = "Navigation";
    navPage.view.center = {0.35f, 0.1f};
    navPage.view.zoom = 1.75f;
    navPage.layers.push_back(mfd::PageLayerDefinition {std::string(kDefaultLayerId)});
    navPage.dynamicReticleBindings.push_back(
        mfd::DynamicReticleLayerBinding {std::string(kDefaultDynamicTemplateId), std::string(kDefaultLayerId), 0U});
    navPage.staticReticles.push_back(MakeReticle("nav_symbol"));

    mfd::MfdDocument document;
    document.pages.push_back(std::move(radarPage));
    document.pages.push_back(std::move(navPage));

    mfd::SceneRegistry registry(std::move(document));

    EXPECT_TRUE(registry.SetPageViewCenter("Radar", {0.7f, 0.8f}));
    EXPECT_TRUE(registry.SetPageZoom("Radar", 2.4f));
    EXPECT_TRUE(registry.SetPageViewCenter("Nav", {-0.6f, -0.4f}));
    EXPECT_TRUE(registry.SetPageZoom("Nav", 2.9f));
    EXPECT_TRUE(registry.SetWindowColorInverted(true));
    EXPECT_TRUE(registry.SetWindowBrightness(0.15f));
    EXPECT_TRUE(registry.SetWindowDisabled(true));
    registry.UpsertDynamicReticle("Radar", MakeDynamicTextReticle("track_alpha"));
    registry.UpsertDynamicReticle("Nav", MakeDynamicTextReticle("ghost"));
    registry.SetActivePage("Nav");

    registry.ResetToInitialState();

    EXPECT_EQ(registry.ActivePageName(), "Radar");
    ASSERT_TRUE(registry.ViewForPage("Radar").has_value());
    EXPECT_FLOAT_EQ(registry.ViewForPage("Radar")->center.x, 0.1f);
    EXPECT_FLOAT_EQ(registry.ViewForPage("Radar")->center.y, -0.2f);
    EXPECT_FLOAT_EQ(registry.ViewForPage("Radar")->zoom, 1.25f);
    ASSERT_TRUE(registry.ViewForPage("Nav").has_value());
    EXPECT_FLOAT_EQ(registry.ViewForPage("Nav")->center.x, 0.35f);
    EXPECT_FLOAT_EQ(registry.ViewForPage("Nav")->center.y, 0.1f);
    EXPECT_FLOAT_EQ(registry.ViewForPage("Nav")->zoom, 1.75f);
    EXPECT_FALSE(registry.WindowDisplay().invertColors);
    EXPECT_FLOAT_EQ(registry.WindowDisplay().brightness, 1.0f);
    EXPECT_FALSE(registry.WindowDisplay().disabled);
    EXPECT_FALSE(registry.HasDynamicReticle("Radar", "track_alpha"));
    EXPECT_FALSE(registry.HasDynamicReticle("Nav", "ghost"));

    const auto strobe = registry.StrobeForPage("Radar");
    ASSERT_TRUE(strobe.has_value());
    EXPECT_TRUE(strobe->visible);
    EXPECT_FLOAT_EQ(strobe->position.x, 0.0f);
    EXPECT_FLOAT_EQ(strobe->position.y, 0.0f);
}

TEST(SceneRegistryTests, CommandProcessorResetWindowCommandResetsRuntimeState)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));
    mfd::CommandProcessor processor(registry);

    EXPECT_TRUE(registry.SetPageViewCenter("Radar", {0.4f, 0.5f}));
    EXPECT_TRUE(registry.SetWindowDisabled(true));
    registry.UpsertDynamicReticle("Radar", MakeDynamicTextReticle("track_alpha"));
    EXPECT_TRUE(registry.SetStrobeActive("Radar", false));

    EXPECT_TRUE(processor.Submit(mfd::UserCommand {mfd::ResetWindowCommand {}}));
    EXPECT_TRUE(processor.LastError().empty());
    EXPECT_EQ(registry.ActivePageName(), "Radar");
    EXPECT_FALSE(registry.WindowDisplay().disabled);
    ASSERT_TRUE(registry.ViewForPage("Radar").has_value());
    EXPECT_FLOAT_EQ(registry.ViewForPage("Radar")->center.x, 0.1f);
    EXPECT_FLOAT_EQ(registry.ViewForPage("Radar")->center.y, -0.2f);
    EXPECT_FLOAT_EQ(registry.ViewForPage("Radar")->zoom, 1.25f);
    EXPECT_FALSE(registry.HasDynamicReticle("Radar", "track_alpha"));
    ASSERT_TRUE(registry.ActiveStrobeSummary().has_value());
    EXPECT_TRUE(registry.ActiveStrobeSummary()->visible);
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.x, 0.0f);
    EXPECT_FLOAT_EQ(registry.ActiveStrobeSummary()->position.y, 0.0f);
}

TEST(SceneRegistryTests, RuntimeSettersRejectNonFiniteAndOutOfBudgetValues)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));
    const auto initialView = registry.ViewForPage("Radar");
    ASSERT_TRUE(initialView.has_value());

    EXPECT_FALSE(registry.SetPageView("Radar",
                                      mfd::PageViewState {
                                          {0.0f, 0.0f},
                                          std::numeric_limits<float>::infinity()}));
    EXPECT_FALSE(registry.SetPageViewCenter(
        "Radar",
        {std::numeric_limits<float>::quiet_NaN(), 0.0f}));
    EXPECT_FALSE(registry.SetPageZoom("Radar", 50000.0f));
    EXPECT_FALSE(registry.SetReticlePosition(
        "Radar",
        "default",
        {std::numeric_limits<float>::infinity(), 0.0f}));
    EXPECT_FALSE(registry.SetReticleRotation(
        "Radar",
        "default",
        std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(registry.SetStrobePosition(
        "Radar",
        {0.0f, std::numeric_limits<float>::quiet_NaN()}));

    const auto finalView = registry.ViewForPage("Radar");
    ASSERT_TRUE(finalView.has_value());
    EXPECT_FLOAT_EQ(finalView->center.x, initialView->center.x);
    EXPECT_FLOAT_EQ(finalView->center.y, initialView->center.y);
    EXPECT_FLOAT_EQ(finalView->zoom, initialView->zoom);
}

TEST(SceneRegistryTests, ApplyReticlePatchRejectsInvalidPayloadsWithoutMutatingState)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));
    const mfd::ReticleGroup* textual = FindReticle(registry.CollectPageReticlePointers("Radar"), "textual");
    ASSERT_NE(textual, nullptr);
    const auto* originalText = FindTextGeometry(*textual, "title");
    ASSERT_NE(originalText, nullptr);

    mfd::ReticlePatch invalidPatch;
    invalidPatch.position = mfd::Vec2 {std::numeric_limits<float>::infinity(), 0.0f};
    invalidPatch.scale = mfd::Vec2 {0.0f, 1.0f};
    invalidPatch.text = std::string(5000U, 'X');

    EXPECT_FALSE(registry.ApplyReticlePatch("Radar", "textual", invalidPatch));

    textual = FindReticle(registry.CollectPageReticlePointers("Radar"), "textual");
    ASSERT_NE(textual, nullptr);
    const auto* patchedText = FindTextGeometry(*textual, "title");
    ASSERT_NE(patchedText, nullptr);
    EXPECT_EQ(textual->transform.position.x, 0.0f);
    EXPECT_EQ(textual->transform.position.y, 0.0f);
    EXPECT_FLOAT_EQ(textual->transform.scale.x, 1.0f);
    EXPECT_FLOAT_EQ(textual->transform.scale.y, 1.0f);
    EXPECT_EQ(patchedText->text, "INIT");
}

TEST(SceneRegistryTests, ApplyDynamicReticlePatchRejectsInvalidPayloadsWithoutMutatingState)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeRuntimePage());

    mfd::SceneRegistry registry(std::move(document));
    mfd::ReticleGroup track = MakeDynamicTextReticle("track_alpha", "track_template");
    track.transform.position = {0.10f, 0.00f};
    registry.UpsertDynamicReticle("Radar", std::move(track));

    mfd::ReticlePatch invalidPatch;
    invalidPatch.position = mfd::Vec2 {std::numeric_limits<float>::quiet_NaN(), 0.0f};
    invalidPatch.scale = mfd::Vec2 {1.0f, 0.0f};

    EXPECT_FALSE(registry.ApplyDynamicReticlePatch("Radar", "track_alpha", invalidPatch));

    const mfd::ReticleGroup* dynamic = FindReticle(registry.CollectPageReticlePointers("Radar"), "track_alpha");
    ASSERT_NE(dynamic, nullptr);
    EXPECT_FLOAT_EQ(dynamic->transform.position.x, 0.10f);
    EXPECT_FLOAT_EQ(dynamic->transform.position.y, 0.00f);
    EXPECT_FLOAT_EQ(dynamic->transform.scale.x, 1.0f);
    EXPECT_FLOAT_EQ(dynamic->transform.scale.y, 1.0f);
}

TEST(SceneRegistryTests, MissingTransportPageReferenceLeavesGeneratedReticleLookupUnusable)
{
    mfd::MfdDocument document;
    document.pages.push_back(MakeBlinkPage());

    mfd::GeneratedTransportMap map;
    map.mappingHash = "map_hash";
    map.pages.push_back({11U, "Radar", "radar", true, false});
    map.reticles.push_back({22U, 999U, "heading_box", "heading_box", "static"});

    mfd::SceneRegistry registry(std::move(document), std::move(map));
    mfd::CommandProcessor processor(registry);

    mfd::CommandBatch batch;
    batch.mappingHash = "map_hash";
    batch.commands.push_back(
        mfd::UpdateReticleCommand {mfd::StaticReticleHandle {"", "", 11U, 22U}, {}});

    EXPECT_FALSE(processor.Submit(batch));
    EXPECT_EQ(processor.LastError(), "Unknown generated static reticle transport id 22");
}
