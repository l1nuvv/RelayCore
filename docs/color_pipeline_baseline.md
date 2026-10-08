# RC-022a/RC-022b colour-pipeline baseline

## Vulkan SDR/HDR audit — 2026-10-08

This section describes the current Vulkan runtime; the OpenGL sections below
are historical evidence. The reported HDR switch is the RelayCore menu setting.

### Root causes and correction

Ordinary startup selected `legacy_gamma`. Enabling menu HDR also selected
`hardware_srgb`, changing lighting arithmetic as well as enabling the FP16 tone
pass. Classic legacy lighting adds encoded colours: a white receiver with two
sRGB 0.5 lights produces green byte **210**, versus **175** with linear
accumulation, including the existing SDR shoulder/display grade. Ordinary
startup now selects `hardware_srgb`. Explicit `--color-pipeline legacy_gamma`
remains available; direct renderer/fixture defaults retain compatibility.

The starting working tree already replaced pre-fog/postfx
`saturate(lighting + emissive)` with an SDR shoulder. That clipping discarded
bright-surface distinctions. This audit retains that prior change and its
0.6/1/2 highlight-separation test rather than claiming it as new work.

The starting HDR shader used an ACES fit with a hidden 0.6 scale. Its toe
darkened shadows and midtones at exposure zero. HDR now uses the existing SDR
shoulder on exposed linear radiance: identity through 0.65, then
`0.65 + h / (1 + h / 0.35)`, with `h = x - 0.65`. Value and first derivative are
continuous; the upper branch approaches 1. Reusing the existing SDR response
and remaining 0.35 headroom avoids adding exposure/gamma/shadow compensation.
Exposure still multiplies radiance by `2^EV` before mapping.

SDR classic hardware-sRGB and MR also applied display postfx directly to linear
RGB. Contrast around 0.5 consequently used the wrong mid-grey; saturation/grade
and grain saw the wrong domain. These paths now encode before display postfx,
then decode for an SRGB attachment, which encodes once. Legacy MR writes the
encoded result directly to UNORM. Classic legacy retains encoded arithmetic.
Shared transfer/mapping functions are in `scene_postfx.slang`; UI is unchanged.

Independent low-signal tests failed before correction. GPU green bytes with
vignette/grain/scanlines disabled, saturation 1 and retained display grade:

| Input/path | Before | Corrected/reference |
| --- | ---: | ---: |
| HDR linear 0.002, EV 0, contrast 1 | 1 | 7 |
| HDR linear 0.04, EV 0, contrast 1 | 31 | 56 |
| HDR linear 0.18, EV 0, contrast 1 | 104 | 117 |
| HDR linear 0.5, EV 0, contrast 1 | 177 | 187 |
| SDR linear 0.04, contrast 1.2 | 0 | 42 |
| SDR linear 0.18, contrast 1.2 | 95 | 115 |

Separate unlit references check exact piecewise transfer at 0, 0.18, 0.5 and 1.
Paired classic/MR ambient tests cover near-black through radiance above 1 at
contrast 1/1.2; linear SDR and HDR agree within two bytes. Authored contrast can
still clip after mapping; that is distinct from pre-tone-map clipping.

### Actual path and formats

| Stage | Legacy classic | Hardware-sRGB / FP16 scene mode |
| --- | --- | --- |
| Source colours | Authored visual sRGB and encoded bitmap bytes [0,1] | Same sources; CPU decodes authored RGB, retaining linear tint/brightness scalars and values above 1 |
| Sampling | RGBA8 UNORM returns normalized encoded bytes; compatibility filtering in that domain | RGBA8 SRGB decodes RGB before filtering/mip generation; alpha linear. Normals use UNORM in both modes |
| Native vertices | Linear product colours encoded for compatibility by `StaticModelCache` | Linear product colours retained; native material factors applied linearly |
| Material/light/fog | Encoded classic arithmetic; MR explicitly decodes sampled colours/light/fog and shades linearly | Linear RGB with sRGB/BT.709 primaries, potentially above 1. Ambient/gain scalars are not sRGB-decoded |
| SDR mapping/target | Classic encoded shoulder/postfx, MR linear shoulder then display postfx; RGBA8 UNORM | Linear shoulder, sRGB display postfx, decode for SRGB view over mutable RGBA8 UNORM storage |
| FP16 scene/tone pass | Rejected with hardware-sRGB diagnostic | RGBA16F linear scene/transparency/viewmodel; exposure/shoulder then display postfx; linear output to SRGB attachment |
| UI | UNORM view over encoded scene, display-space straight-alpha composition | Same view/composition after mapping, unaffected by exposure/postfx |
| Readback/blit | Encoded RGBA8 bytes; UNORM source | Same, without another sRGB conversion |
| Swapchain | BGRA8/RGBA8 UNORM + `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` | Identical SDR contract, including menu HDR |
| OS/display | SDR desktop or SDR content converted by Advanced Color | Same; scene HDR does not imply native HDR presentation |

