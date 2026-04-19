/*
 * This file is part of MFDStudio.
 */
/**
 * @file
 * @brief Non-blocking GoogleTest coverage for NamedShmMonoSlot synchronization.
 */

#include <gtest/gtest.h>

#include "mfd/ipc/ShmPacket.h"
#include "mfd/ipc/windows/NamedShmMonoSlot.h"

TEST(NamedShmMonoSlotTests, TimesOutCleanlyWhenNoData)
{
#ifndef _WIN32
    GTEST_SKIP() << "Named SHM/Event transport is only available on Windows";
#else
    const std::string prefix = "MFDStudioTests_NoData_";
    mfd::ipc::windows::NamedShmMonoSlot reader;
    mfd::ipc::windows::NamedShmMonoSlotConfig config;
    config.sharedMemoryName = prefix + "Mem";
    config.canWriteEventName = prefix + "CanWrite";
    config.hasDataEventName = prefix + "HasData";
    config.timeoutMs = 10U;

    ASSERT_TRUE(reader.Open(config, mfd::ipc::windows::MonoSlotRole::Reader));
    mfd::ShmPacket packet;
    EXPECT_FALSE(reader.Read(packet));
#endif
}
