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

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
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

class GeneratedUiGeometryReticle final : public mfd::client::Reticle
{
public:
    GeneratedUiGeometryReticle()
        : mfd::client::Reticle("Radar", "geometry_panel", 11U, 55U),
          scopeRing(MutableDesiredPatch(), DirtyFlag(), "scope_ring", 66U, PrimitiveTransportIds()),
          lockBox(MutableDesiredPatch(), DirtyFlag(), "lock_box", 67U, PrimitiveTransportIds()),
          warningTriangle(MutableDesiredPatch(), DirtyFlag(), "warning_triangle", 68U, PrimitiveTransportIds()),
          routePolyline(MutableDesiredPatch(), DirtyFlag(), "route_polyline", 69U, PrimitiveTransportIds()),
          guideBezier(MutableDesiredPatch(), DirtyFlag(), "guide_bezier", 70U, PrimitiveTransportIds()),
          scanArc(MutableDesiredPatch(), DirtyFlag(), "scan_arc", 71U, PrimitiveTransportIds())
    {
    }

    mfd::client::RingHandle scopeRing;
    mfd::client::RectangleHandle lockBox;
    mfd::client::TriangleHandle warningTriangle;
    mfd::client::PolylineHandle routePolyline;
    mfd::client::BezierHandle guideBezier;
    mfd::client::ArcHandle scanArc;
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
    explicit GeneratedUiTrackSet(mfd::client::RuntimeFeedbackState* feedbackState = nullptr)
        : mfd::client::GeneratedDynamicReticleSet("Radar", "radar_track", 11U, 77U, feedbackState)
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
    explicit GeneratedUiRadarPage(mfd::client::RuntimeFeedbackState* feedbackState = nullptr)
        : feedbackState_(feedbackState),
          strobe(Name(), MakeStrobeInfo(), 11U),
          statusBanner(),
          geometryPanel(),
          dynamicTracks(feedbackState)
    {
    }

    static constexpr std::string_view Name() noexcept
    {
        return "Radar";
    }

    bool IsActive() const noexcept
    {
        return feedbackState_ != nullptr && feedbackState_->IsPageActive(Name());
    }

    void Reset() noexcept
    {
        strobe.Reset();
        statusBanner.Reset();
        geometryPanel.Reset();
        dynamicTracks.Reset();
    }

    std::size_t AppendCommands(std::vector<mfd::UserCommand>& commands)
    {
        std::size_t count = 0;
        count += strobe.AppendCommands(commands) ? 1U : 0U;
        count += statusBanner.AppendCommands(commands) ? 1U : 0U;
        count += geometryPanel.AppendCommands(commands) ? 1U : 0U;
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
    GeneratedUiGeometryReticle geometryPanel;

private:
    static mfd::client::StrobeInfo MakeStrobeInfo() noexcept
    {
        mfd::client::StrobeInfo info;
        info.valid = true;
        return info;
    }

    mfd::client::RuntimeFeedbackState* feedbackState_ = nullptr;
    GeneratedUiTrackSet dynamicTracks;
};

class GeneratedUiFixture final
{
public:
    static constexpr std::string_view MappingHash() noexcept
    {
        return "generated-ui-fixture-hash";
    }

    GeneratedUiFixture()
        : window_(),
          radar_(&feedbackState_)
    {
    }

    bool ApplyFeedback(const mfd::StrobeStatusFeedback& feedback)
    {
        return feedbackState_.Apply(feedback);
    }

    bool ApplyFeedback(const mfd::ActivePageFeedback& feedback)
    {
        return feedbackState_.Apply(feedback);
    }

