#!/usr/bin/env python3
#
# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path


TEXT_TYPES = {"text", "time"}


@dataclass(frozen=True)
class ReticleSpec:
    reticle_id: str
    member_name: str
    cpp_type: str
    text_primitive_id: str | None


@dataclass(frozen=True)
class PageSpec:
    page_name: str
    page_class_name: str
    accessor_name: str
    ui_member_name: str
    blink_members: list[tuple[str, str]]
    reticles: list[ReticleSpec]
    status_member_name: str | None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate high-level client mockup UI code from one window JSON.")
    parser.add_argument("--window-json", required=True)
    parser.add_argument("--output-header", required=True)
    parser.add_argument("--output-source", required=True)
    parser.add_argument("--namespace", default="mockup_ui")
    parser.add_argument("--ui-class-name")
    parser.add_argument("--page-class-suffix", default="MockupPage")
    parser.add_argument("--ui-class-suffix", default="MockupUi")
    parser.add_argument("--header-include", default="MockupUi.h")
    parser.add_argument("--print-inputs", action="store_true")
    return parser.parse_args()


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def resolve_path(base: Path, raw_path: str) -> Path:
    path = Path(raw_path)
    if path.is_absolute():
        return path.resolve()
    return (base / path).resolve()


def extract_page_node(root: dict) -> dict:
    page = root.get("page")
    return page if isinstance(page, dict) else root


def split_words(value: str) -> list[str]:
    cleaned = re.sub(r"[^0-9A-Za-z]+", " ", value)
    words = [word for word in cleaned.strip().split() if word]
    return words or ["Generated"]


def pascal_case(value: str) -> str:
    words = split_words(value)
    result = "".join(word[:1].upper() + word[1:] for word in words)
    if result[:1].isdigit():
        result = f"Generated{result}"
    return result


def camel_case(value: str) -> str:
    name = pascal_case(value)
    return name[:1].lower() + name[1:]


def cpp_string(value: str) -> str:
    return value.replace("\\", "\\\\").replace("\"", "\\\"")


def page_entries(window_root: dict) -> list[str]:
    pages = window_root.get("pages") or window_root.get("pageFiles") or window_root.get("pageJsons")
    if not isinstance(pages, list) or not pages:
        raise RuntimeError("Window JSON must define a non-empty pages array")

    resolved: list[str] = []
    for entry in pages:
        if isinstance(entry, str):
            resolved.append(entry)
            continue

        if isinstance(entry, dict):
            file_value = entry.get("file") or entry.get("path") or entry.get("json")
            if isinstance(file_value, str):
                resolved.append(file_value)
                continue

        raise RuntimeError("Unsupported page entry in window JSON")

    return resolved


def load_reticle_library(window_root: dict, window_path: Path) -> dict[str, dict]:
    raw_folder = window_root.get("reticleLibraryFolder") or window_root.get("reticles") or window_root.get("reticleFolder")
    folder = resolve_path(window_path.parent, raw_folder or ".")
    library: dict[str, dict] = {}

    if not folder.exists():
        return library

    for file_path in sorted(folder.glob("*.json")):
        root = load_json(file_path)
        template_id = root.get("id") or file_path.stem
        if isinstance(template_id, str):
            library[template_id] = root

    return library


def resolved_elements(reticle_node: dict, template_library: dict[str, dict]) -> list[dict]:
    elements = reticle_node.get("elements")
    if not isinstance(elements, list):
        template_id = reticle_node.get("template") or reticle_node.get("sourceTemplateId")
        template = template_library.get(template_id) if isinstance(template_id, str) else None
        elements = template.get("elements") if isinstance(template, dict) else []

    return [element for element in elements if isinstance(element, dict)]


def text_primitive_ids(reticle_node: dict, template_library: dict[str, dict]) -> tuple[list[str], int]:
    elements = resolved_elements(reticle_node, template_library)
    result: list[str] = []
    for element in elements:
        primitive_id = element.get("id")
        primitive_type = str(element.get("type", "")).lower()
        if isinstance(primitive_id, str) and primitive_type in TEXT_TYPES:
            result.append(primitive_id)

    return result, len(elements)


