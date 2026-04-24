/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
/**
 * @file
 * @brief End-to-end GoogleTest coverage for generated-style UI roots built on the public client API.
 */

#include <gtest/gtest.h>

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "mfd/client/Animation.h"
#include "mfd/client/LatestBatchPublisher.h"
#include "mfd/control/CommandTypes.h"

namespace
{
class GeneratedUiStatusReticle final : public mfd::client::Reticle
{
public:
    GeneratedUiStatusReticle()
        : mfd::client::Reticle("Radar", "status_banner", 11U, 22U),
          statusValue(MutableDesiredPatch(), DirtyFlag(), "status_value", 33U, PrimitiveTransportIds())
    {
    }

    void SetValue(std::string value)
    {
        statusValue.SetText(std::move(value));
    }

    mfd::client::TextHandle statusValue;
};

class GeneratedUiTrackReticle final : public mfd::client::DynamicReticle
{
public:
    explicit GeneratedUiTrackReticle(const std::string_view reticleId)
        : mfd::client::DynamicReticle(reticleId),
          label(MutableDesiredPatch(), DirtyFlag(), "track_label", 44U, PrimitiveTransportIds()),
          vectorLine(MutableDesiredPatch(), DirtyFlag(), "track_vector", 45U, PrimitiveTransportIds())
    {
    }

    mfd::client::TextHandle label;
    mfd::client::LineHandle vectorLine;
};

class GeneratedUiTrackSet final : public mfd::client::GeneratedDynamicReticleSet
{
public:
    GeneratedUiTrackSet()
        : mfd::client::GeneratedDynamicReticleSet("Radar", "radar_track", 11U, 77U)
    {
    }

    GeneratedUiTrackReticle& Create()
    {
        return static_cast<GeneratedUiTrackReticle&>(mfd::client::GeneratedDynamicReticleSet::Create());
    }

protected:
    std::unique_ptr<mfd::client::DynamicReticle> CreateReticle(const std::string_view reticleId) override
    {
        return std::make_unique<GeneratedUiTrackReticle>(reticleId);
    }
};

class GeneratedUiRadarPage final
{
public:
    static constexpr std::string_view Name() noexcept
    {
        return "Radar";
    }

    GeneratedUiRadarPage()
        : strobe(Name(), MakeStrobeInfo(), 11U),
          statusBanner()
    {
    }

    void Reset() noexcept
    {
        strobe.Reset();
        statusBanner.Reset();
        dynamicTracks.Reset();
    }

    std::size_t AppendCommands(std::vector<mfd::UserCommand>& commands)
    {
        std::size_t count = 0;
        count += strobe.AppendCommands(commands) ? 1U : 0U;
        count += statusBanner.AppendCommands(commands) ? 1U : 0U;
        count += dynamicTracks.AppendCommands(commands);
        return count;
    }

    void SetStatusCaption(std::string value)
    {
        statusBanner.SetValue(std::move(value));
    }

    GeneratedUiTrackSet& DynamicRadarTrack() noexcept
    {
        return dynamicTracks;
    }

    mfd::client::StrobeHandle strobe;
    GeneratedUiStatusReticle statusBanner;

private:
    static mfd::client::StrobeInfo MakeStrobeInfo() noexcept
    {
        mfd::client::StrobeInfo info;
        info.valid = true;
        return info;
    }

    GeneratedUiTrackSet dynamicTracks;
};

class GeneratedUiFixture final
{
public:
    static constexpr std::string_view MappingHash() noexcept
    {
        return "generated-ui-fixture-hash";
    }

    void Reset() noexcept
    {
        window_.Reset();
        radar_.Reset();
    }

    std::vector<mfd::UserCommand> BuildBatch()
    {
        std::vector<mfd::UserCommand> commands;
        window_.AppendCommands(commands);
        radar_.AppendCommands(commands);
        return commands;
    }

    mfd::CommandBatch BuildCommandBatch(const std::uint32_t sequence = 0U)
    {
        mfd::CommandBatch batch;
        batch.sequence = sequence;
        batch.mappingHash = std::string {MappingHash()};
        batch.commands = BuildBatch();
        return batch;
    }

    bool SubmitLatest(mfd::client::LatestBatchPublisher& publisher, const std::uint32_t sequence = 0U)
    {
        return publisher.SubmitLatest(BuildCommandBatch(sequence));
    }

    mfd::client::WindowDisplay& Window() noexcept
    {
        return window_;
    }

    GeneratedUiRadarPage& Radar() noexcept
    {
        return radar_;
    }

private:
    mfd::client::WindowDisplay window_ {};
    GeneratedUiRadarPage radar_ {};
};
} // namespace

