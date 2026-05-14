#include <gtest/gtest.h>

#include "mfd/render/MfdRenderer.h"

namespace
{
TEST(MfdRendererOffscreenContractTests, TextureViewValidityReflectsBackendId)
{
    mfd::OffscreenTextureHandleView emptyView {};
    EXPECT_FALSE(emptyView.IsValid());

    mfd::OffscreenTextureHandleView validView {};
    validView.backendTextureId = 7U;
    EXPECT_TRUE(validView.IsValid());
}

TEST(MfdRendererOffscreenContractTests, ResultSucceededRequiresSuccessStatusAndValidTexture)
{
    mfd::OffscreenRenderResult result {};
    result.status = mfd::OffscreenRenderStatus::Success;
    EXPECT_FALSE(result.Succeeded());

    result.texture.backendTextureId = 42U;
    EXPECT_TRUE(result.Succeeded());

    result.status = mfd::OffscreenRenderStatus::RenderTargetUnavailable;
    EXPECT_FALSE(result.Succeeded());
}

TEST(MfdRendererOffscreenContractTests, RequestDefaultsToInvalidDimensions)
{
    const mfd::OffscreenRenderRequest request {};
    EXPECT_EQ(request.width, 0);
    EXPECT_EQ(request.height, 0);
}


TEST(MfdRendererOffscreenContractTests, DefaultResultCarriesUnavailableMessage)
{
    const mfd::OffscreenRenderResult result {};
    EXPECT_EQ(result.status, mfd::OffscreenRenderStatus::RenderTargetUnavailable);
    EXPECT_STREQ(result.message, "render target unavailable");
}
} // namespace
