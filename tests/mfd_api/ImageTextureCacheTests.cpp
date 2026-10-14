/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "ImageTextureCache.h"

#include <gtest/gtest.h>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
// This target links only the cache and these raylib entry points. Count ownership
// deterministically without creating an OpenGL window or exposing cache internals.
struct TextureLifetimeState
{
    bool windowReady = true;
    unsigned int nextId = 1U;
    std::unordered_set<unsigned int> live;
    std::vector<unsigned int> released;
};
TextureLifetimeState textureState;

class ImageTextureCacheTests : public testing::Test
{
protected:
    void SetUp() override { textureState = {}; }
    void TearDown() override { EXPECT_TRUE(textureState.live.empty()); }
};
} // namespace

bool IsWindowReady() { return textureState.windowReady; }
Texture2D LoadTexture(const char*)
{
    const unsigned int id = textureState.nextId++;
    textureState.live.insert(id);
    return Texture2D {id, 2, 2, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}
void UnloadTexture(const Texture2D texture)
{
    EXPECT_TRUE(textureState.windowReady);
    EXPECT_EQ(textureState.live.erase(texture.id), 1U);
    textureState.released.push_back(texture.id);
}
void SetTextureFilter(Texture2D, int) {}
int GetPixelDataSize(int width, int height, int) { return width * height * 4; }

TEST_F(ImageTextureCacheTests, MoveConstructionTransfersOwnershipAndLeavesSourceEmpty)
{
    unsigned int id = 0U;
    {
        mfd::ImageTextureCache source;
        const Texture2D* texture = source.Resolve("source.png");
        ASSERT_NE(texture, nullptr);
        id = texture->id;
        mfd::ImageTextureCache destination(std::move(source));
        EXPECT_EQ(source.Resolve("source.png"), nullptr);
        source.Clear();
        EXPECT_TRUE(textureState.released.empty());
        ASSERT_NE(destination.Resolve("source.png"), nullptr);
        EXPECT_EQ(destination.Resolve("source.png")->id, id);
    }
    ASSERT_EQ(textureState.released.size(), 1U);
    EXPECT_EQ(textureState.released.front(), id);
}

TEST_F(ImageTextureCacheTests, MoveAssignmentReleasesFilledDestinationAndTransfersSource)
{
    unsigned int sourceId = 0U;
    unsigned int destinationId = 0U;
    {
        mfd::ImageTextureCache source;
        mfd::ImageTextureCache destination;
        ASSERT_NE(source.Resolve("source.png"), nullptr);
        ASSERT_NE(destination.Resolve("destination.png"), nullptr);
        sourceId = source.Resolve("source.png")->id;
        destinationId = destination.Resolve("destination.png")->id;
        destination = std::move(source);
        ASSERT_EQ(textureState.released.size(), 1U);
        EXPECT_EQ(textureState.released.front(), destinationId);
        EXPECT_EQ(source.Resolve("source.png"), nullptr);
        source.Clear();
        ASSERT_NE(destination.Resolve("source.png"), nullptr);
        EXPECT_EQ(destination.Resolve("source.png")->id, sourceId);
    }
    ASSERT_EQ(textureState.released.size(), 2U);
    EXPECT_EQ(textureState.released.back(), sourceId);
}

TEST_F(ImageTextureCacheTests, MoveAssignmentIntoEmptyDestinationReleasesOnlyOnDestruction)
{
    {
        mfd::ImageTextureCache source;
        mfd::ImageTextureCache destination;
        ASSERT_NE(source.Resolve("source.png"), nullptr);
        const auto id = source.Resolve("source.png")->id;
        destination = std::move(source);
        EXPECT_TRUE(textureState.released.empty());
        ASSERT_NE(destination.Resolve("source.png"), nullptr);
        EXPECT_EQ(destination.Resolve("source.png")->id, id);
    }
    EXPECT_EQ(textureState.released.size(), 1U);
}

TEST_F(ImageTextureCacheTests, EmptySourceStillReleasesFilledDestination)
{
    mfd::ImageTextureCache source;
    mfd::ImageTextureCache destination;
    ASSERT_NE(destination.Resolve("destination.png"), nullptr);
    destination = std::move(source);
    EXPECT_EQ(textureState.released.size(), 1U);
    EXPECT_TRUE(textureState.live.empty());
    destination.Clear();
    EXPECT_EQ(textureState.released.size(), 1U);
}

TEST_F(ImageTextureCacheTests, SelfMovePreservesTextureAndMovedFromDestinationCanBeAssigned)
{
    mfd::ImageTextureCache source;
    ASSERT_NE(source.Resolve("source.png"), nullptr);
    const auto id = source.Resolve("source.png")->id;
    // Use an alias to exercise the runtime self-assignment guard.
    mfd::ImageTextureCache& alias = source;
    source = std::move(alias);
    EXPECT_TRUE(textureState.released.empty());
    EXPECT_EQ(source.Resolve("source.png")->id, id);
    mfd::ImageTextureCache destination(std::move(source));
    source = std::move(destination);
    EXPECT_EQ(destination.Resolve("source.png"), nullptr);
    EXPECT_EQ(source.Resolve("source.png")->id, id);
}

TEST_F(ImageTextureCacheTests, AssignmentAfterContextShutdownDoesNotCallGpuRelease)
{
    mfd::ImageTextureCache source;
    mfd::ImageTextureCache destination;
    ASSERT_NE(source.Resolve("source.png"), nullptr);
    ASSERT_NE(destination.Resolve("destination.png"), nullptr);
    textureState.windowReady = false;
    textureState.live.clear(); // context destruction reclaimed the GPU resources
    destination = std::move(source);
    destination.Clear();
    EXPECT_TRUE(textureState.released.empty());
}
