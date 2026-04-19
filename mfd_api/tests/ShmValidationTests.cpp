/*
 * This file is part of MFDStudio.
 */
/**
 * @file
 * @brief GoogleTest coverage for SHM packet and fixed payload validation.
 */

#include <gtest/gtest.h>

#include "mfd/core/Validation.h"

TEST(ShmValidationTests, RejectsInvalidPacketMagic)
{
    mfd::ShmPacket packet;
    packet.magic = 0U;

    std::string error;
    EXPECT_FALSE(mfd::ValidateShmPacket(packet, &error));
    EXPECT_FALSE(error.empty());
}

TEST(ShmValidationTests, RejectsOversizedRadarFrameCount)
{
    mfd::RadarFrame frame;
    frame.count = 257U;

    std::string error;
    EXPECT_FALSE(mfd::ValidateRadarFrame(frame, &error));
    EXPECT_FALSE(error.empty());
}