Owners: `authored_color.cpp`, `static_model.cpp`, `vulkan_resources.cpp`,
`vulkan_uploads.cpp`, `mesh.slang`, `tone_mapping.slang`,
`VulkanMeshRenderer::display_format/scene_format/record_ui`,
`VulkanSwapchain::recreate` and its UNORM blit. No confirmed double encoding,
incorrect colour-texture decode, UI encoding or swapchain/content mismatch was
found; texture filtering, clear, raw transfer and UI checks cover those boundaries.

Native HDR output is **absent**: no HDR surface negotiation, ST2084/PQ,
extended-linear scRGB swapchain, BT.2020 conversion, native display tone mapping,
peak-luminance policy or HDR metadata. Menu HDR maps scene radiance to SDR.
Windows HDR On would still be SDR through DWM. As described by
[Microsoft Advanced Color](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range),
DWM converts app colour spaces to linear scRGB composition and supplies SDR
white mapping. No duplicate app-side white compensation or assumed display
peak luminance was introduced.

### Verification and limitations

Changed files for this audit (pre-existing unrelated working-tree edits retained):

| Files | Purpose |
| --- | --- |
| `src/relaycore/app/startup_options.h` | Linear colour pipeline for normal executable startup |
| `assets/shaders/vulkan/{mesh,tone_mapping,scene_postfx}.slang` | Shared shoulder/transfer functions and consistent display-space postfx |
| `tests/{startup_options_tests,vulkan_mesh_tests}.cpp` | Default-mode, independent low-signal/raw-transfer and paired lighting regressions |
| `tests/graphics-baselines/power_restoration-{0,1}{,-shotgun}.rcgb` | Four reviewed MR-postfx reference updates |
| `tests/graphics-baselines/README.md`, `docs/DEVELOPMENT.md`, this document | Evidence, current SDR/HDR semantics and remaining display acceptance |

Windows, RTX 4060 Ti, GPU Vulkan API 1.4.329, driver 32.0.15.9649; existing
`msvc-debug` tree with core/synchronization validation ON. Local dxdiag identifies
two SDR displays and an HDR-capable MAG 273Q X24 in **WCG**, not HDR, with
`DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709` (`build/color-audit/dxdiag.txt`).
No Windows colour setting was changed.

| Windows HDR | Pipeline | Executed |
| --- | --- | --- |
| Off/current SDR-WCG desktop | legacy | Transfer, decode/filtering, highlights, UI, presentation and compatibility lighting |
| Off/current SDR-WCG desktop | hardware | Same plus paired classic/MR SDR/FP16 lighting, menu/reload and HDR episode |
| On | legacy/hardware | Not executed; controlled HDR-desktop acceptance remains open |
| Native HDR | any | Not implemented; no native luminance/capture claim |

Commands actually run from an x64 VS developer environment with
`VCPKG_ROOT=C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg`:

```powershell
cmake --build --preset graphics-debug --target relaycore_vulkan_mesh_tests relaycore_vulkan_presentation_tests --parallel 2
cmake --build --preset graphics-debug --target relaycore_vulkan_mesh_tests relaycore_game_lifecycle --parallel 2
cmake --build --preset graphics-debug --target relaycore_vulkan_mesh_tests relaycore_tests relaycore_demo --parallel 2
ctest --preset test-debug -R '^relaycore\.(startup options|authored visual sRGB|surface composition|linear surface composition)' --output-on-failure --no-tests=error --output-junit color-audit-unit.xml
ctest --preset graphics-debug -R '^graphics\.(vulkan_color_baseline|hdr_pipeline)$' --output-on-failure --no-tests=error --test-output-size-passed 25000 --output-junit color-audit-final-numeric.xml
ctest --preset graphics-debug -R '^graphics\.(vulkan_presentation|mesh_pipeline|game_hud|irradiance_probes|hdr_settings|hdr_episode)$' --output-on-failure --no-tests=error --test-output-size-passed 10000 --test-output-size-failed 6000 --output-junit color-audit-integration.xml
$env:RELAYCORE_ASSET_ROOT = (Resolve-Path 'build/msvc-debug/src/relaygame/assets').Path
./build/msvc-debug/tests/relaycore_vulkan_mesh_tests.exe --metallic-roughness
ctest --preset graphics-debug -R '^graphics\.(vulkan_color_baseline|hdr_pipeline|power_restoration_slice)$' --output-on-failure --no-tests=error --test-output-size-passed 25000 --output-junit color-audit-final-affected.xml
git diff --check
```