def choose_text_binding(primitive_ids: list[str], element_count: int) -> str | None:
    for suffix in ("_value", "_caption", "_text"):
        for primitive_id in primitive_ids:
            if primitive_id.endswith(suffix):
                return primitive_id

    if len(primitive_ids) == 1 and element_count <= 2:
        return primitive_ids[0]

    return None


def build_page_specs(window_root: dict,
                     window_path: Path,
                     page_class_suffix: str) -> list[PageSpec]:
    template_library = load_reticle_library(window_root, window_path)
    page_specs: list[PageSpec] = []

    for page_entry in page_entries(window_root):
        page_path = resolve_path(window_path.parent, page_entry)
        page_root = extract_page_node(load_json(page_path))

        page_name = page_root.get("name") or page_root.get("id")
        if not isinstance(page_name, str) or not page_name:
            raise RuntimeError(f"Page file '{page_path}' does not define a valid name")

        page_base_name = pascal_case(page_name)
        reticles: list[ReticleSpec] = []
        for reticle in page_root.get("staticReticles", []):
            if not isinstance(reticle, dict):
                continue

            reticle_id = reticle.get("id")
            if not isinstance(reticle_id, str) or not reticle_id:
                continue

            member_name = camel_case(reticle_id)
            primitive_ids, element_count = text_primitive_ids(reticle, template_library)
            text_binding = choose_text_binding(primitive_ids, element_count)
            cpp_type = "TextReticle" if text_binding is not None else "Reticle"
            reticles.append(ReticleSpec(reticle_id, member_name, cpp_type, text_binding))

        blink_members: list[tuple[str, str]] = []
        for blink_type in page_root.get("blinkTypes", []):
            if not isinstance(blink_type, dict):
                continue
            blink_name = blink_type.get("name") or blink_type.get("id") or blink_type.get("type")
            if isinstance(blink_name, str) and blink_name:
                blink_members.append((camel_case(blink_name), blink_name))

        expected_status_name = f"{camel_case(page_name)}Status"
        status_member_name = None
        for reticle in reticles:
            if reticle.member_name == expected_status_name and reticle.cpp_type == "TextReticle":
                status_member_name = reticle.member_name
                break

        page_specs.append(PageSpec(
            page_name=page_name,
            page_class_name=f"{page_base_name}{page_class_suffix}",
            accessor_name=page_base_name,
            ui_member_name=f"{camel_case(page_name)}_",
            blink_members=blink_members,
            reticles=reticles,
            status_member_name=status_member_name,
        ))

    return page_specs


def derive_ui_class_name(window_root: dict,
                         page_specs: list[PageSpec],
                         explicit_name: str | None,
                         page_class_suffix: str,
                         ui_class_suffix: str,
                         window_json: str) -> str:
    if explicit_name:
        return explicit_name

    if len(page_specs) == 1:
        base_name = page_specs[0].page_class_name
        if page_class_suffix and base_name.endswith(page_class_suffix):
            base_name = base_name[:-len(page_class_suffix)]
        elif base_name.endswith("Page"):
            base_name = base_name.removesuffix("Page")
        return f"{base_name}{ui_class_suffix}"

    title = window_root.get("title")
    if isinstance(title, str) and title:
        return f"{pascal_case(title)}{ui_class_suffix}"

    return f"{pascal_case(Path(window_json).stem)}{ui_class_suffix}"