    bool ApplyFeedbackPayload(std::string_view payload, std::string* error = nullptr)
    {
        return feedbackState_.ApplyPayload(payload, error);
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
    mfd::client::RuntimeFeedbackState feedbackState_ {};
    mfd::client::WindowDisplay window_ {};
    GeneratedUiRadarPage radar_;
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
    track.vectorLine.SetLineStyle(mfd::client::LineStyle::Dashed);

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
    EXPECT_EQ(upsert->reticles.front().patch.primitivePatchesById.at(45U).lineStyle,
              std::optional<mfd::LineStyle> {mfd::LineStyle::Dashed});

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

TEST(GeneratedUiRuntimeTests, GeneratedPagesAndDynamicReticlesReflectRuntimeFeedbackState)
{
    GeneratedUiFixture ui;
    auto& track = ui.Radar().DynamicRadarTrack().Create();
    std::vector<mfd::UserCommand> commands;
    ASSERT_EQ(ui.Radar().AppendCommands(commands), 1U);
    ASSERT_EQ(commands.size(), 1U);
    const auto* upsert = std::get_if<mfd::UpsertDynamicReticlesCommand>(&commands.front());
    ASSERT_NE(upsert, nullptr);
    ASSERT_EQ(upsert->reticles.size(), 1U);
    EXPECT_FALSE(ui.Radar().IsActive());
    EXPECT_FALSE(track.IsStrobeCaptured());

    mfd::ActivePageFeedback activePage;
    activePage.sequence = 1U;
    activePage.pageName = "Radar";
    EXPECT_TRUE(ui.ApplyFeedback(activePage));
    EXPECT_TRUE(ui.Radar().IsActive());

    mfd::StrobeStatusFeedback strobe;
    strobe.sequence = 2U;
    strobe.pageId = 11U;
    strobe.pageName = "Radar";
    mfd::StrobeFeedbackCapture capture;
    capture.runtimeReticleId = upsert->reticles.front().runtimeReticleId;
    capture.reticleId = track.Id();
    strobe.captureResult = std::move(capture);
    EXPECT_TRUE(ui.ApplyFeedback(strobe));
    EXPECT_TRUE(track.IsStrobeCaptured());

    mfd::StrobeStatusFeedback cleared;
    cleared.sequence = 3U;
    cleared.pageId = 11U;
    cleared.pageName = "Radar";
    EXPECT_TRUE(ui.ApplyFeedback(cleared));
    EXPECT_FALSE(track.IsStrobeCaptured());
}

TEST(GeneratedUiRuntimeTests, GeneratedUiAppliesSerializedRuntimeFeedbackPayloads)
{
    GeneratedUiFixture ui;

    mfd::ActivePageFeedback activePage;
    activePage.sequence = 5U;
    activePage.pageName = "Radar";

    std::string error;
    EXPECT_TRUE(ui.ApplyFeedbackPayload(mfd::SerializeActivePageFeedback(activePage), &error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(ui.Radar().IsActive());
}

TEST(GeneratedUiRuntimeTests, BuildCommandBatchCarriesRichGeneratedPrimitiveGeometryPatches)
{
    GeneratedUiFixture ui;
    auto& geometry = ui.Radar().geometryPanel;

    geometry.scopeRing.SetInnerRadius(0.12f);
    geometry.scopeRing.SetOuterRadius(0.19f);
    geometry.scopeRing.SetSegments(40);
    geometry.lockBox.SetFillColor({1, 2, 3, 180});
    geometry.lockBox.SetFilled(true);
    geometry.warningTriangle.SetPoints(
        std::array<mfd::Vec2, 3> {{{-0.18f, -0.08f}, {0.0f, 0.18f}, {0.16f, -0.06f}}});
    geometry.routePolyline.SetPoints({{-0.20f, -0.08f}, {-0.04f, 0.12f}, {0.12f, 0.10f}, {0.24f, -0.02f}});
    geometry.routePolyline.SetClosed(true);
    geometry.guideBezier.SetControlPoints({{-0.18f, -0.10f}, {-0.06f, 0.14f}, {0.08f, 0.14f}, {0.20f, -0.02f}});
    geometry.guideBezier.SetSegments(18);
    geometry.scanArc.SetRadius(0.22f);
    geometry.scanArc.SetStartAngleDegrees(-45.0f);
    geometry.scanArc.SetEndAngleDegrees(135.0f);
    geometry.scanArc.SetSegments(24);

    const mfd::CommandBatch batch = ui.BuildCommandBatch(91U);
    ASSERT_EQ(batch.commands.size(), 1U);

    const auto* update = std::get_if<mfd::UpdateReticleCommand>(&batch.commands.front());
    ASSERT_NE(update, nullptr);
    EXPECT_EQ(update->target.pageId, 11U);
    EXPECT_EQ(update->target.reticleId, 55U);
    EXPECT_TRUE(update->patch.primitivePatches.empty());
    ASSERT_EQ(update->patch.primitivePatchesById.size(), 6U);

    const auto& ringPatch = update->patch.primitivePatchesById.at(66U);
    ASSERT_TRUE(ringPatch.innerRadius.has_value());
    ASSERT_TRUE(ringPatch.outerRadius.has_value());
    ASSERT_TRUE(ringPatch.segments.has_value());
    EXPECT_FLOAT_EQ(*ringPatch.innerRadius, 0.12f);
    EXPECT_FLOAT_EQ(*ringPatch.outerRadius, 0.19f);
    EXPECT_EQ(*ringPatch.segments, 40);

    const auto& rectanglePatch = update->patch.primitivePatchesById.at(67U);
    ASSERT_TRUE(rectanglePatch.fillColor.has_value());
    ASSERT_TRUE(rectanglePatch.filled.has_value());
    EXPECT_EQ(rectanglePatch.fillColor->r, 1U);
    EXPECT_TRUE(*rectanglePatch.filled);

    const auto& trianglePatch = update->patch.primitivePatchesById.at(68U);
    ASSERT_TRUE(trianglePatch.points.has_value());
    ASSERT_EQ(trianglePatch.points->size(), 3U);
    EXPECT_FLOAT_EQ(trianglePatch.points->at(1).y, 0.18f);

    const auto& polylinePatch = update->patch.primitivePatchesById.at(69U);
    ASSERT_TRUE(polylinePatch.points.has_value());
    ASSERT_TRUE(polylinePatch.closed.has_value());
    ASSERT_EQ(polylinePatch.points->size(), 4U);
    EXPECT_TRUE(*polylinePatch.closed);

    const auto& bezierPatch = update->patch.primitivePatchesById.at(70U);
    ASSERT_TRUE(bezierPatch.points.has_value());
    ASSERT_TRUE(bezierPatch.segments.has_value());
    ASSERT_EQ(bezierPatch.points->size(), 4U);
    EXPECT_EQ(*bezierPatch.segments, 18);

    const auto& arcPatch = update->patch.primitivePatchesById.at(71U);
    ASSERT_TRUE(arcPatch.radius.has_value());
    ASSERT_TRUE(arcPatch.startAngleDegrees.has_value());
    ASSERT_TRUE(arcPatch.endAngleDegrees.has_value());
    ASSERT_TRUE(arcPatch.segments.has_value());
    EXPECT_FLOAT_EQ(*arcPatch.radius, 0.22f);
    EXPECT_FLOAT_EQ(*arcPatch.startAngleDegrees, -45.0f);
    EXPECT_FLOAT_EQ(*arcPatch.endAngleDegrees, 135.0f);
    EXPECT_EQ(*arcPatch.segments, 24);
}
