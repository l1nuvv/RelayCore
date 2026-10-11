# Poly Haven laboratory assets

Original 1K glTF, geometry and textures, downloaded on 2026-10-07 and checked
against the file API's MD5 hashes. All four assets use [CC0](https://polyhaven.com/license).

| Source | Author | Runtime model |
| --- | --- | --- |
| [Vintage Microscope](https://polyhaven.com/a/vintage_microscope) | Luis José Fernández Rodríguez | `lab_microscope` |
| [Retro Multimeter](https://polyhaven.com/a/retro_multimeter) | elli moeller | `lab_multimeter` |
| [Television 02](https://polyhaven.com/a/television_02) | Benny Weimer | `surveillance_monitor` |
| [Vintage Spacecraft Instrument](https://polyhaven.com/a/vintage_spacecraft_instrument) | Michał Wiśniewski | `analog_navigation_instrument` |

`scripts/export-polyhaven-lab.py` imports the selected asset into Blender, places
the origin at the bottom centre, preserves base color and OpenGL normal maps,
and exports `content/models/<runtime model>.blend` and `.glb` with tangents.
This exporter deliberately uses scalar metallic/roughness factors, although the current native importer also supports embedded MR/AO maps:
the source metal mask separates enamel and exposed metal by polygon; the ORM
texture is removed from exported materials. AO is not baked into color. Glass
is exported opaque; the multimeter's clear gauge cover is omitted so that its
scale and needle remain visible with the current opaque renderer.
Unused, near-zero JPEG normal texels become neutral normals;
other short or nonpositive-Z vectors are normalized into the supported tangent hemisphere.
Unused secondary UV sets are removed; source materials address TEXCOORD_0.
The monitor's curved screen uses the original B-17 two-channel display graphic;
its housing, controls and normal map remain from the source model.
Original source files remain available here.

Use `blender --background --factory-startup --disable-autoexec --python scripts/export-polyhaven-lab.py -- --asset=retro_multimeter`.
The other source IDs are accepted by the same command.
