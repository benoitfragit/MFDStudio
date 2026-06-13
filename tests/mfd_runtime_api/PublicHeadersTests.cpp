/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief Compile-time smoke coverage for the public runtime-host package headers.
 */

#include <gtest/gtest.h>

#include "mfd/runtime_api/OffscreenSurface.h"
#include "mfd/runtime_api/RuntimeSession.h"

TEST(RuntimeApiPublicHeadersTests, PublicTypesRemainAvailableWithoutMfdApiHeaders)
{
    EXPECT_NO_THROW(mfd::runtime_api::RuntimeSession {});
    EXPECT_NO_THROW(mfd::runtime_api::OffscreenSurface {});

    const mfd::runtime_api::OffscreenFrameView frameView {};
    EXPECT_FALSE(frameView.Ready());
}