TEST(GeneratedUiRuntimeTests, BuildCommandBatchCarriesMappingHashAndGeneratedIdentifiers)
{
    GeneratedUiFixture ui;
    ui.Window().SetBrightness(0.65f);
    ui.Radar().SetStatusCaption("LOCK");
    ui.Radar().strobe.SetActive(true);

    GeneratedUiTrackReticle& track = ui.Radar().DynamicRadarTrack().Create();
    track.SetPosition({0.15f, -0.20f});
    track.label.SetText("T01");
    track.vectorLine.SetStart({-0.10f, 0.0f});
    track.vectorLine.SetEnd({0.20f, 0.0f});

    const mfd::CommandBatch batch = ui.BuildCommandBatch(42U);
    EXPECT_EQ(batch.sequence, 42U);
    EXPECT_EQ(batch.mappingHash, GeneratedUiFixture::MappingHash());
    ASSERT_EQ(batch.commands.size(), 4U);

    const auto* display = std::get_if<mfd::UpdateWindowDisplayCommand>(&batch.commands[0]);
    ASSERT_NE(display, nullptr);
    ASSERT_TRUE(display->patch.brightness.has_value());
    EXPECT_FLOAT_EQ(*display->patch.brightness, 0.65f);

    const auto* strobe = std::get_if<mfd::UpdateStrobeCommand>(&batch.commands[1]);
    ASSERT_NE(strobe, nullptr);
    EXPECT_EQ(strobe->page, "Radar");
    EXPECT_EQ(strobe->pageId, 11U);
    ASSERT_TRUE(strobe->active.has_value());
    EXPECT_TRUE(*strobe->active);

    const auto* status = std::get_if<mfd::UpdateReticleCommand>(&batch.commands[2]);
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->target.page, "Radar");
    EXPECT_EQ(status->target.pageId, 11U);
    EXPECT_EQ(status->target.reticleId, 22U);
    EXPECT_TRUE(status->patch.primitivePatches.empty());
    ASSERT_EQ(status->patch.primitivePatchesById.size(), 1U);
    ASSERT_NE(status->patch.primitivePatchesById.find(33U), status->patch.primitivePatchesById.end());
    ASSERT_TRUE(status->patch.primitivePatchesById.at(33U).text.has_value());
    EXPECT_EQ(*status->patch.primitivePatchesById.at(33U).text, "LOCK");

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&batch.commands[3]);
    ASSERT_NE(upsert, nullptr);
    EXPECT_EQ(upsert->page, "Radar");
    EXPECT_EQ(upsert->pageId, 11U);
    EXPECT_EQ(upsert->templateTransportId, 77U);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    EXPECT_NE(upsert->reticles.front().runtimeReticleId, 0U);
    EXPECT_TRUE(upsert->reticles.front().patch.primitivePatches.empty());
    ASSERT_EQ(upsert->reticles.front().patch.primitivePatchesById.size(), 2U);
    ASSERT_TRUE(upsert->reticles.front().patch.position.has_value());
    EXPECT_FLOAT_EQ(upsert->reticles.front().patch.position->x, 0.15f);
    EXPECT_EQ(*upsert->reticles.front().patch.primitivePatchesById.at(44U).text, "T01");
    EXPECT_FLOAT_EQ(upsert->reticles.front().patch.primitivePatchesById.at(45U).lineStart->x, -0.10f);

    const std::string payload = mfd::SerializeCommandBatch(batch);
    const auto decoded = mfd::DeserializeCommandBatch(payload);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->sequence, batch.sequence);
    EXPECT_EQ(decoded->mappingHash, batch.mappingHash);
    ASSERT_EQ(decoded->commands.size(), batch.commands.size());
}

TEST(GeneratedUiRuntimeTests, SubmitLatestForwardsGeneratedUiBatchSemantics)
{
    std::mutex mutex;
    bool deliveredReady = false;
    mfd::CommandBatch delivered;

    mfd::client::LatestBatchPublisher publisher(
        [&mutex, &deliveredReady, &delivered](const mfd::CommandBatch& batch)
        {
            std::lock_guard lock(mutex);
            delivered = batch;
            deliveredReady = true;
            return true;
        });

    ASSERT_TRUE(publisher.IsReady());

    GeneratedUiFixture ui;
    ui.Window().SetDisabled(true);
    ui.Radar().SetStatusCaption("STBY");

    GeneratedUiTrackReticle& track = ui.Radar().DynamicRadarTrack().Create();
    track.label.SetText("B02");

    ASSERT_TRUE(ui.SubmitLatest(publisher, 7U));
    publisher.Flush();

    std::lock_guard lock(mutex);
    ASSERT_TRUE(deliveredReady);
    EXPECT_EQ(delivered.sequence, 7U);
    EXPECT_EQ(delivered.mappingHash, GeneratedUiFixture::MappingHash());
    ASSERT_EQ(delivered.commands.size(), 3U);

    const auto* display = std::get_if<mfd::UpdateWindowDisplayCommand>(&delivered.commands[0]);
    ASSERT_NE(display, nullptr);
    ASSERT_TRUE(display->patch.disabled.has_value());
    EXPECT_TRUE(*display->patch.disabled);

    const auto* status = std::get_if<mfd::UpdateReticleCommand>(&delivered.commands[1]);
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->target.pageId, 11U);
    EXPECT_EQ(status->target.reticleId, 22U);
    EXPECT_EQ(*status->patch.primitivePatchesById.at(33U).text, "STBY");

    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&delivered.commands[2]);
    ASSERT_NE(upsert, nullptr);
    EXPECT_EQ(upsert->pageId, 11U);
    EXPECT_EQ(upsert->templateTransportId, 77U);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    EXPECT_EQ(*upsert->reticles.front().patch.primitivePatchesById.at(44U).text, "B02");
}
