# Player health HUD — optimized comparison, 2026-10-18

Owner: [VIS-43 health feedback](../tickets/VIS-43-46.md#health-hud-feedback--2026-10-18).
This measures the health presentation patch; it does not close H-008 or human visual acceptance.

## Reproduction and identity

Before: `f2ecce4983b1c0f5ab482b2291337017b8e61d5f` runtime/UI sources.
After: `38f0017` runtime/UI sources. Both binaries include the same diagnostic
`tests/containment_profile.cpp --health-feedback` workload added with this report.
No gameplay health, save format, renderer quality or dependencies were changed.

The existing `msvc-dev` tree was used for both incremental builds:
`cmake --build --preset demo-dev --target relaycore_containment_profile -j 2`.
MSVC 14.51.36231, RelWithDebInfo `/O2 /Ob1 /DNDEBUG`, symbols and Tracy 0.14.1;
IPO/unity/validation off. The build helper temporarily reverse-applied only the
seven production/UI paths from the HUD commit, built and retained `before.exe`,
restored the patch in `finally`, then built and retained `after.exe`.

Hardware: Ryzen 7 3700X, RTX 4060 Ti 8188 MiB, NVIDIA 610.88, 240 Hz display.
VSync off/immediate presentation, High/custom settings, FOV 87, scale 1, DRS off,
MSAA4, shadows 1536, mesh shaders and instanced scene submission, descriptor
indexing off. FP16 HDR scene with hardware-sRGB SDR output; bloom/SSAO/SSR/TAA/
volumetrics on, SSGI2/reflections2, DOF/motion blur off, sound on at volume 0.79.
Developer ImGui is disabled. Effective settings are retained in each workload log.

Evidence root: `build/health-hud-profile/`. It retains build logs, both executables,
the reversible production patch, common workload patch, engine/settings files,
`build-pair.ps1`, `run.ps1`, `export.py`, `analyze.py`, each capture and exported CSVs.
Each run records executable/config/settings/UI hashes and a complete staged asset
hash inventory. The runtime UI files are selected for that binary, then restored
to the current HUD after every run. Progress/settings use isolated directories.

Run arguments (identical for both variants):

```text
--settings-file E:/Code/C++/RelayCore/build/health-hud-profile/settings.toml
--config-file E:/Code/C++/RelayCore/build/health-hud-profile/engine.toml
--no-captures --scripted-input-only --health-feedback --passes 2
```

The fixture performs its existing observation/workshop/archive/return/retry,
pause and resize states, four seconds each, twice. It retains real scene rendering,
combat updates and sound. Only the gameplay HUD snapshot is replaced by a fixed
diagnostic card/reticle/objective: every two seconds, HP 100 until 0.6 s, HP 75 until
0.7 s, HP 50 until 0.8 s, then HP 20 until 2 s; the cycle repeats with restoration to 100.
This exercises rapid hits, flash, main/ghost drain and resting UI without changing
simulation health. Enemy/world marker presentation is outside this isolated HUD test.

## Measurement boundaries

Runs are serialized before-1 → after-1 → after-2 → before-2; no builds or CSV exports
overlap the timed processes. The first 0.5 s of every state, session preparation, pause and
resize are excluded from gameplay results. The first pass renders at1280×720; the
fixture's resize state leaves 1920×1080 for pass 2. Results must be separated by the
per-frame `containment_width/height` plots, not pooled across these resolutions.

The `health_feedback_active` plot selects 0.6–1.4 s of each cycle; the remaining
settled samples are idle/restoration. CPU results are elapsed scope durations,
including scheduler delays; nested scopes are not added. `render_ui` includes the
snapshot, animation advance, RmlUi update/render and UI preparation. The narrower
`RmlUi update/render` scope excludes snapshot preparation and animation advance.
GPU `RmlUi` is the existing timestamp interval around that draw pass, independent
of scene rendering. GPU intervals are mapped by start time to the CPU frame's
state/resolution and reported with their sample coverage.

Geometry uploads count `RmlUi geometry upload` calls and their elapsed CPU time.
There is no direct layout-invalidation or upload-byte counter here; context-update
time and geometry-call counts are the measured evidence, not inferred zero work.
New process starts reuse OS/driver caches; these are not cold-cache measurements.
Desktop activity and hardware clocks are not globally controlled. Tracy remains
enabled in both variants, so unprofiled player FPS is not established.

## Results

All four workloads and captures exit 0; all 56 prepared states complete, including
the existing hit/damage/death/respawn assertions. No runtime errors or validation
messages occur (validation is disabled for profiling). All 231 asset hashes match
except the intended `ui/hud.rml` and `ui/player.rcss`; config/settings hashes and
each variant's executable hashes remain identical across repetitions.

Before executable SHA-256: `FF578DBE07BC5E897E908AF47365193204DAC4239881CEB355B5827C4A181E25`.
After executable SHA-256: `2D5BC8A1226DE0627871BBD9AAAB5F56E59163A818D6FF71E8630517BD6084F7`.

At 1920×1080, each included CPU frame has a corresponding GPU RmlUi interval
(6296 frames/6296 intervals across the four runs). At 1280×720 there are 12057 CPU
frames and 12060 GPU intervals: three intervals cross the selection boundaries
when assigned by GPU start time. No missing capture block is treated as faster
GPU execution. The selected `render_ui` scopes contain no `Vulkan scene fence`
intervals; subtracting that measured wait leaves the CPU results unchanged.

### 1920×1080

Times are milliseconds; CPU UI is the complete `render_ui` scope, GPU UI is the
`RmlUi` timestamp interval. Wall distributions use consecutive frame starts.

| Run | Frames | Wall mean / p50 / p95 / p99 / max | CPU UI mean | GPU UI mean |
|---|---:|---|---:|---:|
| before-1 | 1593 | 10.981 / 10.226 / 16.215 / 18.976 / 22.771 | 0.329524 | 0.008836 |
| after-1 | 1570 | 11.157 / 10.385 / 17.002 / 18.398 / 23.338 | 0.360978 | 0.009073 |
| after-2 | 1619 | 10.810 / 10.144 / 15.817 / 16.731 / 18.703 | 0.343973 | 0.009501 |
| before-2 | 1514 | 11.557 / 10.950 / 16.794 / 17.904 / 24.775 | 0.338092 | 0.009388 |

### 1280×720

| Run | Frames | Wall mean / p50 / p95 / p99 / max | CPU UI mean | GPU UI mean |
|---|---:|---|---:|---:|
| before-1 | 3074 | 5.692 / 5.313 / 8.769 / 9.565 / 13.329 | 0.276791 | 0.007434 |
| after-1 | 3137 | 5.579 / 5.130 / 8.766 / 9.981 / 11.706 | 0.328233 | 0.007557 |
| after-2 | 3001 | 5.831 / 5.395 / 8.949 / 9.768 / 17.108 | 0.311227 | 0.007692 |
| before-2 | 2845 | 6.152 / 5.721 / 9.743 / 10.666 / 17.373 | 0.316027 | 0.007480 |

### Isolated UI cost and geometry churn

Below, each variant is the arithmetic mean of its two run means, giving each
repetition equal weight. These are not pooled percentiles. Nested CPU rows are
components/evidence, not values to add to the total UI scope.

| Metric, ms except upload counts | 1280 before → after | 1920 before → after |
|---|---|---|
| Complete CPU UI | 0.296409 → 0.319730 | 0.333808 → 0.352476 |
| CPU UI during active feedback | 0.291832 → 0.329009 | 0.339542 → 0.368018 |
| CPU UI during idle/restoration | 0.300320 → 0.311914 | 0.328965 → 0.339296 |
| HUD snapshot / setter | 0.053821 → 0.058869 | 0.059762 → 0.064245 |
| RmlUi update/render | 0.189058 → 0.197274 | 0.214673 → 0.222385 |
| RmlUi context update | 0.138267 → 0.138968 | 0.157273 → 0.157347 |
| RmlUi context render | 0.049345 → 0.056929 | 0.055691 → 0.063409 |
| Geometry upload CPU / frame | 0.000227 → 0.006083 | 0.000485 → 0.007060 |
| Geometry upload calls / frame | 0.016920 → 0.539906 | 0.032206 → 0.557577 |
| Upload calls / active frame | 0.029430 → 1.166350 | 0.056383 → 1.187151 |
| GPU UI interval | 0.007457 → 0.007625 | 0.009112 → 0.009287 |

During active feedback, extra geometry uploads account for about 0.013 ms CPU/frame
at 1280 and 0.014 ms at 1920, inside the measured render scope. After settling at HP 20
(inactive plot and target 20, excluding restoration), **zero geometry uploads**
occur in all 3171 after-patch frames across the two sizes/repetitions; the 3082
corresponding baseline frames also have zero. The idle/restoration row includes
the deliberate heal/reset at the start of each cycle, which explains its nonzero
upload average. This does not measure layout-invalidation counts or heap allocations.

### Conclusion and limits

The complete UI costs an additional 0.023 ms CPU/frame at 1280 and 0.019 ms at 1920 on
these repeated workloads. Active feedback adds 0.037 ms and 0.028 ms respectively;
idle/restoration adds 0.012 ms and 0.010 ms. The added geometry work stops after settling.
GPU UI remains around 0.0076 ms/0.0093 ms; its mean difference is below 0.001 ms and
too small relative to timestamp granularity/outliers to make a strong GPU-cost claim.
At 1920, three of four GPU UI medians and all GPU UI p95 values are 0.009216 ms.

No consistent total-frame regression appears above repeat variation: 1920 run
means span 10.981–11.557 ms before and 10.810–11.157 ms after; 1280 spans 5.692–6.152 ms
before and 5.579–5.831 ms after. All 18353 selected gameplay frames remain below 25 ms.
Scene-GPU run means also vary; the apparently faster aggregate after-results are
not evidence that the HUD improves FPS. Two repetitions per variant and this
hardware/scenario do not prove zero overhead or non-regression on every system.

The requested optimized CPU/GPU comparison is complete. The evidence supports a
small measured CPU cost, bounded UI geometry activity and no demonstrated stable
whole-frame slowdown here. Sound-on human combat readability/visual acceptance
remains a separate open requirement; this synthetic presentation sequence is not
a continuous player route and does not close H-008 or VIS-43–46.

Validation for this profiling change: both targeted optimized builds, all four
full fixture executions/captures, asset/config/variant identity comparisons,
clang-format verification and `git diff --check`. A final incremental
`cmake --build --preset demo-dev --target relaycore_demo -j 2` also passes, keeping
the ordinary executable compatible with the current staged UI assets after the
profile builds. Runtime HUD source content was restored exactly; only the opt-in
diagnostic workload and documentation remain changed.