Unit selection: 12/12 passed. Final affected selection: 3/3 passed
(`color-audit-final-affected.xml`). Integration selection: 6/6 passed, zero skipped,
no unexpected Vulkan validation output. Analytical MR checks passed in both
pipelines. Builds passed; pre-existing C4834 warnings in
`pipeline_authoring_overlay_tests.cpp` remain. Sandbox blocked MSVC PDB IPC
(C1902) and OBJ canonical paths; relevant builds/desktop runs completed outside
it, with explicit user approval after automatic-review timeouts.

Only four `power_restoration-{0,1}{,-shotgun}.rcgb` samples were reviewed and
updated after numeric evidence and an original-SPIR-V control using identical
current assets. The control passes the old power samples; corrected shaders
change MR display postfx. Previously crushed near-black tiles become nonzero,
which the comparator calls a coverage change. Normal alpha/placement/culling
and pass gates remain. Old samples and shader evidence are under
`build/color-audit/{before,after}`. No other baseline or tolerance changed.

The full mesh test already failed on `pressure_transfer-0` before the audit
(mask delta 0.0881, mean RGB delta 57.64). With corrected shaders its RGB mismatch
remains (41.27); that authored-content baseline is outside this correction.
The repeated full-mesh result is in `color-audit-final.xml`; that earlier report
also contains a subsequently corrected unlit-fixture failure. Final numeric/power
results are the separate affected report above.
The full historical suite is not claimed green. Same-camera SDR captures in
`build/msvc-debug/tests/` were inspected: corrected HDR preserves door/wall
detail and closely matches hardware-SDR. Shadow totals are 725402 versus
725542. These are SDR render-target captures, not HDR luminance measurements.

For final physical-display acceptance, enable Windows HDR on the target monitor,
hold SDR content brightness fixed, rerun the colour/HDR/presentation selection,
and launch `RelayCore.exe --color-pipeline hardware_srgb`. In the same power-room
view switch SDR/HDR and inspect near-black door detail, wall/floor highlights,
saturation and unchanged menu brightness; repeat with explicit legacy mode.
Record active monitor, HDR state and SDR white setting. Interpret app readback
separately from Windows HDR screenshots. That Windows-HDR acceptance remains open.

## Historical OpenGL baseline

RC-022a records the current gamma-space behaviour; it does not migrate RelayCore to sRGB. In particular, it does not request an sRGB default framebuffer, enable `GL_FRAMEBUFFER_SRGB`, add shader conversions, or retune authored colours.

## Current contract

`Texture2D::initialize_rgba` requires an explicit `TextureEncoding`: `linear` selects `GL_RGBA8`, while `srgb` selects `GL_SRGB8_ALPHA8`; upload remains `GL_RGBA` with `GL_UNSIGNED_BYTE` and alpha is always linear. The blockout atlas explicitly selects `TextureEncoding::linear`, so the production texture remains `GL_RGBA8`. The two directional shadow maps are `GL_DEPTH_COMPONENT24` and are classified as **Depth**. There are no other production bitmap, UI, debug, normal, mask, or lookup texture loaders in the current tree. A future data texture belongs to **Data** and must not inherit a colour texture's decode policy.

The current scene shader samples raw normalized atlas bytes and multiplies them by vertex colours and lighting. It writes directly to the SDL/OpenGL default framebuffer. The renderer does not request a known sRGB-capable default framebuffer and does not enable `GL_FRAMEBUFFER_SRGB`. RC-022b is test preparation only, not a production hardware-sRGB migration.

## Authored surface-colour contract

`base_color` is an authored visual sRGB colour and must be finite with each channel in `[0,1]`. `tint` is a linear artistic channel multiplier, and `brightness` is a linear scalar multiplier; neither is an sRGB colour and neither is passed through an sRGB conversion. The existing authoring contract permits finite multiplier values above `1` and does not introduce a new upper bound here.

The active legacy path remains exactly:

```text
legacy_encoded = clamp(base_color_srgb * tint_multiplier * brightness, 0, 1)
```

The separately tested, future hardware-sRGB composition is deliberately different:

```text
hardware_linear = srgb_to_linear(base_color_srgb) * tint_multiplier * brightness
```

It does not convert a pre-multiplied gamma-space result, does not convert `tint` or `brightness`, and does not clamp its composed linear result before future lighting analysis. RC-022p2 only defines these contracts: production still calls the legacy helper. The current atlas/vertex double-colour stylization and shader mixing remain unchanged, so the linear formula may visibly differ during the RC-022p1 reference-level A/B review.

## Numeric reference

Build and run the local graphics integration executable from a Visual Studio Developer PowerShell:

```powershell
cmake --build --preset graphics-baseline-debug
ctest --test-dir build/msvc-debug --output-on-failure -R "^graphics\.color_pipeline_baseline$"
```

It creates a hidden SDL OpenGL 4.6 core window; it does not present to a monitor. It uses production `Texture2D` to upload a 5x1 `GL_RGBA8` grey ramp and renders a full-screen primitive to the default framebuffer before `glReadPixels`.

