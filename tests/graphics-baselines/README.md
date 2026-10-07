# Accepted graphics baselines

158 frames from the accepted VK-012 GL/Vulkan parity run are frozen here for native Vulkan regression checks after VK-013 removes GL. Each RCG1 file stores little-endian width, height, tile size (8), then the exact four uint16 channel sums for every 8x8 RGBA tile, in top-down row order. Total: about 4.6 MB.

Tests compare coverage, RGB averages and alpha against these fixed samples and retain their separate geometry, clipping, resource lifetime, lighting, shadow and reload assertions. Tile averaging preserves low-frequency visual differences but does not replace the original full-pixel parity evidence; that evidence is recorded in VULKAN_MIGRATION_PLAN.md. Tests never regenerate the baselines. An intentional visual change requires review of the exported Vulkan captures and an explicit baseline update.

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
