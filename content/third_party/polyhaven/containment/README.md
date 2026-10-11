# Containment surface sources

Downloaded 2026-10-09 from Poly Haven's official API/CDN; four 2048×2048 JPG
files verified against source size and MD5. Exact URLs, checksums and SHA256
are retained in `manifest.json`.

- [Blue Metal Plate](https://polyhaven.com/a/blue_metal_plate), Rob Tuytel,
  source coverage 2.5×2.5 m: painted sheet with scratches, scuffs and seams.
- [Green Metal Rust](https://polyhaven.com/a/green_metal_rust), source coverage
  1×1 m: scratched enamel, localized breached coating and rust streaks.

Both are CC0. Only diffuse and OpenGL tangent-space normals are used.
`scripts/export-containment-glb.py::scanned_metal` recolors the diffuse while
preserving photographic value variation and some oxide chroma. The source
files remain unchanged. These scanned source materials use scalar roughness/metallic factors:
paint is mostly dielectric; exposed steel uses the metal factor. No roughness,
metalness or AO source map is imported by this scanned-metal helper. IMG-01/03 subsequently add locally authored MR/AO maps to selected Guardian/ram materials; those are not downloaded Poly Haven channels.

The surfaces are used by this chapter's six equipment/lining assemblies and
the Guardian restraint/remaining shells. Guardian organic UVs remain unchanged.
Material UV coverage is measured in metres, including rolled panel edges.
