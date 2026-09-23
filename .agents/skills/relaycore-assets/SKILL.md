---
name: relaycore-assets
description: Create, modify, or review RelayCore visual/content assets such as textures, materials, shaders, blockout/content data, UI graphics, decals, signs, or generated game art. Use when asset quality, visual consistency, or authored-content quality matters; do not use for ordinary C++ implementation alone.
---

# RelayCore asset authoring

Create assets for a restrained GoldSource-inspired industrial/technical FPS, not generic AI sci-fi.

Read only the reference relevant to the task:

- [references/visual-direction.md](references/visual-direction.md) for visual/art decisions.
- [references/quality-gate.md](references/quality-gate.md) before finalizing generated or modified assets.

## Workflow

1. Inspect adjacent assets, material/shader usage, naming, dimensions, and authored data before creating anything.
2. Define the asset's gameplay purpose and where it will be seen: wall/floor/prop/UI/signage/decoration/debug/etc.
3. Match current technical constraints instead of inventing a new asset convention.
4. Generate or edit only the requested deliverables. Do not create a large “asset pack” unless requested.
5. Prefer restrained, readable forms and surfaces. Add detail because it communicates material, function, scale, wear, or navigation—not because empty space looks unfinished.
6. Validate in the actual pipeline or in-engine when available. A standalone image looking plausible is not sufficient evidence that the asset works in RelayCore.

## Technical discipline

- Do not fake binary/image output with placeholder text files.
- Do not silently change formats, resolutions, channel conventions, color-space assumptions, shader semantics, or authored schemas.
- Avoid baked light/shadow/AO/perspective unless the current asset type intentionally contains it.
- Preserve alpha intent and edge quality when transparency is involved.
- For tiling textures, verify seams and avoid unique features that reveal repetition too aggressively.
- For signs/UI, use meaningful readable text only when the task supplies or requires it; do not invent pseudo-language glyph noise.
- For shaders/material data, favor the current renderer's actual needs over PBR boilerplate or unused channels.

## Output

Report created/changed asset paths, intended use, technical assumptions, validation performed, and anything that still needs in-engine inspection.
