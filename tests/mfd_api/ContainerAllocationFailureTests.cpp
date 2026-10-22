/* This file is part of MFDStudio. Project author: Benoit Fra. */
#include "mfd/runtime/SceneRegistry.h"
#include <gtest/gtest.h>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

namespace allocation_test
{
thread_local std::ptrdiff_t remaining = -1;
thread_local bool failed = false;

class FailureScope final
{
public:
    explicit FailureScope(const std::ptrdiff_t count) noexcept { remaining = count; failed = false; }
    ~FailureScope() { remaining = -1; }
    FailureScope(const FailureScope&) = delete;
    FailureScope& operator=(const FailureScope&) = delete;
};
}

// This isolated executable links SceneRegistry statically. Replacing allocation
// here exercises the real noexcept boundary without changing production code or
// relying on interception across Windows DLL/CRT allocation boundaries.
void* operator new(const std::size_t size)
{
    if (allocation_test::remaining >= 0 && allocation_test::remaining-- == 0)
    {
        allocation_test::remaining = -1;
        allocation_test::failed = true;
        throw std::bad_alloc {};
    }
    if (void* memory = std::malloc(size == 0U ? 1U : size)) return memory;
    throw std::bad_alloc {};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

namespace
{
mfd::MfdDocument MakeDocument(const std::string& id)
{
    mfd::MfdDocument document;
    mfd::PageDefinition page;
    page.name = page.title = "Containers";
    page.normalizedName = "containers";
    page.layers.push_back(mfd::PageLayerDefinition {"default"});
    mfd::api1::ContainerDefinition container;
    container.id = id;
    page.containers.push_back(container);
    document.pages.push_back(std::move(page));
    return document;
}

void CheckOffset(const mfd::SceneRegistry& scene, const float expected)
{
    std::vector<mfd::api1::ContainerDefinition> definitions;
    scene.CollectActiveContainerDefinitions(definitions);
    ASSERT_EQ(definitions.size(), 1U);
    EXPECT_FLOAT_EQ(definitions.front().scrolling.offset.y, expected);
}
}

TEST(ContainerAllocationFailureTests, EveryInsertionAllocationFailurePreservesStateAndAllowsRetry)
{
    const std::string id(128U, 'p');
    bool reachedSuccess = false;
    std::size_t failedAllocations = 0U;
    for (std::ptrdiff_t failAfter = 0; failAfter < 32; ++failAfter)
    {
        mfd::SceneRegistry scene(MakeDocument(id));
        bool updated = false;
        {
            allocation_test::FailureScope failure(failAfter);
            updated = scene.SetActiveContainerScrollOffset(id, {0.0f, 0.5f});
        }
        if (!allocation_test::failed)
        {
            EXPECT_TRUE(updated);
            reachedSuccess = true;
            break;
        }
        ++failedAllocations;
        EXPECT_FALSE(updated);
        CheckOffset(scene, 0.0f);
        ASSERT_TRUE(scene.SetActiveContainerScrollOffset(id, {0.0f, 0.25f}));
        CheckOffset(scene, 0.25f);
    }
    EXPECT_TRUE(reachedSuccess);
    EXPECT_GT(failedAllocations, 0U);
}

TEST(ContainerAllocationFailureTests, FailedUpdateKeepsTheExistingOverride)
{
    const std::string id(128U, 'p');
    mfd::SceneRegistry scene(MakeDocument(id));
    ASSERT_TRUE(scene.SetActiveContainerScrollOffset(id, {0.0f, 0.25f}));
    bool updated = true;
    {
        allocation_test::FailureScope failure(0);
        updated = scene.SetActiveContainerScrollOffset(id, {0.0f, 0.5f});
    }
    EXPECT_TRUE(allocation_test::failed);
    EXPECT_FALSE(updated);
    CheckOffset(scene, 0.25f);
}