def emit_header(namespace_name: str,
                ui_class_name: str,
                window_json: str,
                page_specs: list[PageSpec]) -> str:
    lines: list[str] = [
        "/*",
        " * This file is part of MFDStudio.",
        " * Project author: Benoit Fra",
        " * Repository: https://github.com/benoitfragit/MFDStudio",
        " *",
        " * This file was generated by client_api_generator.",
        " */",
        "#pragma once",
        "",
        "#include <cstddef>",
        "#include <memory>",
        "#include <string>",
        "#include <string_view>",
        "#include <vector>",
        "",
        '#include "mfd/client/Animation.h"',
        '#include "mfd/control/CommandClient.h"',
        "",
        f"namespace {namespace_name}",
        "{",
        "using BlinkType = mfd::client::BlinkType;",
        "using DynamicReticle = mfd::client::DynamicReticle;",
        "using DynamicReticleSet = mfd::client::DynamicReticleSet;",
        "using Reticle = mfd::client::Reticle;",
        "using ReticleBlink = mfd::client::ReticleBlink;",
        "using TextReticle = mfd::client::TextReticle;",
        "using WindowDisplay = mfd::client::WindowDisplay;",
        "",
    ]

    for page in page_specs:
        lines.extend([
            f"class {page.page_class_name}",
            "{",
            "public:",
            "    static constexpr std::string_view Name() noexcept",
            "    {",
            f'        return "{cpp_string(page.page_name)}";',
            "    }",
            "",
            f"    {page.page_class_name}();",
            "",
            "    void Reset() noexcept;",
            "    std::size_t AppendCommands(std::vector<mfd::UserCommand>& commands);",
            "    std::size_t AppendShutdownCommands(std::vector<mfd::UserCommand>& commands, std::string statusText);",
            "    void SetStatusCaption(std::string value);",
            "    DynamicReticleSet& Dynamic(std::string_view templateId);",
            "",
        ])

        for blink_member_name, blink_name in page.blink_members:
            lines.append(f'    BlinkType {blink_member_name} {{"{cpp_string(blink_name)}"}};')

        if page.blink_members:
            lines.append("")

        for reticle in page.reticles:
            lines.append(f"    {reticle.cpp_type} {reticle.member_name};")

        lines.extend([
            "",
            "private:",
            "    struct DynamicTemplateSet",
            "    {",
            "        std::string templateId;",
            "        std::unique_ptr<DynamicReticleSet> set;",
            "    };",
            "",
            "    DynamicTemplateSet* FindDynamicSet(std::string_view templateId) noexcept;",
            "    std::vector<DynamicTemplateSet> dynamicReticleSets_ {};",
            "};",
            "",
        ])

    lines.extend([
        f"class {ui_class_name}",
        "{",
        "public:",
        "    static constexpr std::string_view WindowFile() noexcept",
        "    {",
        f'        return "{cpp_string(window_json)}";',
        "    }",
        "",
        f"    {ui_class_name}();",
        "",
        "    bool SendStartup(mfd::CommandClient& client, const mfd::PageViewState& view, std::string statusText);",
        "    void Reset() noexcept;",
        "    std::vector<mfd::UserCommand> BuildBatch();",
        "    std::vector<mfd::UserCommand> BuildShutdownBatch(std::string statusText);",
        "",
        "    WindowDisplay& Window() noexcept;",
    ])

    for page in page_specs:
        lines.append(f"    {page.page_class_name}& {page.accessor_name}() noexcept;")

    lines.extend([
        "",
        "private:",
        "    WindowDisplay window_ {};",
    ])

    for page in page_specs:
        lines.append(f"    {page.page_class_name} {page.ui_member_name} {{}};")

    lines.extend([
        "};",
        f"}} // namespace {namespace_name}",
        "",
    ])
    return "\n".join(lines)


