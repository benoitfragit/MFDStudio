#!/usr/bin/env python3
"""Generate the API LEVEL 1 widget showcase assets deterministically."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple

JsonObject = Dict[str, object]
Color = List[int]
VisualFactory = Callable[[str], Tuple[List[JsonObject], List[JsonObject]]]

ROOT = Path(__file__).resolve().parent
WIDGET_FOLDER = ROOT / "assets" / "widgets"
PAGE_FILE = ROOT / "assets" / "pages" / "widgets.json"

STATES = ("normal", "hover", "pressed", "checked", "disabled", "warning", "pending")

BACKGROUND = [8, 17, 27, 255]
SURFACE = [17, 34, 49, 255]
SURFACE_HOVER = [24, 48, 66, 255]
SURFACE_PRESSED = [20, 64, 79, 255]
SURFACE_DISABLED = [35, 43, 50, 255]
TEXT = [228, 242, 247, 255]
MUTED_TEXT = [137, 160, 171, 255]
CYAN = [69, 205, 226, 255]
BRIGHT_CYAN = [118, 230, 243, 255]
GREEN = [66, 214, 151, 255]
AMBER = [245, 190, 76, 255]
ORANGE = [244, 126, 61, 255]
GRAY = [104, 122, 132, 255]


def state_palette(state: str) -> Tuple[Color, Color, Color]:
    """Return surface, accent and text colors for one visual state."""
    if state == "hover":
        return SURFACE_HOVER, BRIGHT_CYAN, TEXT
    if state == "pressed":
        return SURFACE_PRESSED, BRIGHT_CYAN, TEXT
    if state == "checked":
        return SURFACE, GREEN, TEXT
    if state == "disabled":
        return SURFACE_DISABLED, GRAY, MUTED_TEXT
    if state == "warning":
        return SURFACE, ORANGE, TEXT
    if state == "pending":
        return SURFACE, AMBER, TEXT
    return SURFACE, CYAN, TEXT


def add_position(node: JsonObject, position: Tuple[float, float]) -> JsonObject:
    """Add a non-zero primitive position without cluttering generated JSON."""
    if position != (0.0, 0.0):
        node["position"] = list(position)
    return node


def rectangle(primitive_id: str, width: float, height: float, *, position: Tuple[float, float] = (0.0, 0.0),
              fill: Optional[Color] = None, stroke: Optional[Color] = None,
              thickness: float = 0.003) -> JsonObject:
    """Build a rectangle primitive."""
    node: JsonObject = {"id": primitive_id, "type": "rectangle", "width": width, "height": height}
    if fill is not None:
        node.update({"filled": True, "fillColor": fill})
    if stroke is not None:
        node.update({"color": stroke, "thickness": thickness})
    return add_position(node, position)


def ellipse(primitive_id: str, width: float, height: float, *, position: Tuple[float, float] = (0.0, 0.0),
            fill: Optional[Color] = None, stroke: Optional[Color] = None,
            thickness: float = 0.003) -> JsonObject:
    """Build an ellipse primitive."""
    node: JsonObject = {"id": primitive_id, "type": "ellipse", "width": width, "height": height}
    if fill is not None:
        node.update({"filled": True, "fillColor": fill})
    if stroke is not None:
        node.update({"color": stroke, "thickness": thickness})
    return add_position(node, position)


def circle(primitive_id: str, radius: float, *, position: Tuple[float, float] = (0.0, 0.0),
           fill: Optional[Color] = None, stroke: Optional[Color] = None,
           thickness: float = 0.003) -> JsonObject:
    """Build a circle primitive."""
    node: JsonObject = {"id": primitive_id, "type": "circle", "radius": radius}
    if fill is not None:
        node.update({"filled": True, "fillColor": fill})
    if stroke is not None:
        node.update({"color": stroke, "thickness": thickness})
    return add_position(node, position)


def ring(primitive_id: str, outer_radius: float, inner_radius: float, *,
         position: Tuple[float, float] = (0.0, 0.0), fill: Color) -> JsonObject:
    """Build a filled ring primitive."""
    node: JsonObject = {
        "id": primitive_id,
        "type": "ring",
        "outerRadius": outer_radius,
        "innerRadius": inner_radius,
        "filled": True,
        "fillColor": fill,
    }
    return add_position(node, position)


def line(primitive_id: str, start: Tuple[float, float], end: Tuple[float, float], color: Color, *,
         position: Tuple[float, float] = (0.0, 0.0), thickness: float = 0.005) -> JsonObject:
    """Build a line primitive."""
    node: JsonObject = {
        "id": primitive_id,
        "type": "line",
        "start": list(start),
        "end": list(end),
        "color": color,
        "thickness": thickness,
    }
    return add_position(node, position)


def polyline(primitive_id: str, points: List[Tuple[float, float]], color: Color, *,
             position: Tuple[float, float] = (0.0, 0.0), thickness: float = 0.008) -> JsonObject:
    """Build an open polyline primitive."""
    node: JsonObject = {
        "id": primitive_id,
        "type": "polyline",
        "points": [list(point) for point in points],
        "color": color,
        "thickness": thickness,
    }
    return add_position(node, position)


def arc(primitive_id: str, radius: float, start_angle: float, end_angle: float, color: Color, *,
        position: Tuple[float, float] = (0.0, 0.0), thickness: float = 0.008) -> JsonObject:
    """Build an arc primitive."""
    node: JsonObject = {
        "id": primitive_id,
        "type": "arc",
        "radius": radius,
        "startAngleDegrees": start_angle,
        "endAngleDegrees": end_angle,
        "segments": 32,
        "color": color,
        "thickness": thickness,
    }
    return add_position(node, position)


def text(primitive_id: str, value: str, font_size: float, color: Color, *,
         position: Tuple[float, float] = (0.0, 0.0), align: str = "center") -> JsonObject:
    """Build a text primitive."""
    node: JsonObject = {
        "id": primitive_id,
        "type": "text",
        "text": value,
        "fontSize": font_size,
        "align": align,
        "color": color,
    }
    return add_position(node, position)


def value_binding(primitive_id: str, target: str, start: float = 0.0, end: float = 1.0) -> JsonObject:
    """Build a normalized widget-value binding."""
    return {"primitive": primitive_id, "target": target, "from": start, "to": end}


def card(width: float, height: float, state: str) -> List[JsonObject]:
    """Build the shared low-contrast showcase card without hiding control identity."""
    surface, accent, _ = state_palette(state)
    return [
        rectangle("Shadow", width, height, position=(0.0, -0.008), fill=BACKGROUND),
        rectangle("Surface", width, height, fill=surface, stroke=accent, thickness=0.003),
    ]


def button_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    surface, accent, foreground = state_palette(state)
    face = accent if state == "pressed" else surface
    label_color = BACKGROUND if state == "pressed" else foreground
    elements = [
        rectangle("Shadow", 0.44, 0.16, position=(0.0, -0.010), fill=BACKGROUND),
        rectangle("Face", 0.44, 0.16, fill=face, stroke=accent, thickness=0.004),
        line("Accent", (-0.17, 0.055), (0.17, 0.055), accent, thickness=0.006),
        text("Text", "EXECUTE", 0.033, label_color),
    ]
    return elements, [value_binding("Text", "text")]


def toggle_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    checked = state == "checked"
    thumb_x = 0.175 if checked else 0.105
    track_fill = [34, 102, 78, 255] if checked else [8, 20, 29, 255]
    elements = card(0.46, 0.18, state)
    elements.extend([
        text("Text", "MASTER ARM", 0.028, foreground, position=(-0.205, 0.0), align="left"),
        ellipse("Track", 0.16, 0.075, position=(0.14, 0.0), fill=track_fill, stroke=accent, thickness=0.004),
        circle("Thumb", 0.027, position=(thumb_x, 0.0), fill=accent),
    ])
    return elements, [value_binding("Text", "text")]


def checkbox_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = card(0.46, 0.18, state)
    elements.extend([
        rectangle("Box", 0.075, 0.075, position=(-0.165, 0.0), fill=BACKGROUND, stroke=accent, thickness=0.005),
        text("Text", "AUTO SYNC", 0.029, foreground, position=(-0.105, 0.0), align="left"),
    ])
    if state == "checked":
        elements.append(polyline("Check", [(-0.026, 0.0), (-0.006, -0.020), (0.030, 0.025)],
                                 GREEN, position=(-0.165, 0.0), thickness=0.009))
    elif state == "pending":
        elements.append(line("PendingMark", (-0.022, 0.0), (0.022, 0.0), AMBER,
                             position=(-0.165, 0.0), thickness=0.007))
    return elements, [value_binding("Text", "text")]


def radio_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = card(0.46, 0.18, state)
    elements.extend([
        ring("Ring", 0.041, 0.032, position=(-0.165, 0.0), fill=accent),
        text("Text", "NAV SOURCE", 0.029, foreground, position=(-0.105, 0.0), align="left"),
    ])
    if state == "checked":
        elements.append(circle("Selection", 0.021, position=(-0.165, 0.0), fill=GREEN))
    elif state == "pending":
        elements.append(circle("Selection", 0.013, position=(-0.165, 0.0), fill=AMBER))
    return elements, [value_binding("Text", "text")]


def slider_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = card(0.50, 0.22, state)
    elements.extend([
        text("Caption", "LEVEL", 0.025, MUTED_TEXT, position=(-0.21, 0.065), align="left"),
        text("ValueText", "0", 0.030, foreground, position=(0.21, 0.065), align="right"),
        rectangle("Track", 0.38, 0.020, position=(0.0, -0.045), fill=BACKGROUND),
        rectangle("Fill", 0.38, 0.020, position=(-0.19, -0.045), fill=accent),
        circle("Thumb", 0.030, position=(-0.19, -0.045), fill=foreground, stroke=accent, thickness=0.004),
    ])
    bindings = [
        value_binding("ValueText", "text"),
        value_binding("Fill", "scaleX", 0.0, 1.0),
        value_binding("Fill", "positionX", -0.19, 0.0),
        value_binding("Thumb", "positionX", -0.19, 0.19),
    ]
    return elements, bindings


def knob_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = card(0.36, 0.26, state)
    elements.extend([
        text("Caption", "TRIM", 0.024, MUTED_TEXT, position=(-0.145, 0.095), align="left"),
        text("ValueText", "0", 0.027, foreground, position=(0.145, 0.095), align="right"),
        arc("Scale", 0.073, -135.0, 135.0, GRAY, position=(0.0, -0.030), thickness=0.009),
        circle("Dial", 0.055, position=(0.0, -0.030), fill=BACKGROUND, stroke=accent, thickness=0.004),
        line("Needle", (0.0, 0.0), (0.0, 0.045), accent, position=(0.0, -0.030), thickness=0.007),
        circle("Hub", 0.009, position=(0.0, -0.030), fill=foreground),
    ])
    return elements, [value_binding("ValueText", "text"), value_binding("Needle", "rotation", -135.0, 135.0)]


def gauge_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = card(0.46, 0.26, state)
    elements.extend([
        text("Caption", "POWER", 0.024, MUTED_TEXT, position=(-0.195, 0.095), align="left"),
        text("ValueText", "0", 0.028, foreground, position=(0.195, 0.095), align="right"),
        arc("Scale", 0.080, -120.0, 120.0, GRAY, position=(0.0, -0.035), thickness=0.012),
        line("LowTick", (-0.075, -0.060), (-0.060, -0.050), MUTED_TEXT, thickness=0.004),
        line("HighTick", (0.075, -0.060), (0.060, -0.050), MUTED_TEXT, thickness=0.004),
        line("Needle", (0.0, 0.0), (0.0, 0.068), accent, position=(0.0, -0.035), thickness=0.006),
        circle("Hub", 0.012, position=(0.0, -0.035), fill=foreground),
    ])
    return elements, [value_binding("ValueText", "text"), value_binding("Needle", "rotation", -120.0, 120.0)]


def progress_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = card(0.50, 0.20, state)
    elements.extend([
        text("Caption", "UPLOAD", 0.025, MUTED_TEXT, position=(-0.21, 0.045), align="left"),
        text("ValueText", "0", 0.028, foreground, position=(0.21, 0.045), align="right"),
        rectangle("Track", 0.40, 0.030, position=(0.0, -0.045), fill=BACKGROUND, stroke=GRAY, thickness=0.002),
        rectangle("Fill", 0.40, 0.030, position=(-0.20, -0.045), fill=accent),
    ])
    bindings = [
        value_binding("ValueText", "text"),
        value_binding("Fill", "scaleX", 0.0, 1.0),
        value_binding("Fill", "positionX", -0.20, 0.0),
    ]
    return elements, bindings


def indicator_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    lamp = GREEN if state == "checked" else accent
    elements = card(0.32, 0.22, state)
    elements.extend([
        ring("Bezel", 0.052, 0.043, position=(0.0, 0.025), fill=GRAY),
        circle("Lamp", 0.034, position=(0.0, 0.025), fill=lamp),
        text("Text", "LINK", 0.025, foreground, position=(0.0, -0.065)),
    ])
    return elements, [value_binding("Text", "text")]


def numeric_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = card(0.42, 0.22, state)
    elements.extend([
        rectangle("Accent", 0.012, 0.16, position=(-0.185, 0.0), fill=accent),
        text("Caption", "ALTITUDE", 0.022, MUTED_TEXT, position=(-0.145, 0.065), align="left"),
        text("ValueText", "0", 0.052, foreground, position=(-0.145, -0.020), align="left"),
        text("Unit", "FT", 0.022, accent, position=(0.165, -0.030), align="right"),
    ])
    return elements, [value_binding("ValueText", "text")]


def label_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    surface, accent, foreground = state_palette(state)
    elements = [
        rectangle("Surface", 0.50, 0.16, fill=surface),
        circle("Status", 0.014, position=(-0.215, 0.0), fill=accent),
        text("Text", "SYSTEM NOMINAL", 0.027, foreground, position=(-0.18, 0.0), align="left"),
        line("Rule", (-0.215, -0.055), (0.215, -0.055), accent, thickness=0.003),
    ]
    return elements, [value_binding("Text", "text")]


def definition(widget_id: str, widget_type: str, size: Tuple[float, float], default_text: str,
               factory: VisualFactory, *, default_value: float = 25.0, default_checked: bool = False) -> JsonObject:
    """Build one complete widget definition with all seven runtime states."""
    visuals: List[JsonObject] = []
    for state in STATES:
        elements, bindings = factory(state)
        visuals.append({
            "state": state,
            "reticle": {"id": f"{widget_id}Visual", "elements": elements},
            "bindings": bindings,
        })
    return {
        "schemaVersion": 1,
        "id": widget_id,
        "type": widget_type,
        "preferredSize": list(size),
        "hitRegion": {"position": [0.0, 0.0], "size": list(size)},
        "range": {"min": 0.0, "max": 100.0, "step": 1.0, "decimals": 0},
        "defaults": {
            "text": default_text,
            "checked": default_checked,
            "value": default_value,
        },
        "visuals": visuals,
    }


def widget_documents() -> Dict[str, JsonObject]:
    """Return every authored definition used by the showcase."""
    return {
        "button.json": definition("button", "button", (0.44, 0.16), "EXECUTE", button_visual),
        "toggle.json": definition("toggle", "toggle", (0.46, 0.18), "MASTER ARM", toggle_visual),
        "checkbox.json": definition("checkbox", "checkbox", (0.46, 0.18), "AUTO SYNC", checkbox_visual),
        "radio.json": definition("radio", "radio", (0.46, 0.18), "NAV SOURCE", radio_visual),
        "slider.json": definition("slider", "slider", (0.50, 0.22), "LEVEL", slider_visual, default_value=42.0),
        "knob.json": definition("knob", "knob", (0.36, 0.26), "TRIM", knob_visual, default_value=35.0),
        "gauge.json": definition("gauge", "gauge", (0.46, 0.26), "POWER", gauge_visual, default_value=68.0),
        "progressBar.json": definition(
            "progressBar", "progressBar", (0.50, 0.20), "UPLOAD", progress_visual, default_value=68.0),
        "indicator.json": definition(
            "indicator", "indicator", (0.32, 0.22), "LINK", indicator_visual, default_checked=True),
        "numericDisplay.json": definition(
            "numericDisplay", "numericDisplay", (0.42, 0.22), "ALTITUDE", numeric_visual, default_value=68.0),
        "label.json": definition("label", "label", (0.50, 0.16), "SYSTEM NOMINAL", label_visual),
    }


def placement(widget_id: str, row: int, column: int) -> JsonObject:
    """Build one centered grid placement for a widget instance."""
    return {
        "id": widget_id,
        "kind": "widget",
        "row": row,
        "column": column,
        "horizontalAlignment": "center",
        "verticalAlignment": "center",
    }


def page_document() -> JsonObject:
    """Build the mixed root, grid and scrolling showcase page."""
    grid_children = [
        {
            "id": "CellCanvas",
            "row": 0,
            "column": 0,
            "horizontalAlignment": "stretch",
            "verticalAlignment": "stretch",
        },
        placement("ToggleB", 0, 1),
        placement("PrivateButton", 0, 2),
        placement("Checkbox", 1, 0),
        placement("Radio", 1, 1),
        placement("Slider", 1, 2),
        placement("Knob", 2, 0),
        placement("Gauge", 2, 1),
        placement("ProgressBar", 2, 2),
        placement("Indicator", 3, 0),
        placement("NumericDisplay", 3, 1),
        placement("Label", 3, 2),
    ]
    containers: List[JsonObject] = [
        {
            "id": "Panel",
            "type": "canvas",
            "size": [1.90, 1.36],
            "position": [0.0, -0.12],
            "children": [{"id": "Grid"}],
            "appearance": {
                "backgroundVisible": True,
                "backgroundColor": [10, 23, 35, 255],
                "borderVisible": True,
                "borderColor": [43, 91, 112, 255],
            },
        },
        {
            "id": "Grid",
            "type": "grid",
            "size": [1.78, 1.24],
            "padding": 0.04,
            "spacing": 0.035,
            "grid": {"rows": 4, "columns": 3},
            "children": grid_children,
        },
        {
            "id": "CellCanvas",
            "type": "canvas",
            "size": [0.54, 0.26],
            "children": [{"id": "ScrollLinear", "resizeMode": "stretch"}],
        },
        {
            "id": "ScrollLinear",
            "type": "linear",
            "size": [0.52, 0.24],
            "orientation": "vertical",
            "spacing": 0.025,
            "scrolling": {"vertical": "automatic", "clipContent": True},
            "children": [
                {"id": "ToggleA", "kind": "widget"},
                {"id": "ScrollLabel", "kind": "widget"},
            ],
        },
    ]
    widgets: List[JsonObject] = [
        {
            "id": "RootAction",
            "definition": "../widgets/button.json",
            "position": [-0.55, 0.70],
            "exposed": True,
            "overrides": {"text": "RUN CHECK"},
        },
        {
            "id": "ToggleA",
            "definition": "../widgets/toggle.json",
            "exposed": True,
            "overrides": {"text": "AUX BUS"},
        },
        {
            "id": "ToggleB",
            "definition": "../widgets/toggle.json",
            "exposed": True,
            "overrides": {"text": "MASTER ARM", "checked": True},
        },
        {
            "id": "PrivateButton",
            "definition": "../widgets/button.json",
            "overrides": {"text": "LOCAL RESET"},
        },
        {"id": "Checkbox", "definition": "../widgets/checkbox.json", "exposed": True},
        {"id": "Radio", "definition": "../widgets/radio.json", "exposed": True},
        {"id": "Slider", "definition": "../widgets/slider.json", "exposed": True},
        {"id": "Knob", "definition": "../widgets/knob.json", "exposed": True},
        {"id": "Gauge", "definition": "../widgets/gauge.json", "exposed": True},
        {"id": "ProgressBar", "definition": "../widgets/progressBar.json", "exposed": True},
        {"id": "Indicator", "definition": "../widgets/indicator.json", "exposed": True},
        {"id": "NumericDisplay", "definition": "../widgets/numericDisplay.json", "exposed": True},
        {"id": "Label", "definition": "../widgets/label.json", "exposed": True},
        {
            "id": "RootToggle",
            "definition": "../widgets/toggle.json",
            "position": [0.55, 0.70],
            "exposed": True,
            "overrides": {"text": "NIGHT MODE", "checked": True},
        },
        {
            "id": "ScrollLabel",
            "definition": "../widgets/label.json",
            "overrides": {"text": "SCROLL FOR MORE"},
            "size": [0.46, 0.16],
        },
    ]
    return {
        "name": "Widgets",
        "title": "API LEVEL 1 WIDGET SHOWCASE",
        "bg": "#07111BFF",
        "layers": [{"id": "main"}],
        "containers": containers,
        "widgets": widgets,
    }


def serialized(document: JsonObject) -> str:
    """Serialize generated JSON with the repository's stable formatting."""
    return json.dumps(document, indent=2, ensure_ascii=False) + "\n"


def expected_outputs() -> Dict[Path, str]:
    """Return every generated path and its expected content."""
    outputs = {WIDGET_FOLDER / name: serialized(document) for name, document in widget_documents().items()}
    outputs[PAGE_FILE] = serialized(page_document())
    return outputs


def update_assets(check_only: bool) -> int:
    """Write generated assets, or report drift without changing the workspace."""
    drifted: List[Path] = []
    for path, expected in expected_outputs().items():
        current = path.read_text(encoding="utf-8") if path.exists() else None
        if current == expected:
            continue
        drifted.append(path)
        if not check_only:
            path.parent.mkdir(parents=True, exist_ok=True)
            with path.open("w", encoding="utf-8", newline="\n") as output_file:
                output_file.write(expected)

    if not drifted:
        print("Widget showcase assets are up to date.")
        return 0

    verb = "Outdated" if check_only else "Updated"
    for path in drifted:
        print(f"{verb}: {path.relative_to(ROOT)}")
    return 1 if check_only else 0


def main() -> int:
    """Parse command-line options and update or verify the showcase assets."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify generated assets without writing them")
    arguments = parser.parse_args()
    return update_assets(arguments.check)


if __name__ == "__main__":
    raise SystemExit(main())
