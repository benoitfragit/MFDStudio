#!/usr/bin/env python3
"""Integration tests for the client API UI generator."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

GENERATOR_PATH: Path | None = None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--generator", required=True)
    return parser.parse_known_args()[0]


class GenerateUiTests(unittest.TestCase):
    def setUp(self) -> None:
        assert GENERATOR_PATH is not None
        self.generator = GENERATOR_PATH
        self.assertTrue(self.generator.exists(), f"Generator script not found: {self.generator}")

    def test_generates_header_and_source_covering_full_ui_api(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page_one = root / "radar_page.json"
            page_two = root / "status_page.json"
            template_dir = root / "reticles"
            template_dir.mkdir()

            (template_dir / "status_template.json").write_text(
                json.dumps(
                    {
                        "id": "status_template",
                        "elements": [
                            {"id": "status_value", "type": "text"},
                            {"id": "decor", "type": "line"},
                        ],
                    }
                ),
                encoding="utf-8",
            )
            (template_dir / "cursor_template.json").write_text(
                json.dumps(
                    {
                        "id": "cursor_template",
                        "elements": [
                            {"id": "cursor_line", "type": "line", "exposed": True},
                        ],
                    }
                ),
                encoding="utf-8",
            )

            page_one.write_text(
                json.dumps(
                    {
                        "page": {
                            "name": "Radar",
                            "layers": [{"id": "default"}],
                            "activeStrobe": "Default",
                            "dynamicReticleBindings": [
                                {"templateId": "status_template", "layerId": "default", "orderInLayer": 0}
                            ],
                            "blinkTypes": [{"name": "attention"}],
                            "strobes": [
                                {
                                    "name": "Default",
                                    "id": "radar_strobe_default",
                                    "template": "cursor_template",
                                    "capture": {"shape": "rectangle", "radius": 0.12, "size": [0.30, 0.20]},
                                    "magnet": {"enabled": True, "radius": 0.18, "strength": 0.65},
                                },
                                {
                                    "name": "Strobe1",
                                    "id": "radar_strobe_1",
                                    "elements": [
                                        {"id": "marker_label", "type": "text", "text": "ALT", "exposed": True},
                                    ],
                                    "capture": {"shape": "circle", "radius": 0.09, "size": [0.18, 0.18]},
                                    "magnet": {"enabled": False, "radius": 0.10, "strength": 1.0},
                                },
                            ],
                            "staticReticles": [
                                {
                                    "id": "radar_status",
                                    "template": "status_template",
                                    "layerId": "default",
                                },
                                {
                                    "id": "track_box",
                                    "layerId": "default",
                                    "elements": [{"id": "shape", "type": "line"}],
                                },
                            ],
                        }
                    }
                ),
                encoding="utf-8",
            )

            page_two.write_text(
                json.dumps(
                    {
                        "name": "System",
                        "layers": [{"id": "default"}],
                        "staticReticles": [
                            {
                                "id": "systemStatus",
                                "layerId": "default",
                                "elements": [{"id": "systemStatus_value", "type": "text"}],
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )

            window = root / "window.json"
            window.write_text(
                json.dumps(
                    {
                        "title": "Cockpit",
                        "defaultPage": "system",
                        "reticleLibraryFolder": "reticles",
                        "pages": [
                            {"file": page_one.name},
                            page_two.name,
                        ],
                    }
                ),
                encoding="utf-8",
            )

            output_header = root / "GeneratedUi.h"
            output_source = root / "GeneratedUi.cpp"
            output_map = root / "GeneratedUi.generated.map"

            subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(output_header),
                    "--output-source",
                    str(output_source),
                    "--output-map",
                    str(output_map),
                    "--namespace",
                    "generated_ui",
                    "--header-include",
                    "GeneratedUi.h",
                ],
                check=True,
            )

            header_content = output_header.read_text(encoding="utf-8")
            source_content = output_source.read_text(encoding="utf-8")
            map_content = json.loads(output_map.read_text(encoding="utf-8"))

            for expected in [
                "AUTO-GENERATED FILE.",
                '#include "mfd/client/GeneratedUiSupport.h"',
                "class RadarMockupPage",
                "class SystemMockupPage",
                "class CockpitMockupUi",
                "class StatusTemplateDynamicReticle final : private mfd::client::DynamicReticle",
                "using mfd::client::DynamicReticle::ClearDirty;",
                "using mfd::client::DynamicReticle::IsAlive;",
                "using mfd::client::DynamicReticle::SetPosition;",
                "class StatusTemplateDynamicReticleSet final : private mfd::client::GeneratedDynamicReticleSet",
                "using mfd::client::GeneratedDynamicReticleSet::AppendCommands;",
                "using mfd::client::GeneratedDynamicReticleSet::ResetToAuthored;",
                "using mfd::client::GeneratedDynamicReticleSet::SetStrobeMagnetEnabled;",
                "using LineStyle = mfd::client::LineStyle;",
                "class RadarRadarStatusReticle final : private mfd::client::Reticle",
                "using mfd::client::Reticle::AppendCommands;",
                "using mfd::client::Reticle::ClearDirty;",
                "using mfd::client::Reticle::ResetToAuthored;",
                "using mfd::client::Reticle::SetScale;",
                "class RadarTrackBoxReticle final : private mfd::client::Reticle",
                "class RadarDefaultStrobeReticle final : private mfd::client::Reticle",
                "class RadarStrobe1StrobeReticle final : private mfd::client::Reticle",
                "class SystemSystemStatusReticle final : private mfd::client::Reticle",
                "void SetValue(std::string value);",
                "using MfdGeneratedPageTag = mfd::CommandClient::GeneratedPageTag;",
                "using RuntimeFeedbackState = mfd::client::RuntimeFeedbackState;",
                "static constexpr mfd::TransportId GeneratedId() noexcept",
                "static constexpr std::string_view MappingHash() noexcept",
                "bool SendStartup(mfd::CommandClient& client, const mfd::PageViewState& view, std::string statusText);",
                "bool ApplyFeedback(const mfd::StrobeStatusFeedback& feedback);",
                "bool ApplyFeedback(const mfd::ActivePageFeedback& feedback);",
                "bool ApplyFeedbackPayload(std::string_view payload, std::string* error = nullptr);",
                "std::size_t PollFeedback(mfd::IExchangeChannel& channel, std::size_t maxMessages = 64, std::string* error = nullptr);",
                "void Run() noexcept;",
                "void Initialize() noexcept;",
                "std::vector<mfd::UserCommand> BuildBatch();",
                "std::vector<mfd::UserCommand> BuildResetBatch();",
                "mfd::CommandBatch BuildCommandBatch(std::uint32_t sequence = 0);",
                "mfd::CommandBatch BuildResetCommandBatch(std::uint32_t sequence = 0);",
                "bool SubmitLatest(mfd::client::LatestBatchPublisher& publisher, std::uint32_t sequence = 0);",
                "bool SubmitReset(mfd::client::LatestBatchPublisher& publisher, std::uint32_t sequence = 0);",
                "std::vector<mfd::UserCommand> BuildShutdownBatch(std::string statusText);",
                "mfd::CommandBatch BuildShutdownCommandBatch(std::uint32_t sequence, std::string statusText);",
                "bool SubmitShutdown(mfd::client::LatestBatchPublisher& publisher, std::uint32_t sequence, std::string statusText);",
                "WindowDisplay& Window() noexcept;",
                "RadarMockupPage& Radar() noexcept;",
                "SystemMockupPage& System() noexcept;",
                "explicit RadarMockupPage(RuntimeFeedbackState* feedbackState = nullptr);",
                "bool IsActive() const noexcept;",
                "BlinkType attention {\"attention\",",
                "using StrobeType = mfd::client::StrobeType;",
                "StatusTemplateDynamicReticleSet& DynamicStatusTemplate() noexcept;",
                "TextHandle& StatusValue() noexcept;",
                "LineHandle& Shape() noexcept;",
                "LineHandle& CursorLine() noexcept;",
                "TextHandle& MarkerLabel() noexcept;",
                "TextHandle& SystemStatusValue() noexcept;",
                "StrobeType defaultStrobe = StrobeType {\"Default\",",
                "StrobeType strobe1 = StrobeType {\"Strobe1\",",
                "StrobeHandle strobe;",
                "RadarDefaultStrobeReticle defaultReticle;",
                "RadarStrobe1StrobeReticle strobe1Reticle;",
                "RadarRadarStatusReticle radarStatus;",
                "RadarTrackBoxReticle trackBox;",
                "SystemSystemStatusReticle systemStatus;",
            ]:
                self.assertIn(expected, header_content)

            self.assertNotIn('#include "mfd/control/CommandClient.h"', header_content)
            self.assertNotIn('#include "mfd/client/Animation.h"', header_content)
            self.assertNotIn('#include "mfd/client/LatestBatchPublisher.h"', header_content)
            self.assertNotIn("DynamicReticleSet& Dynamic(std::string_view templateId);", header_content)
            self.assertNotIn("using Reticle = mfd::client::Reticle;", header_content)
            self.assertNotIn("using DynamicReticle = mfd::client::DynamicReticle;", header_content)
            self.assertNotIn("using GeneratedDynamicReticleSet = mfd::client::GeneratedDynamicReticleSet;", header_content)

            for expected in [
                "StatusTemplateDynamicReticle::StatusTemplateDynamicReticle(std::string_view reticleId)",
                "StatusTemplateDynamicReticleSet::StatusTemplateDynamicReticleSet(std::string_view pageName, const mfd::TransportId pageTransportId, RuntimeFeedbackState* feedbackState)",
                "mfd::client::GeneratedDynamicReticleSet(pageName, \"status_template\", pageTransportId, ",
                "StatusTemplateDynamicReticleSet& RadarMockupPage::DynamicStatusTemplate() noexcept",
                "RadarMockupPage::RadarMockupPage(RuntimeFeedbackState* feedbackState) :",
                "feedbackState_(feedbackState)",
                "strobe(Name(), {defaultStrobe, strobe1}, ",
                "bool RadarMockupPage::IsActive() const noexcept",
                "return feedbackState_ != nullptr && feedbackState_->IsPageActive(Name());",
                "void RadarMockupPage::Run() noexcept",
                "strobe.ClearDirty();",
                "strobe.ResetToAuthored();",
                "CockpitMockupUi::CockpitMockupUi() :",
                "radar_(&feedbackState_)",
                "window_.ClearDirty();",
                "window_.ResetToAuthored();",
                "SystemMockupPage::SetStatusCaption(std::string value)",
                "RadarRadarStatusReticle::SetValue(std::string value)",
                "systemStatus.SystemStatusValue().SetText(std::move(value));",
                "mfd::client::GeneratedDynamicReticleSet::Create()",
                "mfd::client::GeneratedDynamicReticleSet::Remove(reticle);",
                "if (!client.ActivatePage(system_))",
                "if (!client.SetPageView(system_, view.center, view.zoom))",
                "bool CockpitMockupUi::ApplyFeedback(const mfd::StrobeStatusFeedback& feedback)",
                "bool CockpitMockupUi::ApplyFeedback(const mfd::ActivePageFeedback& feedback)",
                "bool CockpitMockupUi::ApplyFeedbackPayload(std::string_view payload, std::string* error)",
                "std::size_t CockpitMockupUi::PollFeedback(mfd::IExchangeChannel& channel, const std::size_t maxMessages, std::string* error)",
                "void CockpitMockupUi::Run() noexcept",
                "void CockpitMockupUi::Initialize() noexcept",
                "resetPending_ = true;",
                "commands.emplace_back(mfd::ResetWindowCommand {});",
                "std::vector<mfd::UserCommand> CockpitMockupUi::BuildResetBatch()",
                "mfd::CommandBatch CockpitMockupUi::BuildResetCommandBatch(const std::uint32_t sequence)",
                "batch.mappingHash = ",
                "return publisher.SubmitLatest(BuildCommandBatch(sequence));",
                "return publisher.SubmitLatest(BuildResetCommandBatch(sequence));",
                "return publisher.SubmitLatest(BuildShutdownCommandBatch(sequence, std::move(statusText)));",
                "radar_.AppendShutdownCommands(commands, std::string {});",
                "system_.AppendShutdownCommands(commands, std::move(statusText));",
                "count += strobe.AppendCommands(commands) ? 1U : 0U;",
                "RadarDefaultStrobeReticle::RadarDefaultStrobeReticle() :",
                "RadarStrobe1StrobeReticle::RadarStrobe1StrobeReticle() :",
                "if (strobe.IsSelected(defaultStrobe))",
                "count += defaultReticle.AppendCommands(commands) ? 1U : 0U;",
                "if (strobe.IsSelected(strobe1))",
                "count += strobe1Reticle.AppendCommands(commands) ? 1U : 0U;",
            ]:
                self.assertIn(expected, source_content)

            self.assertIn(map_content["mappingHash"], header_content)
            self.assertIn(map_content["mappingHash"], source_content)
            self.assertEqual(header_content.count("static constexpr std::string_view MappingHash() noexcept"), 3)

            self.assertEqual(map_content["schemaVersion"], 1)
            self.assertTrue(map_content["mappingHash"])
            self.assertEqual(map_content["window"]["source"], "window.json")
            self.assertEqual(len(map_content["pages"]), 2)
            self.assertEqual(len(map_content["reticles"]), 5)
            self.assertEqual(len(map_content["templates"]), 1)
            self.assertEqual(len(map_content["blinkTypes"]), 1)
            self.assertEqual(len(map_content["strobes"]), 2)
            self.assertEqual(len(map_content["primitives"]), 6)
            self.assertTrue(all("strobe" not in row for row in map_content["pages"]))
            self.assertTrue(any(row["strobeName"] == "Default" and row["defaultActive"] for row in map_content["strobes"]))
            self.assertTrue(any(row["strobeName"] == "Strobe1" and not row["defaultActive"] for row in map_content["strobes"]))
            self.assertTrue(any(row["reticleId"] == "radar_strobe_default" and row["source"] == "strobe"
                                for row in map_content["reticles"]))
            self.assertTrue(any(row["reticleId"] == "radar_strobe_1" and row["source"] == "strobe"
                                for row in map_content["reticles"]))
            primitive_ids = [row["primitiveId"] for row in map_content["primitives"]]
            self.assertEqual(primitive_ids.count("status_value"), 2)
            self.assertIn("shape", primitive_ids)
            self.assertIn("systemStatus_value", primitive_ids)
            self.assertIn("cursor_line", primitive_ids)
            self.assertIn("marker_label", primitive_ids)

    def test_template_based_strobe_without_explicit_id_uses_template_id_for_runtime_bindings(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            template_dir = root / "reticles"
            template_dir.mkdir()

            (template_dir / "cursor_template.json").write_text(
                json.dumps(
                    {
                        "id": "cursor_template",
                        "elements": [
                            {"id": "cursor_line", "type": "line", "exposed": True},
                        ],
                    }
                ),
                encoding="utf-8",
            )

            page = root / "radar_page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Radar",
                        "layers": [{"id": "default"}],
                        "activeStrobe": "Default",
                        "strobes": [
                            {
                                "name": "Default",
                                "template": "cursor_template",
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )

            window = root / "window.json"
            window.write_text(
                json.dumps(
                    {
                        "title": "Cockpit",
                        "reticleLibraryFolder": "reticles",
                        "pages": [page.name],
                    }
                ),
                encoding="utf-8",
            )

            output_header = root / "GeneratedUi.h"
            output_source = root / "GeneratedUi.cpp"
            output_map = root / "GeneratedUi.generated.map"

            subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(output_header),
                    "--output-source",
                    str(output_source),
                    "--output-map",
                    str(output_map),
                    "--namespace",
                    "generated_ui",
                    "--header-include",
                    "GeneratedUi.h",
                ],
                check=True,
            )

            source_content = output_source.read_text(encoding="utf-8")
            map_content = json.loads(output_map.read_text(encoding="utf-8"))

            self.assertIn('mfd::client::Reticle("Radar", "cursor_template", ', source_content)
            self.assertEqual(map_content["reticles"][0]["reticleId"], "cursor_template")
            self.assertEqual(map_content["reticles"][0]["source"], "strobe")
            self.assertEqual(map_content["strobes"][0]["reticleId"], "cursor_template")

    def test_generates_primitive_specialized_accessors_for_static_and_dynamic_handles(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            template_dir = root / "reticles"
            template_dir.mkdir()

            (template_dir / "geometry_template.json").write_text(
                json.dumps(
                    {
                        "id": "geometry_template",
                        "elements": [
                            {"id": "track_label", "type": "text"},
                            {"id": "heading_line", "type": "line", "exposed": True},
                            {"id": "cursor_circle", "type": "circle", "exposed": True},
                            {"id": "scope_ring", "type": "ring", "exposed": True},
                            {"id": "lock_box", "type": "rectangle", "exposed": True},
                            {"id": "warning_triangle", "type": "triangle", "exposed": True},
                            {"id": "route_polyline", "type": "polyline", "exposed": True},
                            {"id": "overlay_image", "type": "image", "exposed": True},
                        ],
                    }
                ),
                encoding="utf-8",
            )

            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Radar",
                        "layers": [{"id": "default"}],
                        "dynamicReticleBindings": [
                            {"templateId": "geometry_template", "layerId": "default", "orderInLayer": 0}
                        ],
                        "staticReticles": [
                            {
                                "id": "geometry_widget",
                                "template": "geometry_template",
                                "layerId": "default",
                            },
                            {
                                "id": "geometry_panel",
                                "layerId": "default",
                                "elements": [
                                    {"id": "ellipse_zone", "type": "ellipse", "exposed": True},
                                    {"id": "square_marker", "type": "square", "exposed": True},
                                    {"id": "diamond_cue", "type": "diamond", "exposed": True},
                                    {"id": "guide_bezier", "type": "bezier", "exposed": True},
                                    {"id": "scan_arc", "type": "arc", "exposed": True},
                                    {"id": "panel_image", "type": "image", "exposed": True},
                                    {"id": "mission_time", "type": "time"},
                                ],
                            },
                        ],
                    }
                ),
                encoding="utf-8",
            )

            window = root / "window.json"
            window.write_text(
                json.dumps(
                    {
                        "title": "Cockpit",
                        "reticleLibraryFolder": "reticles",
                        "pages": [page.name],
                    }
                ),
                encoding="utf-8",
            )

            output_header = root / "GeneratedUi.h"
            output_source = root / "GeneratedUi.cpp"
            output_map = root / "GeneratedUi.generated.map"

            subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(output_header),
                    "--output-source",
                    str(output_source),
                    "--output-map",
                    str(output_map),
                    "--namespace",
                    "generated_ui",
                    "--header-include",
                    "GeneratedUi.h",
                ],
                check=True,
            )

            header_content = output_header.read_text(encoding="utf-8")
            source_content = output_source.read_text(encoding="utf-8")
            map_content = json.loads(output_map.read_text(encoding="utf-8"))

            for expected in [
                "class GeometryTemplateDynamicReticle final : private mfd::client::DynamicReticle",
                "using TriangleHandle = mfd::client::TriangleHandle;",
                "using PolylineHandle = mfd::client::PolylineHandle;",
                "using BezierHandle = mfd::client::BezierHandle;",
                "using ArcHandle = mfd::client::ArcHandle;",
                "using ImageHandle = mfd::client::ImageHandle;",
                "TextHandle& TrackLabel() noexcept;",
                "LineHandle& HeadingLine() noexcept;",
                "CircleHandle& CursorCircle() noexcept;",
                "RingHandle& ScopeRing() noexcept;",
                "RectangleHandle& LockBox() noexcept;",
                "TriangleHandle& WarningTriangle() noexcept;",
                "PolylineHandle& RoutePolyline() noexcept;",
                "ImageHandle& OverlayImage() noexcept;",
                "class RadarGeometryWidgetReticle final : private mfd::client::Reticle",
                "class RadarGeometryPanelReticle final : private mfd::client::Reticle",
                "EllipseHandle& EllipseZone() noexcept;",
                "SquareHandle& SquareMarker() noexcept;",
                "DiamondHandle& DiamondCue() noexcept;",
                "BezierHandle& GuideBezier() noexcept;",
                "ArcHandle& ScanArc() noexcept;",
                "ImageHandle& PanelImage() noexcept;",
                "TimeHandle& MissionTime() noexcept;",
            ]:
                self.assertIn(expected, header_content)

            self.assertNotIn("void ClearDirty() noexcept;", header_content)
            self.assertNotIn("void Reset() noexcept;", header_content)

            for expected in [
                'headingLine_(MutableDesiredPatch(), DirtyFlag(), "heading_line", ',
                'cursorCircle_(MutableDesiredPatch(), DirtyFlag(), "cursor_circle", ',
                'scopeRing_(MutableDesiredPatch(), DirtyFlag(), "scope_ring", ',
                'lockBox_(MutableDesiredPatch(), DirtyFlag(), "lock_box", ',
                'warningTriangle_(MutableDesiredPatch(), DirtyFlag(), "warning_triangle", ',
                'routePolyline_(MutableDesiredPatch(), DirtyFlag(), "route_polyline", ',
                'overlayImage_(MutableDesiredPatch(), DirtyFlag(), "overlay_image", ',
                'ellipseZone_(MutableDesiredPatch(), DirtyFlag(), "ellipse_zone", ',
                'squareMarker_(MutableDesiredPatch(), DirtyFlag(), "square_marker", ',
                'diamondCue_(MutableDesiredPatch(), DirtyFlag(), "diamond_cue", ',
                'guideBezier_(MutableDesiredPatch(), DirtyFlag(), "guide_bezier", ',
                'scanArc_(MutableDesiredPatch(), DirtyFlag(), "scan_arc", ',
                'panelImage_(MutableDesiredPatch(), DirtyFlag(), "panel_image", ',
                'missionTime_(MutableDesiredPatch(), DirtyFlag(), "mission_time", ',
            ]:
                self.assertIn(expected, source_content)

            self.assertNotIn("void RadarMockupPage::ClearDirty() noexcept", source_content)
            self.assertNotIn("void CockpitMockupUi::ClearDirty() noexcept", source_content)
            self.assertNotIn("void CockpitMockupUi::Reset() noexcept", source_content)
            self.assertNotIn("strobe.Reset();", source_content)
            self.assertNotIn("window_.Reset();", source_content)

            self.assertEqual(len(map_content["templates"]), 1)
            self.assertEqual(len(map_content["reticles"]), 2)
            primitive_ids = [row["primitiveId"] for row in map_content["primitives"]]
            for primitive_id in [
                "warning_triangle",
                "route_polyline",
                "overlay_image",
                "guide_bezier",
                "scan_arc",
                "panel_image",
            ]:
                self.assertIn(primitive_id, primitive_ids)

    def test_generates_dynamic_accessors_only_for_templates_selected_on_each_page(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            template_dir = root / "reticles"
            template_dir.mkdir()

            (template_dir / "alpha_template.json").write_text(
                json.dumps(
                    {
                        "id": "alpha_template",
                        "elements": [{"id": "alpha_text", "type": "text"}],
                    }
                ),
                encoding="utf-8",
            )
            (template_dir / "cursor_template.json").write_text(
                json.dumps(
                    {
                        "id": "cursor_template",
                        "elements": [
                            {"id": "cursor_line", "type": "line", "exposed": True},
                        ],
                    }
                ),
                encoding="utf-8",
            )
            (template_dir / "beta_template.json").write_text(
                json.dumps(
                    {
                        "id": "beta_template",
                        "elements": [{"id": "beta_text", "type": "text"}],
                    }
                ),
                encoding="utf-8",
            )

            alpha_page = root / "alpha_page.json"
            alpha_page.write_text(
                json.dumps(
                    {
                        "name": "Alpha",
                        "layers": [{"id": "default"}],
                        "dynamicReticleBindings": [
                            {"templateId": "alpha_template", "layerId": "default", "orderInLayer": 0}
                        ],
                    }
                ),
                encoding="utf-8",
            )

            beta_page = root / "beta_page.json"
            beta_page.write_text(
                json.dumps(
                    {
                        "name": "Beta",
                        "layers": [{"id": "default"}],
                        "dynamicReticleBindings": [
                            {"templateId": "beta_template", "layerId": "default", "orderInLayer": 0}
                        ],
                    }
                ),
                encoding="utf-8",
            )

            window = root / "window.json"
            window.write_text(
                json.dumps(
                    {
                        "title": "Selective",
                        "reticleLibraryFolder": "reticles",
                        "pages": [alpha_page.name, beta_page.name],
                    }
                ),
                encoding="utf-8",
            )

            output_header = root / "GeneratedUi.h"
            output_source = root / "GeneratedUi.cpp"
            output_map = root / "GeneratedUi.generated.map"

            subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(output_header),
                    "--output-source",
                    str(output_source),
                    "--output-map",
                    str(output_map),
                    "--namespace",
                    "generated_ui",
                    "--header-include",
                    "GeneratedUi.h",
                ],
                check=True,
            )

            header_content = output_header.read_text(encoding="utf-8")
            source_content = output_source.read_text(encoding="utf-8")
            map_content = json.loads(output_map.read_text(encoding="utf-8"))

            self.assertIn("class AlphaTemplateDynamicReticle final : private mfd::client::DynamicReticle", header_content)
            self.assertIn("class BetaTemplateDynamicReticle final : private mfd::client::DynamicReticle", header_content)
            self.assertIn("AlphaTemplateDynamicReticleSet& DynamicAlphaTemplate() noexcept;", header_content)
            self.assertIn("BetaTemplateDynamicReticleSet& DynamicBetaTemplate() noexcept;", header_content)

            self.assertIn(
                "AlphaTemplateDynamicReticleSet& AlphaMockupPage::DynamicAlphaTemplate() noexcept",
                source_content,
            )
            self.assertNotIn(
                "BetaTemplateDynamicReticleSet& AlphaMockupPage::DynamicBetaTemplate() noexcept",
                source_content,
            )
            self.assertIn(
                "BetaTemplateDynamicReticleSet& BetaMockupPage::DynamicBetaTemplate() noexcept",
                source_content,
            )
            self.assertNotIn(
                "AlphaTemplateDynamicReticleSet& BetaMockupPage::DynamicAlphaTemplate() noexcept",
                source_content,
            )

            self.assertEqual({row["templateId"] for row in map_content["templates"]}, {"alpha_template", "beta_template"})

    def test_omits_dynamic_generation_when_page_does_not_configure_dynamic_templates(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            template_dir = root / "reticles"
            template_dir.mkdir()

            (template_dir / "alpha_template.json").write_text(
                json.dumps(
                    {
                        "id": "alpha_template",
                        "elements": [{"id": "alpha_text", "type": "text"}],
                    }
                ),
                encoding="utf-8",
            )

            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Alpha",
                        "layers": [{"id": "default"}],
                    }
                ),
                encoding="utf-8",
            )

            window = root / "window.json"
            window.write_text(
                json.dumps(
                    {
                        "title": "Selective",
                        "reticleLibraryFolder": "reticles",
                        "pages": [page.name],
                    }
                ),
                encoding="utf-8",
            )

            output_header = root / "GeneratedUi.h"
            output_source = root / "GeneratedUi.cpp"
            output_map = root / "GeneratedUi.generated.map"

            subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(output_header),
                    "--output-source",
                    str(output_source),
                    "--output-map",
                    str(output_map),
                ],
                check=True,
            )

            header_content = output_header.read_text(encoding="utf-8")
            source_content = output_source.read_text(encoding="utf-8")
            map_content = json.loads(output_map.read_text(encoding="utf-8"))

            self.assertNotIn("AlphaTemplateDynamicReticle", header_content)
            self.assertNotIn("DynamicAlphaTemplate()", header_content)
            self.assertNotIn("DynamicAlphaTemplate()", source_content)
            self.assertEqual(map_content["templates"], [])

    def test_rejects_page_without_layers(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(json.dumps({"name": "Alpha"}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")
            output_map = root / "GeneratedUi.generated.map"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(root / "GeneratedUi.h"),
                    "--output-source",
                    str(root / "GeneratedUi.cpp"),
                    "--output-map",
                    str(output_map),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("Page layers must be a non-empty JSON array", completed.stderr)

    def test_rejects_static_reticle_without_layer_id(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Radar",
                        "layers": [{"id": "default"}],
                        "staticReticles": [
                            {
                                "id": "track_box",
                                "elements": [{"id": "shape", "type": "line"}],
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")
            output_map = root / "GeneratedUi.generated.map"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(root / "GeneratedUi.h"),
                    "--output-source",
                    str(root / "GeneratedUi.cpp"),
                    "--output-map",
                    str(output_map),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("Static reticle 'track_box' must define a non-empty layerId", completed.stderr)

    def test_rejects_dynamic_binding_with_unknown_layer(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            template_dir = root / "reticles"
            template_dir.mkdir()
            (template_dir / "track_template.json").write_text(
                json.dumps({"id": "track_template", "elements": [{"id": "shape", "type": "line"}]}),
                encoding="utf-8",
            )

            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Radar",
                        "layers": [{"id": "default"}],
                        "dynamicReticleBindings": [
                            {"templateId": "track_template", "layerId": "overlay", "orderInLayer": 0}
                        ],
                    }
                ),
                encoding="utf-8",
            )
            window = root / "window.json"
            window.write_text(
                json.dumps({"reticleLibraryFolder": "reticles", "pages": [page.name]}),
                encoding="utf-8",
            )
            output_map = root / "GeneratedUi.generated.map"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(root / "GeneratedUi.h"),
                    "--output-source",
                    str(root / "GeneratedUi.cpp"),
                    "--output-map",
                    str(output_map),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("references unknown runtime layer 'overlay'", completed.stderr)

    def test_rejects_duplicate_dynamic_binding_order_within_one_layer(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            template_dir = root / "reticles"
            template_dir.mkdir()
            (template_dir / "alpha_template.json").write_text(
                json.dumps({"id": "alpha_template", "elements": [{"id": "shape", "type": "line"}]}),
                encoding="utf-8",
            )
            (template_dir / "beta_template.json").write_text(
                json.dumps({"id": "beta_template", "elements": [{"id": "shape", "type": "line"}]}),
                encoding="utf-8",
            )

            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Radar",
                        "layers": [{"id": "default"}],
                        "dynamicReticleBindings": [
                            {"templateId": "alpha_template", "layerId": "default", "orderInLayer": 0},
                            {"templateId": "beta_template", "layerId": "default", "orderInLayer": 0},
                        ],
                    }
                ),
                encoding="utf-8",
            )
            window = root / "window.json"
            window.write_text(
                json.dumps({"reticleLibraryFolder": "reticles", "pages": [page.name]}),
                encoding="utf-8",
            )
            output_map = root / "GeneratedUi.generated.map"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(root / "GeneratedUi.h"),
                    "--output-source",
                    str(root / "GeneratedUi.cpp"),
                    "--output-map",
                    str(output_map),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("Duplicate page dynamic binding orderInLayer 0 on layer 'default'", completed.stderr)



    def test_detects_single_text_primitive_without_suffix_hint(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Main",
                        "layers": [{"id": "default"}],
                        "staticReticles": [
                            {
                                "id": "multi_element_status",
                                "layerId": "default",
                                "elements": [
                                    {"id": "label", "type": "text"},
                                    {"id": "shape_one", "type": "line"},
                                    {"id": "shape_two", "type": "line"},
                                ],
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")

            output_header = root / "GeneratedUi.h"
            output_source = root / "GeneratedUi.cpp"
            output_map = root / "GeneratedUi.generated.map"

            subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(output_header),
                    "--output-source",
                    str(output_source),
                    "--output-map",
                    str(output_map),
                ],
                check=True,
            )

            header_content = output_header.read_text(encoding="utf-8")
            self.assertIn("MainMultiElementStatusReticle multiElementStatus;", header_content)
            self.assertIn("TextHandle& Label() noexcept;", header_content)

    def test_mapping_does_not_invent_default_page_when_window_json_omits_it(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(json.dumps({"name": "Main", "layers": [{"id": "default"}]}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")

            output_header = root / "GeneratedUi.h"
            output_source = root / "GeneratedUi.cpp"
            output_map = root / "GeneratedUi.generated.map"

            subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(output_header),
                    "--output-source",
                    str(output_source),
                    "--output-map",
                    str(output_map),
                ],
                check=True,
            )

            map_content = json.loads(output_map.read_text(encoding="utf-8"))
            self.assertEqual(len(map_content["pages"]), 1)
            self.assertFalse(map_content["pages"][0]["defaultPage"])

    def test_print_inputs_lists_window_and_page_files(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(json.dumps({"name": "Main", "layers": [{"id": "default"}]}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")
            output_map = root / "unused.generated.map"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(root / "unused.h"),
                    "--output-source",
                    str(root / "unused.cpp"),
                    "--output-map",
                    str(output_map),
                    "--print-inputs",
                ],
                check=True,
                capture_output=True,
                text=True,
            )

            output_lines = [line.strip() for line in completed.stdout.splitlines() if line.strip()]
            normalized_lines = {line.replace("\\", "/") for line in output_lines}
            self.assertIn(str(window.resolve()).replace("\\", "/"), normalized_lines)
            self.assertIn(str(page.resolve()).replace("\\", "/"), normalized_lines)

    def test_requires_output_map_argument(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(json.dumps({"name": "Main", "layers": [{"id": "default"}]}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")
            output_map = root / "GeneratedUi.generated.map"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(root / "GeneratedUi.h"),
                    "--output-source",
                    str(root / "GeneratedUi.cpp"),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("--output-map", completed.stderr)

    def test_rejects_invalid_namespace(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(json.dumps({"name": "Main", "layers": [{"id": "default"}]}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")
            output_map = root / "GeneratedUi.generated.map"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(root / "GeneratedUi.h"),
                    "--output-source",
                    str(root / "GeneratedUi.cpp"),
                    "--output-map",
                    str(output_map),
                    "--namespace",
                    "invalid-namespace",
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("Invalid C++ namespace", completed.stderr)

    def test_rejects_duplicate_generated_page_names(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page_one = root / "page_one.json"
            page_two = root / "page_two.json"
            page_one.write_text(json.dumps({"name": "Radar Main", "layers": [{"id": "default"}]}), encoding="utf-8")
            page_two.write_text(json.dumps({"name": "Radar_Main", "layers": [{"id": "default"}]}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page_one.name, page_two.name]}), encoding="utf-8")
            output_map = root / "GeneratedUi.generated.map"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(root / "GeneratedUi.h"),
                    "--output-source",
                    str(root / "GeneratedUi.cpp"),
                    "--output-map",
                    str(output_map),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("Duplicate generated page class name", completed.stderr)

    def test_rejects_duplicate_generated_static_reticle_names(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Radar",
                        "layers": [{"id": "default"}],
                        "staticReticles": [
                            {
                                "id": "lock-box",
                                "layerId": "default",
                                "elements": [{"id": "left_value", "type": "text"}],
                            },
                            {
                                "id": "lock_box",
                                "layerId": "default",
                                "elements": [{"id": "right_value", "type": "text"}],
                            },
                        ],
                    }
                ),
                encoding="utf-8",
            )
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")
            output_map = root / "GeneratedUi.generated.map"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(root / "GeneratedUi.h"),
                    "--output-source",
                    str(root / "GeneratedUi.cpp"),
                    "--output-map",
                    str(output_map),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("Duplicate generated reticle class name", completed.stderr)

    def test_rejects_duplicate_generated_primitive_accessor_names(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Radar",
                        "layers": [{"id": "default"}],
                        "staticReticles": [
                            {
                                "id": "geometry_panel",
                                "layerId": "default",
                                "elements": [
                                    {"id": "track-label", "type": "text", "exposed": True},
                                    {"id": "track_label", "type": "line", "exposed": True},
                                ],
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")
            output_map = root / "GeneratedUi.generated.map"

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(root / "GeneratedUi.h"),
                    "--output-source",
                    str(root / "GeneratedUi.cpp"),
                    "--output-map",
                    str(output_map),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("Duplicate generated primitive accessor name", completed.stderr)

    def test_requires_force_flag_to_overwrite_existing_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(json.dumps({"name": "Main", "layers": [{"id": "default"}]}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")
            output_header = root / "GeneratedUi.h"
            output_source = root / "GeneratedUi.cpp"
            output_map = root / "GeneratedUi.generated.map"
            output_header.write_text("// existing", encoding="utf-8")
            output_source.write_text("// existing", encoding="utf-8")
            output_map.write_text("// existing", encoding="utf-8")

            completed = subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(output_header),
                    "--output-source",
                    str(output_source),
                    "--output-map",
                    str(output_map),
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("Refusing to overwrite existing output file(s)", completed.stderr)

            subprocess.run(
                [
                    sys.executable,
                    str(self.generator),
                    "--window-json",
                    str(window),
                    "--output-header",
                    str(output_header),
                    "--output-source",
                    str(output_source),
                    "--output-map",
                    str(output_map),
                    "--force-overwrite",
                ],
                check=True,
            )


if __name__ == "__main__":
    parsed_args = parse_args()
    GENERATOR_PATH = Path(parsed_args.generator).resolve()
    sys.argv = [sys.argv[0]]
    unittest.main()
