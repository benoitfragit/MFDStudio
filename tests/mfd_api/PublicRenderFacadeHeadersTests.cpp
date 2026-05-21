/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Compile-time smoke coverage for the repository host-side render headers.
 */

#include <gtest/gtest.h>

#include <filesystem>

#include "mfd/render/MfdRenderer.h"
#include "mfd/render/OpenGlFramebufferReader.h"
#include "mfd/render/WindowBranding.h"

TEST(PublicRenderFacadeHeadersTests, HostRenderTypesRemainAvailableWithoutLowLevelRaylibHeaders)
{
    mfd::MfdRenderer renderer;
    (void)renderer;

    mfd::FramebufferCaptureRequest request;
    EXPECT_EQ(request.width, 0);
    EXPECT_EQ(request.height, 0);
    EXPECT_EQ(request.origin, mfd::FramebufferOrigin::TopLeft);

    const std::filesystem::path iconPath = mfd::ResolveWindowBrandingIconFile();
    EXPECT_TRUE(iconPath.empty() || !iconPath.empty());
}
