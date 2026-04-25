#!/usr/bin/env python3
#
# This file is part of MFDStudio.
# Project author: Benoit Fra
# Repository: https://github.com/benoitfragit/MFDStudio

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path


TEXT_TYPES = {"text", "time"}
PRIMITIVE_HANDLE_TYPES = {
    "text": "TextHandle",
    "time": "TimeHandle",
    "line": "LineHandle",
    "circle": "CircleHandle",
    "ring": "RingHandle",
    "rectangle": "RectangleHandle",
    "ellipse": "EllipseHandle",
    "square": "SquareHandle",
    "diamond": "DiamondHandle",
}


@dataclass(frozen=True)
class PrimitiveSpec:
    primitive_id: str
    member_name: str
    accessor_name: str
    cpp_type: str
    primitive_type: str
    canonical_key: str
    transport_id: int


@dataclass(frozen=True)
class ReticleSpec:
    reticle_id: str
    member_name: str
    wrapper_class_name: str
    canonical_key: str
    transport_id: int
    primitives: list[PrimitiveSpec]
    status_primitive_accessor_name: str | None


@dataclass(frozen=True)
class PageSpec:
    page_name: str
    page_class_name: str
    accessor_name: str
    ui_member_name: str
    canonical_key: str
    transport_id: int
    blink_members: list["BlinkSpec"]
    reticles: list[ReticleSpec]
    status_member_name: str | None
    status_primitive_accessor_name: str | None
    strobe: "StrobeSpec"


@dataclass(frozen=True)
class StrobeSpec:
    valid: bool
    capture_shape: str
    capture_radius: float
    capture_size_x: float
    capture_size_y: float
    magnet_enabled: bool
    magnet_radius: float
    magnet_strength: float


@dataclass(frozen=True)
class TemplateSpec:
    template_id: str
    normalized_template_id: str
    canonical_key: str
    transport_id: int
    primitives: list[PrimitiveSpec]
    dynamic_reticle_class_name: str
    dynamic_set_class_name: str
    dynamic_accessor_name: str
    dynamic_member_name: str
    status_primitive_accessor_name: str | None


@dataclass(frozen=True)
class BlinkSpec:
    member_name: str
    blink_name: str
    duration_ms: int
    canonical_key: str
    transport_id: int


@dataclass(frozen=True)
class PageEntry:
    path: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate high-level client mockup UI code from one window JSON.")
    parser.add_argument("--window-json", required=True)
    parser.add_argument("--output-header", required=True)
    parser.add_argument("--output-source", required=True)
    parser.add_argument("--output-map")
    parser.add_argument("--namespace", default="mockup_ui")
    parser.add_argument("--ui-class-name")
    parser.add_argument("--page-class-suffix", default="MockupPage")
    parser.add_argument("--ui-class-suffix", default="MockupUi")
    parser.add_argument("--header-include", default="MockupUi.h")
    parser.add_argument("--print-inputs", action="store_true")
    parser.add_argument("--force-overwrite", action="store_true")
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


def normalize_name(value: str) -> str:
    return re.sub(r"[^0-9A-Za-z]+", "", value).lower()


def normalize_lookup_name(value: str) -> str:
    return value.strip().lower()


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


def stable_transport_id(canonical_key: str) -> int:
    digest = hashlib.sha256(canonical_key.encode("utf-8")).digest()
    return int.from_bytes(digest[-8:], byteorder="big", signed=False)


def float_literal(value: float) -> str:
    text = f"{value:.8g}"
    if "." not in text and "e" not in text and "E" not in text:
        text += ".0"
    return f"{text}f"


def explicit_exposure(element: dict) -> bool:
    direct_keys = ("expose", "exposed", "public", "clientExpose", "runtimeControl")
    for key in direct_keys:
        if isinstance(element.get(key), bool):
            return bool(element[key])

    client_node = element.get("client")
    if isinstance(client_node, dict):
        if isinstance(client_node.get("expose"), bool):
            return bool(client_node["expose"])
        if isinstance(client_node.get("public"), bool):
            return bool(client_node["public"])

    return False


def collect_named_elements(elements: list[dict]) -> list[dict]:
    result: list[dict] = []
    for element in elements:
        primitive_id = element.get("id")
        if isinstance(primitive_id, str) and primitive_id:
            result.append(element)
    return result


def should_expose_primitive(element: dict, elements: list[dict]) -> bool:
    primitive_id = element.get("id")
    if not isinstance(primitive_id, str) or not primitive_id:
        return False

    if explicit_exposure(element):
        return True

    primitive_type = str(element.get("type", "")).lower()
    if primitive_type in TEXT_TYPES:
        return True

    return len(collect_named_elements(elements)) == 1


def primitive_cpp_type(primitive_type: str) -> str:
    return PRIMITIVE_HANDLE_TYPES.get(primitive_type, "PrimitiveHandle")


def primitive_canonical_key(owner_kind: str,
                            owner_name: str,
                            primitive_id: str) -> str:
    return f"{owner_kind}/{normalize_lookup_name(owner_name)}/primitive/{normalize_lookup_name(primitive_id)}"


