# RC-022p1 visual reference

RC-022p1 provides local, reproducible visual-review input for the later legacy-gamma versus hardware-sRGB decision. It does not change the production colour pipeline, and captures are not committed as cross-GPU golden images.

## Build and launch

In a Visual Studio Developer PowerShell:

```powershell
cmake --build --preset demo-debug
$env:RELAYCORE_ASSET_ROOT = (Resolve-Path assets)
& .\build\msvc-debug\src\relaygame\RelayCore.exe --config (Resolve-Path assets\config\rc022_capture.toml)
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

After RC-022c, repeat the same fresh-process procedure as `rc022_reference_scene_srgb.png`, `rc022_reference_scene_srgb_imgui.png`, and `rc022_reference_world_debug_srgb.png`.

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
