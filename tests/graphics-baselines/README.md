# Accepted graphics baselines

158 frames from the accepted VK-012 GL/Vulkan parity run are frozen here for native Vulkan regression checks after VK-013 removes GL. Each RCG1 file stores little-endian width, height, tile size (8), then the exact four uint16 channel sums for every 8x8 RGBA tile, in top-down row order. Total: about 4.6 MB.

Tests compare coverage, RGB averages and alpha against these fixed samples and retain their separate geometry, clipping, resource lifetime, lighting, shadow and reload assertions. Tile averaging preserves low-frequency visual differences but does not replace the original full-pixel parity evidence; that evidence is recorded in VULKAN_MIGRATION_PLAN.md. Tests never regenerate the baselines. An intentional visual change requires review of the exported Vulkan captures and an explicit baseline update.
