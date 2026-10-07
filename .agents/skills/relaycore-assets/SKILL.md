---
name: relaycore-assets
description: Produce or diagnose RelayCore assets through source export, native compilation, receipts, staging and runtime loading. Use for GLB/rcmodel, collision/probe products, textures/audio/UI asset delivery or pipeline changes; not for art direction alone, gameplay-only TOML edits or unrelated C++ fixes.
---

# RelayCore asset production

Paths below are relative to the repository root. For visual decisions use `relaycore-art-direction`; this workflow owns technical delivery.

For external texture/model discovery and source selection, follow `docs/ASSET_SOURCES.md`. Inspect the actual downloaded mesh/maps and adapt them to the supported native contract; a source site or game-ready label does not establish compatibility.

## Find the owning pipeline

- Native models: read the static-model section of `docs/DEVELOPMENT.md`, current importer diagnostics and `cmake/RelayCoreModelAssets.cmake`. The importer supports a deliberate subset of glTF; a valid GLB is not necessarily a supported RelayCore model.
- Collision: read `docs/COLLISION_ASSETS.md` and `cmake/RelayCoreCollisionAssets.cmake`. Collision is authored separately and explicitly paired with a model, not inferred from render triangles.
- Irradiance: inspect `cmake/RelayCoreIrradianceAssets.cmake` and the affected bake inputs. Use current fingerprint/staleness checks before reusing products; unchanged-looking files or old reports are insufficient evidence.
- Other assets: inspect the adjacent asset README, generator and runtime consumer for dimensions, units, channels, color space, alpha, tiling and naming. Run only the generator needed by the request; some exporters regenerate multiple assets.

## Compile and validate

Trace the changed source through its declaration, compiler/product, receipt or fingerprint, staged path and runtime reference. Each runtime path has one authored or generated owner. Do not put generated native products into source `assets/` or repair stale state by editing receipts/manifests.

Use existing CMake targets and `relaycore-verify` to select checks. Model receipts are checked by `scripts/check-model-assets.ps1`; collision pairing/staleness by `cmake/check_collision_assets.cmake`. Follow their current parameters. Isolated physics fixtures avoid shader/irradiance work; demo staging may trigger expensive bakes. Serialize builds sharing an output tree and avoid rebuilding locked runtime DLLs.

CTest supplies asset roots. For manual renderer checks use a complete staged runtime root, including compiled SPIR-V and native products, rather than source `assets/`. Rebuild/stage before F7; editor saves are user copies, not source-map edits.

Check the affected product/loader and map or representative rendered view. For images also check seams, alpha edges, consumed channels and unintended baked lighting. A plausible standalone image or successful export is not runtime validation. Report the source/product/staged paths, checks performed and any remaining visual/runtime gap. Stop once the affected path is verified; packaging requires a packaging/release request.
