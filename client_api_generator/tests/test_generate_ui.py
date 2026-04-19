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
            output_shm_header = root / "GeneratedShm.h"
            output_shm_source = root / "GeneratedShm.cpp"

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
                    "--namespace",
                    "generated_ui",
                    "--header-include",
                    "GeneratedUi.h",
                    "--output-shm-header",
                    str(output_shm_header),
                    "--output-shm-source",
                    str(output_shm_source),
                ],
                check=True,
            )

            header_content = output_header.read_text(encoding="utf-8")
            source_content = output_source.read_text(encoding="utf-8")
            shm_header_content = output_shm_header.read_text(encoding="utf-8")
            shm_source_content = output_shm_source.read_text(encoding="utf-8")

            for expected in [
                "class RadarMockupPage",
                "class SystemMockupPage",
                "class CockpitMockupUi",
                "bool SendStartup(mfd::CommandClient& client, const mfd::PageViewState& view, std::string statusText);",
                "void Reset() noexcept;",
                "std::vector<mfd::UserCommand> BuildBatch();",
                "bool SubmitLatest(mfd::client::LatestBatchPublisher& publisher, std::uint32_t sequence = 0);",
                "std::vector<mfd::UserCommand> BuildShutdownBatch(std::string statusText);",
                "bool SubmitShutdown(mfd::client::LatestBatchPublisher& publisher, std::uint32_t sequence, std::string statusText);",
                "WindowDisplay& Window() noexcept;",
                "RadarMockupPage& Radar() noexcept;",
                "SystemMockupPage& System() noexcept;",
                "BlinkType attention {\"attention\"};",
                "TextReticle radarStatus;",
                "Reticle trackBox;",
                "TextReticle systemStatus;",
            ]:
                self.assertIn(expected, header_content)

            for expected in [
                "class ShmClientPublisher",
                "class MfdRadarShmAdapterPlugin",
                "CreateMfdShmAdapterPlugin",
            ]:
                self.assertIn(expected, shm_header_content)

            self.assertIn('extern "C" mfd::IMfdShmAdapterPlugin* CreateMfdShmAdapterPlugin()', shm_source_content)

            for expected in [
                "SystemMockupPage::SetStatusCaption(std::string value)",
                "systemStatus.SetValue(std::move(value));",
                "if (!client.ActivatePage(SystemMockupPage::Name()))",
                "if (!client.SetPageView(SystemMockupPage::Name(), view.center, view.zoom))",
                "return publisher.SubmitLatest(BuildBatch(), sequence);",
                "return publisher.SubmitLatest(BuildShutdownBatch(std::move(statusText)), sequence);",
            ]:
                self.assertIn(expected, source_content)

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

    def test_requires_force_flag_to_overwrite_existing_outputs(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            page = root / "page.json"
            page.write_text(json.dumps({"name": "Main"}), encoding="utf-8")
            window = root / "window.json"
            window.write_text(json.dumps({"pages": [page.name]}), encoding="utf-8")
            output_header = root / "GeneratedUi.h"
            output_source = root / "GeneratedUi.cpp"
            output_shm_header = root / "GeneratedShm.h"
            output_shm_source = root / "GeneratedShm.cpp"
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
