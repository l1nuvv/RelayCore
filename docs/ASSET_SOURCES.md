# RelayCore asset sources and selection

Read when a requested art/content task needs external textures, materials, HDRIs or models. This is an agent workflow and source directory, not a queue to download libraries or replace existing assets. Source pages were checked on 2026-10-07; availability, downloads and pricing can change.

## Project intent and priorities

The owner intends the engine and game to remain private. Do not make licensing audits, attribution paperwork or public-release preparation routine gates for asset selection. Keep the asset page URL and ID for retrieval, re-download and tracking local adaptations. Do not publish project files or upload private models to a service without an explicit request.

Choose sources by visual fit, useful geometry, texture quality, editable formats, adaptation cost and price. Prefer free available downloads when they satisfy the task; this catalog does not authorize purchases, subscriptions or account changes. A site's listing here does not mean it has a connected tool or automated download API.

[ART_DIRECTION.md](ART_DIRECTION.md) owns style: twentieth-century scientific-industrial and urban Art Deco spaces, late-1980s/1990s analog equipment, and the owner's overt ritual/infernal motifs. The [static-model section of DEVELOPMENT.md](MODEL_ASSETS.md#статические-модели-glb--native-product) owns supported interchange and runtime materials. Use [relaycore-blender](../.agents/skills/relaycore-blender/SKILL.md) for Blender authoring and [relaycore-assets](../.agents/skills/relaycore-assets/SKILL.md) for native compilation/staging.

## Agent workflow

1. Identify the actual need: tiling surface, unique prop texture, decal, HDRI, standalone model or modular kit. Record gameplay use, dimensions, viewing distance and the required finish. Inspect relevant existing project assets first.
2. Choose the search route below. Search for the real object/material and period, rather than generic `sci-fi`. Open a few promising asset pages and inspect images from useful angles. Search results and beauty renders alone do not establish usable geometry.
3. Select a coherent candidate, or a small compatible set. For models, inspect the downloadable format, wireframe/triangle count when available, UVs, material slots, texture dependencies and pivots. For surfaces, inspect tiling, physical coverage, pattern scale and available maps. Mark unknowns instead of inventing specifications.
4. Download only what the authorized asset task needs. Keep the source page/ID, original filenames, selected resolution and meaningful local changes beside the durable source. Use an isolated working directory under `build/` for evaluation; adopt accepted files through the existing `scripts` / `content/models` conventions.
5. Adapt in Blender: scale, origin, orientation, geometry density, UVs, materials and construction must fit RelayCore. Preserve useful detail. A high-poly scan may need retopology; an attractive render asset may need substantial preparation for gameplay.
6. Inspect multiview renders and relevant close-ups, then validate the actual exported GLB and affected native product. A successful Blender import is not a runtime compatibility pass. Report the selected source, adaptation and checks actually performed.

Stop searching once a candidate answers the task. Reuse accepted materials and sources across iterations. If a source is unavailable, try another appropriate site; do not let one network failure block all modeling or silently claim a download succeeded.

## Textures, surfaces, materials and HDRIs

These descriptions are search guidance, not a guarantee that every listed material fits the current engine.

