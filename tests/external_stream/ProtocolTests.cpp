#include "StreamProtocol.h"
#include <gtest/gtest.h>
#include <limits>
#if defined(_WIN32)
#include "NativeStream.h"
#include "mfd/stream/StreamProducer.h"
#include "mfd/stream/StreamReceiver.h"
#endif

namespace
{
using mfd::stream::detail::WireHeader;
using mfd::stream::detail::SlotState;
void SetSlot(WireHeader& header, std::size_t index, SlotState state, std::uint64_t frame)
{
    header.slots[index].state = static_cast<std::uint32_t>(state);
    header.slots[index].frame = frame;
    header.slots[index].epoch = header.epoch;
}
TEST(ExternalStreamProtocol, RejectsUnboundedMetadataAndNames)
{
    EXPECT_FALSE(mfd::IsValidStreamConfiguration(UINT32_MAX, 1080, mfd::StreamPixelFormat::Rgba32));
    EXPECT_FALSE(mfd::IsValidStreamConfiguration(640, 0, mfd::StreamPixelFormat::Rgba32));
    EXPECT_FALSE(mfd::IsValidStreamConfiguration(640, 360, static_cast<mfd::StreamPixelFormat>(3)));
    EXPECT_TRUE(mfd::IsValidStreamConfiguration(1920, 1080, mfd::StreamPixelFormat::Bgra32));
    EXPECT_FALSE(mfd::IsValidStreamId("../other"));
    EXPECT_FALSE(mfd::IsValidStreamId(std::string(65, 'a')));
    EXPECT_TRUE(mfd::IsValidStreamId("camera_2-a"));
}
TEST(ExternalStreamProtocol, PreservesGpuOwnedAndPartiallyWrittenSurfaces)
{
    WireHeader header;
    SetSlot(header, 0, SlotState::Reading, 1);
    SetSlot(header, 1, SlotState::Writing, 2);
    SetSlot(header, 2, SlotState::Pending, 3);
    EXPECT_EQ(mfd::stream::detail::FindWritable(header), mfd::stream::detail::kNoSlot);
    EXPECT_EQ(mfd::stream::detail::FindLatest(header), mfd::stream::detail::kNoSlot);
    SetSlot(header, 2, SlotState::Ready, 3);
    EXPECT_EQ(mfd::stream::detail::FindWritable(header), 2U);
    EXPECT_EQ(mfd::stream::detail::FindLatest(header), 2U);
}
TEST(ExternalStreamProtocol, LatestCompleteFrameWinsAndOldestReadyCanBeDropped)
{
    WireHeader header;
    SetSlot(header, 0, SlotState::Ready, 10);
    SetSlot(header, 1, SlotState::Ready, 30);
    SetSlot(header, 2, SlotState::Ready, 20);
    EXPECT_EQ(mfd::stream::detail::FindLatest(header), 1U);
    EXPECT_EQ(mfd::stream::detail::FindWritable(header), 0U);
}
TEST(ExternalStreamProtocol, StopEpochRejectsLatePublicationsWithoutReusingGpuOwnedBuffers)
{
    WireHeader header;
    SetSlot(header, 0, SlotState::Pending, 10);
    SetSlot(header, 1, SlotState::Reading, 9);
    SetSlot(header, 2, SlotState::Ready, 11);
    ++header.epoch;
    mfd::stream::detail::InvalidateReady(header);
    EXPECT_EQ(header.slots[0].state, static_cast<std::uint32_t>(SlotState::Pending));
    EXPECT_EQ(header.slots[1].state, static_cast<std::uint32_t>(SlotState::Reading));
    EXPECT_EQ(header.slots[2].state, static_cast<std::uint32_t>(SlotState::Free));
    header.slots[0].state = static_cast<std::uint32_t>(SlotState::Ready);
    EXPECT_EQ(mfd::stream::detail::FindLatest(header), mfd::stream::detail::kNoSlot);
}
TEST(ExternalStreamProtocol, ChecksVersionSizeAndSlotEnums)
{
    WireHeader header;
    header.bytes = sizeof(header); header.session = 1;
    EXPECT_TRUE(mfd::stream::detail::ValidHeader(header));
    header.version = 2;
    EXPECT_FALSE(mfd::stream::detail::ValidHeader(header));
    header.version = 1; header.slots[1].state = UINT32_MAX;
    EXPECT_FALSE(mfd::stream::detail::ValidHeader(header));
}
#if defined(_WIN32)
TEST(ExternalStreamProtocol, ProducerLeaseRejectsDuplicateAndCanReconnectWithoutGpu)
{
    const auto instance = mfd::stream::NewStreamInstance();
    mfd::stream::detail::Endpoint host;
    host.Open(instance, "control", true);
    mfd::stream::StreamProducer first;
    mfd::stream::StreamProducer second;
    EXPECT_EQ(first.Open(instance, "control", 640, 360, mfd::StreamPixelFormat::Rgba32), mfd::StreamResult::Unavailable);
    const auto request = host.Header().requestSerial;
    EXPECT_EQ(second.Open(instance, "control", 640, 360, mfd::StreamPixelFormat::Rgba32), mfd::StreamResult::Unavailable);
    EXPECT_EQ(host.Header().requestSerial, request);
    first.Close();
    EXPECT_EQ(second.Open(instance, "control", 640, 360, mfd::StreamPixelFormat::Bgra32), mfd::StreamResult::Unavailable);
    EXPECT_GT(host.Header().requestSerial, request);
    host.Header().hostPid = 0;
    EXPECT_EQ(second.Poll(), mfd::StreamResult::Disconnected);
}
TEST(ExternalStreamProtocol, RecreatedConsumerResetsSessionAndOldSlotMetadataWithoutGpu)
{
    const auto instance = mfd::stream::NewStreamInstance();
    mfd::stream::detail::Endpoint host;
    host.Open(instance, "control", true);
    mfd::stream::detail::Endpoint observer;
    observer.Open(instance, "control", false);
    const auto oldSession = host.Header().session;
    host.Header().generation = 17;
    SetSlot(host.Header(), 0, SlotState::Ready, 99);
    host.Header().hostPid = 0;
    host.Close();
    host.Open(instance, "control", true);
    EXPECT_NE(host.Header().session, oldSession);
    EXPECT_EQ(host.Header().generation, 0U);
    EXPECT_EQ(mfd::stream::detail::FindLatest(host.Header()), mfd::stream::detail::kNoSlot);
}
#endif
}