def validate_cpp_namespace(namespace_name: str) -> None:
    if not isinstance(namespace_name, str) or not namespace_name:
        raise RuntimeError("The namespace must be a non-empty string")

    if namespace_name.startswith("::") or namespace_name.endswith("::"):
        raise RuntimeError(f"Invalid C++ namespace '{namespace_name}'")

    identifiers = namespace_name.split("::")
    if any(not identifier for identifier in identifiers):
        raise RuntimeError(f"Invalid C++ namespace '{namespace_name}'")

    identifier_pattern = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
    for identifier in identifiers:
        if not identifier_pattern.match(identifier):
            raise RuntimeError(f"Invalid C++ namespace '{namespace_name}'")


def duplicate_generated_values(values: list[str]) -> list[str]:
    counts: dict[str, int] = {}
    for value in values:
        counts[value] = counts.get(value, 0) + 1
    return sorted([value for value, count in counts.items() if count > 1])


def ensure_unique_page_spec_names(page_specs: list[PageSpec]) -> None:
    class_names = [page.page_class_name for page in page_specs]
    accessors = [page.accessor_name for page in page_specs]
    members = [page.ui_member_name for page in page_specs]

    for field_name, values in (
        ("page class name", class_names),
        ("page accessor name", accessors),
        ("UI member name", members),
    ):
        duplicates = duplicate_generated_values(values)
        if duplicates:
            duplicate_list = ", ".join(duplicates)
            raise RuntimeError(f"Duplicate generated {field_name}(s): {duplicate_list}")


def ensure_unique_template_spec_names(template_specs: list[TemplateSpec]) -> None:
    class_names = [template.dynamic_reticle_class_name for template in template_specs]
    set_class_names = [template.dynamic_set_class_name for template in template_specs]
    accessors = [template.dynamic_accessor_name for template in template_specs]
    members = [template.dynamic_member_name for template in template_specs]

    for field_name, values in (
        ("dynamic reticle class name", class_names),
        ("dynamic set class name", set_class_names),
        ("dynamic accessor name", accessors),
        ("dynamic member name", members),
    ):
        duplicates = duplicate_generated_values(values)
        if duplicates:
            duplicate_list = ", ".join(duplicates)
            raise RuntimeError(f"Duplicate generated {field_name}(s): {duplicate_list}")


def ensure_unique_reticle_spec_names(page_name: str, reticle_specs: list[ReticleSpec]) -> None:
    for field_name, values in (
        ("reticle class name", [reticle.wrapper_class_name for reticle in reticle_specs]),
        ("reticle member name", [reticle.member_name for reticle in reticle_specs]),
    ):
        duplicates = duplicate_generated_values(values)
        if duplicates:
            duplicate_list = ", ".join(duplicates)
            raise RuntimeError(
                f"Duplicate generated {field_name}(s) on page '{page_name}': {duplicate_list}"
            )


def ensure_unique_primitive_spec_names(owner_kind: str,
                                       owner_name: str,
                                       primitive_specs: list[PrimitiveSpec]) -> None:
    for field_name, values in (
        ("primitive accessor name", [primitive.accessor_name for primitive in primitive_specs]),
        ("primitive member name", [primitive.member_name for primitive in primitive_specs]),
    ):
        duplicates = duplicate_generated_values(values)
        if duplicates:
            duplicate_list = ", ".join(duplicates)
            raise RuntimeError(
                f"Duplicate generated {field_name}(s) in {owner_kind} '{owner_name}': {duplicate_list}"
            )


def ensure_output_paths(output_header: Path,
                        output_source: Path,
                        output_map: Path | None,
                        force_overwrite: bool) -> None:
    if output_header.resolve() == output_source.resolve():
        raise RuntimeError("Output header and output source must be different files")

    if output_map is not None:
        output_map_resolved = output_map.resolve()
        if output_map_resolved == output_header.resolve() or output_map_resolved == output_source.resolve():
            raise RuntimeError("Output map must be different from generated C++ outputs")

    if force_overwrite:
        return

    candidate_paths = [output_header, output_source]
    if output_map is not None:
        candidate_paths.append(output_map)

    existing_paths = [path.as_posix() for path in candidate_paths if path.exists()]
    if existing_paths:
        raise RuntimeError(
            "Refusing to overwrite existing output file(s): "
            + ", ".join(existing_paths)
            + " (pass --force-overwrite to allow overwrite)")


def page_entries(window_root: dict) -> list[PageEntry]:
    pages = window_root.get("pages") or window_root.get("pageFiles") or window_root.get("pageJsons")
    if not isinstance(pages, list) or not pages:
        raise RuntimeError("Window JSON must define a non-empty pages array")

    resolved: list[PageEntry] = []
    for entry in pages:
        if isinstance(entry, str):
            resolved.append(PageEntry(entry))
            continue

        if isinstance(entry, dict):
            file_value = entry.get("file") or entry.get("path") or entry.get("json")
            if isinstance(file_value, str):
                if "default" in entry:
                    raise RuntimeError(
                        "pages[].default is no longer supported; use the root-level defaultPage field instead")

                resolved.append(PageEntry(file_value))
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