| Source | Search there for | Agent guidance |
| --- | --- | --- |
| [Poly Haven textures](https://polyhaven.com/textures) / [HDRIs](https://polyhaven.com/hdris) | Scanned surfaces and environment lighting | Primary source for material look development. The installed rendering skill includes a Poly Haven search/download CLI; see its `references/poly-haven-materials.md`. HDRIs help Blender review and renders; they do not establish runtime IBL support. |
| [ambientCG](https://ambientcg.com/) | Concrete, plaster, brick, tile, metal, wood and other tiling surfaces | Search here early for walls/floors. Compare physical coverage and pattern scale before selecting resolution. |
| [CGBookcase](https://www.cgbookcase.com/textures) | PBR surface sets for environments and props | Useful alternative when the first libraries lack the required surface. Inspect the map set and normal convention. |
| [ShareTextures](https://www.sharetextures.com/) | Architectural and interior surfaces | Compare several restrained variants for a consistent facility material palette. |
| [TextureCan](https://www.texturecan.com/) | PBR surfaces and patterned materials | Useful for more specific patterns; check repetition and physical scale in a representative mesh. |
| [Textures.com](https://www.textures.com/) | PBR materials, surface photographs, facade and object-detail imagery | Useful for building custom texture sheets and unique prop details as well as tiling materials. Check the actual downloadable resolution and account/price requirements. |
| [Fab decals](https://www.fab.com/category/decal) / [Fab](https://www.fab.com/) | Megascans surfaces, cracks, leakage, stains, damage and markings | Distinguish texture sets from engine-specific material packages. Keep dirt/damage local and meaningful. Use the project's supported decal or baked-texture path; do not assume a new runtime decal system. |
| [Blendkit / BlenderKit](https://www.blendkit.com/) | Ready-made Blender materials and material look development | Distinguish texture-based materials from procedural node graphs. Inspect/export/bake only to the subset the destination actually supports. |

For RelayCore, start with a compact palette of concrete/plaster, painted steel, ceramic tile, terrazzo/linoleum, rubber, wood and restrained copper/brass. Add local wear and contamination after clean construction and material boundaries read correctly. Do not collect dozens of unrelated materials just because they are available.

Useful queries: `painted metal`, `powder coated steel`, `brushed steel`, `aged brass`, `concrete wall`, `painted plaster`, `terrazzo`, `linoleum`, `ceramic tiles`, `rubber`, `oil stains`, `water leakage`. Add the missing finish, scale or condition to the query.

## Models and game asset packs

| Source | Search there for | Agent guidance |
| --- | --- | --- |
| [Poly Haven models](https://polyhaven.com/models) | Scanned objects and environment props | Good for grounded detail where the catalog matches the need. Inspect scan density, underside/back surfaces and texture dependencies. |
| [Sketchfab search](https://sketchfab.com/search) | Specific objects, retro electronics, industrial/scientific props and scans | Enable `Downloadable`; many viewable models have no downloadable files. Use the viewer to inspect all sides and wireframe when available. The site's [download overview](https://sketchfab.com/features/free-3d-models) also directs store purchases to Fab. |
| [CGTrader](https://www.cgtrader.com/) | Laboratory equipment, machinery, tools, electronics, furniture and specific realistic props | Useful for exact real-world subjects. Distinguish game/low-poly assets from render-only and 3D-print models; inspect formats and included textures. |
| [Fab 3D models](https://www.fab.com/category/3d-model) | Game-ready props, modular interiors, environment kits, weapons and characters | Prefer available source meshes/texture sets that Blender can read. An Unreal/Unity package by itself is not a RelayCore interchange asset. Megascans also lives here. |
| [TurboSquid](https://www.turbosquid.com/) | Detailed realistic objects, architecture, machinery and furniture | A fallback for a specific subject. Inspect editability, geometry density and actual formats; a renderer-specific scene can cost more to adapt than to model locally. |
| [Blendkit / BlenderKit](https://www.blendkit.com/) | Editable Blender props, furniture, equipment and scenes | Convenient for Blender preparation. Extract only the required object and inspect linked data, textures, modifiers and render-only shader features. |
| [Blend Swap](https://blendswap.com/) | Community `.blend` models and construction examples | Useful editable source candidates; quality and dependencies vary. Inspect the scene and external files before accepting it. |
| [itch.io 3D assets](https://itch.io/game-assets/tag-3d) | Small themed prop packs, modular kits, retro/FPS assets | Prefer internally consistent packs. Read the actual file list; not every game asset pack includes portable source meshes. |
| [Kenney assets](https://kenney.nl/assets) | Simple coherent 3D kits, industrial/city props and prototype assets | Useful for blockouts or an explicitly compatible simplified style. Refine or replace visible placeholders when the task calls for detailed grounded art. |
| [Quaternius](https://quaternius.com/) | Stylized game kits, props, base characters and animation source | Useful as source structure or for an intentional stylized target. Adapt proportions/materials to the brief; do not introduce cartoon kits into a grounded facility by default. |
| [OpenGameArt](https://opengameart.org/) | Community game models, props and supplementary asset packs | A secondary search source. Filter for 3D and inspect the files; the catalog also contains 2D, audio and unrelated formats. |

Choose individual realistic props from Sketchfab/CGTrader/Poly Haven/Blendkit for specific facility equipment; inspect Fab/itch.io for coherent modular kits. Kenney/Quaternius are useful for prototyping and deliberately simplified art, not an automatic finish target for detailed assets. These are project search priorities, not quality rankings of every asset on a site.

Useful queries: `1990s CRT monitor`, `analog oscilloscope`, `laboratory power supply`, `electrical cabinet`, `industrial ventilation`, `steel workbench`, `microscope`, `pressure gauge`, `industrial valve`, `cable tray`, `modular industrial corridor`, `laboratory props`, `retro electronics`. Prefer plausible period hardware and functional construction over generic futuristic panel decoration.

## Texture and model handoff

- For Blender look development, use the full material inputs that the requested render needs. Color/albedo is sRGB; normal/roughness/metalness/height are data. Check normal-map convention and physical tiling scale.
- For game delivery, recheck the current importer in `docs/DEVELOPMENT.md`. The native v5 path supports embedded base color, tangent-space normals, metallic/roughness maps and factors (explicit lighting opt-in), AO and core emissive maps/factors. UV0, tangents, supported samplers and channel/color-space rules still apply. Glass/transparency, emissive-strength extensions, displacement, procedural shader graphs, skins and animations remain unsupported; a full downloaded PBR set does not export unchanged.
- A model advertised as rigged or animated does not add skeletal/animation support to RelayCore. Keep such source work separate from the accepted static runtime deliverable unless the requested milestone provides the consumer.
- Keep collision explicitly authored and separate from the render mesh. Do not use high-density scan triangles as collision merely because the visual model is available.
- Prefer a portable `.blend` or mesh plus textures for preparation, then export to the project's GLB/native workflow. Validate scale, normals/tangents, UVs, visible material response and representative gameplay viewing distance.

For the exact Blender execution and visual review steps, use [relaycore-blender](../.agents/skills/relaycore-blender/SKILL.md). For native ownership, receipts, staging and runtime evidence, use [relaycore-assets](../.agents/skills/relaycore-assets/SKILL.md).
