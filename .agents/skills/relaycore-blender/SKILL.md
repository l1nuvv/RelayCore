---
name: relaycore-blender
description: Create or refine RelayCore models with Blender and Python, including reference-based form, hard-surface construction, materials, multiview review and native-compatible GLB handoff. Use for scripted modeling and Blender asset critique; not for unrelated C++ changes or profiling.
---

# RelayCore Blender modeling

Paths are relative to the repository root. Use `relaycore-art-direction` for visual decisions and `relaycore-assets` for source/export/native delivery. Read `docs/ART_DIRECTION.md` and the relevant section of `docs/MODEL_ASSETS.md` for the requested asset. Their project constraints override the generic workflows below.

For external textures, HDRIs and models, follow `docs/ASSET_SOURCES.md`: it maps each asset need to suitable sites and records the private-project workflow. Keep search/adaptation within the requested asset task.

## Installed specialists

Load only the relevant skill:

- [blender](../blender/SKILL.md): Python API, `bpy.data`, `bmesh`, transform math, operator context and evaluated meshes.
- [blender-modeling-workflow](../blender-modeling-workflow/SKILL.md): reference analysis, forms, topology, UVs and material refinement.
- [blender-procedural-workflow](../blender-procedural-workflow/SKILL.md): Geometry Nodes and repeated or parameterized construction when the asset actually needs it.
- [blender-rendering-workflow](../blender-rendering-workflow/SKILL.md): material look development, authored lighting and presentation.
- [blender-asset-validation](../blender-asset-validation/SKILL.md): mesh inspection, fresh GLB import and fixed multiview renders.
- [blender-iterative-refinement](../blender-iterative-refinement/SKILL.md): a distinct critique and repair pass when improving a completed candidate.

These are standalone skills, not an installed MCP plugin. Use their bundled CLI scripts when a named MCP tool is unavailable. Upstream mentions of character, animation, simulation, intake or benchmark specialists are optional routes outside this installed set; do not assume those skills or runtime capabilities exist. The project art-direction skill owns intake here. Ordinary construction already includes visual self-review; a second agent and user approval after every stage are unnecessary.

## Model to the requested finish

Inspect references that answer silhouette, proportion, profile, thickness, negative-space and construction questions. Record useful observations and uncertain hidden structure beside the source; do not substitute an uninspected image collection for analysis.

Choose construction per form: explicit mesh profiles with `bmesh`, extrusion/inset/bridge operations, swept curves, revolved profiles, boolean cuts with reviewed shading, or local subdivision with controlled edge support. Primitives are starting geometry. Refine primary shapes and connections before adding surface detail. Use physical dimensions and deliberately sized bevels; a universal cube-plus-bevel recipe does not fit every object.

Check material scale, grain direction, roughness and surface response. Inspect neutral renders as well as the final presentation. Fix visible faceting, inflated profiles, floating parts, unexplained intersections and unresolved blockout shapes. Detail should express the object's construction and remain useful at its intended gameplay distance. Do not chase realism by increasing polygon counts or adding uniform dirt and decorative greebles.

Keep source scripts reproducible with named parts, dimensions and deterministic seeds where needed. Preserve a good candidate before a targeted repair. Open fixed perspective/front/back/left/right/top evidence and relevant close-ups; compare repairs with the same camera and lighting. Numerical geometry checks supplement visual review.

## Execute locally

Resolve an existing Blender executable from `BLENDER_EXECUTABLE`, PATH, or the Windows Blender installation (including the Steam uninstall registry entry's `InstallLocation`). Record its `--version`; the imported workflows target Blender 5.2 LTS. Do not install or upgrade Blender silently. Run `bpy` scripts inside Blender, not system Python, and consult the matching [Blender Python API](https://docs.blender.org/api/current/) for version-sensitive calls.

Prefer `bpy.data` for datablocks, `bmesh` for connected mesh edits, and `mathutils` for transforms. Operators need explicit active object, selection and mode. Measure evaluated modifier results through the dependency graph, not only the original mesh. Free temporary BMesh/evaluated meshes when done.

```powershell
& $env:BLENDER_EXECUTABLE --background --factory-startup --disable-autoexec `
  --threads 2 --python-exit-code 1 --python scripts/<asset-generator>.py
```

Serialize Blender jobs on the shared desktop. Use the rendering workflow's verified GPU selection for substantial renders and retain a small CPU thread budget. Store generated review images, metrics and temporary scenes under `build/blender-review/<asset>/<revision>`; do not overwrite authored assets for a tool smoke check. Follow existing source paths for durable `.py`, `.blend`, textures and `.glb`.

For material/HDRI lookup, the rendering skill includes `scripts/poly-haven-cli.ts`. Bun can run it, or Node.js with native TypeScript stripping (verified with Node 24). This is optional network access, not a modeling prerequisite. Keep a source manifest for downloaded assets; do not claim downloads or texture review when the provider is unavailable.

## Preserve the native contract

Use the existing meter scale, origin and Blender-to-runtime basis conventions. Follow the current exporter and importer rather than inventing export flags. Realize required procedural geometry and inspect the evaluated result before export.

The current importer accepts static opaque meshes, base color and tangent-space normals. Metallic/roughness scalar factors require the existing explicit opt-in. It rejects skins/animation/morphs, alpha/double-sided materials and occlusion/emissive/metallic-roughness maps. Procedural shader graphs, glass, volumes, HDRI lighting and Blender compositor effects do not become supported runtime features through GLB export. Adapt the asset to the documented subset; a render-only request may retain Blender-specific materials.

Fresh-import the GLB and use `relaycore-assets` for the affected native compilation/staging and runtime check when delivering a game asset. Keep collision separate from the render mesh. Report exactly which views, export/native checks and runtime inspection were performed. A skill setup request does not authorize new models, roadmap work or packaging.
