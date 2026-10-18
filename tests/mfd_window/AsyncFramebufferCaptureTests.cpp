/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#include "AsyncFramebufferCapture.h"
#include <gtest/gtest.h>
#include <array>

namespace
{
using mfd::window::detail::AsyncFramebufferCapture;
using mfd::window::detail::CapturedFramebuffer;
using mfd::window::detail::FenceStatus;
using mfd::window::detail::FramebufferReadbackApi;
using mfd::window::detail::GlSyncHandle;

struct ReadbackState
{
    bool contextReady = true;
    bool mapSucceeds = true;
    bool unmapSucceeds = true;
    bool syncSucceeds = true;
    FenceStatus status = FenceStatus::Ready;
    unsigned int generated = 0;
    unsigned int deleted = 0;
    unsigned int bound = 0;
    unsigned int lastRead = 0;
    unsigned int readCount = 0;
    unsigned int syncCount = 0;
    unsigned int diagnostics = 0;
    unsigned int polls = 0;
    unsigned int releasedFences = 0;
    unsigned int createdFences = 0;
    std::array<int, 2> handles {};
    std::array<bool, 2> activeFences {};
    std::array<std::array<std::byte, 64>, 2> buffers {};
};

class ControlledReadback final : public FramebufferReadbackApi
{
public:
    explicit ControlledReadback(ReadbackState& state) : state_(state) {}
    bool ContextReady() const noexcept override { return state_.contextReady; }
    bool Available() const noexcept override { return true; }
    void FlushRendering() const override {}
    CapturedFramebuffer ReadSynchronously(const int width, const int height,
                                          const MfdWindowFramebufferPixelFormat format) const override
    {
        ++state_.syncCount;
        CapturedFramebuffer frame;
        if (state_.syncSucceeds)
        {
            frame.width = width;
            frame.height = height;
            frame.pixelFormat = format;
            frame.pixels.assign(static_cast<std::size_t>(width * height * 4), std::byte {99});
        }
        return frame;
    }
    void ReportFailure() const noexcept override { ++state_.diagnostics; }
    void GenBuffers(const int count, unsigned int* buffers) const noexcept override
    {
        for (int index = 0; index < count; ++index)
        {
            buffers[index] = ++state_.generated;
        }
    }
    void DeleteBuffers(const int count, const unsigned int*) const noexcept override
    {
        EXPECT_TRUE(state_.contextReady);
        state_.deleted += static_cast<unsigned int>(count);
    }
    void BindPixelPackBuffer(const unsigned int id) const noexcept override { state_.bound = id; }
    void AllocatePixelPackBuffer(std::size_t) const noexcept override {}
    void ReadPixels(int, int, MfdWindowFramebufferPixelFormat) const noexcept override
    {
        state_.lastRead = state_.bound - 1;
        ++state_.readCount;
        state_.buffers[state_.lastRead].fill(static_cast<std::byte>(state_.readCount));
    }
    void* MapPixelPackBufferReadOnly() const noexcept override
    {
        return state_.mapSucceeds ? state_.buffers[state_.bound - 1].data() : nullptr;
    }
    bool UnmapPixelPackBuffer() const noexcept override { return state_.unmapSucceeds; }
    GlSyncHandle CreateFence() const noexcept override
    {
        EXPECT_FALSE(state_.activeFences[state_.lastRead]);
        state_.activeFences[state_.lastRead] = true;
        ++state_.createdFences;
        return &state_.handles[state_.lastRead];
    }
    FenceStatus PollFence(GlSyncHandle) const noexcept override
    {
        ++state_.polls;
        return state_.status;
    }
    void DeleteFence(const GlSyncHandle fence) const noexcept override
    {
        EXPECT_TRUE(state_.contextReady);
        for (std::size_t index = 0; index < state_.handles.size(); ++index)
        {
            if (fence == &state_.handles[index])
            {
                EXPECT_TRUE(state_.activeFences[index]);
                state_.activeFences[index] = false;
                ++state_.releasedFences;
                return;
            }
        }
        ADD_FAILURE() << "Unexpected fence";
    }
private:
    ReadbackState& state_;
};

TEST(AsyncFramebufferCaptureTests, DecodesEveryGlWaitStatus)
{
    EXPECT_EQ(mfd::window::detail::DecodeFenceStatus(0x911A), FenceStatus::Ready);
    EXPECT_EQ(mfd::window::detail::DecodeFenceStatus(0x911C), FenceStatus::Ready);
    EXPECT_EQ(mfd::window::detail::DecodeFenceStatus(0x911B), FenceStatus::Pending);
    EXPECT_EQ(mfd::window::detail::DecodeFenceStatus(0x911D), FenceStatus::Failed);
    EXPECT_EQ(mfd::window::detail::DecodeFenceStatus(0), FenceStatus::Failed);
}

TEST(AsyncFramebufferCaptureTests, PendingNeverPublishesOldFrameOrOverwritesPendingSlot)
{
    ReadbackState state;
    ControlledReadback api(state);
    {
        AsyncFramebufferCapture capture(MfdWindowFramebufferPixelFormat_Rgba32, api);
        EXPECT_FALSE(capture.Capture(1, 1).has_value());
        const auto first = capture.Capture(1, 1);
        ASSERT_TRUE(first.has_value());
        EXPECT_EQ(first->pixels.front(), std::byte {1});
        state.status = FenceStatus::Pending;
        for (int frame = 0; frame < 10; ++frame)
        {
            EXPECT_FALSE(capture.Capture(1, 1).has_value());
        }
        EXPECT_EQ(state.readCount, 3U);
        EXPECT_EQ(state.syncCount, 0U);
        state.status = FenceStatus::Ready;
        const auto next = capture.Capture(1, 1);
        ASSERT_TRUE(next.has_value());
        EXPECT_NE(next->pixels.front(), first->pixels.front());
    }
    EXPECT_EQ(state.generated, state.deleted);
    EXPECT_EQ(state.createdFences, state.releasedFences);
}

TEST(AsyncFramebufferCaptureTests, FailedFenceFallsBackOnceAndNeverReturnsStalePixels)
{
    ReadbackState state;
    ControlledReadback api(state);
    AsyncFramebufferCapture capture(MfdWindowFramebufferPixelFormat_Bgra32, api);
    EXPECT_FALSE(capture.Capture(1, 1).has_value());
    ASSERT_TRUE(capture.Capture(1, 1).has_value());
    state.status = FenceStatus::Failed;
    const auto fallback = capture.Capture(1, 1);
    ASSERT_TRUE(fallback.has_value());
    EXPECT_EQ(fallback->pixels.front(), std::byte {99});
    EXPECT_EQ(fallback->pixelFormat, MfdWindowFramebufferPixelFormat_Bgra32);
    const auto polls = state.polls;
    state.syncSucceeds = false;
    for (int frame = 0; frame < 10; ++frame)
    {
        EXPECT_FALSE(capture.Capture(1, 1).has_value());
    }
    EXPECT_EQ(state.polls, polls);
    EXPECT_EQ(state.diagnostics, 1U);
    EXPECT_EQ(state.generated, state.deleted);
    EXPECT_EQ(state.createdFences, state.releasedFences);
}

TEST(AsyncFramebufferCaptureTests, ResizeDropsPendingFencesAndTeardownReleasesExactlyOnce)
{
    ReadbackState state;
    ControlledReadback api(state);
    {
        AsyncFramebufferCapture capture(MfdWindowFramebufferPixelFormat_Rgba32, api);
        EXPECT_FALSE(capture.Capture(1, 1).has_value());
        EXPECT_FALSE(capture.Capture(2, 2).has_value());
        const auto resized = capture.Capture(2, 2);
        ASSERT_TRUE(resized.has_value());
        EXPECT_EQ(resized->width, 2);
        EXPECT_EQ(resized->pixels.size(), 16U);
    }
    EXPECT_EQ(state.generated, state.deleted);
    EXPECT_EQ(state.createdFences, state.releasedFences);
}

TEST(AsyncFramebufferCaptureTests, NoGlCallsAfterContextTeardown)
{
    ReadbackState state;
    ControlledReadback api(state);
    {
        AsyncFramebufferCapture capture(MfdWindowFramebufferPixelFormat_Rgba32, api);
        EXPECT_FALSE(capture.Capture(1, 1).has_value());
        state.contextReady = false;
        EXPECT_FALSE(capture.Capture(1, 1).has_value());
    }
    EXPECT_EQ(state.deleted, 0U); // The destroyed context owns reclamation here.
    EXPECT_EQ(state.releasedFences, 0U);
    EXPECT_EQ(state.syncCount, 0U);
}

TEST(AsyncFramebufferCaptureTests, MappingAndUnmappingFailuresReleaseResourcesAndFallback)
{
    for (const bool failMapping : {true, false})
    {
        ReadbackState state;
        ControlledReadback api(state);
        AsyncFramebufferCapture capture(MfdWindowFramebufferPixelFormat_Rgba32, api);
        EXPECT_FALSE(capture.Capture(1, 1).has_value());
        state.mapSucceeds = !failMapping;
        state.unmapSucceeds = failMapping;
        const auto fallback = capture.Capture(1, 1);
        ASSERT_TRUE(fallback.has_value());
        EXPECT_EQ(fallback->pixels.front(), std::byte {99});
        EXPECT_EQ(state.generated, state.deleted);
        EXPECT_EQ(state.createdFences, state.releasedFences);
    }
}
}
