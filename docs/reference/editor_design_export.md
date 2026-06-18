# Editor Design Export

This page documents the design-document export workflow available in
`mfd_editor`.

## Menu Entry

Open:

`File > Export > Export design...`

The action exports documentation for the currently loaded window without
modifying authored JSON assets.

## Output

The export creates one folder containing:

- `README.md`
- `window_icd.md`
- `pages/*.md`
- `images/*_design_exploded.png`
- `images/*_design_page.png`
- `data/design_manifest.json`

If the selected output folder is not empty, the editor creates one timestamped
subfolder instead of overwriting unrelated files.

## Markdown Content

The generated Markdown documents include:

- window title and source file
- page list with relative links
- per-page reticle tables
- canvas `x` and `y` coordinates when enabled
- strobe and blink sections when enabled
- primitive ids when available
- generic name-based `mfd::CommandClient` snippets
- generated transport ids and mapping hash when a companion generated transport map is available

When generated metadata is unavailable, the export writes `not available`
instead of failing.

## Exploded Designer Views

Each page can also export one exploded designer view image.

The current MVP layout:

- renders the page canvas
- places labels on the right side
- sorts labels by reticle `y`
- draws one straight line from each label to the visible reticle outline instead of the reticle center when geometry is available
- prints authored `x` and `y` coordinates next to the reticle name when enabled
- derives callout anchors from the supported primitive geometries used by the reticle model, with a bounds fallback when no outline can be sampled

These images are design documentation, not pixel-perfect runtime screenshots.

## Clean Page Views

Alongside each exploded view, the export also writes one clean page image
(`*_design_page.png`) that renders the same page canvas **without** any reticle
labels or callout lines. It is a complement to the exploded view, not a
replacement: the exploded view stays the annotated reference while the clean
view provides a label-free render suitable for embedding elsewhere.

Both images share the same view framing and are gated by the single
`Export exploded designer views` option.

If one image cannot be rendered, the Markdown files are still generated and the
export reports a warning.
