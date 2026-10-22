#!/usr/bin/env python3
"""API LEVEL 1 widget generation, loaded only for referenced widget instances."""
from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path
import hashlib
import json
import math
import re

TYPES = ("button", "toggle", "checkbox", "radio", "slider", "knob", "gauge", "progressBar", "indicator", "numericDisplay", "label")
STATES = ("normal", "hover", "pressed", "checked", "disabled", "warning", "pending")
TARGETS = ("text", "rotation", "scaleX", "scaleY", "positionX", "positionY", "visible")

@dataclass(frozen=True)
class WidgetSpec:
    widget_id: str
    accessor_name: str
    member_name: str
    canonical_key: str
    transport_id: int
    exposed: bool
    cpp_type: str
    visible: bool
    enabled: bool
    checked: bool
    warning: bool
    value: float
    text: str
    minimum: float
    maximum: float

def fields(node, allowed):
    if not isinstance(node, dict) or set(node) - set(allowed):
        raise RuntimeError("Unsupported widget object or field")

def number(value):
    if isinstance(value, bool) or not isinstance(value, (float, int)) or not math.isfinite(value):
        raise RuntimeError("Widget value must be finite and numeric")
    return float(value)

def vector(value, positive=False):
    if not isinstance(value, list) or len(value) != 2:
        raise RuntimeError("Widget vector requires two numbers")
    values = [number(v) for v in value]
    if any(abs(v) > 10000 or (positive and v <= 0) for v in values):
        raise RuntimeError("Widget geometry out of range")
    return values

def identifier(value):
    if not isinstance(value, str) or not value.strip() or len(value.encode('utf-8')) > 256 or "\0" in value:
        raise RuntimeError("Invalid widget identifier")
    return value

def asset_root(window_path):
    for folder in window_path.resolve().parents:
        if folder.name == 'assets':
            return folder
    return window_path.resolve().parent

def resolve_asset(base, root, reference):
    if not isinstance(reference, str) or not reference or len(reference) > 4096 or "\0" in reference or Path(reference).is_absolute():
        raise RuntimeError("Widget references must be relative paths")
    path = (base / reference).resolve()
    if not path.is_relative_to(root.resolve()):
        raise RuntimeError("Widget dependency escapes the asset root")
    return path

def definition(path, root, templates=None, template_folder=None):
    if path.suffix != '.json' or not path.is_file() or path.stat().st_size > 1048576:
        raise RuntimeError(f"Missing, oversized or invalid widget asset: {path}")
    node = json.loads(path.read_text(encoding='utf-8'))
    fields(node, ('schemaVersion','id','type','preferredSize','hitRegion','range','defaults','visuals'))
    if type(node.get('schemaVersion')) is not int or node['schemaVersion'] != 1:
        raise RuntimeError("Unsupported widget schemaVersion")
    identifier(node.get('id'))
    if node.get('type') not in TYPES:
        raise RuntimeError("Unsupported widget type")
    vector(node.get('preferredSize', [0.3,0.12]), True)
    if 'hitRegion' in node:
        fields(node['hitRegion'], ('position','size'))
        vector(node['hitRegion'].get('position',[0,0]))
        vector(node['hitRegion'].get('size',node.get('preferredSize',[0.3,0.12])),True)
    ranges = node.get('range', {})
    fields(ranges, ('min','max','step','decimals'))
    minimum, maximum = number(ranges.get('min',0)), number(ranges.get('max',1))
    if minimum >= maximum or not math.isfinite(maximum-minimum) or number(ranges.get('step',0)) < 0:
        raise RuntimeError("Invalid widget range")
    if type(ranges.get('decimals',2)) is not int or not 0 <= ranges.get('decimals',2) <= 9:
        raise RuntimeError("Invalid widget decimals")
    visuals = node.get('visuals')
    if not isinstance(visuals,list) or not 1 <= len(visuals) <= 7:
        raise RuntimeError("Widget requires a bounded visual state list")
    states=set()
    dependencies={path}
    for visual in visuals:
        fields(visual, ('state','reticle','bindings'))
        state=visual.get('state')
        if state not in STATES or state in states:
            raise RuntimeError("Unknown or duplicate widget visual state")
        states.add(state)
        if not isinstance(visual.get('reticle'),dict):
            raise RuntimeError("Widget visual requires a reticle composition")
        composition = dict(visual['reticle'])
        visual_base = path.parent
        if 'template' in composition:
            template_id = composition['template']
            if templates is None or template_id not in templates:
                raise RuntimeError(f"Missing widget visual reticle template: {template_id}")
            merged = dict(templates[template_id])
            merged.update(composition)
            if 'elements' not in composition:
                visual_base = template_folder
            composition = merged
        clipping = composition.get('clipping', {})
        if clipping and (not isinstance(clipping, dict) or clipping.get('mode', 'none') != 'none'):
            raise RuntimeError("Widget visuals cannot erase Page layers; use container clipping")
        elements = composition.get('elements', [])
        if not isinstance(elements, list) or len(elements) > 256:
            raise RuntimeError("Widget visual primitive budget exceeded")
        primitive_types = {}
        for primitive in elements:
            if not isinstance(primitive, dict):
                raise RuntimeError("Widget primitive must be an object")
            primitive_id = identifier(primitive.get('id'))
            if primitive_id in primitive_types:
                raise RuntimeError("Duplicate widget visual primitive ID")
            primitive_types[primitive_id] = primitive.get('type', 'line')
        bindings=visual.get('bindings',[])
        if not isinstance(bindings,list) or len(bindings)>64:
            raise RuntimeError("Invalid widget bindings")
        for binding in bindings:
            fields(binding,('primitive','target','from','to'))
            primitive = identifier(binding.get('primitive'))
            if primitive not in primitive_types or (binding.get('target') == 'text' and primitive_types[primitive] != 'text'):
                raise RuntimeError("Invalid widget binding primitive reference or text target")
            if binding.get('target') not in TARGETS or abs(number(binding.get('from',0)))>10000 or abs(number(binding.get('to',1)))>10000:
                raise RuntimeError("Invalid widget binding")
        stack=[(composition,0)]
        visited=0
        while stack:
            value,depth=stack.pop()
            visited+=1
            if depth>64 or visited>16384:
                raise RuntimeError("Widget visual exceeds structural budget")
            if isinstance(value,dict):
                for key,child in value.items():
                    if key == 'file' and isinstance(child,str):
                        dependency=resolve_asset(visual_base,root,child)
                        if not dependency.is_file():
                            raise RuntimeError(f"Missing widget resource: {dependency}")
                        dependencies.add(dependency)
                    stack.append((child,depth+1))
            elif isinstance(value,list):
                stack.extend((child,depth+1) for child in value)
            elif isinstance(value,float):
                number(value)
    if 'normal' not in states:
        raise RuntimeError("Widget requires a normal visual")
    return node,minimum,maximum,dependencies