def choose_status_primitive(primitives: list[PrimitiveSpec]) -> PrimitiveSpec | None:
    text_primitives = [primitive for primitive in primitives if primitive.primitive_type == "text"]
    for suffix in ("_value", "_caption", "_text"):
        for primitive in text_primitives:
            if primitive.primitive_id.endswith(suffix):
                return primitive

    if len(text_primitives) == 1:
        return text_primitives[0]

    return None


def build_primitive_specs(owner_kind: str,
                          owner_name: str,
                          elements: list[dict]) -> list[PrimitiveSpec]:
    primitive_specs: list[PrimitiveSpec] = []

    for element in elements:
        primitive_id = element.get("id")
        primitive_type = str(element.get("type", "")).lower()
        if not should_expose_primitive(element, elements):
            continue
        if not isinstance(primitive_id, str) or not primitive_id:
            continue

        canonical_key = primitive_canonical_key(owner_kind, owner_name, primitive_id)
        primitive_specs.append(PrimitiveSpec(
            primitive_id=primitive_id,
            member_name=f"{camel_case(primitive_id)}_",
            accessor_name=pascal_case(primitive_id),
            cpp_type=primitive_cpp_type(primitive_type),
            primitive_type=primitive_type,
            canonical_key=canonical_key,
            transport_id=stable_transport_id(canonical_key),
        ))

    return primitive_specs


def resolve_strobe_spec(page_root: dict) -> StrobeSpec:
    strobe_node = page_root.get("strobe")
    if not isinstance(strobe_node, dict):
        return StrobeSpec(False, "circle", 0.0875, 0.175, 0.175, False, 0.075, 1.0)

    capture_node = strobe_node.get("capture")
    magnet_node = strobe_node.get("magnet")

    capture_shape = "circle"
    capture_radius = 0.0875
    capture_size_x = 0.175
    capture_size_y = 0.175
    if isinstance(capture_node, dict):
        if isinstance(capture_node.get("shape"), str):
            capture_shape = capture_node["shape"]
        if isinstance(capture_node.get("radius"), (int, float)):
            capture_radius = float(capture_node["radius"])
        size_value = capture_node.get("size")
        if isinstance(size_value, list) and len(size_value) == 2:
            capture_size_x = float(size_value[0])
            capture_size_y = float(size_value[1])

    magnet_enabled = False
    magnet_radius = 0.075
    magnet_strength = 1.0
    if isinstance(magnet_node, dict):
        if isinstance(magnet_node.get("enabled"), bool):
            magnet_enabled = bool(magnet_node["enabled"])
        if isinstance(magnet_node.get("radius"), (int, float)):
            magnet_radius = float(magnet_node["radius"])
        if isinstance(magnet_node.get("strength"), (int, float)):
            magnet_strength = float(magnet_node["strength"])

    return StrobeSpec(
        valid=True,
        capture_shape=capture_shape,
        capture_radius=capture_radius,
        capture_size_x=capture_size_x,
        capture_size_y=capture_size_y,
        magnet_enabled=magnet_enabled,
        magnet_radius=magnet_radius,
        magnet_strength=magnet_strength,
    )


def build_template_specs(template_library: dict[str, dict]) -> list[TemplateSpec]:
    templates: list[TemplateSpec] = []
    seen_ids: set[int] = set()

    for template_id, template_node in sorted(template_library.items()):
        canonical_key = f"template/{normalize_lookup_name(template_id)}"
        transport_id = stable_transport_id(canonical_key)
        if transport_id in seen_ids:
            raise RuntimeError(f"Transport ID collision detected for template '{template_id}'")
        seen_ids.add(transport_id)

        primitives = build_primitive_specs("template", template_id, resolved_elements(template_node, template_library))
        ensure_unique_primitive_spec_names("template", template_id, primitives)
        status_primitive = choose_status_primitive(primitives)
        template_base_name = pascal_case(template_id)
        templates.append(TemplateSpec(
            template_id=template_id,
            normalized_template_id=normalize_lookup_name(template_id),
            canonical_key=canonical_key,
            transport_id=transport_id,
            primitives=primitives,
            dynamic_reticle_class_name=f"{template_base_name}DynamicReticle",
            dynamic_set_class_name=f"{template_base_name}DynamicReticleSet",
            dynamic_accessor_name=f"Dynamic{template_base_name}",
            dynamic_member_name=f"dynamic{template_base_name}_",
            status_primitive_accessor_name=None if status_primitive is None else status_primitive.accessor_name,
        ))

    return templates


