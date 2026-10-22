#!/usr/bin/env python3
"""Black-box widget generation and dependency regression tests."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

GENERATOR = Path(__file__).resolve().parents[2] / "mfd_client_api/generator/scripts/generate_ui.py"

class WidgetGeneratorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "assets"
        for folder in ("windows", "pages", "widgets", "reticles"):
            (self.root / folder).mkdir(parents=True)
        self.window = {"title": "Widgets", "apiLevel": 1, "pages": ["../pages/main.json"], "reticleLibraryFolder": "../reticles"}
        self.definition = {"schemaVersion": 1, "id": "Shared", "type": "toggle",
            "visuals": [{"state": "normal", "reticle": {"id": "Visual", "elements": [{"id": "Text", "type": "text", "text": "OK"}]}}]}
        self.page = {"name": "Main", "layers": [{"id": "main"}], "widgets": [
            {"id": "First", "definition": "../widgets/shared.json", "exposed": True},
            {"id": "Private", "definition": "../widgets/shared.json", "exposed": False}]}
        self.header = self.root / "Generated.h"
        self.source = self.root / "Generated.cpp"
        self.mapping = self.root / "Generated.map"

    def run_generator(self, success=True, inputs=False):
        for name, value in (("windows/window.json", self.window), ("pages/main.json", self.page), ("widgets/shared.json", self.definition)):
            (self.root / name).write_text(json.dumps(value), encoding="utf-8")
        command = [sys.executable, str(GENERATOR), "--window-json", str(self.root / "windows/window.json")]
        if inputs:
            command.append("--print-inputs")
        command += ["--output-header", str(self.header), "--output-source", str(self.source),
                        "--output-map", str(self.mapping), "--namespace", "test_ui", "--header-include", "Generated.h", "--force-overwrite"]
        result = subprocess.run(command, capture_output=True, text=True)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        return result

    def test_widget_only_private_identity_and_public_typed_handle(self):
        self.run_generator()
        header = self.header.read_text()
        self.assertIn("CheckedWidget", header)
        self.assertIn("FirstWidget", header)
        self.assertNotIn("PrivateWidget", header)
        mapping = json.loads(self.mapping.read_text())
        self.assertEqual(len(mapping["widgets"]), 2)
        self.assertNotEqual(mapping["widgets"][0]["id"], mapping["widgets"][1]["id"])
        before = [p.read_bytes() for p in (self.header, self.source, self.mapping)]
        self.run_generator()
        self.assertEqual(before, [p.read_bytes() for p in (self.header, self.source, self.mapping)])

    def test_dependencies_include_widget_file_and_generator(self):
        output = self.run_generator(inputs=True).stdout.replace("\\", "/")
        self.assertIn("widgets/shared.json", output)
        self.assertIn("widgets/generator/generate_widgets.py", output)

    def test_legacy_and_empty_widgets_outputs_are_identical(self):
        self.window.pop("apiLevel")
        self.page.pop("widgets")
        self.run_generator()
        before = [p.read_bytes() for p in (self.header, self.source, self.mapping)]
        self.page["widgets"] = []
        self.run_generator()
        self.assertEqual(before, [p.read_bytes() for p in (self.header, self.source, self.mapping)])

    def test_level_zero_rejected_in_generation_and_dependency_scan(self):
        self.window["apiLevel"] = 0
        self.run_generator(False)
        self.run_generator(False, inputs=True)

    def test_unknown_schema_fields_and_versions_fail(self):
        for update in ({"schemaVersion": 2}, {"futureField": 1}, {"type": "future"}):
            with self.subTest(update=update):
                original = dict(self.definition)
                self.definition.update(update)
                self.run_generator(False)
                self.definition = original

    def test_case_collision_and_cpp_accessor_collision_fail(self):
        for identity in (" first ", "First"):
            with self.subTest(identity=identity):
                self.page["widgets"][1]["id"] = identity
                self.run_generator(False)
        self.page["widgets"][1]["id"] = "Private"
        self.page["containers"] = [{"id": "FirstWidget", "exposed": True}]
        self.run_generator(False)

    def test_invalid_default_cannot_be_hidden_by_instance_override(self):
        self.definition["defaults"] = {"value": 2}
        for widget in self.page["widgets"]:
            widget["overrides"] = {"value": 0}
        self.run_generator(False)

    def test_binding_requires_existing_primitive_and_text_geometry(self):
        visual = self.definition["visuals"][0]
        visual["bindings"] = [{"primitive": "Missing", "target": "text"}]
        self.run_generator(False)
        visual["bindings"][0]["primitive"] = "Text"
        self.run_generator()
        visual["reticle"]["elements"][0]["type"] = "rectangle"
        self.run_generator(False)

    def test_root_escape_and_symlink_escape_fail(self):
        self.page["widgets"][0]["definition"] = "../../outside.json"
        self.run_generator(False)
        outside = Path(self.temp.name) / "outside.json"
        outside.write_text(json.dumps(self.definition))
        link = self.root / "widgets/link.json"
        try:
            link.symlink_to(outside)
        except OSError:
            return  # Windows hosts may lack the symlink privilege.
        self.page["widgets"][0]["definition"] = "../widgets/link.json"
        self.run_generator(False)

    def test_mixed_ownership_and_exact_reference_validation(self):
        self.page["containers"] = [{"id": "Parent", "children": [{"kind": "widget", "id": "First"}]}]
        self.run_generator()
        self.page["containers"][0]["children"][0]["id"] = "first"
        self.run_generator(False)

if __name__ == "__main__":
    unittest.main()
