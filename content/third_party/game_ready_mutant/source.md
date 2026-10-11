# C-19 Guardian source

GAME-READY STUDIOS, **Free Biomechanical Mutant**:
https://sketchfab.com/3d-models/free-biomechanical-mutant-bdb9c9cfcf9c4a5eafec9f58b170bf98

The source page identifies the asset as CC Attribution. Author download folder:
https://drive.google.com/drive/u/2/folders/10vdYMgIRDCtMe0KhKYs1uCniYdIRzZK4
Mesh archive: `1kzsbZg1-38UB5H6RnU19ySFOGflAZLcb`; Unity texture package:
`15U1HhNHX6F9rX2UsewUqXBSlhMnmfvig`. Only mesh and image data were extracted;
package scripts were not executed. Original archives remain in `build/g019`.

Preserved here: source FBX and four Skin_1 Bloody Top/Bottom albedo/normal maps.
The sculpted, elongated anatomy and authored textures replace the rejected
procedural primitive creature. User references in `content/references/g019`
establish exposed bloody anatomy, industrial restraint and mechanical limbs;
The earlier 50–60% mechanical visual target was revised by the author on
2026-10-09: the entire strike arm is exposed bloody anatomy, with approximately
half the back uncovered. The character must read as an organism in a restraint.

`scripts/export-guardian-glb.py` poses the source rig offline, scales the body to
3.55 metres, separates the organic strike arm at its runtime shoulder pivot and adds fitted cast
leg/shoulder/forearm/back panels, bearings, return pistons and the C-19 chest
restraint. Strike-arm shells and its mechanical sleeve are removed; the dorsal
carapace covers only the left half. The slender first silhouette and original
organic albedo are retained. Remaining mechanical surfaces use the scanned
materials described in [containment sources](../polyhaven/containment/README.md).
The percentage is an art target, not a measured mesh-area statistic.

Runtime receives static opaque GLBs with normal maps and metallic/roughness factors; IMG-01/03 additionally author embedded MR/AO maps for selected materials. The source albedo/normal attribution above remains unchanged.
The source skeleton/animations are not exported. Runtime uses rigid body/arm
transforms and capture compression; there is no skeletal walking animation.
Images are reduced to 2048 pixels. Normal vectors are normalized into the
positive tangent hemisphere required by the native importer (47 corrected Top
seam texels, zero Bottom). Original 4096-pixel images are unchanged.
Metal panels receive projected UVs; unused tangent attributes are omitted on
materials without normal maps. The authored organic UVs and normal-map basis
are preserved. Editable assembly sources and exports are in `content/models`.