def build_page_specs(window_root: dict,
                     window_path: Path,
                     page_class_suffix: str) -> tuple[list[PageSpec], list[TemplateSpec]]:
    template_library = load_reticle_library(window_root, window_path)
    page_specs: list[PageSpec] = []
    seen_ids: set[int] = set()

    for page_entry in page_entries(window_root):
        page_path = resolve_path(window_path.parent, page_entry.path)
        page_root = extract_page_node(load_json(page_path))

        page_name = page_root.get("name") or page_root.get("id")
        if not isinstance(page_name, str) or not page_name:
            raise RuntimeError(f"Page file '{page_path}' does not define a valid name")

        page_base_name = pascal_case(page_name)
        page_canonical_key = f"page/{normalize_lookup_name(page_name)}"
        page_transport_id = stable_transport_id(page_canonical_key)
        if page_transport_id in seen_ids:
            raise RuntimeError(f"Transport ID collision detected for page '{page_name}'")
        seen_ids.add(page_transport_id)

        reticles: list[ReticleSpec] = []
        for reticle in page_root.get("staticReticles", []):
            if not isinstance(reticle, dict):
                continue

            reticle_id = reticle.get("id")
            if not isinstance(reticle_id, str) or not reticle_id:
                continue

            member_name = camel_case(reticle_id)
            canonical_key = f"page/{normalize_lookup_name(page_name)}/reticle/{normalize_lookup_name(reticle_id)}"
            transport_id = stable_transport_id(canonical_key)
            if transport_id in seen_ids:
                raise RuntimeError(f"Transport ID collision detected for reticle '{reticle_id}'")
            seen_ids.add(transport_id)

            primitives = build_primitive_specs("reticle", f"{page_name}/{reticle_id}", resolved_elements(reticle, template_library))
            ensure_unique_primitive_spec_names("reticle", f"{page_name}/{reticle_id}", primitives)
            status_primitive = choose_status_primitive(primitives)
            reticles.append(ReticleSpec(
                reticle_id=reticle_id,
                member_name=member_name,
                wrapper_class_name=f"{page_base_name}{pascal_case(reticle_id)}Reticle",
                canonical_key=canonical_key,
                transport_id=transport_id,
                primitives=primitives,
                status_primitive_accessor_name=None if status_primitive is None else status_primitive.accessor_name,
            ))

        blink_members: list[BlinkSpec] = []
        for blink_type in page_root.get("blinkTypes", []):
            if not isinstance(blink_type, dict):
                continue
            blink_name = blink_type.get("name") or blink_type.get("id") or blink_type.get("type")
            if isinstance(blink_name, str) and blink_name:
                canonical_key = f"page/{normalize_lookup_name(page_name)}/blink/{normalize_lookup_name(blink_name)}"
                transport_id = stable_transport_id(canonical_key)
                if transport_id in seen_ids:
                    raise RuntimeError(f"Transport ID collision detected for blink type '{blink_name}'")
                seen_ids.add(transport_id)

                duration_value = blink_type.get("durationMs")
                if not isinstance(duration_value, int):
                    duration_value = blink_type.get("duration")
                blink_members.append(BlinkSpec(
                    member_name=camel_case(blink_name),
                    blink_name=blink_name,
                    duration_ms=1000 if not isinstance(duration_value, int) else duration_value,
                    canonical_key=canonical_key,
                    transport_id=transport_id,
                ))

        expected_status_name = f"{camel_case(page_name)}Status"
        status_member_name = None
        status_primitive_accessor_name = None
        for reticle in reticles:
            if reticle.member_name == expected_status_name and reticle.status_primitive_accessor_name is not None:
                status_member_name = reticle.member_name
                status_primitive_accessor_name = reticle.status_primitive_accessor_name
                break

        ensure_unique_reticle_spec_names(page_name, reticles)
        page_specs.append(PageSpec(
            page_name=page_name,
            page_class_name=f"{page_base_name}{page_class_suffix}",
            accessor_name=page_base_name,
            ui_member_name=f"{camel_case(page_name)}_",
            canonical_key=page_canonical_key,
            transport_id=page_transport_id,
            blink_members=blink_members,
            reticles=reticles,
            status_member_name=status_member_name,
            status_primitive_accessor_name=status_primitive_accessor_name,
            strobe=resolve_strobe_spec(page_root),
        ))

    return page_specs, build_template_specs(template_library)


def resolve_startup_page(page_specs: list[PageSpec], window_root: dict) -> PageSpec:
    default_page_name = window_root.get("defaultPage")
    if default_page_name is None:
        return page_specs[0]

    if not isinstance(default_page_name, str):
        raise RuntimeError("Window root defaultPage must be a string")

    normalized = normalize_name(default_page_name)
    for page in page_specs:
        if normalize_name(page.page_name) == normalized:
            return page

    raise RuntimeError(f"Unknown defaultPage '{default_page_name}' in window JSON")


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


def emit_strobe_initializer(strobe: StrobeSpec) -> str:
    capture_shape = "mfd::StrobeCaptureShape::Rectangle" if strobe.capture_shape.strip().lower() == "rectangle" else "mfd::StrobeCaptureShape::Circle"
    valid = "true" if strobe.valid else "false"
    magnet_enabled = "true" if strobe.magnet_enabled else "false"
    return (
        "StrobeInfo {"
        f"{valid}, "
        f"mfd::StrobeCaptureConfig {{{capture_shape}, {float_literal(strobe.capture_radius)}, "
        f"mfd::Vec2 {{{float_literal(strobe.capture_size_x)}, {float_literal(strobe.capture_size_y)}}}}}, "
        f"mfd::StrobeMagnetConfig {{{magnet_enabled}, {float_literal(strobe.magnet_radius)}, {float_literal(strobe.magnet_strength)}}}"
        "}"
    )


