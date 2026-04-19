/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief GoogleTest coverage for CommandProcessor.
 */

#include <gtest/gtest.h>

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "mfd/control/CommandProcessor.h"
#include "mfd/control/CommandTypes.h"
#include "mfd/ipc/ExchangeChannel.h"
#include "mfd/model/PageDefinition.h"
#include "mfd/runtime/SceneRegistry.h"

namespace
{
class ScriptedExchangeChannel final : public mfd::IExchangeChannel
{
public:
    bool IsReady() const noexcept override
    {
        return true;
    }

    bool Send(const mfd::ByteView) override
    {
        return true;
    }

    std::optional<std::vector<std::byte>> TryReceive() override
    {
        if (payloads_.empty())
        {
            return std::nullopt;
        }

        auto payload = std::move(payloads_.front());
        payloads_.pop_front();
        return payload;
    }

    std::string LastError() const override
    {
        return lastError_;
    }

    void PushPayload(std::vector<std::byte> payload)
    {
        payloads_.push_back(std::move(payload));
    }

    void SetLastError(std::string error)
    {
        lastError_ = std::move(error);
    }

private:
    std::deque<std::vector<std::byte>> payloads_ {};
    std::string lastError_ {};
};

std::vector<std::byte> ToBytes(const std::string& payload)
{
    const auto* first = reinterpret_cast<const std::byte*>(payload.data());
    return std::vector<std::byte>(first, first + payload.size());
}

mfd::SceneRegistry MakeRegistry()
{
    mfd::PageDefinition page;
    page.name = "Radar";
    page.normalizedName = "radar";
    page.title = "Radar";

    mfd::MfdDocument document;
    document.pages.push_back(std::move(page));
    return mfd::SceneRegistry(std::move(document));
}
} // namespace

TEST(CommandProcessorTests, PollDoesNotOverrideSuccessfulDispatchWithStickyChannelError)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);
    ScriptedExchangeChannel channel;

    mfd::ActivatePageCommand command;
    command.page = "Radar";
    channel.PushPayload(ToBytes(mfd::SerializeUserCommand(command)));
    channel.SetLastError("stale transport error");

    EXPECT_TRUE(processor.Poll(channel));
    EXPECT_TRUE(processor.LastError().empty());
}

TEST(CommandProcessorTests, PollReportsTransportErrorWhenNoPayloadWasProcessed)
{
    mfd::SceneRegistry registry = MakeRegistry();
    mfd::CommandProcessor processor(registry);
    ScriptedExchangeChannel channel;
    channel.SetLastError("transport unavailable");

    EXPECT_FALSE(processor.Poll(channel));
    EXPECT_EQ(processor.LastError(), "transport unavailable");
}
