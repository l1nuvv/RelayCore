# Concrete Wall 001

Source: [Poly Haven](https://polyhaven.com/a/concrete_wall_001), powered by Poly Haven.
Photography: Dimitrios Savva; processing: Rico Cilliers.
Downloaded 2026-10-07. [CC0 license](https://polyhaven.com/license).

- File: `concrete_wall_001_diff_1k.jpg`, 1024×1024, 79,320 bytes.
- MD5: `7be71d950acf3cf0d7927e4255a63b38`, verified against the official API.
- Official file: <https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/concrete_wall_001/concrete_wall_001_diff_1k.jpg>.
- Coverage: 1.5×1.5 m, from the source metadata.
- Used channel: diffuse/albedo only; no normal, roughness or displacement map.

`scripts/export-pressure-transfer-glb.py` converts the linearized image to a
desaturated gray-green palette with restrained local variation and packs the
result into `transfer_room_lining.blend` / `.glb`. UV0 repeats at 1.5 m. Runtime
roughness is a scalar 0.97, metallic 0. No embedded directional lighting is added.
The actual source and final in-game wall were visually reviewed. Concrete Wall
003 was also downloaded and inspected, but rejected as excessively weathered
for this room; it remains only in the ignored review directory.

The full-file CDN transfer stalled in this environment. HTTP Range transfers
completed and the reassembled file matched the official checksum. ambientCG
Concrete033 did not complete and is not part of these assets.
