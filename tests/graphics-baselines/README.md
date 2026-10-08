# Accepted graphics baselines

2026-10-08, colour audit: only the four `power_restoration-{0,1}{,-shotgun}`
samples were updated for corrected MR display-space postfx, after independent
numeric checks, a passing original-SPIR-V control with identical assets, and
review of all four full-resolution captures. Existing comparison tolerances
remain. Previously crushed near-black pixels now contribute RGB and can change
the comparator's nonzero-colour coverage metric. The unrelated pressure sample
discrepancy remains. Evidence: [colour audit](../../docs/color_pipeline_baseline.md#vulkan-sdrhdr-audit--2026-10-08)
and `build/color-audit/{before,after}`.

158 frames from the accepted VK-012 GL/Vulkan parity run are frozen here for native Vulkan regression checks after VK-013 removes GL. Each RCG1 file stores little-endian width, height, tile size (8), then the exact four uint16 channel sums for every 8x8 RGBA tile, in top-down row order. Total: about 4.6 MB.

Tests compare coverage, RGB averages and alpha against these fixed samples and retain their separate geometry, clipping, resource lifetime, lighting, shadow and reload assertions. Tile averaging preserves low-frequency visual differences but does not replace the original full-pixel parity evidence; that evidence is recorded in VULKAN_MIGRATION_PLAN.md. Tests never regenerate the baselines. An intentional visual change requires review of the exported Vulkan captures and an explicit baseline update.

2026-10-08: `graphics.mesh_pipeline` (`--pipeline-only`) uses the frozen
`render_authoring.toml` from `d57f8d3`, the commit accepting `mesh-reload-*`.
This isolates mesh/texture reload, shader rollback, target resize and render-pass state from later
game lighting changes. The previous full-test failure at `mesh-reload-0` was
caused by loading the current, darker ambient values against those fixed samples.
The sample files and comparison tolerances are unchanged; world checks still use
the current shipped configuration.

2026-10-07: the owner-authorized pressure-transfer visual slice explicitly replaces
`pressure_transfer-1.rcgb` and `pressure_transfer-1-shotgun.rcgb` after capture
review. These two samples now represent the revised room, not the historical
VK-012 scene. Other samples retain their existing authority. See
[slice evidence](../../docs/art/pressure-transfer-visual-slice.md); the full test's
pre-existing `mesh-reload-0` mismatch was reproduced on the before-map snapshot.

2026-10-08: G-016 explicitly replaces the same two pressure-transfer samples for
the supported turret placement and retuned mixed-room lights. Reviewed both
exported legacy captures and the actual 1280x720 HDR before/after views; the latter
retain visible equipment, cover and controls. Comparison tolerances are unchanged.
See [implementation and pending human acceptance](../../docs/playtests/G-011-2026-09-29.md#g-016--техническая-доводка-2026-10-08).

2026-10-08: D5 explicitly updates the four `power_restoration-{0,1}` samples,
with and without the shotgun, for the current facility art. A causal run with
historical configuration/map/atlas/model inputs from `d57f8d3` passed all four
old samples on the current renderer. The current inputs include darker ambient,
revised floor/wall textures, native equipment and replacement light fixtures.
All four current captures were reviewed before updating the tile sums. A rebuilt
test with normal comparisons reports zero coverage/RGB differences for these
four samples; tolerances and renderer/game assets are unchanged. Evidence and
the remaining `pressure_transfer-0` failure are recorded under D5 in
[PROJECT_PLAN.md](../../docs/PROJECT_PLAN.md#d5--приёмка-эпизода-и-measured-hardening).

2026-10-08, VIS-05–07: these four power samples were reviewed and updated again
after the requested native room/equipment pass. The map adds six render-only
families; authored solids, gameplay and lighting inputs remain equal to the
saved starting tree. New wall finishes, portals, signage and equipment explain
the coverage/color changes. The prior samples and current captures are retained
under `build/visual-revamp/power/accepted-samples`. The selected
`graphics.power_restoration_slice` checks both views with/without the shotgun,
scene reload, culling and presentation: final coverage/color/RGB differences
are zero. Tolerances, frozen fixture lighting and shaders were not changed.
[Same-camera evidence and validation](../../docs/art/power-restoration-visual-slice.md)
record the SDR/HDR checks and remaining human acceptance. This does not close
the pre-existing pressure sample discrepancy or claim the full mesh suite green.
