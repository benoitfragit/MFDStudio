/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/**
 * @file
 * @brief Unit tests covering page-preview draw-order resolution.
 */

#include "internal/application/EditorPagePreviewDrawOrderController.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
mfd::ReticleGroup MakeStaticReticle(const std::string& id, const std::string& layerId, const bool drawOnTop = false)
{
    mfd::ReticleGroup reticle;
    reticle.id = id;
    reticle.layerId = layerId;
    reticle.drawOnTop = drawOnTop;
    return reticle;
}

mfd::PageDefinition MakeLayeredPage()
{
    mfd::PageDefinition page;
    page.name = "Radar";
    page.layers = {
        mfd::PageLayerDefinition {"background"},
        mfd::PageLayerDefinition {"overlay"}};
    return page;
}
} // namespace

TEST(PagePreviewDrawOrderControllerTests, CollectStaticReticleDrawOrderMatchesRuntimeLayerAndPassOrder)
{
    mfd::PageDefinition page = MakeLayeredPage();
    page.staticReticles.push_back(MakeStaticReticle("overlay_normal", "overlay"));
    page.staticReticles.push_back(MakeStaticReticle("background_normal", "background"));
    page.staticReticles.push_back(MakeStaticReticle("overlay_top", "overlay", true));
    page.staticReticles.push_back(MakeStaticReticle("background_top", "background", true));

    editor::PagePreviewDrawOrderController controller;
    std::vector<int> orderedIndices;

    controller.CollectStaticReticleDrawOrder(page, orderedIndices);

    EXPECT_EQ(orderedIndices, std::vector<int>({1, 0, 3, 2}));
}

TEST(PagePreviewDrawOrderControllerTests, BuildStaticReticleDrawOrderKeyUsesCaseInsensitiveKnownLayersBeforeUnknownOnes)
{
    mfd::PageDefinition page = MakeLayeredPage();
    page.layers.push_back(mfd::PageLayerDefinition {"Tracks"});
    page.staticReticles.push_back(MakeStaticReticle("known", "tracks"));
    page.staticReticles.push_back(MakeStaticReticle("unknown", "ghost"));

    editor::PagePreviewDrawOrderController controller;

    const editor::PagePreviewDrawOrderKey knownKey = controller.BuildStaticReticleDrawOrderKey(page, 0);
    const editor::PagePreviewDrawOrderKey unknownKey = controller.BuildStaticReticleDrawOrderKey(page, 1);

    EXPECT_EQ(knownKey.layerOrder, 2U);
    EXPECT_EQ(unknownKey.layerOrder, page.layers.size());
    EXPECT_TRUE(knownKey < unknownKey);
}

TEST(PagePreviewDrawOrderControllerTests, BuildStrobeAndTitleDrawOrderKeysStayAboveStaticReticles)
{
    mfd::PageDefinition page = MakeLayeredPage();
    page.staticReticles.push_back(MakeStaticReticle("overlay_top", "overlay", true));

    editor::PagePreviewDrawOrderController controller;

    const editor::PagePreviewDrawOrderKey staticKey = controller.BuildStaticReticleDrawOrderKey(page, 0);
    const editor::PagePreviewDrawOrderKey strobeKey = controller.BuildStrobeDrawOrderKey(0U);
    const editor::PagePreviewDrawOrderKey titleKey = controller.BuildTitleDrawOrderKey();

    EXPECT_TRUE(staticKey < strobeKey);
    EXPECT_TRUE(strobeKey < titleKey);
}
