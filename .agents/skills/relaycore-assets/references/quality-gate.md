# Asset quality gate

Before finalizing, check only the items relevant to the asset.

## Project fit

- It has a specific purpose and location.
- It matches nearby naming, scale, texel/detail density, palette, and material language.
- It does not look like a detached “cool sci-fi” concept pasted into the project.

## AI-slop check

Remove or reduce:

- meaningless micro-detail;
- random glowing lines;
- fake typography/glyphs;
- duplicated motifs with no functional reason;
- uniform dirt/scratches;
- inconsistent perspective or impossible construction;
- noisy edges and accidental halos;
- overprocessed contrast/sharpening;
- embedded lighting that fights the engine lighting;
- accidental asymmetry/repetition artifacts from generation.

## Technical check

- Correct file type and dimensions for the current pipeline.
- Correct alpha behavior if present.
- Correct color-space/linear-vs-sRGB assumption for its use.
- Tiling is actually seamless when required.
- No unintended baked illumination or shadows.
- No new channels/maps are added unless the renderer consumes them.
- Referenced paths and authored data resolve correctly.

## In-engine check

When possible, inspect the asset under representative lighting and at normal gameplay distance. Check readability first, then seams/artifacts, then fine detail.