def emit_source(namespace_name: str,
                header_include: str,
                ui_class_name: str,
                page_specs: list[PageSpec]) -> str:
    startup_page = page_specs[0]
    lines: list[str] = [
        "/*",
        " * This file is part of MFDStudio.",
        " * Project author: Benoit Fra",
        " * Repository: https://github.com/benoitfragit/MFDStudio",
        " *",
        " * This file was generated by client_api_generator.",
        " */",
        f'#include "{header_include}"',
        "",
        "#include <algorithm>",
        "#include <memory>",
        "#include <utility>",
        "",
        f"namespace {namespace_name}",
        "{",
    ]

    for page in page_specs:
        ctor_initializers: list[str] = []
        for reticle in page.reticles:
            if reticle.text_primitive_id is None:
                ctor_initializers.append(
                    f'    {reticle.member_name}(Name(), "{cpp_string(reticle.reticle_id)}")')
            else:
                ctor_initializers.append(
                    f'    {reticle.member_name}(Name(), "{cpp_string(reticle.reticle_id)}", "{cpp_string(reticle.text_primitive_id)}")')

        if ctor_initializers:
            lines.append(f"{page.page_class_name}::{page.page_class_name}() :")
            lines.append(",\n".join(ctor_initializers))
            lines.append("{")
            lines.append("}")
            lines.append("")
        else:
            lines.extend([
                f"{page.page_class_name}::{page.page_class_name}() = default;",
                "",
            ])

        lines.extend([
            f"void {page.page_class_name}::Reset() noexcept",
            "{",
        ])
        for reticle in page.reticles:
            lines.append(f"    {reticle.member_name}.Reset();")
        lines.extend([
            "    for (auto& dynamicSet : dynamicReticleSets_)",
            "    {",
            "        dynamicSet.set->Reset();",
            "    }",
            "}",
            "",
            f"std::size_t {page.page_class_name}::AppendCommands(std::vector<mfd::UserCommand>& commands)",
            "{",
            "    std::size_t count = 0;",
            "",
        ])
        for reticle in page.reticles:
            lines.append(f"    count += {reticle.member_name}.AppendCommands(commands) ? 1U : 0U;")
        lines.extend([
            "",
            "    for (auto& dynamicSet : dynamicReticleSets_)",
            "    {",
            "        count += dynamicSet.set->AppendCommands(commands);",
            "    }",
            "",
            "    return count;",
            "}",
            "",
            f"std::size_t {page.page_class_name}::AppendShutdownCommands(std::vector<mfd::UserCommand>& commands, std::string statusText)",
            "{",
            "    std::size_t count = 0;",
            "",
        ])
        if page.status_member_name is not None:
            lines.extend([
                "    SetStatusCaption(std::move(statusText));",
                "    count += AppendCommands(commands);",
            ])
        else:
            lines.append("    (void)statusText;")
        lines.extend([
            "",
            "    for (auto& dynamicSet : dynamicReticleSets_)",
            "    {",
            "        count += dynamicSet.set->AppendRemovalCommands(commands);",
            "    }",
            "",
            "    return count;",
            "}",
            "",
            f"void {page.page_class_name}::SetStatusCaption(std::string value)",
            "{",
        ])
        if page.status_member_name is not None:
            lines.append(f"    {page.status_member_name}.SetValue(std::move(value));")
        else:
            lines.append("    (void)value;")
        lines.extend([
            "}",
            "",
            f"DynamicReticleSet& {page.page_class_name}::Dynamic(std::string_view templateId)",
            "{",
            "    if (DynamicTemplateSet* dynamicSet = FindDynamicSet(templateId); dynamicSet != nullptr)",
            "    {",
            "        return *dynamicSet->set;",
            "    }",
            "",
            "    DynamicTemplateSet entry;",
            "    entry.templateId = std::string(templateId);",
            "    entry.set = std::make_unique<DynamicReticleSet>(Name(), templateId);",
            "    dynamicReticleSets_.push_back(std::move(entry));",
            "    return *dynamicReticleSets_.back().set;",
            "}",
            "",
            f"{page.page_class_name}::DynamicTemplateSet* {page.page_class_name}::FindDynamicSet(std::string_view templateId) noexcept",
            "{",
            "    const auto iterator = std::find_if(",
            "        dynamicReticleSets_.begin(),",
            "        dynamicReticleSets_.end(),",
            "        [templateId](const DynamicTemplateSet& candidate)",
            "        {",
            "            return candidate.templateId == templateId;",
            "        });",
            "",
            "    return iterator == dynamicReticleSets_.end() ? nullptr : &(*iterator);",
            "}",
            "",
        ])

    lines.extend([
        f"{ui_class_name}::{ui_class_name}()",
        "{",
        "    window_.SetColorInverted(false);",
        "    window_.SetBrightness(1.0f);",
        "    window_.SetDisabled(false);",
        "}",
        "",
        f"bool {ui_class_name}::SendStartup(mfd::CommandClient& client,",
        "                             const mfd::PageViewState& view,",
        "                             std::string statusText)",
        "{",
        f"    if (!client.ActivatePage({startup_page.page_class_name}::Name()))",
        "    {",
        "        return false;",
        "    }",
        "",
        f"    if (!client.SetPageView({startup_page.page_class_name}::Name(), view.center, view.zoom))",
        "    {",
        "        return false;",
        "    }",
        "",
        "    std::vector<mfd::UserCommand> commands;",
        "    window_.AppendCommands(commands);",
        f"    {startup_page.ui_member_name}.SetStatusCaption(std::move(statusText));",
        f"    {startup_page.ui_member_name}.AppendCommands(commands);",
        "    return commands.empty() || client.SendBatch(commands, 0);",
        "}",
        "",
        f"void {ui_class_name}::Reset() noexcept",
        "{",
        "    window_.Reset();",
    ])
    for page in page_specs:
        lines.append(f"    {page.ui_member_name}.Reset();")
    lines.extend([
        "}",
        "",
        f"std::vector<mfd::UserCommand> {ui_class_name}::BuildBatch()",
        "{",
        "    std::vector<mfd::UserCommand> commands;",
        "    window_.AppendCommands(commands);",
    ])
    for page in page_specs:
        lines.append(f"    {page.ui_member_name}.AppendCommands(commands);")
    lines.extend([
        "    return commands;",
        "}",
        "",
        f"std::vector<mfd::UserCommand> {ui_class_name}::BuildShutdownBatch(std::string statusText)",
        "{",
        "    std::vector<mfd::UserCommand> commands;",
    ])
    for index, page in enumerate(page_specs):
        if index == 0:
            lines.append(f"    {page.ui_member_name}.AppendShutdownCommands(commands, std::move(statusText));")
        else:
            lines.append(f"    {page.ui_member_name}.AppendShutdownCommands(commands, std::string {{}});")
    lines.extend([
        "    return commands;",
        "}",
        "",
        f"WindowDisplay& {ui_class_name}::Window() noexcept",
        "{",
        "    return window_;",
        "}",
        "",
    ])
    for page in page_specs:
        lines.extend([
            f"{page.page_class_name}& {ui_class_name}::{page.accessor_name}() noexcept",
            "{",
            f"    return {page.ui_member_name};",
            "}",
            "",
        ])

    lines.extend([
        f"}} // namespace {namespace_name}",
        "",
    ])
    return "\n".join(lines)


