/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */

/** @file @brief Runtime DLL handshake and malformed-buffer tests for API LEVEL 1. */

#include "mfd/apilevel1/ContainerRuntimeAbi.h"
#include "ContainerRuntimeHostValidation.h"

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

TEST(ContainerRuntimeHostValidationTests, RejectsMalformedExtensionOutput)
{
    MfdContainerLayoutV1 layout {};
    layout.source_index = 0U;
    layout.parent_source_index = std::numeric_limits<std::size_t>::max();
    layout.bounds.size = {1.0f, 1.0f};
    layout.viewport.size = {1.0f, 1.0f};
    std::string error;
    ASSERT_TRUE(mfd::api1::detail::ValidateContainerRuntimeOutput(&layout, 1U, 1U, error)) << error;

    layout.bounds.center.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(mfd::api1::detail::ValidateContainerRuntimeOutput(&layout, 1U, 1U, error));
    layout.bounds.center.x = 0.0f;
    layout.effective_visible = 2U;
    EXPECT_FALSE(mfd::api1::detail::ValidateContainerRuntimeOutput(&layout, 1U, 1U, error));
    layout.effective_visible = 0U;
    layout.scroll_offset.x = 1.0f;
    EXPECT_FALSE(mfd::api1::detail::ValidateContainerRuntimeOutput(&layout, 1U, 1U, error));
    layout.scroll_offset.x = 0.0f;
    layout.source_index = 1U;
    EXPECT_FALSE(mfd::api1::detail::ValidateContainerRuntimeOutput(&layout, 1U, 1U, error));
}

TEST(ContainerRuntimeHostValidationTests, RejectsInconsistentHierarchy)
{
    std::array<MfdContainerLayoutV1, 2U> layouts {};
    layouts[0].source_index = 0U;
    layouts[0].parent_source_index = std::numeric_limits<std::size_t>::max();
    layouts[0].bounds.size = {1.0f, 1.0f};
    layouts[0].viewport.size = {1.0f, 1.0f};
    layouts[1] = layouts[0];
    layouts[1].source_index = 1U;
    layouts[1].parent_source_index = 0U;
    layouts[1].depth = 1U;
    std::string error;
    ASSERT_TRUE(mfd::api1::detail::ValidateContainerRuntimeOutput(
        layouts.data(), layouts.size(), layouts.size(), error)) << error;

    layouts[1].parent_source_index = 1U;
    EXPECT_FALSE(mfd::api1::detail::ValidateContainerRuntimeOutput(
        layouts.data(), layouts.size(), layouts.size(), error));
}
