---
name: svg-precision-skill
description: Generate deterministic SVGs from structured specs with validation and rendering for RelayCore icons, signage, equipment diagrams, charts, UI mockups and technical drawings. Use for vector artwork, not mesh modeling or RML/RCSS implementation.
---

# SVG Precision Skill

Build SVGs from explicit scene specifications, then validate before handing them off.

## RelayCore use

For in-world signs, equipment graphics and player-facing icons, use `relaycore-art-direction` and the relevant part of `docs/ART_DIRECTION.md`. Match nearby authored symbols, typography, palette and viewing scale. The generic recipes below provide construction patterns, not a replacement style guide; rounded modern cards and default fonts are optional examples.

Keep the JSON specification and SVG as editable source. Current RmlUi integration disables SVG support; for game delivery, render a PNG at the consumer's required resolution and route it through `relaycore-assets`. Use `relaycore-rmlui` for RML/RCSS, screen layout and UI integration. Do not enable a new SVG runtime dependency to use this authoring skill.

Use explicit fonts with the needed Cyrillic glyphs; inspect long labels, thin strokes, alpha edges and readability at the actual display size. Check dimensions, color/alpha conventions and staging against the existing consumer. A valid SVG or attractive PNG does not establish runtime appearance or human readability in motion.

## Workflow

1. Translate the request into a concrete spec with fixed dimensions and coordinates.
2. Use `references/spec.md` for templates and `references/recipes.md` for stable layout patterns.
3. Build the SVG with `scripts/svg_cli.py build`.
4. Validate with `scripts/svg_cli.py validate`.
5. Render a PNG preview when the user needs a quick visual check.

## Rules

- Set `viewBox`, width, and height explicitly.
- Prefer absolute coordinates and simple shapes.
- Treat text as risky when exact rendering matters.
- Avoid exotic filters unless they are necessary and testable.

## Windows execution

Resolve scripts and references relative to this skill. Create outputs in an ignored task directory under `build/`, outside the skill's own scripts. [references/windows.md](references/windows.md) covers PowerShell commands, preview dependencies and verification. Build/validation use Python's standard library; PNG preview needs CairoSVG and native Cairo, and rendered diffs also need Pillow.

Preserve all upstream modes: icons, diagrams, charts, mockups, technical drawings and image comparison. Treat validation warnings as information to review, not a visual pass. Upstream pin and local adaptations are recorded in [../MEDIA_SVG_SOURCES.md](../MEDIA_SVG_SOURCES.md).