def collect_input_paths(window_path: Path) -> list[Path]:
    window_root = load_json(window_path)
    input_paths: set[Path] = {window_path}

    for page_entry in page_entries(window_root):
        input_paths.add(resolve_path(window_path.parent, page_entry))

    raw_folder = window_root.get("reticleLibraryFolder") or window_root.get("reticles") or window_root.get("reticleFolder")
    folder = resolve_path(window_path.parent, raw_folder or ".")
    if folder.exists():
        input_paths.update(sorted(folder.glob("*.json")))

    return sorted(input_paths)


def main() -> int:
    args = parse_args()

    window_json = args.window_json.replace("\\", "/")
    window_path = Path(window_json).resolve()
    if not window_path.exists():
        raise RuntimeError(f"Window JSON not found: {window_json}")

    if args.print_inputs:
        for input_path in collect_input_paths(window_path):
            print(input_path.as_posix())
        return 0

    window_root = load_json(window_path)
    page_specs = build_page_specs(window_root, window_path, args.page_class_suffix)
    if not page_specs:
        raise RuntimeError("The window JSON does not expose any page to generate")

    ui_class_name = derive_ui_class_name(
        window_root,
        page_specs,
        args.ui_class_name,
        args.page_class_suffix,
        args.ui_class_suffix,
        window_json)

    header_text = emit_header(args.namespace, ui_class_name, window_json, page_specs)
    source_text = emit_source(args.namespace, args.header_include, ui_class_name, page_specs)

    output_header = Path(args.output_header)
    output_source = Path(args.output_source)
    output_header.parent.mkdir(parents=True, exist_ok=True)
    output_source.parent.mkdir(parents=True, exist_ok=True)
    output_header.write_text(header_text, encoding="utf-8")
    output_source.write_text(source_text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exception:  # noqa: BLE001
        print(str(exception), file=sys.stderr)
        raise SystemExit(1)
