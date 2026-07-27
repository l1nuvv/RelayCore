# RC-022p1 visual reference

RC-022p1 provides local, reproducible visual-review input for the later legacy-gamma versus hardware-sRGB decision. It does not change the production colour pipeline, and captures are not committed as cross-GPU golden images.

## Build and launch

In a Visual Studio Developer PowerShell:

```powershell
cmake --build --preset demo-debug
$env:RELAYCORE_ASSET_ROOT = (Resolve-Path assets)
& .\build\msvc-debug\src\relaygame\RelayCore.exe --config (Resolve-Path assets\config\rc022_capture.toml) --color-pipeline legacy_gamma
& .\build\msvc-debug\src\relaygame\RelayCore.exe --config (Resolve-Path assets\config\rc022_capture.toml) --color-pipeline hardware_srgb
& .\build\msvc-debug\src\relaygame\RelayCore.exe --config (Resolve-Path assets\config\rc022_capture_debug.toml) --color-pipeline legacy_gamma
& .\build\msvc-debug\src\relaygame\RelayCore.exe --config (Resolve-Path assets\config\rc022_capture_debug.toml) --color-pipeline hardware_srgb
```

Both configs load `levels/rc022_reference.toml` in the current windowed-only window contract at 2560x1440. The fixed `player_spawn` is `[0,0,0]`; authored angles `[0,0,0]` resolve to yaw `+90°` / `1.570796 rad` and pitch `0`, facing `+Z`. Audio and time-varying post effects are disabled. No player input, interaction, noclip, snapshot, or moving gameplay state is required.

`rc022_capture.toml` is the primary reference: `shadow_debug_cascades = false`. `rc022_capture_debug.toml` is identical except for `shadow_debug_cascades = true`, and exists only for the separate world-debug capture.

## Capture states

Start a fresh process for every image and press `Escape` after capture.

| Capture | Config | UI state | Purpose |
| --- | --- | --- | --- |
| `rc022_reference_scene_legacy.png` | `rc022_capture.toml` | Press `F2` once to hide diagnostics; fresh HUD is inactive | Primary scene review without cascade tint |
| `rc022_reference_scene_legacy_imgui.png` | `rc022_capture.toml` | Leave default diagnostics visible; fresh HUD remains inactive | Same scene with ImGui |
| `rc022_reference_world_debug_legacy.png` | `rc022_capture_debug.toml` | F2 is optional | World-rendered cascade debug RGB only |

For each launch, use the same `rc022_reference.toml` level and close its own process with `Escape` after capture. The two pipeline modes are the only A/B variable; captures remain local review artifacts, not cross-GPU golden images.

The cascade-debug capture is not a neutral, lighting, glass, or fog calibration frame. It intentionally tints scene rendering and must never be used to compare the primary colour probes.

## Probe areas

| Probe | Location in the fixed +Z view | Purpose |
| --- | --- | --- |
| representative material palette | seven full-height panels at Z=16, across X=-7 to +7 | General atlas/material response; the materials differ, so this is not an isolated base-colour test |
| controlled tint 0.5/1.0/>1 | upper row at Z=12, X=-4/-2/0; `WallPanel`, `brightness=1.0` | Legacy channel-multiplier behaviour: `[0.50,0.50,0.50]`, `[1.00,1.00,1.00]`, `[1.16,0.86,0.62]` |
| controlled brightness 0.5/1.0/>1 | lower row at Z=12, X=-4/-2/0; `WallPanel`, `tint=[1,1,1]` | Scalar multiplier behaviour: `0.50`, `1.00`, `1.20` |
| neutral lit panel | central palette panels around X=-2 to +2, Z=16 | Ambient plus directional lighting |
| local-light region | floor and ceiling at the foreground blue/orange pools | Local-light colour and intensity |
| emissive glass | glass at X=-5.7, Z=5 | Static glass emissive contribution |
| glass over split background | behind the same glass at X=-6.325/-5.075, Z=6.4 | Straight-alpha blending |
| distant panels | central panels at Z=20/27/33 | Fog mix |
| separate debug capture | any cascade-tinted surface under `rc022_capture_debug.toml` | World debug RGB |

The tint strip holds material, dimensions, orientation, distance, `brightness`, and row lighting constant. Its `>1` tint is copied from `assets/levels/test_room.toml` (`[1.16, 0.86, 0.62]`). The brightness strip holds `WallPanel` and `tint=[1,1,1]` constant. Both are away from the glass and outside the local-light radii.

The strips record the existing CPU-side contract `base_color * tint * brightness -> clamp`; they neither choose a future linear tint formula nor change authoring semantics. RC-022a/b remain the only required numeric GL fixtures; all probes here are visual review only.

## Metadata sidecar

Create a local `.txt` or `.md` sidecar next to every capture. Record: commit SHA; executable/config/level revisions; colour-pipeline mode; window and framebuffer extents; GPU; driver; OS; monitor colour and HDR state; camera position; yaw/pitch; HUD and ImGui state; capture tool; and timestamp.

## RC-022d local A/B review

Review date: 2026-07-27.

The review used fresh `legacy_gamma` and `hardware_srgb` processes with the same
`assets/config/rc022_capture.toml` reference scene, fixed camera, 2560x1440 drawable extent, ImGui diagnostics
visible, and the HUD in the same inactive state. The captures remain local review artifacts rather than
cross-GPU golden images.

### Runtime evidence

The hardware run completed startup and shutdown without GL debug errors. The Windows SDL/WGL path reported:

```text
SDL reported capability=1
attachment encoding query=GL_LINEAR
behavioral disabled RGBA=127,127,127,255
behavioral enabled RGBA=188,188,188,255
verification=passed
```

This confirms Outcome B: the attachment encoding query is unreliable on this path, while controlled
draw/readback proves that `GL_FRAMEBUFFER_SRGB` performs the expected hardware linear-to-sRGB conversion.

### Visual result

- No obvious double encoding was observed.
- ImGui diagnostics and the HUD retained the expected encoded-space appearance across both modes.
- Compared with `legacy_gamma`, `hardware_srgb` lifts dark ceiling, floor, and wall values.
- Blue and orange lighting gradients cover a broader area and appear smoother.
- Neutral material separation and local shadow contrast are reduced.
- The bright right-side column and blue transparent surface become more visually dominant.
- These differences are consistent with existing lighting and material values having been tuned for the
  legacy gamma-space pipeline; they are not evidence that the hardware-sRGB conversion is malfunctioning.

### Decision

`legacy_gamma` remains the production default. `hardware_srgb` remains an explicit startup opt-in for
validation and future retuning. The pipeline mathematics, behavioral framebuffer verification, texture
encoding contract, and ImGui state boundary remain unchanged.

Any visual matching work belongs to RC-022e and must retune authored lighting/material values rather than
reintroducing gamma-space operations into the hardware path.
