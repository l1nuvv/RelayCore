# VIS-25 — sector signage and occult research, 2026-10-08

Pressure, cooling and power now have local procedures and hazard pictograms
instead of B-17 procedure/sector labels. The six SVG/PNG masters share typography,
margins, framing, numbering and the institutional green/ivory/yellow palette.
Pressure identifies the service/gallery/bypass sequence and pressurized line;
cooling identifies isolation, cooling start and exit with hot-coolant warnings;
power lists the fuse, isolation, reserve bus, breaker, purge and cooling sequence.
Existing room signs and the explicit F-01/P-02/F-04 equipment IDs are reused.

The original B-17 board and notice construction is reused by six native model
variants. The pressure/power procedure placements and six existing notice
placements are retained; cooling receives a board on its entry wall.

## Owner-directed occult additions

The owner approved the generated blood-red pentagram after rejecting a crude
vector draft and its SERVICE RECORD backing. The final motif follows the supplied
reference's star, concentric rings and interior glyphs with crimson smears and
drips. Following the owner's placement correction, it is removed from the
procedure graphics and painted at large scale on walls in pressure, cooling and
power. There is no backing, rectangular plaque, glow or gameplay collider.

A separate B-17 `occult_research_board` replaces the sample workstation's duplicate
procedure board; the observation procedure is retained. Its red-marker glyphs,
partial circle, tentative translations, crossed-out hypotheses, erasure ghosts
and unanswered questions show incomplete research. The existing frame, dimensions,
placement and `sample_notes` identity are retained.

The full industrial-occult and design-systems moodboards were inspected. Owner
direction is now recorded in `ART_DIRECTION.md` and the art-direction skill:
twentieth-century science/industry mixed with urban Art Deco, statues, skulls,
candles, blood, ritual symbols and infernal imagery. The mix has visual authority;
each occult detail need not be justified as scientific equipment.

## Sources and reproduction

- Graphics generator: `scripts/build-sector-graphics.cjs`; no full lab regeneration.
- Blender exporter: `scripts/export-sector-signage-glb.py` on Blender 5.2.2 LTS.
- Models: six `{pressure,cooling,power}_{lab_notes_board,safety_notice}` variants,
  `blood_pentagram` and `occult_research_board` under `content/models`.
- Native declaration: `cmake/RelayCoreModelAssets.cmake`; existing importer,
  receipts, model manifest and runtime staging own the products.
- Transparent blood source: `content/graphics/sector_signage/blood_pentagram.png`.
  Its native variant uses 6,396 triangles cut from coverage and opaque RGB.
  This preserves the current opaque contract without a rectangular surface.
- Research texture: `content/graphics/sector_signage/occult_research_board.png`.

Both raster masters were produced using built-in imagegen. The blood prompt
specified: one upright regular pentagram in concentric circles, intricate
alchemical glyphs, dark-crimson hand-painted blood with smears and gravity-driven
drips; transparent background; no glow, board, frame, labels or SERVICE RECORD.
The owner's reference image supplied the composition. The research prompt used
the accepted blood motif only as glyph reference and specified: flat 16:9 warm
off-white dry-erase albedo, red-marker partial circle and symbol studies,
tentative links, crossed-out hypotheses, erasure ghosts, unfinished work and
handwritten Russian annotations including B-17 / РАСШИФРОВКА, повтор?, совпадение,
нет данных, проверить and версия 02; no blood, drips, frame, perspective or glow.

## Verification

Focused cameras in `relaycore_power_restoration_graphics --signage-views` and
`--signage-views-sdr` show three procedures, three notice plates, three large wall
motifs and the B-17 research board. CTests are `graphics.sector_signage` and
`graphics.sector_signage_sdr`. Working captures, logs, inspector metrics and
pre-change source snapshots are kept under `build/vis25`.

The owner approved the blood master and the revised research-board layout.
The later board revisions align glyph specimens with the wall motif, then
make the handwriting and marker strokes less mechanically regular. Gameplay, solids and lighting authoring
are preserved; only render props/graphics and verification cameras are changed.


### Final visual revisions

The power-room wall pentagram is scaled to 0.82 (about 1.4 m) to clear the
COOLING RESEARCH / INTAKE sign. No change to the sign or gameplay.

The original accepted research board and the first aligned candidate were
preserved for comparison. The owner selected the aligned layout, then asked
for recognizable reference glyphs on the right and less uniform handwriting.
These edits use built-in imagegen; they are illustrative hand copies and
tentative interpretations, not an exact geometric transcription of the wall.
The right-side hypotheses retain questions, crossouts and incomplete links.

Final edit prompt: preserve the accepted layout, glyph identities, Russian
words and worn surface; make the actual black handwriting and red-marker
strokes naturally uneven, with drifting baselines, varied letter heights,
spacing/slant/pressure, dry-marker gaps, asymmetric circles, overshoots and
local retracing. Keep it legible; no new symbols/text, frame or perspective.

Before the handwriting refinement, both Debug and RelWithDebInfo assets built,
29 focused content tests passed, and both HDR/SDR signage graphics tests passed.

After the final handwriting edit: Blender 5.2.2 authored/fresh-import checks passed;
Debug and RelWithDebInfo native compilation and staging passed. All 58 model
products match between profiles; receipts and staged hashes passed for both.
The matching optimized B-17 bake was reused and its current signature verified
by the Debug baker (14708618083590780887). Both signage graphics tests passed
again (HDR and SDR, 38.44 seconds total); the current in-game board was inspected.
Geometry and gameplay-preservation checks and git diff --check passed. The
previous 29 content tests remain applicable because the final edit changes only
the board's raster artwork. No archive, commit or release was created.

### Owner follow-up, 2026-10-13

The shared wall-pentagram asset now has a crimson emissive map. After the owner's
brightness feedback, its linear emission factor is 0.35; separate red lights in
power_restoration and containment_loop use intensity 1.8 to strengthen the wall
halo. The research board is unchanged. The exporter supports
`-- --pentagram-only` for this isolated update.

The same request adds explicit power_pump collision above the existing enclosure,
box collision to the first level's utility pipes, and separate head/stand/tripod
collision for the B-17 observation camera. Sources are
`content/collision/{power_pump,b17_observation_camera}.toml`, paired with their
models in `cmake/RelayCoreCollisionAssets.cmake` and staged under `collision/`.

MSVC Development demo/content/level-check/signage targets built. Initial sandbox
attempts failed with C1902 (PDB manager) and Access denied on temporary save-file
renames; the same checks passed outside the sandbox. Final checks:

- `ctest --preset validation-dev -R '^validation\.(power restoration|power pump pipes|all shipped maps)' --output-on-failure --no-tests=error`: 12 passed,
  including native ray hits, player movement blocking and adjacent clearance.
- `ctest --preset validation-dev -R '^graphics\.sector_signage(_sdr)?$' --output-on-failure --no-tests=error`: 2 passed, zero skips, repeated after the brightness adjustment.
- `relaycore_level_check` on power_restoration, occult_lab_b17 and containment_loop:
  valid, zero warnings. Blender 5.2.2 fresh-import: 6396 faces, silhouette retained.
- HDR power-wall capture inspected at
  `build/msvc-dev/tests/power-captures/hdr/shadows/signage-power-wall.bmp`.

Development runtime assets are updated. These are technical and rendered checks;
no independent player run or new art acceptance is claimed. The Development GPU
checks run without Vulkan validation layers. No package was produced.
