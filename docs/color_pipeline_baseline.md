# RC-022a/RC-022b colour-pipeline baseline

RC-022a records the current gamma-space behaviour; it does not migrate RelayCore to sRGB. In particular, it does not request an sRGB default framebuffer, enable `GL_FRAMEBUFFER_SRGB`, add shader conversions, or retune authored colours.

## Current contract

`Texture2D::initialize_rgba` requires an explicit `TextureEncoding`: `linear` selects `GL_RGBA8`, while `srgb` selects `GL_SRGB8_ALPHA8`; upload remains `GL_RGBA` with `GL_UNSIGNED_BYTE` and alpha is always linear. The blockout atlas explicitly selects `TextureEncoding::linear`, so the production texture remains `GL_RGBA8`. The two directional shadow maps are `GL_DEPTH_COMPONENT24` and are classified as **Depth**. There are no other production bitmap, UI, debug, normal, mask, or lookup texture loaders in the current tree. A future data texture belongs to **Data** and must not inherit a colour texture's decode policy.

The current scene shader samples raw normalized atlas bytes and multiplies them by vertex colours and lighting. It writes directly to the SDL/OpenGL default framebuffer. The renderer does not request a known sRGB-capable default framebuffer and does not enable `GL_FRAMEBUFFER_SRGB`. RC-022b is test preparation only, not a production hardware-sRGB migration.

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
