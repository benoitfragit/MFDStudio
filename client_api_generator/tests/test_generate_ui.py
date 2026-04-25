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

            page_one.write_text(
                json.dumps(
                    {
                        "page": {
                            "name": "Radar",
                            "blinkTypes": [{"name": "attention"}],
                            "staticReticles": [
                                {
                                    "id": "radar_status",
                                    "template": "status_template",
                                },
                                {
                                    "id": "track_box",
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
                        "staticReticles": [
                            {
                                "id": "systemStatus",
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
                "class RadarMockupPage",
                "class SystemMockupPage",
                "class CockpitMockupUi",
                "class StatusTemplateDynamicReticle final : public DynamicReticle",
                "class StatusTemplateDynamicReticleSet final : public GeneratedDynamicReticleSet",
                "class RadarRadarStatusReticle final : public Reticle",
                "class RadarTrackBoxReticle final : public Reticle",
                "class SystemSystemStatusReticle final : public Reticle",
                "void SetValue(std::string value);",
                "static constexpr std::string_view MappingHash() noexcept",
                "bool SendStartup(mfd::CommandClient& client, const mfd::PageViewState& view, std::string statusText);",
                "void Reset() noexcept;",
                "std::vector<mfd::UserCommand> BuildBatch();",
                "mfd::CommandBatch BuildCommandBatch(std::uint32_t sequence = 0);",
                "bool SubmitLatest(mfd::client::LatestBatchPublisher& publisher, std::uint32_t sequence = 0);",
                "std::vector<mfd::UserCommand> BuildShutdownBatch(std::string statusText);",
                "mfd::CommandBatch BuildShutdownCommandBatch(std::uint32_t sequence, std::string statusText);",
                "bool SubmitShutdown(mfd::client::LatestBatchPublisher& publisher, std::uint32_t sequence, std::string statusText);",
                "WindowDisplay& Window() noexcept;",
                "RadarMockupPage& Radar() noexcept;",
                "SystemMockupPage& System() noexcept;",
                "BlinkType attention {\"attention\",",
                "StatusTemplateDynamicReticleSet& DynamicStatusTemplate() noexcept;",
                "TextHandle& StatusValue() noexcept;",
                "LineHandle& Shape() noexcept;",
                "TextHandle& SystemStatusValue() noexcept;",
                "StrobeHandle strobe;",
                "RadarRadarStatusReticle radarStatus;",
                "RadarTrackBoxReticle trackBox;",
                "SystemSystemStatusReticle systemStatus;",
            ]:
                self.assertIn(expected, header_content)

            self.assertNotIn("DynamicReticleSet& Dynamic(std::string_view templateId);", header_content)

            for expected in [
                "StatusTemplateDynamicReticle::StatusTemplateDynamicReticle(std::string_view reticleId)",
                "StatusTemplateDynamicReticleSet::StatusTemplateDynamicReticleSet(std::string_view pageName, const mfd::TransportId pageTransportId)",
                "GeneratedDynamicReticleSet(pageName, \"status_template\", pageTransportId, ",
                "StatusTemplateDynamicReticleSet& RadarMockupPage::DynamicStatusTemplate() noexcept",
                "SystemMockupPage::SetStatusCaption(std::string value)",
                "RadarRadarStatusReticle::SetValue(std::string value)",
                "systemStatus.SystemStatusValue().SetText(std::move(value));",
                "if (!client.ActivatePage(SystemMockupPage::Name()))",
                "if (!client.SetPageView(SystemMockupPage::Name(), view.center, view.zoom))",
                "batch.mappingHash = ",
                "return publisher.SubmitLatest(BuildCommandBatch(sequence));",
                "return publisher.SubmitLatest(BuildShutdownCommandBatch(sequence, std::move(statusText)));",
                "radar_.AppendShutdownCommands(commands, std::string {});",
                "system_.AppendShutdownCommands(commands, std::move(statusText));",
                "count += strobe.AppendCommands(commands) ? 1U : 0U;",
            ]:
                self.assertIn(expected, source_content)

            self.assertIn(map_content["mappingHash"], header_content)
            self.assertIn(map_content["mappingHash"], source_content)

            self.assertEqual(map_content["schemaVersion"], 1)
            self.assertTrue(map_content["mappingHash"])
            self.assertEqual(map_content["window"]["source"], "window.json")
            self.assertEqual(len(map_content["pages"]), 2)
            self.assertEqual(len(map_content["reticles"]), 3)
            self.assertEqual(len(map_content["templates"]), 1)
            self.assertEqual(len(map_content["blinkTypes"]), 1)
            self.assertEqual(len(map_content["primitives"]), 4)
            self.assertTrue(all("strobe" not in row for row in map_content["pages"]))
            primitive_ids = [row["primitiveId"] for row in map_content["primitives"]]
            self.assertEqual(primitive_ids.count("status_value"), 2)
            self.assertIn("shape", primitive_ids)
            self.assertIn("systemStatus_value", primitive_ids)

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
                        "staticReticles": [
                            {
                                "id": "geometry_widget",
                                "template": "geometry_template",
                            },
                            {
                                "id": "geometry_panel",
                                "elements": [
                                    {"id": "ellipse_zone", "type": "ellipse", "exposed": True},
                                    {"id": "square_marker", "type": "square", "exposed": True},
                                    {"id": "diamond_cue", "type": "diamond", "exposed": True},
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
                "class GeometryTemplateDynamicReticle final : public DynamicReticle",
                "TextHandle& TrackLabel() noexcept;",
                "LineHandle& HeadingLine() noexcept;",
                "CircleHandle& CursorCircle() noexcept;",
                "RingHandle& ScopeRing() noexcept;",
                "RectangleHandle& LockBox() noexcept;",
                "class RadarGeometryWidgetReticle final : public Reticle",
                "class RadarGeometryPanelReticle final : public Reticle",
                "EllipseHandle& EllipseZone() noexcept;",
                "SquareHandle& SquareMarker() noexcept;",
                "DiamondHandle& DiamondCue() noexcept;",
                "TimeHandle& MissionTime() noexcept;",
            ]:
                self.assertIn(expected, header_content)

            for expected in [
                'headingLine_(MutableDesiredPatch(), DirtyFlag(), "heading_line", ',
                'cursorCircle_(MutableDesiredPatch(), DirtyFlag(), "cursor_circle", ',
                'scopeRing_(MutableDesiredPatch(), DirtyFlag(), "scope_ring", ',
                'lockBox_(MutableDesiredPatch(), DirtyFlag(), "lock_box", ',
                'ellipseZone_(MutableDesiredPatch(), DirtyFlag(), "ellipse_zone", ',
                'squareMarker_(MutableDesiredPatch(), DirtyFlag(), "square_marker", ',
                'diamondCue_(MutableDesiredPatch(), DirtyFlag(), "diamond_cue", ',
                'missionTime_(MutableDesiredPatch(), DirtyFlag(), "mission_time", ',
            ]:
                self.assertIn(expected, source_content)

            self.assertEqual(len(map_content["templates"]), 1)
            self.assertEqual(len(map_content["reticles"]), 2)
            self.assertEqual(len(map_content["primitives"]), 14)



    def test_detects_single_text_primitive_without_suffix_hint(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Main",
                        "staticReticles": [
                            {
                                "id": "multi_element_status",
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
            page.write_text(json.dumps({"name": "Main"}), encoding="utf-8")
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
            page.write_text(json.dumps({"name": "Main"}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")

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

    def test_rejects_invalid_namespace(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(json.dumps({"name": "Main"}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")

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
            page_one.write_text(json.dumps({"name": "Radar Main"}), encoding="utf-8")
            page_two.write_text(json.dumps({"name": "Radar_Main"}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page_one.name, page_two.name]}), encoding="utf-8")

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
            self.assertIn("Duplicate generated page class name", completed.stderr)

    def test_rejects_duplicate_generated_static_reticle_names(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Radar",
                        "staticReticles": [
                            {"id": "lock-box", "elements": [{"id": "left_value", "type": "text"}]},
                            {"id": "lock_box", "elements": [{"id": "right_value", "type": "text"}]},
                        ],
                    }
                ),
                encoding="utf-8",
            )
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")

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
            self.assertIn("Duplicate generated reticle class name", completed.stderr)

    def test_rejects_duplicate_generated_primitive_accessor_names(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(
                json.dumps(
                    {
                        "name": "Radar",
                        "staticReticles": [
                            {
                                "id": "geometry_panel",
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
            self.assertIn("Duplicate generated primitive accessor name", completed.stderr)

    def test_requires_force_flag_to_overwrite_existing_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(json.dumps({"name": "Main"}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")
            output_header = root / "GeneratedUi.h"
            output_source = root / "GeneratedUi.cpp"
            output_header.write_text("// existing", encoding="utf-8")
            output_source.write_text("// existing", encoding="utf-8")

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
                    "--force-overwrite",
                ],
                check=True,
            )


if __name__ == "__main__":
    parsed_args = parse_args()
    GENERATOR_PATH = Path(parsed_args.generator).resolve()
    sys.argv = [sys.argv[0]]
    unittest.main()
