/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/** @file @brief Runtime DLL handshake and malformed-buffer tests for API LEVEL 1. */

#include "mfd/apilevel1/ContainerRuntimeAbi.h"

#include <array>
#include <limits>

#include <gtest/gtest.h>

TEST(ContainerRuntimeAbiTests, RejectsSmallHandshakeAndReturnsCapabilities)
{
    std::array<char, 256U> message {};
    MfdContainersRuntimeApiV1 api {};
    api.struct_size = sizeof(api) - 1U;
    EXPECT_NE(mfd_get_containers_runtime_api_v1(&api, {message.data(), message.size()}), 0U);
    EXPECT_NE(message[0], '\0');

    api = {};
    api.struct_size = sizeof(api);
    ASSERT_EQ(mfd_get_containers_runtime_api_v1(&api, {message.data(), message.size()}), 0U);
    EXPECT_EQ(api.abi_version, MFD_CONTAINERS_RUNTIME_ABI_VERSION);
    EXPECT_NE(api.capabilities & MFD_CONTAINERS_CAPABILITY_LAYOUT, 0U);
    ASSERT_NE(api.evaluate_layout, nullptr);
}

TEST(ContainerRuntimeAbiTests, RejectsMalformedInputWithoutWritingPastOutput)
{
    MfdContainersRuntimeApiV1 api {};
    api.struct_size = sizeof(api);
    ASSERT_EQ(mfd_get_containers_runtime_api_v1(&api, {}), 0U);
    std::array<char, 256U> message {};
    std::size_t outputCount = 77U;
    EXPECT_NE(api.evaluate_layout(nullptr, 1U, nullptr, 0U, nullptr, 0U, &outputCount,
                                  {message.data(), message.size()}),
              0U);
    EXPECT_EQ(outputCount, 0U);
    EXPECT_NE(message[0], '\0');
}