Stored byte values are `0, 63, 127, 191, 255`. Expected RGB readback values, with a one-byte tolerance for framebuffer quantization, are:

| Operation | Expected bytes |
|---|---|
| multiplier `1.0` | `0, 63, 127, 191, 255` |
| multiplier `0.5` | `0, 31, 63, 95, 127` |
| straight alpha source `(0.8, 0.2, 0.4, 0.5)` over destination `(64,128,192)/255` | `(134, 90, 147)` |

The half-light row is the intentional RC-022a record of raw stored-value multiplication: the stored value near `0.5` reads back near `0.25`, not a linear-light result. The blend uses RelayCore's current `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` contract. The executable prints GL version, vendor, renderer, default framebuffer RGBA bit depths, `GL_FRAMEBUFFER_SRGB` state, and default framebuffer colour encoding when the driver makes it queryable. It also explicitly reports that RelayCore did not request an sRGB-capable default framebuffer.

RC-022b additionally uses production `Texture2D` to upload a controlled `GL_SRGB8_ALPHA8` texture containing RGB bytes `0,127,255`, samples it with `texelFetch`, renders to a `GL_RGBA16F` FBO with `GL_FRAMEBUFFER_SRGB` disabled, and reads floats. It verifies standard piecewise sRGB decode (`127` is about `0.2122`) and verifies that alpha byte `128` remains about `128/255`.

A separate controlled `GL_SRGB8_ALPHA8` FBO receives shader outputs in linear RGB `0.0,0.18,0.5,1.0` with alpha `0.5`. With `GL_FRAMEBUFFER_SRGB` enabled its expected readback is approximately `0,118,188,255`; disabled, the raw stored values are approximately `0,46,128,255`. Both paths check alpha separately and the enabled path has a double-conversion guard. These controlled references do not change the SDL default framebuffer, renderer state, ImGui, shaders, or authored colours. Default-framebuffer/SDL/ImGui sRGB contracts are still not implemented, and RC-022 is not closed.

The CTest is labelled `integration;graphics`, `RUN_SERIAL`, and has `SKIP_RETURN_CODE 77`. It is intentionally excluded from the normal `test-debug` preset: Windows interactive, headless CI, and software-renderer capability have not yet been demonstrated for this project. SDL/video/context/GL-loader unavailability, a missing `GL_BACK` draw/read path, or fewer than 8 RGB bits prints `graphics_color_pipeline_baseline=skipped: ...` and returns `77`, which CTest reports as **Skipped**. Once the context is ready, shader, texture, GL-operation, state, or numeric failures print `...=failed: ...` and return `1`; only a completed measurement returns `0` and prints `...=passed`.

Before measurement the executable explicitly selects `GL_BACK` for draw/read, disables `GL_DITHER`, and disables `GL_FRAMEBUFFER_SRGB`. It checks every state operation for a GL error and requires framebuffer RGB bit depths of at least 8. Successful runs print `context_ready`, `measurement_started`, and `passed` markers.

## Manual shipped-scene capture

Use the unmodified demo and retain local captures as review artifacts, not committed cross-driver golden images.

1. In a Developer PowerShell, build `cmake --build --preset demo-debug` and run the resulting `build/msvc-debug/src/relaygame/RelayCore.exe` with `RELAYCORE_ASSET_ROOT` set to the absolute `assets` path if needed by the local launcher.
2. Use `assets/config/engine.toml`: `levels/test_room.toml`, 2560x1440, default render/shadow/fog settings. Start a fresh process for each capture.
3. Capture `rc022a_spawn_overview.png`: player spawn `[0,0,0]`, spawn angles `[0,0,0]`, diagnostics and HUD hidden.
4. Capture `rc022a_glass.png` only as a local review artifact. This project currently has no fixed glass camera preset; do not treat a noclip-derived position as a cross-run golden baseline.
5. Capture `rc022a_spawn_imgui.png`: return to the exact fresh spawn state and enable the diagnostics overlay.

The current project has no screenshot writer or fixed glass camera preset, so manual captures are documented review artifacts rather than a cross-run golden baseline. Do not add a new image dependency for RC-022a. Record the executable build, GPU/driver, desktop colour configuration, window pixel extent, config revision, camera position/orientation, and overlay state next to each image.

## Decision gate for RC-022b+

Proceed with staged hardware-sRGB work only after this baseline is reliable, colour/data semantics remain separable, a default-framebuffer capability/request/fallback contract is demonstrated, ImGui composition has an explicit state contract, before/after captures are reproducible, and authored retuning is bounded. Close RC-022 as an accepted visual limitation if the baseline shows that the stylized appearance is intentional and migration would require broad non-deterministic retuning or disproportionate UI/default-framebuffer changes.
