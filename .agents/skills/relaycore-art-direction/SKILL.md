---
name: relaycore-art-direction
description: Define or review RelayCore art direction for environments, props, modeling, materials, decals, signage, lighting, and generated concepts. Use for visual language and style consistency; asset export and pipeline validation belong to relaycore-assets, and ordinary C++ work does not require this skill.
---

# RelayCore art direction

Read `docs/ART_DIRECTION.md` relative to the repository root before visual decisions. Inspect its linked style board when composition, palette, materials, or prop design matters. The document contains the visual vocabulary, examples, exclusions, and priority order; do not duplicate or silently redefine them.

The owner's full moodboards define an intentional mixture: twentieth-century scientific-industrial environments, analog technology, urban Art Deco, statues, skulls, candles, blood, sigils, pentagrams and infernal imagery. Laboratory technology from the 1980s/1990s is one strand, not a restriction on the entire setting. Occult elements can be ritual, overt or unexplained as well as engineered. Gameplay readability governs composition and detail.

When a requested asset needs external materials or models, use `docs/ASSET_SOURCES.md` for task-specific sites, search routes and adaptation guidance. Select sources to fit this art direction and the intended gameplay view.

## Apply the direction

- Identify the asset category, gameplay purpose, viewing distance, detail budget, and reuse or modularity potential.
- Inspect relevant existing assets and their actual technical constraints. Prefer reuse, material variation, decals, and small edits when they satisfy the task.
- Keep construction and room function readable while giving the moodboards' occult and urban motifs their intended presence. Symbols and props can communicate research, rituals, unresolved phenomena or environmental story; an equipment function is not required for every occult element.
- Use the document's material, technology, lighting, and signage vocabulary. Named games and the supplied board are references for principles; create original project-specific assets, labels, symbols, and layouts.
- Scope output to the user's request. The B-17 environment proposal is a reference target, not permission to build a scene or change the roadmap.

For Blender/Python production, also use `relaycore-blender` and its installed modeling, rendering and validation specialists. It owns the staged modeling process; this skill remains the visual authority.

## Modeling and composition

- Establish blockout scale, proportions, routes, sightlines, and interaction readability before detail.
- Use geometry for silhouette, depth, interaction, and construction; use materials and decals for secondary surface detail where the pipeline supports them.
- Prefer reusable modular dimensions and readable large forms. Background props need only the detail justified by their viewing distance; hero props can concentrate detail in a few meaningful features.
- Use the moodboards' practical, ritual and infernal lighting hierarchy with deliberate focal points. Do not translate the board's scene lighting into baked texture shadows or glow unless the requested asset type requires it.
- Follow current origin, naming, scale, material, and export conventions. For asset production, use `relaycore-assets` for pipeline checks; the art reference does not authorize new renderer or pipeline capabilities.

## Conflicts and review

When a concept or imported asset conflicts with the direction, identify the mismatch and apply the smallest adaptation that makes it fit. Explicit user choices and newer explicit project decisions take precedence; explain material deviations instead of silently changing the style.

Review function, believable construction, material consistency, occult context, silhouette at gameplay distance, justified detail, and fit with the existing modular kit. Revise concrete mismatches rather than treating the principles as a mechanical checklist.

Report what changed, what was reused, any intentional deviations, actual checks, and anything still unverified. Do not claim an import, render, playtest, or in-engine inspection unless performed.