def emit_header(namespace_name: str,
                ui_class_name: str,
                window_json: str,
                mapping_hash: str,
                page_specs: list[PageSpec],
                template_specs: list[TemplateSpec]) -> str:
    lines: list[str] = [
        "/*",
        " * This file is part of MFDStudio.",
        " * Project author: Benoit Fra",
        " * Repository: https://github.com/benoitfragit/MFDStudio",
        " *",
        " * AUTO-GENERATED FILE.",
        " * DO NOT EDIT MANUALLY.",
        " * This file was generated by client_api_generator.",
        " */",
        "#pragma once",
        "",
        "#include <cstddef>",
        "#include <cstdint>",
        "#include <memory>",
        "#include <string>",
        "#include <string_view>",
        "#include <vector>",
        "",
        '#include "mfd/client/Animation.h"',
        '#include "mfd/client/LatestBatchPublisher.h"',
        '#include "mfd/control/CommandClient.h"',
        "",
        f"namespace {namespace_name}",
        "{",
        "using BlinkType = mfd::client::BlinkType;",
        "using DynamicReticle = mfd::client::DynamicReticle;",
        "using DynamicReticleSet = mfd::client::DynamicReticleSet;",
        "using GeneratedDynamicReticleSet = mfd::client::GeneratedDynamicReticleSet;",
        "using CircleHandle = mfd::client::CircleHandle;",
        "using DiamondHandle = mfd::client::DiamondHandle;",
        "using EllipseHandle = mfd::client::EllipseHandle;",
        "using LineHandle = mfd::client::LineHandle;",
        "using PrimitiveHandle = mfd::client::PrimitiveHandle;",
        "using Reticle = mfd::client::Reticle;",
        "using ReticleBlink = mfd::client::ReticleBlink;",
        "using RingHandle = mfd::client::RingHandle;",
        "using RectangleHandle = mfd::client::RectangleHandle;",
        "using SquareHandle = mfd::client::SquareHandle;",
        "using StrobeHandle = mfd::client::StrobeHandle;",
        "using StrobeInfo = mfd::client::StrobeInfo;",
        "using TextHandle = mfd::client::TextHandle;",
        "using TimeHandle = mfd::client::TimeHandle;",
        "using WindowDisplay = mfd::client::WindowDisplay;",
        "",
    ]

    for template in template_specs:
        lines.extend([
            f"class {template.dynamic_reticle_class_name} final : public DynamicReticle",
            "{",
            "public:",
            f"    explicit {template.dynamic_reticle_class_name}(std::string_view reticleId);",
        ])

        if template.status_primitive_accessor_name is not None:
            lines.append("    void SetValue(std::string value);")

        for primitive in template.primitives:
            lines.append(f"    {primitive.cpp_type}& {primitive.accessor_name}() noexcept;")

        lines.extend([
            "",
            "private:",
        ])

        for primitive in template.primitives:
            lines.append(f"    {primitive.cpp_type} {primitive.member_name};")

        if not template.primitives:
            lines.append("    // No exposed primitive for this authored template.")

        lines.extend([
            "};",
            "",
            f"class {template.dynamic_set_class_name} final : public GeneratedDynamicReticleSet",
            "{",
            "public:",
            f"    explicit {template.dynamic_set_class_name}(std::string_view pageName, mfd::TransportId pageTransportId = 0);",
            f"    {template.dynamic_reticle_class_name}& Create();",
            f"    void Remove({template.dynamic_reticle_class_name}& reticle);",
            "",
            "protected:",
            "    std::unique_ptr<DynamicReticle> CreateReticle(std::string_view reticleId) override;",
            "};",
            "",
        ])

    for page in page_specs:
        for reticle in page.reticles:
            lines.extend([
                f"class {reticle.wrapper_class_name} final : public Reticle",
                "{",
                "public:",
                f"    {reticle.wrapper_class_name}();",
            ])

            if reticle.status_primitive_accessor_name is not None:
                lines.append("    void SetValue(std::string value);")

            for primitive in reticle.primitives:
                lines.append(f"    {primitive.cpp_type}& {primitive.accessor_name}() noexcept;")

            lines.extend([
                "",
                "private:",
            ])

            for primitive in reticle.primitives:
                lines.append(f"    {primitive.cpp_type} {primitive.member_name};")

            if not reticle.primitives:
                lines.append("    // No exposed primitive for this authored reticle.")

            lines.extend([
                "};",
                "",
            ])

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
            "",
        ])

        for template in template_specs:
            lines.append(f"    {template.dynamic_set_class_name}& {template.dynamic_accessor_name}() noexcept;")

        if template_specs:
            lines.append("")

        for blink in page.blink_members:
            lines.append(
                f'    BlinkType {blink.member_name} {{"{cpp_string(blink.blink_name)}", {blink.transport_id}U}};')

        if page.blink_members:
            lines.append("")

        lines.append("    StrobeHandle strobe;")

        for reticle in page.reticles:
            lines.append(f"    {reticle.wrapper_class_name} {reticle.member_name};")

        lines.extend([
            "",
            "private:",
        ])

        for template in template_specs:
            lines.append(f"    {template.dynamic_set_class_name} {template.dynamic_member_name};")

        lines.extend([
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
        "    static constexpr std::string_view MappingHash() noexcept",
        "    {",
        f'        return "{mapping_hash}";',
        "    }",
        "",
        f"    {ui_class_name}();",
        "",
        "    bool SendStartup(mfd::CommandClient& client, const mfd::PageViewState& view, std::string statusText);",
        "    void Reset() noexcept;",
        "    std::vector<mfd::UserCommand> BuildBatch();",
        "    mfd::CommandBatch BuildCommandBatch(std::uint32_t sequence = 0);",
        "    bool SubmitLatest(mfd::client::LatestBatchPublisher& publisher, std::uint32_t sequence = 0);",
        "    std::vector<mfd::UserCommand> BuildShutdownBatch(std::string statusText);",
        "    mfd::CommandBatch BuildShutdownCommandBatch(std::uint32_t sequence, std::string statusText);",
        "    bool SubmitShutdown(mfd::client::LatestBatchPublisher& publisher, std::uint32_t sequence, std::string statusText);",
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
                mapping_hash: str,
                startup_page: PageSpec,
                page_specs: list[PageSpec],
                template_specs: list[TemplateSpec]) -> str:
    lines: list[str] = [
        "/*",
        " * This file is part of MFDStudio.",
        " * Project author: Benoit Fra",
        " * Repository: https://github.com/benoitfragit/MFDStudio",
        " *",
        " * AUTO-GENERATED FILE.",
        " * DO NOT EDIT MANUALLY.",
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

    for template in template_specs:
        ctor_initializers = ["    DynamicReticle(reticleId)"]
        for primitive in template.primitives:
            ctor_initializers.append(
                f'    {primitive.member_name}(MutableDesiredPatch(), DirtyFlag(), "{cpp_string(primitive.primitive_id)}", {primitive.transport_id}U, PrimitiveTransportIds())')

        lines.append(f"{template.dynamic_reticle_class_name}::{template.dynamic_reticle_class_name}(std::string_view reticleId) :")
        lines.append(",\n".join(ctor_initializers))
        lines.append("{")
        lines.append("}")
        lines.append("")

        if template.status_primitive_accessor_name is not None:
            lines.extend([
                f"void {template.dynamic_reticle_class_name}::SetValue(std::string value)",
                "{",
                f"    {template.status_primitive_accessor_name}().SetText(std::move(value));",
                "}",
                "",
            ])

        for primitive in template.primitives:
            lines.extend([
                f"{primitive.cpp_type}& {template.dynamic_reticle_class_name}::{primitive.accessor_name}() noexcept",
                "{",
                f"    return {primitive.member_name};",
                "}",
                "",
            ])

        lines.extend([
            f"{template.dynamic_set_class_name}::{template.dynamic_set_class_name}(std::string_view pageName, const mfd::TransportId pageTransportId) :",
            f'    GeneratedDynamicReticleSet(pageName, "{cpp_string(template.template_id)}", pageTransportId, {template.transport_id}U)',
            "{",
            "}",
            "",
            f"{template.dynamic_reticle_class_name}& {template.dynamic_set_class_name}::Create()",
            "{",
            f"    return static_cast<{template.dynamic_reticle_class_name}&>(GeneratedDynamicReticleSet::Create());",
            "}",
            "",
            f"void {template.dynamic_set_class_name}::Remove({template.dynamic_reticle_class_name}& reticle)",
            "{",
            "    GeneratedDynamicReticleSet::Remove(reticle);",
            "}",
            "",
            f"std::unique_ptr<DynamicReticle> {template.dynamic_set_class_name}::CreateReticle(std::string_view reticleId)",
            "{",
            f"    return std::make_unique<{template.dynamic_reticle_class_name}>(reticleId);",
            "}",
            "",
        ])

    for page in page_specs:
        for reticle in page.reticles:
            ctor_initializers = [
                f'    Reticle("{cpp_string(page.page_name)}", "{cpp_string(reticle.reticle_id)}", {page.transport_id}U, {reticle.transport_id}U)']
            for primitive in reticle.primitives:
                ctor_initializers.append(
                    f'    {primitive.member_name}(MutableDesiredPatch(), DirtyFlag(), "{cpp_string(primitive.primitive_id)}", {primitive.transport_id}U, PrimitiveTransportIds())')

            lines.append(f"{reticle.wrapper_class_name}::{reticle.wrapper_class_name}() :")
            lines.append(",\n".join(ctor_initializers))
            lines.append("{")
            lines.append("}")
            lines.append("")

            if reticle.status_primitive_accessor_name is not None:
                lines.extend([
                    f"void {reticle.wrapper_class_name}::SetValue(std::string value)",
                    "{",
                    f"    {reticle.status_primitive_accessor_name}().SetText(std::move(value));",
                    "}",
                    "",
                ])

            for primitive in reticle.primitives:
                lines.extend([
                    f"{primitive.cpp_type}& {reticle.wrapper_class_name}::{primitive.accessor_name}() noexcept",
                    "{",
                    f"    return {primitive.member_name};",
                    "}",
                    "",
                ])

        page_ctor_initializers: list[str] = [
            f"    strobe(Name(), {emit_strobe_initializer(page.strobe)}, {page.transport_id}U)",
        ]
        for reticle in page.reticles:
            page_ctor_initializers.append(f"    {reticle.member_name}()")
        for template in template_specs:
            page_ctor_initializers.append(f"    {template.dynamic_member_name}(Name(), {page.transport_id}U)")

        lines.append(f"{page.page_class_name}::{page.page_class_name}() :")
        lines.append(",\n".join(page_ctor_initializers))
        lines.append("{")
        lines.append("}")
        lines.append("")

        lines.extend([
            f"void {page.page_class_name}::Reset() noexcept",
            "{",
            "    strobe.Reset();",
        ])
        for reticle in page.reticles:
            lines.append(f"    {reticle.member_name}.Reset();")
        for template in template_specs:
            lines.append(f"    {template.dynamic_member_name}.Reset();")
        lines.extend([
            "}",
            "",
            f"std::size_t {page.page_class_name}::AppendCommands(std::vector<mfd::UserCommand>& commands)",
            "{",
            "    std::size_t count = 0;",
            "",
            "    count += strobe.AppendCommands(commands) ? 1U : 0U;",
        ])
        for reticle in page.reticles:
            lines.append(f"    count += {reticle.member_name}.AppendCommands(commands) ? 1U : 0U;")
        for template in template_specs:
            lines.append(f"    count += {template.dynamic_member_name}.AppendCommands(commands);")
        lines.extend([
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
        lines.append("")
        for template in template_specs:
            lines.append(f"    count += {template.dynamic_member_name}.AppendRemovalCommands(commands);")
        lines.extend([
            "",
            "    return count;",
            "}",
            "",
            f"void {page.page_class_name}::SetStatusCaption(std::string value)",
            "{",
        ])
        if page.status_member_name is not None and page.status_primitive_accessor_name is not None:
            lines.append(
                f"    {page.status_member_name}.{page.status_primitive_accessor_name}().SetText(std::move(value));")
        else:
            lines.append("    (void)value;")
        lines.extend([
            "}",
            "",
        ])
        for template in template_specs:
            lines.extend([
                f"{template.dynamic_set_class_name}& {page.page_class_name}::{template.dynamic_accessor_name}() noexcept",
                "{",
                f"    return {template.dynamic_member_name};",
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
        "    if (commands.empty())",
        "    {",
        "        return true;",
        "    }",
        "",
        "    mfd::CommandBatch batch;",
        "    batch.sequence = 0;",
        f'    batch.mappingHash = "{mapping_hash}";',
        "    batch.commands = std::move(commands);",
        "    return client.SendBatch(batch);",
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
        f"mfd::CommandBatch {ui_class_name}::BuildCommandBatch(const std::uint32_t sequence)",
        "{",
        "    mfd::CommandBatch batch;",
        "    batch.sequence = sequence;",
        f'    batch.mappingHash = "{mapping_hash}";',
        "    batch.commands = BuildBatch();",
        "    return batch;",
        "}",
        "",
        f"bool {ui_class_name}::SubmitLatest(mfd::client::LatestBatchPublisher& publisher, const std::uint32_t sequence)",
        "{",
        "    return publisher.SubmitLatest(BuildCommandBatch(sequence));",
        "}",
        "",
        f"std::vector<mfd::UserCommand> {ui_class_name}::BuildShutdownBatch(std::string statusText)",
        "{",
        "    std::vector<mfd::UserCommand> commands;",
    ])
    for page in page_specs:
        if page.page_name == startup_page.page_name:
            lines.append(f"    {page.ui_member_name}.AppendShutdownCommands(commands, std::move(statusText));")
        else:
            lines.append(f"    {page.ui_member_name}.AppendShutdownCommands(commands, std::string {{}});")
    lines.extend([
        "    return commands;",
        "}",
        "",
        f"mfd::CommandBatch {ui_class_name}::BuildShutdownCommandBatch(const std::uint32_t sequence, std::string statusText)",
        "{",
        "    mfd::CommandBatch batch;",
        "    batch.sequence = sequence;",
        f'    batch.mappingHash = "{mapping_hash}";',
        "    batch.commands = BuildShutdownBatch(std::move(statusText));",
        "    return batch;",
        "}",
        "",
        f"bool {ui_class_name}::SubmitShutdown(mfd::client::LatestBatchPublisher& publisher,",
        "                              const std::uint32_t sequence,",
        "                              std::string statusText)",
        "{",
        "    return publisher.SubmitLatest(BuildShutdownCommandBatch(sequence, std::move(statusText)));",
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


def mapping_document(window_root: dict,
                     window_path: Path,
                     page_specs: list[PageSpec],
                     template_specs: list[TemplateSpec]) -> dict:
    pages = [
        {
            "id": page.transport_id,
            "name": page.page_name,
            "normalizedName": normalize_lookup_name(page.page_name),
            "hasStrobe": page.strobe.valid,
            "defaultPage": False,
        }
        for page in sorted(page_specs, key=lambda entry: entry.canonical_key)
    ]

    if window_root.get("defaultPage") is not None:
        default_page = resolve_startup_page(page_specs, window_root)
        for page in pages:
            page["defaultPage"] = page["name"] == default_page.page_name

    reticles = []
    primitives = []
    for page in sorted(page_specs, key=lambda entry: entry.canonical_key):
        for reticle in sorted(page.reticles, key=lambda entry: entry.canonical_key):
            reticles.append({
                "id": reticle.transport_id,
                "pageId": page.transport_id,
                "reticleId": reticle.reticle_id,
                "normalizedReticleId": normalize_lookup_name(reticle.reticle_id),
                "source": "static",
            })

            for primitive in sorted(reticle.primitives, key=lambda entry: entry.canonical_key):
                primitives.append({
                    "id": primitive.transport_id,
                    "ownerKind": "reticle",
                    "ownerId": reticle.transport_id,
                    "primitiveId": primitive.primitive_id,
                    "normalizedPrimitiveId": normalize_lookup_name(primitive.primitive_id),
                    "primitiveType": primitive.primitive_type,
                    "exposed": True,
                })

    templates = []
    for template in sorted(template_specs, key=lambda entry: entry.canonical_key):
        templates.append({
            "id": template.transport_id,
            "templateId": template.template_id,
            "normalizedTemplateId": template.normalized_template_id,
        })
        for primitive in sorted(template.primitives, key=lambda entry: entry.canonical_key):
            primitives.append({
                "id": primitive.transport_id,
                "ownerKind": "template",
                "ownerId": template.transport_id,
                "primitiveId": primitive.primitive_id,
                "normalizedPrimitiveId": normalize_lookup_name(primitive.primitive_id),
                "primitiveType": primitive.primitive_type,
                "exposed": True,
            })

    blink_types = []
    for page in sorted(page_specs, key=lambda entry: entry.canonical_key):
        for blink in sorted(page.blink_members, key=lambda entry: entry.canonical_key):
            blink_types.append({
                "id": blink.transport_id,
                "pageId": page.transport_id,
                "blinkType": blink.blink_name,
                "normalizedBlinkType": normalize_lookup_name(blink.blink_name),
                "durationMs": blink.duration_ms,
            })

    seen_ids: dict[int, str] = {}
    for table_name, rows, id_field in (
        ("pages", pages, "id"),
        ("reticles", reticles, "id"),
        ("primitives", primitives, "id"),
        ("templates", templates, "id"),
        ("blinkTypes", blink_types, "id"),
    ):
        for row in rows:
            row_id = row[id_field]
            existing = seen_ids.get(row_id)
            if existing is not None:
                raise RuntimeError(f"Transport ID collision detected between {existing} and {table_name}:{row}")
            seen_ids[row_id] = f"{table_name}:{row}"

    mapping_without_hash = {
        "schemaVersion": 1,
        "window": {
            "name": window_path.stem,
            "title": window_root.get("title", ""),
            "source": window_path.name,
        },
        "pages": pages,
        "reticles": reticles,
        "primitives": primitives,
        "templates": templates,
        "blinkTypes": blink_types,
    }

    canonical_payload = json.dumps(mapping_without_hash, ensure_ascii=True, separators=(",", ":"), sort_keys=True)
    mapping_hash = hashlib.sha256(canonical_payload.encode("utf-8")).hexdigest()
    return {
        "schemaVersion": 1,
        "mappingHash": mapping_hash,
        "window": mapping_without_hash["window"],
        "pages": pages,
        "reticles": reticles,
        "primitives": primitives,
        "templates": templates,
        "blinkTypes": blink_types,
    }


def emit_map(document: dict) -> str:
    return json.dumps(document, indent=2) + "\n"


def collect_input_paths(window_path: Path) -> list[Path]:
    window_root = load_json(window_path)
    input_paths: set[Path] = {window_path}

    for page_entry in page_entries(window_root):
        input_paths.add(resolve_path(window_path.parent, page_entry.path))

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
    validate_cpp_namespace(args.namespace)
    page_specs, template_specs = build_page_specs(window_root, window_path, args.page_class_suffix)
    if not page_specs:
        raise RuntimeError("The window JSON does not expose any page to generate")
    ensure_unique_page_spec_names(page_specs)
    ensure_unique_template_spec_names(template_specs)

    ui_class_name = derive_ui_class_name(
        window_root,
        page_specs,
        args.ui_class_name,
        args.page_class_suffix,
        args.ui_class_suffix,
        window_json)

    map_document = mapping_document(window_root, window_path, page_specs, template_specs)
    header_text = emit_header(
        args.namespace,
        ui_class_name,
        window_json,
        map_document["mappingHash"],
        page_specs,
        template_specs)
    startup_page = resolve_startup_page(page_specs, window_root)
    source_text = emit_source(
        args.namespace,
        args.header_include,
        ui_class_name,
        map_document["mappingHash"],
        startup_page,
        page_specs,
        template_specs)
    map_text = None if args.output_map is None else emit_map(map_document)

    output_header = Path(args.output_header)
    output_source = Path(args.output_source)
    output_map = None if args.output_map is None else Path(args.output_map)
    ensure_output_paths(output_header, output_source, output_map, args.force_overwrite)
    output_header.parent.mkdir(parents=True, exist_ok=True)
    output_source.parent.mkdir(parents=True, exist_ok=True)
    output_header.write_text(header_text, encoding="utf-8")
    output_source.write_text(source_text, encoding="utf-8")
    if output_map is not None and map_text is not None:
        output_map.parent.mkdir(parents=True, exist_ok=True)
        output_map.write_text(map_text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exception:  # noqa: BLE001
        print(str(exception), file=sys.stderr)
        raise SystemExit(1)
