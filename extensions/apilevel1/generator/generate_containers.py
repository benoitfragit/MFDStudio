#!/usr/bin/env python3
"""LEVEL 1 container support loaded by the historic MFDStudio generator.

This module deliberately owns parsing and naming for generated container
handles.  The LEVEL 0 generator imports it only after container content is
observed, which keeps the legacy generation path and its output untouched.
"""

from __future__ import annotations

import hashlib
import re
from dataclasses import dataclass


@dataclass(frozen=True)
class ContainerSpec:
    """One exposed page-local container emitted into the generated C++ API."""

    container_id: str
    accessor_name: str
    member_name: str
    canonical_key: str
    transport_id: int
    visible: bool
    enabled: bool
    scroll_x: float
    scroll_y: float


def _normalize(value: str) -> str:
    return value.strip().lower()


def _pascal_case(value: str) -> str:
    words = [word for word in re.sub(r"[^0-9A-Za-z]+", " ", value).strip().split() if word]
    result = "".join(word[:1].upper() + word[1:] for word in (words or ["Container"]))
    return f"Generated{result}" if result[:1].isdigit() else result


def _transport_id(canonical_key: str) -> int:
    digest = hashlib.sha256(canonical_key.encode("utf-8")).digest()
    return int.from_bytes(digest[-8:], byteorder="big", signed=False)


def _finite_number(value: object, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise RuntimeError(f"Container {field} must be finite and numeric")
    parsed = float(value)
    if parsed != parsed or parsed in (float("inf"), float("-inf")):
        raise RuntimeError(f"Container {field} must be finite and numeric")
    return parsed


def _scroll_offset(container: dict) -> tuple[float, float]:
    scrolling = container.get("scrolling", {})
    if not isinstance(scrolling, dict):
        raise RuntimeError("Container scrolling must be a JSON object")
    offset = scrolling.get("offset", [0.0, 0.0])
    if isinstance(offset, dict):
        return (_finite_number(offset.get("x", 0.0), "scrolling.offset.x"),
                _finite_number(offset.get("y", 0.0), "scrolling.offset.y"))
    if isinstance(offset, list) and len(offset) == 2:
        return (_finite_number(offset[0], "scrolling.offset[0]"),
                _finite_number(offset[1], "scrolling.offset[1]"))
    raise RuntimeError("Container scrolling.offset must be a two-axis value")


def collect_container_specs(page_name: str, page_root: dict, seen_ids: set[int]) -> list[ContainerSpec]:
    """Validate LEVEL 1 hierarchy identities and return exposed handles only."""

    raw_containers = page_root.get("containers", [])
    if not isinstance(raw_containers, list):
        raise RuntimeError(f"Page '{page_name}' containers must be a JSON array")
    if len(raw_containers) > 4096:
        raise RuntimeError(f"Page '{page_name}' exceeds 4096 containers")

    identifiers: set[str] = set()
    accessor_names: set[str] = set()
    child_owners: dict[str, str] = {}
    nodes: dict[str, dict] = {}
    for raw in raw_containers:
        if not isinstance(raw, dict):
            raise RuntimeError(f"Page '{page_name}' contains a non-object container")
        container_id = raw.get("id")
        if not isinstance(container_id, str) or not container_id.strip():
            raise RuntimeError(f"Page '{page_name}' contains a container without a non-empty id")
        normalized = _normalize(container_id)
        if normalized in identifiers:
            raise RuntimeError(f"Page '{page_name}' contains duplicate container id '{container_id}'")
        identifiers.add(normalized)
        nodes[normalized] = raw

    for normalized, raw in nodes.items():
        children = raw.get("children", [])
        if not isinstance(children, list):
            raise RuntimeError(f"Container '{raw['id']}' children must be a JSON array")
        for child in children:
            if not isinstance(child, dict) or not isinstance(child.get("id"), str):
                raise RuntimeError(f"Container '{raw['id']}' has an invalid child placement")
            child_normalized = _normalize(child["id"])
            if child_normalized not in nodes:
                raise RuntimeError(f"Container '{raw['id']}' references unknown child '{child['id']}'")
            if child_normalized in child_owners:
                raise RuntimeError(f"Container '{child['id']}' has more than one parent")
            child_owners[child_normalized] = normalized

    for start in nodes:
        visited: set[str] = set()
        current = start
        while current in child_owners:
            if current in visited:
                raise RuntimeError(f"Page '{page_name}' contains a container hierarchy cycle")
            visited.add(current)
            current = child_owners[current]
            if len(visited) >= 64:
                raise RuntimeError(f"Page '{page_name}' container hierarchy exceeds 64 levels")

    result: list[ContainerSpec] = []
    for raw in raw_containers:
        if raw.get("exposed") is not True:
            continue
        container_id = raw["id"]
        accessor_name = _pascal_case(container_id)
        if accessor_name in accessor_names:
            raise RuntimeError(f"Generated container accessor collision for '{container_id}'")
        accessor_names.add(accessor_name)
        canonical_key = f"page/{_normalize(page_name)}/container/{_normalize(container_id)}"
        transport_id = _transport_id(canonical_key)
        if transport_id in seen_ids:
            raise RuntimeError(f"Transport ID collision detected for container '{container_id}'")
        seen_ids.add(transport_id)
        scroll_x, scroll_y = _scroll_offset(raw)
        result.append(ContainerSpec(
            container_id=container_id,
            accessor_name=accessor_name,
            member_name=accessor_name[:1].lower() + accessor_name[1:] + "Container_",
            canonical_key=canonical_key,
            transport_id=transport_id,
            visible=raw.get("visible", True) is not False,
            enabled=raw.get("enabled", True) is not False,
            scroll_x=scroll_x,
            scroll_y=scroll_y,
        ))
    return result
