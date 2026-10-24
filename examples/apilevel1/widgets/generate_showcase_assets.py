#!/usr/bin/env python3
"""Generate the API LEVEL 1 widget showcase assets deterministically."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple

JsonObject = Dict[str, object]
Color = List[int]
VisualFactory = Callable[[str], Tuple[List[JsonObject], List[JsonObject]]]

ROOT = Path(__file__).resolve().parent
WIDGET_FOLDER = ROOT / "assets" / "widgets"
PAGE_FILE = ROOT / "assets" / "pages" / "widgets.json"

STATES = ("normal", "hover", "pressed", "checked", "disabled", "warning", "pending")

BACKGROUND = [4, 7, 9, 255]
SURFACE = [13, 19, 23, 255]
SURFACE_HOVER = [22, 35, 40, 255]
SURFACE_PRESSED = [27, 57, 61, 255]
SURFACE_DISABLED = [17, 21, 24, 255]
TEXT = [231, 239, 241, 255]
MUTED_TEXT = [130, 151, 158, 255]
CYAN = [108, 200, 218, 255]
BRIGHT_CYAN = [175, 235, 242, 255]
GREEN = [65, 239, 93, 255]
AMBER = [246, 192, 66, 255]
ORANGE = [255, 164, 61, 255]
GRAY = [57, 77, 85, 255]
TRANSPARENT = [0, 0, 0, 0]

# Page coordinates are shared by the canvas placements and the classic pipe reticles.
TANK_Y = 0.02
MAIN_TANK_X = 0.43
MANIFOLD_Y = 0.38
VALVE_Y = 0.53
VALVE_RADIUS = 0.038


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
        node["color"] = TRANSPARENT
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
        node["color"] = TRANSPARENT
    if stroke is not None:
        node.update({"color": stroke, "thickness": thickness})
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


def polygon(primitive_id: str, points: List[Tuple[float, float]], *,
            position: Tuple[float, float] = (0.0, 0.0), fill: Optional[Color] = None,
            stroke: Optional[Color] = None, thickness: float = 0.003) -> JsonObject:
    """Build one closed polygon used by non-rectangular cockpit components."""
    node: JsonObject = {
        "id": primitive_id,
        "type": "polyline",
        "points": [list(point) for point in points],
        "closed": True,
    }
    if fill is not None:
        node.update({"filled": True, "fillColor": fill})
        node["color"] = TRANSPARENT
    if stroke is not None:
        node.update({"color": stroke, "thickness": thickness})
    return add_position(node, position)


def squircle_points(width: float, height: float, exponent: float = 4.0,
                    segments: int = 16) -> List[Tuple[float, float]]:
    """Sample a convex superellipse suitable for compact avionics controls."""
    points: List[Tuple[float, float]] = []
    for index in range(segments):
        angle = 2.0 * math.pi * index / segments
        cosine = math.cos(angle)
        sine = math.sin(angle)
        x = math.copysign(abs(cosine) ** (2.0 / exponent), cosine) * width * 0.5
        y = math.copysign(abs(sine) ** (2.0 / exponent), sine) * height * 0.5
        points.append((round(x, 6), round(y, 6)))
    return points


def squircle(primitive_id: str, width: float, height: float, *,
             position: Tuple[float, float] = (0.0, 0.0), fill: Optional[Color] = None,
             stroke: Optional[Color] = None, thickness: float = 0.003,
             exponent: float = 4.0) -> JsonObject:
    """Build a filled or outlined squircle from one bounded convex polyline."""
    node: JsonObject = {
        "id": primitive_id,
        "type": "polyline",
        "points": [list(point) for point in squircle_points(width, height, exponent)],
        "closed": True,
    }
    if fill is not None:
        node.update({"filled": True, "fillColor": fill})
        node["color"] = TRANSPARENT
    if stroke is not None:
        node.update({"color": stroke, "thickness": thickness})
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
        "letterSpacing": 0.0008,
        "align": align,
        "color": color,
    }
    return add_position(node, position)


def value_binding(primitive_id: str, target: str, start: float = 0.0, end: float = 1.0) -> JsonObject:
    """Build a normalized widget-value binding."""
    return {"primitive": primitive_id, "target": target, "from": start, "to": end}




def button_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    """Build the same compact, nearly rectangular softkey on every edge of the display."""
    surface, accent, foreground = state_palette(state)
    border = accent if state in ("hover", "pressed", "warning", "pending") else GRAY
    face_points = [
        (-0.135, 0.050), (0.127, 0.050), (0.135, 0.042), (0.135, -0.042),
        (0.127, -0.050), (-0.127, -0.050), (-0.135, -0.042), (-0.135, 0.042),
    ]
    elements = [
        polygon("Face", face_points, fill=surface, stroke=border, thickness=0.002),
        line("KeyMark", (-0.134, -0.018), (-0.134, 0.018), accent, thickness=0.004),
        text("Text", "XFER TEST", 0.027, foreground),
    ]
    return elements, [value_binding("Text", "text")]


def toggle_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    checked = state == "checked"
    track_fill = [16, 48, 28, 255] if checked else SURFACE
    control_color = GREEN if checked else accent
    elements = [
        text("Text", "BOOST PUMP", 0.025, foreground, position=(0.0, 0.040)),
        squircle("Track", 0.108, 0.048, position=(0.0, -0.025), fill=track_fill,
                 stroke=control_color, thickness=0.002, exponent=5.0),
        squircle("Thumb", 0.033, 0.034, position=(0.031 if checked else -0.031, -0.025),
                 fill=control_color, exponent=4.0),
        text("SwitchState", "ON" if checked else "OFF", 0.019, control_color,
             position=(0.082, -0.025), align="left"),
    ]
    return elements, [value_binding("Text", "text")]


def checkbox_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = [
        rectangle("Box", 0.036, 0.036, position=(-0.100, 0.0), fill=BACKGROUND,
                  stroke=GREEN if state == "checked" else accent, thickness=0.002),
        text("Text", "CTR PUMPS", 0.024, foreground, position=(-0.067, 0.0), align="left"),
    ]
    if state == "checked":
        elements.append(polyline("Check", [(-0.012, 0.0), (-0.003, -0.010), (0.013, 0.012)],
                                 GREEN, position=(-0.100, 0.0), thickness=0.003))
    elif state == "pending":
        elements.append(line("PendingMark", (-0.010, 0.0), (0.010, 0.0), AMBER,
                             position=(-0.100, 0.0), thickness=0.003))
    return elements, [value_binding("Text", "text")]


def radio_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = [
        circle("Ring", 0.018, position=(-0.100, 0.0), stroke=accent, thickness=0.002),
        text("Text", "MANUAL", 0.024, foreground, position=(-0.067, 0.0), align="left"),
    ]
    if state == "checked":
        elements.append(circle("Selection", 0.010, position=(-0.100, 0.0), fill=GREEN))
    elif state == "pending":
        elements.append(circle("Selection", 0.008, position=(-0.100, 0.0), fill=AMBER))
    return elements, [value_binding("Text", "text")]


def slider_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = [
        text("Caption", "BINGO", 0.025, MUTED_TEXT, position=(-0.30, 0.048), align="left"),
        text("ValueText", "0", 0.034, foreground, position=(0.25, 0.048), align="right"),
        text("Unit", "KG", 0.023, MUTED_TEXT, position=(0.29, 0.048), align="left"),
        rectangle("Track", 0.60, 0.006, position=(0.0, -0.025), fill=GRAY),
        rectangle("Fill", 0.60, 0.006, position=(-0.30, -0.025), fill=accent),
        squircle("Thumb", 0.018, 0.047, position=(-0.30, -0.025), fill=foreground,
                 stroke=accent, thickness=0.0015),
    ]
    bindings = [
        value_binding("ValueText", "text"),
        value_binding("Fill", "scaleX", 0.0, 1.0),
        value_binding("Fill", "positionX", -0.30, 0.0),
        value_binding("Thumb", "positionX", -0.30, 0.30),
    ]
    return elements, bindings


def knob_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = [
        text("Caption", "XFER RATE", 0.024, MUTED_TEXT, position=(0.0, 0.087)),
        arc("Scale", 0.060, -135.0, 135.0, GRAY, position=(-0.035, -0.020), thickness=0.002),
        circle("Dial", 0.046, position=(-0.035, -0.020), fill=SURFACE, stroke=GRAY, thickness=0.002),
        line("Needle", (0.0, 0.0), (0.0, 0.036), accent, position=(-0.035, -0.020), thickness=0.003),
        text("ValueText", "0", 0.031, foreground, position=(0.052, -0.020), align="left"),
    ]
    return elements, [value_binding("ValueText", "text"), value_binding("Needle", "rotation", -135.0, 135.0)]


def gauge_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    """Use an ECAM-style quantity readout with a quiet linear reserve scale."""
    _, accent, foreground = state_palette(state)
    elements = [
        text("Caption", "FUEL ON BOARD", 0.026, foreground, position=(-0.21, 0.075), align="left"),
        text("ValueText", "0", 0.068, GREEN if state == "normal" else accent,
             position=(-0.21, -0.010), align="left"),
        text("Unit", "KG", 0.027, CYAN, position=(0.20, -0.010), align="right"),
        line("Track", (-0.21, -0.072), (0.21, -0.072), GRAY, thickness=0.003),
        line("Needle", (0.0, -0.081), (0.0, -0.063), GREEN,
             position=(-0.21, 0.0), thickness=0.004),
    ]
    return elements, [value_binding("ValueText", "text"), value_binding("Needle", "positionX", -0.21, 0.21)]


def fuel_tank_visual(state: str, label: str, shape: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    """Build one contiguous wing cell and crop its animated fill with local convex masks."""
    _, accent, foreground = state_palette(state)
    quantity_color = ORANGE if state == "warning" else GREEN
    level_color = [82, 53, 13, 255] if state == "warning" else [10, 47, 23, 255]
    outline_color = foreground if state in ("normal", "checked") else accent
    if shape == "center":
        tank_shape = [(-0.16, 0.15), (0.16, 0.15), (0.20, -0.15), (-0.20, -0.15)]
        width = 0.40
        fill_x = 0.0
        masks = [
            [(-0.201, 0.151), (-0.16, 0.15), (-0.20, -0.151)],
            [(0.16, 0.15), (0.201, 0.151), (0.20, -0.151)],
        ]
    else:
        tank_shape = [(-0.28, -0.005), (0.27, 0.15), (0.23, -0.15), (-0.28, -0.15)]
        masks = [
            [(-0.281, -0.005), (0.27, 0.15), (-0.281, 0.151)],
            [(0.27, 0.15), (0.271, -0.151), (0.23, -0.151)],
        ]
        width = 0.55
        fill_x = -0.005
        if shape == "right":
            tank_shape = [(-x, y) for x, y in reversed(tank_shape)]
            masks = [[(-x, y) for x, y in reversed(mask)] for mask in masks]
            fill_x = 0.005
    elements = [
        polygon("TankBackdrop", tank_shape, fill=BACKGROUND),
        rectangle("Fill", width, 0.30, position=(fill_x, -0.15), fill=level_color),
        line("LevelLine", (-width * 0.5, 0.0), (width * 0.5, 0.0), quantity_color,
             position=(fill_x, -0.15), thickness=0.002),
    ]
    for index, mask in enumerate(masks):
        elements.append(polygon(f"FillMask{index}", mask, fill=BACKGROUND))
    elements.extend([
        polygon("TankOutline", tank_shape, stroke=outline_color, thickness=0.003),
        text("ValueText", "0", 0.063, quantity_color, position=(0.0, -0.060)),
        text("Caption", label, 0.025, foreground, position=(0.0, -0.205)),
        text("Unit", "KG", 0.020, CYAN, position=(0.0, -0.246)),
        rectangle("LeftPump", 0.033, 0.045, position=(-0.043, 0.102),
                  fill=BACKGROUND, stroke=quantity_color, thickness=0.0015),
        rectangle("RightPump", 0.033, 0.045, position=(0.043, 0.102),
                  fill=BACKGROUND, stroke=quantity_color, thickness=0.0015),
        line("PumpHeader", (-0.043, 0.125), (0.043, 0.125), quantity_color, thickness=0.002),
        line("LeftPumpInlet", (-0.043, 0.080), (-0.043, 0.035), quantity_color, thickness=0.002),
        line("RightPumpInlet", (0.043, 0.080), (0.043, 0.035), quantity_color, thickness=0.002),
        line("FeedPort", (0.0, 0.125), (0.0, 0.170), quantity_color, thickness=0.0025),
    ])
    bindings = [
        value_binding("ValueText", "text"),
        value_binding("Fill", "scaleY", 0.0, 1.0),
        value_binding("Fill", "positionY", -0.15, 0.0),
        value_binding("LevelLine", "positionY", -0.15, 0.15),
    ]
    return elements, bindings


def left_tank_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    """Build the left-main fuel tank visual."""
    return fuel_tank_visual(state, "L MAIN", "left")


def center_tank_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    """Build the center fuel tank visual."""
    return fuel_tank_visual(state, "CENTER", "center")


def right_tank_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    """Build the right-main fuel tank visual."""
    return fuel_tank_visual(state, "R MAIN", "right")


def fuel_valve_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    """Build only the interactive valve; page reticles own the connected fuel pipes."""
    _, accent, foreground = state_palette(state)
    open_valve = state in ("checked", "pressed")
    valve_color = GREEN if state in ("normal", "checked") else accent
    blade_start = (0.0, -VALVE_RADIUS) if open_valve else (-VALVE_RADIUS, 0.0)
    blade_end = (0.0, VALVE_RADIUS) if open_valve else (VALVE_RADIUS, 0.0)
    elements = [
        circle("ValveBody", VALVE_RADIUS, fill=BACKGROUND, stroke=valve_color, thickness=0.003),
        line("Blade", blade_start, blade_end, valve_color, thickness=0.003),
        text("Text", "S1", 0.026, foreground, position=(0.063, 0.0), align="left"),
    ]
    return elements, [value_binding("Text", "text")]


def crossfeed_valve_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    """Orient the manifold valve horizontally and keep its label clear of both pipes."""
    elements, bindings = fuel_valve_visual(state)
    for primitive in elements:
        if primitive["id"] == "Blade":
            primitive["start"] = list(reversed(primitive["start"]))
            primitive["end"] = list(reversed(primitive["end"]))
        elif primitive["id"] == "Text":
            primitive["position"] = [0.0, 0.073]
            primitive["align"] = "center"
    return elements, bindings


def indicator_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    lamp = GREEN if state == "checked" else accent
    elements = [
        circle("Lamp", 0.009, position=(-0.105, 0.0), fill=lamp),
        text("Text", "FUEL OK", 0.024, foreground, position=(-0.080, 0.0), align="left"),
    ]
    return elements, [value_binding("Text", "text")]


def numeric_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = [
        text("Caption", "XFER FLOW", 0.026, foreground, position=(-0.21, 0.075), align="left"),
        text("ValueText", "0", 0.068, GREEN if state == "normal" else accent,
             position=(-0.21, -0.010), align="left"),
        text("Unit", "KG/MIN", 0.025, CYAN, position=(0.22, -0.010), align="right"),
    ]
    return elements, [value_binding("ValueText", "text")]


def label_visual(state: str) -> Tuple[List[JsonObject], List[JsonObject]]:
    _, accent, foreground = state_palette(state)
    elements = [
        circle("Status", 0.006, position=(-0.265, 0.0), fill=accent),
        text("Text", "FUEL SYSTEM NORMAL", 0.025, foreground, position=(-0.24, 0.0), align="left"),
    ]
    return elements, [value_binding("Text", "text")]


def definition(widget_id: str, widget_type: str, size: Tuple[float, float], default_text: str,
               factory: VisualFactory, *, default_value: float = 25.0, default_checked: bool = False,
               maximum: float = 100.0, step: float = 1.0) -> JsonObject:
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
        "range": {"min": 0.0, "max": maximum, "step": step, "decimals": 0},
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
        "button.json": definition("button", "button", (0.28, 0.14), "XFER TEST", button_visual),
        "toggle.json": definition("toggle", "toggle", (0.28, 0.18), "BOOST PUMP", toggle_visual),
        "checkbox.json": definition("checkbox", "checkbox", (0.28, 0.11), "CTR PUMPS", checkbox_visual),
        "radio.json": definition("radio", "radio", (0.28, 0.11), "MANUAL", radio_visual),
        "slider.json": definition("slider", "slider", (0.70, 0.18), "BINGO", slider_visual,
                                  default_value=2100.0, maximum=6000.0, step=10.0),
        "knob.json": definition("knob", "knob", (0.28, 0.24), "XFER RATE", knob_visual, default_value=45.0),
        "gauge.json": definition("gauge", "gauge", (0.48, 0.24), "FUEL ON BOARD", gauge_visual,
                                 default_value=3520.0, maximum=6000.0),
        "progressBar.json": definition(
            "progressBar", "progressBar", (0.40, 0.54), "CENTER", center_tank_visual,
            default_value=1080.0, maximum=2000.0),
        "leftTank.json": definition(
            "leftTank", "progressBar", (0.56, 0.54), "L MAIN", left_tank_visual,
            default_value=1280.0, maximum=2000.0),
        "rightTank.json": definition(
            "rightTank", "progressBar", (0.56, 0.54), "R MAIN", right_tank_visual,
            default_value=1160.0, maximum=2000.0),
        "valve.json": definition(
            "valve", "toggle", (0.11, 0.11), "S1", fuel_valve_visual),
        "crossfeedValve.json": definition(
            "crossfeedValve", "toggle", (0.11, 0.11), "S3", crossfeed_valve_visual),
        "indicator.json": definition(
            "indicator", "indicator", (0.28, 0.10), "FUEL OK", indicator_visual, default_checked=True),
        "numericDisplay.json": definition(
            "numericDisplay", "numericDisplay", (0.48, 0.24), "XFER FLOW", numeric_visual,
            default_value=0.0, maximum=2880.0),
        "label.json": definition("label", "label", (0.56, 0.09), "FUEL SYSTEM NORMAL", label_visual),
    }


def pipe_reticle(reticle_id: str, start: Tuple[float, float], end: Tuple[float, float],
                 *, arrow: bool = False) -> JsonObject:
    """Author one straight classic reticle; separate runs keep selection bounds local."""
    elements = [line("Pipe", start, end, GREEN, thickness=0.0025)]
    if arrow:
        middle_x = (start[0] + end[0]) * 0.5
        middle_y = (start[1] + end[1]) * 0.5
        elements.append(polyline("FlowArrow", [
            (middle_x - 0.012, middle_y - 0.010), (middle_x, middle_y + 0.010),
            (middle_x + 0.012, middle_y - 0.010),
        ], GREEN, thickness=0.002))
    return {"id": reticle_id, "layerId": "main", "elements": elements}


def legend(reticle_id: str, value: str, position: Tuple[float, float],
           *, font_size: float = 0.027, color: Optional[Color] = None,
           align: str = "center") -> JsonObject:
    """Keep each authored legend independently selectable in the editor."""
    return {"id": reticle_id, "layerId": "main", "elements": [
        text("Text", value, font_size, TEXT if color is None else color,
             position=position, align=align),
    ]}


def cockpit_static_reticles() -> List[JsonObject]:
    """Compose an aircraft fuel schematic from short, ordinary Page reticles."""
    tank_port_y = TANK_Y + 0.168
    return [
        legend("FuelTitle", "FUEL", (-0.67, 0.82), font_size=0.068, align="left"),
        legend("FuelSubtitle", "SYSTEM / TRANSFER", (-0.67, 0.755),
               font_size=0.023, color=MUTED_TEXT, align="left"),
        legend("FuelStatus", "NORM", (0.67, 0.82), font_size=0.029, color=GREEN, align="right"),
        legend("LeftEngine", "1", (-MAIN_TANK_X, 0.705), font_size=0.045),
        legend("RightEngine", "2", (MAIN_TANK_X, 0.705), font_size=0.045),
        legend("ManifoldLegend", "CROSSFEED", (0.0, 0.53), font_size=0.024),
        pipe_reticle("LeftEngineFeed", (-MAIN_TANK_X, VALVE_Y + 0.036),
                     (-MAIN_TANK_X, 0.660), arrow=True),
        pipe_reticle("RightEngineFeed", (MAIN_TANK_X, VALVE_Y + 0.036),
                     (MAIN_TANK_X, 0.660), arrow=True),
        pipe_reticle("LeftTankFeed", (-MAIN_TANK_X, tank_port_y),
                     (-MAIN_TANK_X, VALVE_Y - 0.036)),
        pipe_reticle("RightTankFeed", (MAIN_TANK_X, tank_port_y),
                     (MAIN_TANK_X, VALVE_Y - 0.036)),
        pipe_reticle("LeftManifold", (-MAIN_TANK_X, MANIFOLD_Y), (-0.036, MANIFOLD_Y)),
        pipe_reticle("RightManifold", (0.036, MANIFOLD_Y), (MAIN_TANK_X, MANIFOLD_Y)),
        pipe_reticle("CenterTankFeed", (0.0, tank_port_y), (0.0, MANIFOLD_Y - 0.036)),
        {"id": "InstrumentRule", "layerId": "main", "elements": [
            line("Rule", (-0.67, -0.315), (0.67, -0.315), GRAY, thickness=0.0015),
        ]},
        {"id": "FooterRule", "layerId": "main", "elements": [
            line("Rule", (-0.67, -0.745), (0.67, -0.745), GRAY, thickness=0.0015),
        ]},
    ]


def cockpit_containers() -> List[JsonObject]:
    """Keep the aircraft canvas central and the reusable controls on the perimeter."""
    clear = {"backgroundVisible": False, "borderVisible": False}
    return [
        {
            "id": "Panel", "type": "canvas", "size": [2.10, 1.80],
            "appearance": clear.copy(),
            "children": [
                {"id": "FuelSynoptic"}, {"id": "LeftControls"}, {"id": "RightControls"},
                {"id": "FuelInstruments"}, {"id": "AnnunciatorOverlay"},
            ],
        },
        {
            "id": "FuelSynoptic", "name": "Aircraft fuel schematic",
            "type": "canvas", "size": [1.44, 1.34], "appearance": clear.copy(),
            "children": [
                {"id": "LeftTank", "kind": "widget"}, {"id": "ProgressBar", "kind": "widget"},
                {"id": "RightTank", "kind": "widget"}, {"id": "LeftValve", "kind": "widget"},
                {"id": "RightValve", "kind": "widget"}, {"id": "RootToggle", "kind": "widget"},
            ],
        },
        {
            "id": "LeftControls", "name": "Pump controls",
            "type": "linear", "position": [-0.88, 0.19], "size": [0.28, 0.72],
            "orientation": "vertical", "alignment": "center", "spacing": 0.045,
            "appearance": clear.copy(),
            "children": [
                {"id": "ToggleA", "kind": "widget"}, {"id": "Checkbox", "kind": "widget"},
                {"id": "ScrollLinear"},
            ],
        },
        {
            "id": "ScrollLinear", "name": "Pump status",
            "type": "linear", "size": [0.28, 0.15], "orientation": "vertical",
            "spacing": 0.02, "appearance": clear.copy(),
            "scrolling": {"vertical": "automatic", "clipContent": True, "appearance": {
                "trackColor": SURFACE, "thumbColor": GRAY, "hoverColor": CYAN,
                "activeColor": GREEN, "thickness": 0.007, "minimumThumbSize": 0.045,
            }},
            "children": [
                {"id": "ScrollLabel", "kind": "widget"}, {"id": "PumpStatusLabel", "kind": "widget"},
            ],
        },
        {
            "id": "RightControls", "name": "Feed controls",
            "type": "linear", "position": [0.88, 0.19], "size": [0.28, 0.72],
            "orientation": "vertical", "alignment": "center", "spacing": 0.045,
            "appearance": clear.copy(),
            "children": [
                {"id": "ToggleB", "kind": "widget"}, {"id": "Radio", "kind": "widget"},
                {"id": "Indicator", "kind": "widget"},
            ],
        },
        {
            "id": "FuelInstruments", "name": "Fuel quantities",
            "type": "grid", "position": [0.0, -0.445], "size": [1.16, 0.24],
            "grid": {"rows": 1, "columns": 2}, "spacing": 0.16,
            "appearance": clear.copy(),
            "children": [
                {"id": "Gauge", "kind": "widget", "row": 0, "column": 0,
                 "horizontalAlignment": "center", "verticalAlignment": "center"},
                {"id": "NumericDisplay", "kind": "widget", "row": 0, "column": 1,
                 "horizontalAlignment": "center", "verticalAlignment": "center"},
            ],
        },
        {
            "id": "AnnunciatorOverlay", "name": "Fuel advisory",
            "type": "overlay", "position": [0.0, -0.815], "size": [0.80, 0.10],
            "appearance": clear.copy(), "children": [{"id": "Label", "kind": "widget"}],
        },
    ]


def cockpit_widgets() -> List[JsonObject]:
    """Preserve generated handles while placing controls around the aircraft schematic."""
    return [
        {"id": "RootAction", "definition": "../widgets/button.json", "position": [-0.88, 0.78],
         "exposed": True, "overrides": {"text": "XFER TEST"}},
        {"id": "Refuel", "definition": "../widgets/button.json", "position": [0.88, 0.78],
         "exposed": True, "overrides": {"text": "REFUEL"}},
        {"id": "ToggleA", "definition": "../widgets/toggle.json", "exposed": True,
         "overrides": {"text": "L BOOST", "checked": True}},
        {"id": "ToggleB", "definition": "../widgets/toggle.json", "exposed": True,
         "overrides": {"text": "R BOOST", "checked": True}},
        {"id": "PrivateButton", "definition": "../widgets/button.json", "position": [0.88, -0.80],
         "overrides": {"text": "ACK"}},
        {"id": "Checkbox", "definition": "../widgets/checkbox.json", "exposed": True,
         "overrides": {"text": "CTR PUMPS", "checked": True}},
        {"id": "Radio", "definition": "../widgets/radio.json", "exposed": True,
         "overrides": {"text": "MANUAL"}},
        {"id": "Slider", "definition": "../widgets/slider.json", "position": [0.0, -0.65],
         "exposed": True, "overrides": {"value": 2100.0}},
        {"id": "Knob", "definition": "../widgets/knob.json", "position": [-0.88, -0.47],
         "exposed": True, "overrides": {"value": 45.0}},
        {"id": "Gauge", "definition": "../widgets/gauge.json", "exposed": True},
        {"id": "ProgressBar", "definition": "../widgets/progressBar.json",
         "position": [0.0, TANK_Y], "exposed": True},
        {"id": "Indicator", "definition": "../widgets/indicator.json", "exposed": True},
        {"id": "NumericDisplay", "definition": "../widgets/numericDisplay.json", "exposed": True},
        {"id": "Label", "definition": "../widgets/label.json", "exposed": True},
        {"id": "RootToggle", "definition": "../widgets/crossfeedValve.json",
         "position": [0.0, MANIFOLD_Y], "exposed": True,
         "overrides": {"text": "S3", "checked": True}},
        {"id": "ScrollLabel", "definition": "../widgets/label.json", "size": [0.26, 0.095],
         "overrides": {"text": "L PRESS OK"}},
        {"id": "PumpStatusLabel", "definition": "../widgets/label.json", "size": [0.26, 0.095],
         "overrides": {"text": "CTR READY"}},
        {"id": "LeftTank", "definition": "../widgets/leftTank.json",
         "position": [-MAIN_TANK_X, TANK_Y], "exposed": True},
        {"id": "RightTank", "definition": "../widgets/rightTank.json",
         "position": [MAIN_TANK_X, TANK_Y], "exposed": True},
        {"id": "LeftValve", "definition": "../widgets/valve.json",
         "position": [-MAIN_TANK_X, VALVE_Y], "exposed": True,
         "overrides": {"text": "S1", "checked": True}},
        {"id": "RightValve", "definition": "../widgets/valve.json",
         "position": [MAIN_TANK_X, VALVE_Y], "exposed": True,
         "overrides": {"text": "S2", "checked": False}},
    ]


def page_document() -> JsonObject:
    """Build the live cockpit by composing static legends, containers and widgets."""
    return {
        "name": "Widgets",
        "title": "FUEL | SYNOPTIC",
        "titleDisplay": {"visible": False},
        "bg": "#040709FF",
        "layers": [{"id": "main"}],
        "staticReticles": cockpit_static_reticles(),
        "containers": cockpit_containers(),
        "widgets": cockpit_widgets(),
    }





def canonical_numbers(value: object) -> object:
    """Use stable compact numeric literals in the generated JSON."""
    if isinstance(value, float) and value.is_integer():
        return int(value)
    if isinstance(value, list):
        return [canonical_numbers(item) for item in value]
    if isinstance(value, dict):
        return {key: canonical_numbers(item) for key, item in value.items()}
    return value


def serialized(document: JsonObject) -> str:
    """Serialize generated JSON with the repository's stable formatting."""
    return json.dumps(canonical_numbers(document), indent=2, ensure_ascii=False) + "\n"


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