def validate_values(values, minimum, maximum):
    if type(values['checked']) is not bool or type(values['warning']) is not bool:
        raise RuntimeError("Widget state flags must be booleans")
    if not minimum <= number(values['value']) <= maximum:
        raise RuntimeError("Widget value outside range")
    text = values['text']
    if not isinstance(text, str) or len(text.encode('utf-8')) > 4096 or "\0" in text:
        raise RuntimeError("Invalid widget text")

def collect_widget_specs(page_name, page_root, page_path, asset_root, seen_ids, templates=None, template_folder=None):
    raw=page_root.get('widgets',[])
    if not isinstance(raw,list) or len(raw)>4096:
        raise RuntimeError("Page widgets must be a bounded array")
    container_ids={c['id'].strip().lower() for c in page_root.get('containers',[])}
    ids=set(container_ids)
    exact=set()
    accessors=set()
    result=[]
    for instance in raw:
        fields(instance,('id','definition','position','size','visible','enabled','exposed','overrides'))
        name=identifier(instance.get('id'))
        if name.strip().lower() in ids:
            raise RuntimeError("Widget/container identifier collision")
        ids.add(name.strip().lower())
        exact.add(name)
        path=resolve_asset(page_path.parent,asset_root,instance.get('definition'))
        node,minimum,maximum,dependencies=definition(path,asset_root,templates,template_folder)
        vector(instance.get('position',[0,0]))
        if 'size' in instance:
            vector(instance['size'],True)
        defaults=node.get('defaults',{})
        overrides=instance.get('overrides',{})
        fields(defaults,('checked','warning','value','text'))
        fields(overrides,('checked','warning','value','text'))
        values={'checked':False,'warning':False,'value':minimum,'text':''}
        values.update(defaults)
        validate_values(values, minimum, maximum)
        values.update(overrides)
        validate_values(values, minimum, maximum)
        for flag in ('visible','enabled','exposed'):
            if type(instance.get(flag,False if flag=='exposed' else True)) is not bool:
                raise RuntimeError("Widget flags must be booleans")
        if type(values['checked']) is not bool or type(values['warning']) is not bool:
            raise RuntimeError("Widget state flags must be booleans")
        value=number(values['value'])
        if not minimum<=value<=maximum:
            raise RuntimeError("Widget value outside range")
        text=values['text']
        if not isinstance(text,str) or len(text.encode('utf-8'))>4096 or "\0" in text:
            raise RuntimeError("Invalid widget text")
        accessor=''.join(w[:1].upper()+w[1:] for w in re.sub(r'[^0-9A-Za-z]+',' ',name).split())+'Widget'
        if accessor[:1].isdigit(): accessor='Generated'+accessor
        if accessor in accessors: raise RuntimeError("Widget accessor collision")
        accessors.add(accessor)
        key=f"page/{page_name.strip().lower()}/widget/{name.strip().lower()}"
        transport_id=int.from_bytes(hashlib.sha256(key.encode()).digest()[-8:],'big')
        if transport_id==0 or transport_id in seen_ids: raise RuntimeError("Widget transport ID collision")
        seen_ids.add(transport_id)
        kind=node['type']
        cpp_type='CheckedWidget' if kind in ('toggle','checkbox','radio') else 'NumericWidget' if kind not in ('button','label') else 'Widget'
        result.append(WidgetSpec(name,accessor,accessor[:1].lower()+accessor[1:]+'_',key,transport_id,
            instance.get('exposed',False),cpp_type,instance.get('visible',True),instance.get('enabled',True),
            values['checked'],values['warning'],value,text,minimum,maximum))
    owned=set()
    if sum(len(c.get('children', [])) for c in page_root.get('containers', [])) > 8192:
        raise RuntimeError('Mixed ownership-edge budget exceeded')
    for container in page_root.get('containers',[]):
        for child in container.get('children',[]):
            if child.get('kind','container') not in ('container','widget'):
                raise RuntimeError("Unsupported child kind")
            if child.get('kind')=='widget':
                if child.get('id') not in exact or child['id'] in owned:
                    raise RuntimeError("Missing or multiply owned widget child")
                owned.add(child['id'])
    return result

def input_paths(page_root,page_path,asset_root,templates=None,template_folder=None):
    paths=set()
    for instance in page_root.get('widgets',[]):
        path=resolve_asset(page_path.parent,asset_root,instance.get('definition'))
        node,minimum,maximum,dependencies=definition(path,asset_root,templates,template_folder)
        paths.update(dependencies)
    return paths
