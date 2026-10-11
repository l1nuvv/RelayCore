# RelayCore — Vulkan 1.4 Migration and Advanced Renderer Modernization Plan

**Status:** VK14-00/01/02/03 are committed separately; VK14-04's four execution points are complete. Their covered local technical gates passed on both then-supported API profiles, with measured scheduling benefit and unchanged image parity. The owner requested Vulkan 1.4 as the sole runtime baseline on 2026-10-12; 1.3 compatibility gates are retired. Other-GPU and human acceptance remain separate; the historical presentation discrepancy is not explained by the passing checks.
**Prepared:** 2026-10-11
**Repository:** [`l1nuvv/RelayCore`](https://github.com/l1nuvv/RelayCore), branch `dev`
**Verified source baseline:** `c9b4dcb619def034f81eca0cca5604bb5e63709c` (2026-10-10 21:44:15 UTC), checked through the connected GitHub repository.
**Audience:** project owner and coding agents (including Codex).
**Scope:** require **Vulkan 1.4 for the loader and selected GPU**, with no Vulkan 1.3 fallback or forced-1.3 mode; improve Vulkan correctness, GPU scheduling/resource management, the renderer, and the asset pipeline; implement the advanced rendering features listed here in controlled, verifiable slices.

> **Important:** Execution evidence for VK14-00 through VK14-04 is recorded below; its 1.3/forced-profile results are historical, not requirements for future work. Later features remain planned. Existing project plans, current source, tests and runtime traces take priority over historical assumptions. The owner's Vulkan 1.4 modernization objective does not cancel the accepted gameplay/art direction or automatically authorize a single giant implementation pass.

---

## Current local delta — 2026-10-11

Local HEAD matches the supplied baseline `c9b4dcb619def034f81eca0cca5604bb5e63709c`, but the working tree contains uncommitted changes. Product `VERSION` and manifest are **1.4.1**; this product version is independent of the Vulkan API version. Source still requires Vulkan 1.3; VK14-01 now negotiates preferred 1.4. Targeted source inspection confirms synchronous mesh/upload consumption, per-material pools, pipelines without a cache and SDR swapchain formats. These findings do not establish a performance regression.

G-019 is **done by explicit owner acceptance on 2026-10-10**. The subsequent environment pass has recorded technical evidence; final environment review, independent duration/sound-on playtest, individual VIS/IMG acceptance and other-GPU coverage retain their own open gates. Historical `in_progress` entries are not current status. See [owning roadmap](PLAYABLE_SLICE_ROADMAP.md).

The initial documentation update did not build or run the game. Subsequent owner-authorized VK14-00/01 builds, captures, working budgets and verification limits are recorded below; they supersede that initial evidence gap without approving a weaker-hardware target.

## VK14-00 execution — 2026-10-11

Technical baseline captured before any API-negotiation or scheduling change. Owner renewed acceptance of the working product and authorized sequential development; independent-player duration and other-hardware evidence remain separate. Runtime source is HEAD `c9b4dcb619def034f81eca0cca5604bb5e63709c` plus the pre-existing documentation/version delta and the new profiling fixture. `build/vk14-00/source-delta.patch`, `containment_profile.cpp`, `provenance.json` and `assets.json` retain the tracked delta, fixture, executable/config hashes and every staged asset hash. No release/package was created.

**Scope/design:** add only `relaycore_containment_profile` to the existing test build, using ordinary `Application`, `DemoGame`, isolated disk Continue and sound. Two four-second passes cover observation camera sweep, workshop firing, archive shooter, powered apparatus return, death/respawn, pause and resize. Positions and equipment/shooter states are seeded; this is not continuous grounded traversal or an independent playthrough. Readback frames and the first 0.5 seconds of each state are excluded from warm statistics. Wall-clock frame deltas are measured independently of the gameplay timer's 100 ms cap. Loading and first frames are separate. Resize mixes 1280×720 and 1920×1080 and is excluded from fixed-resolution comparisons. Rollback is removal of the fixture/target; renderer/gameplay/assets were not changed.

**Build/hardware:** Windows 11, MSVC 14.51.36231, `msvc-relwithdebinfo` (Tracy on, validation off), RTX 4060 Ti 8188 MiB, NVIDIA 596.49, physical API 1.4.329. Vulkan headers/loader 1.4.357, vk-bootstrap 1.4.357, VMA 3.4.0, Slang 2026.18.2, Tracy client/capture 0.14.1; current shaders remain SPIR-V 1.5. Runtime instance/device-use/VMA contract remains 1.3. Graphics/present queue family 0, required dynamic rendering/sync2/timeline/swapchain-maintenance enabled, anisotropy 16. No dependency versions changed.

**Working profile, not an approved minimum hardware target:** 1920×1080, VSync off, internal scene HDR/exposure 0 with SDR hardware-sRGB presentation, FOV 70°, 4× MSAA, existing shadows, sound on at volume 1. Provisional steady-frame p99 budget 16.67 ms (60 FPS); GPU Scene 12 ms, world 8 ms, shadows 2 ms, viewmodel 1 ms, UI 0.5 ms, copy/presentation GPU 0.5 ms. These nested pass ceilings cannot be summed. Working device-memory ceiling 4096 MiB; exact application/VMA usage and weaker-GPU budgets remain unverified. One-hertz `nvidia-smi` observations show **907–2482 MiB system-wide GPU usage**, including other processes, not application-only allocations. OS/driver caches were not cleared; this is not a cold-cache run.

`build/vk14-00/final.tracy`, `final-workload.log`, `analysis.json`, exported CPU/GPU CSVs and `gpu-memory.csv` retain the final evidence. 10,585 warm executed CPU frames, including 1,358 resize frames; 9,227 fixed-resolution frames. All times below are ms; warm CPU intervals use Tracy `Application::run_frame`, with source wall-clock frame distributions in the workload log.

| Pass / state | Loading | First CPU frame | Warm p50 / p95 / p99 | GPU Scene p99 | Scene fences p99 |
|---|---:|---:|---|---:|---:|
| 0 / observation | 3313.027 | 338.424 | 5.063 / 5.506 / 5.987 | 3.065 | 3.468 |
| 0 / workshop | 854.231 | 12.287 | 4.040 / 4.391 / 4.829 | 2.301 | 2.594 |
| 0 / archive | 857.735 | 5.772 | 3.995 / 4.353 / 4.559 | 2.137 | 2.436 |
| 0 / return | 850.406 | 6.241 | 4.642 / 5.054 / 5.293 | 2.873 | 3.129 |
| 0 / retry | 862.392 | 5.678 | 4.476 / 4.930 / 5.441 | 2.656 | 2.910 |
| 0 / pause | 879.959 | 5.716 | 5.224 / 5.608 / 5.914 | 3.049 | 3.450 |
| 1 / observation | 849.300 | 9.299 | 5.067 / 5.406 / 5.653 | 3.064 | 3.360 |
| 1 / workshop | 849.726 | 10.842 | 4.032 / 4.378 / 4.647 | 2.303 | 2.574 |
| 1 / archive | 860.975 | 5.399 | 3.986 / 4.325 / 4.657 | 2.139 | 2.429 |
| 1 / return | 867.871 | 6.978 | 4.615 / 4.968 / 5.186 | 2.860 | 3.061 |
| 1 / retry | 850.442 | 5.651 | 4.447 / 4.855 / 5.478 | 2.657 | 2.917 |
| 1 / pause | 870.047 | 5.524 | 5.166 / 5.510 / 5.782 | 3.049 | 3.432 |

**Measured findings:** steady p99 fits the working frame budget on this GPU. Scene host fences account for 56–63% of executed warm CPU-frame time; this overlaps GPU execution, not extra independent GPU work. GPU p99 ranges: world 1.066–1.875, shadows 0.723–0.927, viewmodel 0.135–0.163, RmlUi 0.010–0.030, tone mapping 0.050, presentation copy 0.022–0.023. Removing waits is deferred to VK14-03/04 lifetime work, with no speedup promised.

The retry state has repeatable isolated CPU spikes (51.56 / 47.14 ms wall clock). In the worst frame, `app_update` takes 46.60 ms, world rendering 4.29 ms, UI 0.48 ms; nested scene fences total 2.89 ms. The actual respawn operation is earlier and takes 0.010 ms, and no slow AudioSystem call lies inside the spike. Root cause remains unresolved; do not label it a renderer wait or audio decode regression. Initial observation's 338 ms first frame is also separate first-use evidence. Two warm texture-upload/wait events occur on first-pass workshop/retry; no warm mesh uploads or second-pass upload events. Upload/font/resource preparation needs its own nested analysis before attributing a hitch.

Seven matching captures were inspected in `build/vk14-00/containment-profile/` (overview `views.jpg`): scene, enemies, powered apparatus, HUD, death UI and pause UI render; this is technical visual evidence, not new subjective art approval. The baseline does not settle continuous-route performance, minimize/restore, hot shader/material reload, physical HDR, exact VMA memory, weaker GPU or native 1.3-only coverage.

**Commands:** Visual Studio x64 developer environment; existing installed manifests reused via `cmake --preset msvc-relwithdebinfo -DVCPKG_MANIFEST_INSTALL=OFF`; `cmake --build --preset demo-relwithdebinfo --target relaycore_containment_profile -j 2`; staged absolute `RELAYCORE_ASSET_ROOT`; execute `build/msvc-relwithdebinfo/tests/relaycore_containment_profile.exe` from `build/vk14-00`; Tracy capture `-o final.tracy -s 100`; `python build/vk14-00/analyze.py`. Final workload exits 0 and checks firing hits, shooter damage, disk Continue, pause entry and death/respawn. Debug validation regression results are recorded below when complete.

**VK14-01 capability/acceptance contract:** retain minimum 1.3 and current GPU selection/required extensions/features; request up to 1.4 from the loader, effective API is the minimum of requested instance and selected physical API. Forced-1.3 limits instance/effective/VMA API to 1.3 without disabling independent extensions. Introduce only consumed API diagnostics; no unused 1.4 feature enablement, shader change or effect. Re-run device allocation/timeline, presentation/resize, G-019 SDR/internal HDR, RmlUi and lifecycle on preferred and forced profiles; genuine 1.3-only hardware remains unavailable. Preserve the baseline capture before implementation and compare current render output through the existing fixtures.

Debug baseline: `cmake --preset msvc-debug -DVCPKG_MANIFEST_INSTALL=OFF`; targeted build of `relaycore_containment_loop_graphics`, `relaycore_vulkan_device_tests`, `relaycore_vulkan_presentation_tests`, `relaycore_player_ui_graphics`, `relaycore_game_lifecycle`; `ctest --preset graphics-debug -R '^graphics\.(containment_loop(_hdr)?|vulkan_device|vulkan_presentation|player_ui|game_lifecycle)$' --output-on-failure --no-tests=error --output-junit vk14-00-results.xml`: **5 passed / 1 failed / 0 skipped**. G-019 SDR/internal HDR, RmlUi, device and lifecycle pass with core/synchronization validation and no warning/error. Presentation fails repeatedly **before VK14-01** on its second cycle's initial IMMEDIATE clear, expected red `255`, observed white `16777215`. Added phase/exposure diagnostics prove the client window is exposed at `(960,540)`; do not attribute this to occlusion without further evidence. FIFO and first-cycle resize/minimize/restore/hide/show complete before the failure. Retained reruns: `vk14-00-presentation-retry.xml`, `vk14-00-presentation-diagnostic.xml` in `build/msvc-debug`. Baseline is evidence of this open defect, not a green presentation gate. Negotiation can proceed independently, retaining this known baseline failure and its coverage limitation.

## VK14-01 execution — 2026-10-11

Implementation complete; **full milestone acceptance remains open** for the baseline screen-pixel failure and unavailable native 1.3-only hardware. Selected-device preference/required features/extensions are unchanged. Loader ≥1.4 requests instance 1.4; a suitable physical device limited to 1.3 selects effective 1.3. `RELAYCORE_VULKAN_FORCE_1_3=1` caps instance/effective/VMA to 1.3; absent/`0` prefers 1.4, invalid values fail with an actionable diagnostic. Existing Vulkan 1.2/1.3 feature chains and SPIR-V 1.5 remain; no 1.4-only command, optional feature consumer, scheduling change, dependency or content-format change was added. No performance improvement is claimed.

Changed implementation: `vulkan_device.h/.cpp` API capability fields/selection/VMA and loader-instance-physical-effective/driver/queue/extension diagnostics; `renderer.cpp` reports actual effective API; `vulkan_device_tests.cpp` checks negotiation bounds, timeline/allocation/lifetime and invalid override; CMake adds forced-1.3 and invalid-override cases. `vulkan_presentation_tests.cpp` only adds failure phase/window-exposure information, retaining strict color checks. Rollback is the local negotiation/report/test patch; no assets/save data need migration.

Debug checks use the same baseline build targets and regex, expanded with `vulkan_device(_force_1_3|_invalid_api_override)?`. The first preferred run was interrupted after OBS hook loader warnings were identified; it is not a completed suite. The installed implicit `VK_LAYER_OBS_HOOK` advertises API 1.3 and generates a strict validation warning when the application requests 1.4. Existing `DISABLE_VULKAN_OBS_CAPTURE=1` was applied **only to subsequent child test processes**, without changing installed/global hooks or relaxing warning checks.

| Executed profile | Result | Evidence in `build/msvc-debug` |
|---|---|---|
| Preferred 1.4, OBS isolated; normal selection plus forced-device and invalid-override cases | **7 passed / 1 failed / 0 skipped** | `vk14-01-preferred-isolated.xml` |
| Forced 1.3, OBS isolated; device, presentation, RmlUi, G-019 SDR/internal HDR, lifecycle | **5 passed / 1 failed / 0 skipped** | `vk14-01-forced-isolated.xml` |

Both failed cases are the same pre-change `graphics.vulkan_presentation` IMMEDIATE initial red-versus-white screen-pixel check. It also fails with OBS isolated, so OBS has not been established as its cause. All other selected cases pass without validation warnings/errors; logs confirm instance/effective 1.4 versus 1.3 on physical 1.4.329 and retain swapchain/maintenance extensions in both. All **134** G-019 SDR/HDR BMP hashes match the pre-negotiation Debug baseline in both profiles (`build/vk14-00/debug-captures-before.json`). These captures and executed lifecycle/UI checks establish covered scene parity; they do not replace the failed on-screen presentation check.

A focused forced-1.3 RenderDoc diagnostic retains `build/vk14-00/presentation_frame1.rdc` and `presentation-renderdoc.json`: the captured first-cycle green resize attachment replays green with no debug messages. Queued frame 150/200 captures were not produced, so this capture does not explain or close the failing second-cycle IMMEDIATE pixel. Native 1.3-only GPU/driver, separate queue-family hardware and physical HDR remain unavailable; force-1.3 is emulation coverage only. Next renderer milestone stays pending while the known presentation discrepancy is unresolved.

The normal **RelayCore — Play** executable was rebuilt with `cmake --build --preset demo-dev -j 2`. Additional `msvc-dev` build of `relaycore_vulkan_device_tests` and `relaycore_containment_profile` passed. `ctest --test-dir build/msvc-dev -R '^graphics\.vulkan_device(_force_1_3|_invalid_api_override)?$' --no-tests=error --output-on-failure --output-junit vk14-01-optimized-device.xml`: **3/3 passed, zero skips**, with Tracy-required device features. The two-pass integrated G-019 workload on this optimized/Tracy build in preferred 1.4 also exited 0, covering sound, combat, disk Continue, respawn, pause and resize; logs/capture are `build/vk14-01-dev/workload.log` and `runtime.tracy`. GPU CSV export confirms 10,015 executed scene/presentation/RmlUi intervals and shadow/viewmodel events, not just enabled profiling flags. This development build disables IPO/unity and is not a controlled performance comparison to the `msvc-relwithdebinfo` baseline. `git diff --check` passed after source/docs edits; no commit/push/package.

### VK14-01 presentation follow-up — 2026-10-11

Owner requested the next planned step. Reused the current Debug tree and built only
`relaycore_vulkan_presentation_tests`, `relaycore_vulkan_device_tests`,
`relaycore_player_ui_graphics`, `relaycore_containment_loop_graphics` and
`relaycore_game_lifecycle` through `graphics-debug`, in the Visual Studio x64
developer environment with the existing `VCPKG_ROOT`. Build completed successfully;
no source, shader, asset, dependency or color tolerance changed in this follow-up.

The historical IMMEDIATE red-versus-white screen-pixel failure did **not** reproduce.
With process-local `DISABLE_VULKAN_OBS_CAPTURE=1`, both presentation and injected
recreation-failure checks passed five consecutive runs each on preferred 1.4 and
forced 1.3: **20 executed checks, zero failures/skips**, including FIFO/IMMEDIATE,
resize, minimize/restore, hide/show and allocation-failure recovery. Evidence:
`build/msvc-debug/vk14-01-presentation-repeat.xml` and
`vk14-01-presentation-force13-repeat.xml`. These successful runs do not establish
the cause of the previously recorded white pixel or prove that it was fixed.

The first broader sandbox run returned **5 passed / 4 failed / 0 skipped**:
UI/G-019 failed at `filesystem::canonical` with access denied on existing staged
models; lifecycle failed at a temporary snapshot rename with access denied.
`vk14-01-preferred-recheck.xml` retains these results. The identical tests then ran
outside the filesystem sandbox with process-local OBS isolation:

| Profile | Result | Evidence in `build/msvc-debug` |
|---|---|---|
| Preferred 1.4, including forced-device and invalid-override cases | **9 passed / 0 failed / 0 skipped** | `vk14-01-preferred-desktop.xml` |
| Forced 1.3, device/presentation/recreation/UI/G-019 SDR and HDR/lifecycle | **7 passed / 0 failed / 0 skipped** | `vk14-01-forced-desktop.xml` |

Both completed suites passed the unchanged validation warning/error and shutdown
gates. Logs confirm effective 1.4 and 1.3 respectively. All **134** G-019 capture
SHA256 values match `build/vk14-00/debug-captures-before.json` after each suite.
`git diff --check` passed. No package, commit or push was made.

**Current acceptance:** VK14-01 passes the covered local technical regression on
both effective profiles. Native 1.3-only hardware remains unavailable; the historical
presentation discrepancy remains unexplained and must be reopened if it recurs.
No speculative swapchain correction or relaxed pixel check is justified by this run.
The next bounded milestone is **VK14-02**, reproducing the SDR/exposure complaint
on matching views before selecting a color correction. Later effects/scheduling and
human visual acceptance remain separate.

## VK14-02 execution — 2026-10-11

**Status:** local technical gate complete; physical-display/subjective acceptance
and native Vulkan 1.3-only hardware coverage remain open.

**Scope/design:** recheck the existing colour correction from the 2026-10-08
[audit](color_pipeline_baseline.md#vulkan-sdrhdr-audit--2026-10-08) before changing
the renderer. Current hardware-sRGB SDR and FP16 HDR share the highlight shoulder
and display-space grading; HDR additionally exposes linear radiance by `2^EV`.
UI composes encoded colours through a UNORM view after scene mapping; readback
and the SDR swapchain consume encoded RGBA8 without another transfer conversion.
Internal HDR remains an SDR display path. Legacy compatibility is retained.

The pre-change desktop selection passed 6/6 checks: colour transfer/highlights,
HDR numeric references, settings/shadows, RmlUi and G-019 SDR/HDR. The sandbox
selection passed 2/6; four failures came from denied canonical model paths and
the resulting absent scene/save, not a colour failure. Reports:
`build/msvc-debug/vk14-02-before{,-desktop}.xml` and corresponding `build/` logs.

Six matched G-019 views at 1280×720, hardware-sRGB, EV 0, default authored
lighting/postfx and 4× MSAA were inspected: observation, laboratory bench,
apparatus frame, powered crosshead, ready-state HUD and Guardian front.
Mean absolute RGB differences range from 0.0194 to 0.1418 byte levels; every
view has p99 difference 1 and zero fully-white (all channels ≥250) or clipped
(any channel 255) pixels. `build/vk14-02/matched-views.png`,
`matched-metrics.json` and `analyze.py` retain the comparison. This does not
reproduce the earlier broad SDR overexposure/HDR-darkening defect, which was
already corrected; it does not settle subjective brightness or all cameras.

**Bounded change:** extend the existing containment fixture with `--color-audit`
and `graphics.containment_color`. Three fixed views compare SDR to HDR EV 0 in
one process and test HDR EV −1/0/+1 through prepared backend reloads. The paired
gate allows mean RGB error ≤0.5 byte and at most 1% channels differing by >2,
covering attachment quantization and sparse transparency/MSAA differences;
exposure must increase total brightness and never reverse a channel by >2.
The fixture restores SDR/EV 0 before its existing gameplay/save/exit checks.
Existing analytical highlight/UI tests remain independent of these scene limits.
No runtime/shader/art correction is justified by the current evidence. Rollback
is removal of this fixture extension/test; no assets, settings or saves migrate.
Physical HDR/display acceptance and native 1.3-only hardware remain untested.

The new gate first passed independently (1/1, zero skips). Mean encoded RGB
brightness for HDR EV −1/0/+1 is 46.181/66.746/94.263 in observation,
32.073/47.885/69.026 at the laboratory bench and 39.196/57.412/81.827 at the
apparatus frame. No channel reverses by >2. The exposure montage and measured
values are retained in `build/vk14-02/exposure-{views.png,metrics.json}`.

**Build/check commands:** Visual Studio x64 developer environment,
`VCPKG_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg`;
`cmake --preset msvc-debug -DVCPKG_MANIFEST_INSTALL=OFF`, then
`cmake --build --preset graphics-debug --target relaycore_containment_loop_graphics relaycore_vulkan_mesh_tests relaycore_player_ui_graphics relaycore_game_lifecycle relaycore_vulkan_presentation_tests -j 2`.
Configure passed; the sandbox build failed with MSVC C1902/PDB IPC, and the
same targeted build passed outside the sandbox. Logs, command scripts and
source/executable/config SHA256 are retained in `build/vk14-02/`.

CTest selection was discovered with `-N` (10 tests), then executed with
`ctest --preset graphics-debug -R '^graphics\.(containment_color|vulkan_color_baseline|hdr_pipeline|msaa|hdr_settings|player_ui|containment_loop(_hdr)?|vulkan_presentation|game_lifecycle)$' --output-on-failure --no-tests=error --test-output-size-passed 30000 --output-junit vk14-02-<profile>.xml`.
Child processes use `DISABLE_VULKAN_OBS_CAPTURE=1`; forced coverage adds
`RELAYCORE_VULKAN_FORCE_1_3=1` (preferred uses `0`). No global hook/display
setting or validation warning gate changed.

| Final effective profile | Result | Report in `build/msvc-debug` |
|---|---|---|
| Preferred 1.4 | 10/10 passed, zero failures/skips | `vk14-02-preferred.xml` |
| Forced 1.3 | 10/10 passed, zero failures/skips | `vk14-02-force13.xml` |

Both reports confirm the actual effective API and pass the unchanged core/sync
validation warning/error and shutdown gates on the RTX 4060 Ti, NVIDIA 596.49.
All 134 original G-019 BMP SHA256 values match the retained VK14-00 baseline
after each profile; all nine new exposure captures match between profiles.
Metrics are identical between profiles. Separate capture copies and parity
reports are in `build/vk14-02/{preferred,force13}-captures/` and
`{preferred,force13}-parity.json`. Formatting and `git diff --check` passed.
No graphics baseline/tolerance, runtime, shader, dependency, authored light or
material changed; no package, commit or push was made. No speedup is claimed.
The next bounded renderer milestone is VK14-03 resource ownership/completion;
it was not started in this ticket.

## VK14-03 execution — 2026-10-11

**Status:** implementation complete; covered scene/lifetime validation passes on both effective API profiles. Full local acceptance remains open for desktop presentation/recreation checks; the active screensaver prevents screen-pixel readback.

**Scope/design:** introduce renderer-local `VulkanSceneSubmissions` on the existing
serialized graphics queue. Each successful submission signals its own timeline at
`ALL_COMMANDS`, waiting for the preceding scene value. Completion identity includes
the owning timeline; foreign/unsubmitted values and empty/foreign-device retirement
are rejected before taking resource ownership. Failed submission does not publish
a value. Upload completion remains a separate domain and uploads are still waited
before scene consumption. No new API requirement, dependency, shader or asset format.

Scene mesh/shadow and material draws collect deduplicated resource references during
recording and publish `last_use` only after successful submission. Expired mesh
buffers and complete material bundles (descriptor pool/set, uniform buffer and all
textures) enter deferred retirement and release only after that use completes.
Reflection replacement retires old material bundles before the shared cube. Unused
prepared resources have no GPU use. Frame-wide uniforms, pipelines, targets, shadows,
readback and query reuse/destruction still drain the scene completion first.
Presentation copy/acquire/present fences remain independent; the copy still completes
before the scene image can be replaced. RmlUi/ImGui retain the current synchronous
submission and teardown contract; asynchronous UI/frame ownership is VK14-04 work.

All existing host waits remain at the same boundaries, now using the scene timeline
instead of a reusable scene fence. This is a lifetime foundation, not concurrent
frames, GPU-only upload consumption or a speedup. Cached reference vectors retain
capacity; no submission-time allocation is introduced. Rollback is the scene owner,
cache-use/retirement patch, wait zones, source registration and regression extension;
no content/save migration is needed.

**Regression:** extend `vulkan_upload_tests.cpp` with actual two-submission buffer
and texture copies behind a host-signalled GPU gate. Collection must retain four
pending VMA allocations and release all four after scene completion. Foreign/future
retirement keeps the original buffer; a rejected empty command submission leaves
its completion unchanged. A separate pending-copy owner drains at shutdown without
an explicit caller wait/collect. Existing mesh pipeline/resource preparation checks
cover churn, successful/failed shader reload, invalid preparation and recovery;
MSAA/reflection/UI/G-019/lifecycle/presentation checks exercise shared owners.

**Profiling:** retain historical `Vulkan scene fence` / `Vulkan frame fences` aggregate
zones for comparison and add nested scene completion, acquire, presentation copy,
present, source-image release, scene command reuse, scene/material retirement and shutdown/swapchain drain
zones. Nested zones are not additive independent costs. Reuse the two-pass G-019
fixture and optimized `msvc-dev` build; the VK14-00 capture stays unchanged. This run
establishes instrumentation and current costs, not a controlled throughput comparison
to the IPO/unity VK14-00 build. No OS/driver caches are cleared.

**Build/check commands:** existing Visual Studio x64 environment and
`VCPKG_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg`.
`cmake --preset msvc-debug -DVCPKG_MANIFEST_INSTALL=OFF` passed. The first sandbox
build failed with MSVC C1902/PDB IPC; the same targeted upload build passed outside
the sandbox. Build the existing `graphics-debug` targets
`relaycore_vulkan_upload_tests`, `relaycore_vulkan_mesh_tests`,
`relaycore_player_ui_graphics`, `relaycore_containment_loop_graphics`,
`relaycore_game_lifecycle`, `relaycore_vulkan_presentation_tests`; build
`relaycore_containment_profile` and `relaycore_demo` through `demo-dev`.
Build logs/scripts are in `build/vk14-03/`; dependency-required native staging ran,
existing reflection/probe signatures were reused. No release/package was created.

CTest discovery with `-N` selected 11 cases. Execute `ctest --preset graphics-debug
-R '^graphics\.(vulkan_upload|mesh_pipeline|resource_preparation|material_maps_reflections|msaa|player_ui|containment_loop(_hdr)?|game_lifecycle|vulkan_presentation|vulkan_recreation_failure)$'
--output-on-failure --no-tests=error --test-output-size-passed 30000
--output-junit vk14-03-preferred.xml`, with process-local
`DISABLE_VULKAN_OBS_CAPTURE=1`, `RELAYCORE_VULKAN_FORCE_1_3=0`, outside the
filesystem sandbox. Forced 1.3 uses `RELAYCORE_VULKAN_FORCE_1_3=1` and the same
selection excluding the two unavailable desktop-pixel cases.

| Effective profile / selection | Result | JUnit in `build/msvc-debug/` |
|---|---|---|
| Preferred 1.4, 11 cases | 9 passed / 2 failed / 0 skipped | `vk14-03-preferred.xml` |
| Forced 1.3, 9 scene/lifetime cases | 9 passed / 0 failed / 0 skipped | `vk14-03-force13.xml` |
| Initial focused retirement regression | 1 passed / 0 failed / 0 skipped | `vk14-03-upload.xml` |

No completed check reports core/synchronization validation warnings/errors or
shutdown failures. Both effective API values are confirmed in logs. All **134**
original G-019 BMP SHA256 values match the retained VK14-00 baseline after each
profile (`{preferred,force13}-parity.json`). Existing baselines/tolerances were
unchanged. The final source-image wait zone is instrumentation only; Debug and
Play/profiling binaries were rebuilt after it, without changing wait behavior.

**Desktop blocker:** both preferred presentation cases fail at their initial
FIFO screen-pixel gate with `GetPixel == CLR_INVALID` (`4294967295`), before
recreation/failure recovery can be covered. Direct diagnostics also return
`CLR_INVALID` at `(0,0)` and the test location. `OpenInputDesktop` names the
active input desktop **Screen-saver**, while the test thread uses **Default**;
`SPI_GETSCREENSAVERRUNNING` is true. Evidence: `desktop-diagnostics.json`,
`desktop-names.json`, the failed JUnit and `preferred.log`. Owner cannot dismiss
the screensaver in this session. This is an unavailable desktop-readback gate,
not green presentation coverage and not the historical valid white-pixel value
(`16777215`). Neither strict pixel checks nor validation policy were relaxed.
Re-run the two presentation/recreation cases on preferred and forced profiles
when an ordinary interactive desktop is available. Native 1.3-only/separate-queue
hardware and physical HDR remain unavailable.

**Final instrumentation capture:** `build/vk14-03/runtime-final.tracy`,
`workload.log`, `analysis.json`, `summary.json` and CPU/GPU CSV exports retain the
completed two-pass optimized workload. `initial-profile/` retains the first capture
before the additional source-image wait zone. Run `profile-final.ps1` (existing
Tracy 0.14.1 capture `-o runtime-final.tracy -s 100`, then the fixture from this
folder with absolute staged `RELAYCORE_ASSET_ROOT`) and `python analyze.py`.
No builds or graphics tests ran concurrently. The active screensaver was not
changed; other desktop/GPU activity is uncontrolled, so this is instrumentation
proof and descriptive data, **not a clean performance baseline or speedup claim**.

RTX 4060 Ti / NVIDIA 596.49, effective 1.4, `msvc-dev` RelWithDebInfo with Tracy on,
validation/IPO/unity off; 1920×1080, VSync off, hardware-sRGB SDR display with FP16
scene HDR/EV 0, 4× MSAA, existing shadows/FOV 70 and sound on. 10,495 warm executed
CPU frames, including 1,496 resize frames; 8,999 fixed-resolution frames. Warm
fixed-state CPU p99 ranges 4.753–6.026 ms under these desktop conditions. Loading,
first 0.5 seconds and readback are separate; resize varies resolution. This seeded
camera-sweep fixture is not an independent continuous grounded playthrough.

| Recorded CPU zone (entire capture, including load/first use) | Events | Individual-zone p99, ms |
|---|---:|---:|
| Scene completion wait | 23,801 | 3.344012 |
| Acquire fence scope | 11,214 | 0.005140 |
| Presentation copy fence reuse scope | 11,214 | 0.000051 |
| Present fence scope | 11,214 | 0.001140 |
| Source-image release wait after copy | 11,194 | 0.016080 |
| Scene retirement collection | 11,237 | 0.000380 |
| Material retirement collection | 11,237 | 0.001490 |

Fence reuse scopes also record calls with no pending work. In particular, the
copy fence normally completes in the separate source-image release wait, before
its frame slot is reused. Do not interpret the near-zero reuse scope as the
whole presentation copy cost. GPU export contains **11,194** actual scene,
presentation copy, RmlUi and tone-mapping intervals plus shadow/viewmodel events.
The aggregate scene fence zone still accounts for 55–63% of warm executed CPU
frame time, overlapping GPU execution; it is not an independent additive GPU cost.
Seven workload captures were inspected (`views.png`): scene/combat/apparatus,
death/pause UI and resized output render. This does not provide the blocked
on-screen pixel proof or new subjective art approval.

`runtime-source-delta.patch`, copied new scene-owner sources, `provenance.json`,
`runtime-config.json`, `debug-executables.json` and `assets.json` retain source,
executable/config and staged-file hashes. The final profiling-only source-image
zone was built into Debug presentation/upload and optimized fixture/Play targets;
its Debug macro is disabled and existing wait operations are unchanged. Formatting
and `git diff --check` passed. No commit, push, package, host-wait removal or
VK14-04 implementation. Full VK14-03 acceptance remains open until the two
strict desktop cases execute successfully on both effective profiles.

## VK14-04 execution — 2026-10-12

Owner selected VK14-04 and requested a commit after each plan point, with the
pending VK14-00/01/02/03 work committed separately. Unrelated working-tree edits
are excluded. Prerequisite commits: `ce5fbd3`, `82afaf2`, `a6fda78`, `03a38e9`.

**Bounded plan and rollback:**

1. Recheck prerequisites on preferred 1.4 and forced 1.3; capture the current
   optimized `msvc-dev` workload before changing its executable. Use the same
   G-019 fixture, assets, settings and capture boundaries for the comparison.
2. Replace ordinary mesh/material construction host waits with an upload-timeline
   dependency at scene submission. Bound pending staging to 64 MiB/32 batches;
   an individually larger upload runs alone. Retain deliberate backpressure,
   reflection replacement, readback, failed preparation and shutdown waits.
   Protect resources prepared but never drawn in the upload completion domain.
3. Use two scene slots, each owning mutable uniforms/descriptors, targets and
   command buffers. Keep shared shadows/pipelines behind the serialized scene
   timeline. Chain scene → RmlUi → presentation copy on that timeline; retain
   RmlUi draw resources until the slot completes. Keep ImGui/readback synchronous.
   Wait for a slot before rewriting it, and drain both slots on reconfiguration
   and shutdown. No API/dependency/shader/content-format change.
4. Run validation/lifetime/UI/G-019/presentation and image parity on both API
   profiles, then repeat the optimized workload. Retain scheduling only if
   comparable measurements support its complexity. Record limits separately
   from native 1.3/other GPU and human acceptance. Commit each completed point.

Rollback is each local implementation commit; no asset/save migration or package.
Build, CTest, traces and parity evidence live in `build/vk14-04/` and
`build/msvc-debug/vk14-04-*.xml`. Initial sandbox profiling failed on the fixture's
temporary save rename; the retry uses the ordinary desktop outside that sandbox.

**Point 1 complete:** unchanged prerequisite binaries passed 15/15 selected cases
on preferred 1.4 and 15/15 on forced 1.3, zero failures/skips; the strict desktop
presentation/recreation cases now execute. Reports are
`vk14-04-prerequisite-{preferred,force13}.xml`. This supersedes VK14-03's unavailable
desktop gate without explaining the historical white pixel. Process-local OBS
isolation and unchanged validation/pixel gates were retained.

The before workload exits 0 and captures real CPU/GPU intervals in
`before/runtime-final.tracy`; `before/analysis.json` separates loading/readback,
first use and resize. It uses the existing `msvc-dev` VK14-03 executable, RTX
4060 Ti/NVIDIA 596.49, effective 1.4, 1920×1080/VSync off/internal HDR/EV 0,
4× MSAA/default shadows/FOV 70/sound on, validation/IPO/unity off. Warm fixed-state
CPU p50 ranges 4.265–6.762 ms; p99
5.260–8.506 ms. OS/driver caches and other desktop
activity are uncontrolled; this is a seeded camera sweep, not a grounded playthrough.

**Point 2 complete:** mesh/material uploads publish into cache entries allocated
before submission; a failed construction drains uploads before erasing the partial
entry. Successful scene submits wait on the upload timeline at ALL_COMMANDS.
Prepared-but-undrawn meshes retire through uploads; material bundles require both
upload and final scene completion. Synchronous renders collect completed staging
before returning. Ordinary construction has no per-buffer/texture host wait;
reflection replacement, failure, staging backpressure and teardown retain waits.
Pending staging is capped at 64 MiB/32 batches (one larger upload runs alone).

`cmake --build --preset graphics-debug --target relaycore_vulkan_upload_tests
relaycore_vulkan_mesh_tests -j 2` passes in the existing VS x64 environment.
The focused `vulkan_upload|mesh_pipeline|resource_preparation|material_maps_reflections`
CTest selection passes **4/4 on each profile**, zero skips, with unchanged validation
and allocation/lifetime gates. Reports: `vk14-04-upload-{preferred,force13}.xml`.
The new actual GPU-gated consumer returns before uploads complete, then verifies
copied mesh bytes after scene completion. Scene scheduling is still synchronous
at this point; its impact is measured after point 3.

**Point 3 complete:** each of two scene slots owns five mutable uniform buffers,
its material/tone-mapping descriptor sets, color/depth/MSAA/readback targets,
command pool/three buffers, profiler and final completion. Ordinary scene/RmlUi
submissions return without a host completion wait; slot reuse waits for its final
presentation copy. Capture-only renders reuse their current slot synchronously;
ImGui remains synchronous. Shared shadow images/pipelines and immutable materials
are ordered through the existing graphics timeline, without duplicating shadows.

The presentation copy waits for the prior scene/UI value plus acquisition and
signals both the scene timeline and WSI semaphore. Its completion extends the
source slot before present can fail. Swapchain acquire/copy/present fence reuse
and maintenance lifetime remain independent. Reload/reflection replacement and
backend shutdown drain both slots. RmlUi snapshots hold geometry, textures and
their descriptor/pipeline owner, including after context teardown; cached snapshot
vectors reuse storage in steady rendering.

Debug builds the existing upload, mesh, player UI, G-019, lifecycle and presentation
targets. Focused `vulkan_upload|mesh_pipeline|resource_preparation|material_maps_reflections|game_hud`
checks pass **5/5 on each effective profile**, zero failures/skips, with core/sync
validation. `vk14-04-scheduling-{preferred,force13}.xml` retains the results.
Actual host-gated tests prove two outstanding scene slots, shader reload afterward,
unchanged pixels/settled VMA allocations, and destruction of an in-flight RmlUi
context without losing its draw or prematurely destroying GPU objects. Existing
HUD checks include four resolutions, Cyrillic, hit/hurt/death and ImGui coexistence.
The common test queue gate is shared by the upload and mesh fixtures.

**Point 4 complete — retain scheduling:** the existing Debug targets and optimized
`cmake --build --preset demo-dev --target relaycore_containment_profile relaycore_demo -j 2`
build successfully. The final selection in `build/vk14-04/run-final.ps1` passes
**17/17 on preferred 1.4 and 17/17 on forced 1.3**, zero failures/skips. It covers
uploads, meshes/resource churn, HUD/player UI, SDR/HDR/MSAA/reflections/shadows,
G-019, lifecycle and strict presentation/recreation. Core/synchronization validation
and pixel/lifetime assertions remain enabled. Reports are
`build/msvc-debug/vk14-04-{preferred,force13}.xml`; each profile matches all **134**
original G-019 BMP SHA-256 hashes, with zero differences. This completes the
previously unavailable VK14-03 local desktop gate without resolving its historical
white-pixel observation.

The single after workload and Tracy capture exit 0. Before/after executable hashes,
source revisions and **189 identical optimized staged assets** are recorded in
`build/vk14-04/{before,after}/provenance.json`. Both runs use the point 1 settings
and fixture boundaries, with loading, readback, the initial 0.5 seconds and resize
separated from settled fixed-state results. The source revisions are `03a38e9`
before and `166369f` after; full intervals are in `runtime-final.tracy`, per-stage
statistics in `analysis.json`, and frame-start cadence in `comparison.json`.

CPU frame times in milliseconds (p50 / p95 / p99):

| Pass / state | Before | After |
|---|---|---|
| 0 observation | 5.438 / 6.224 / 6.518 | 3.301 / 3.910 / 4.296 |
| 0 workshop | 4.402 / 5.035 / 5.548 | 2.776 / 6.020 / 10.505 |
| 0 archive | 4.265 / 4.845 / 5.260 | 2.538 / 3.034 / 3.325 |
| 0 return | 5.034 / 5.767 / 6.232 | 3.356 / 3.924 / 4.248 |
| 0 retry | 4.692 / 5.730 / 7.357 | 2.881 / 3.657 / 4.033 |
| 0 pause | 5.974 / 6.639 / 7.044 | 5.024 / 6.016 / 6.668 |
| 1 observation | 5.446 / 6.185 / 6.656 | 3.265 / 3.856 / 4.224 |
| 1 workshop | 4.369 / 5.003 / 6.267 | 2.473 / 2.928 / 3.242 |
| 1 archive | 4.420 / 5.292 / 6.004 | 2.293 / 2.828 / 3.193 |
| 1 return | 5.000 / 5.794 / 6.338 | 3.063 / 3.549 / 3.803 |
| 1 retry | 4.765 / 6.455 / 7.691 | 2.732 / 3.430 / 3.955 |
| 1 pause | 6.762 / 7.863 / 8.506 | 5.293 / 6.139 / 6.742 |
| 0 resize | 4.978 / 5.934 / 6.544 | 2.422 / 3.631 / 3.879 |
| 1 resize | 5.335 / 6.331 / 8.460 | 2.812 / 3.951 / 5.891 |

The 12 fixed-state windows contain 8,260 before and 13,282 after frame samples.
Every state's p50 decreases **15.9–48.1%**, and frame-start p50 agrees within
0.015 ms with the CPU measurement, supporting improved throughput rather than
only shorter submission spans. The first-pass workshop tail regresses (p95 +19.6%,
p99 +89.3%); its second pass improves. Eleven of twelve fixed-state p99 values
improve. This single paired capture does not establish universal tail stability or
input/display latency. The initial load/first frame are 3740.178/361.126 ms before
and 3402.385/154.965 ms after; uncontrolled caches prevent attributing those cold
differences solely to this patch. First-use results across later stages are mixed.

Unconditional source-image host waits fall from 8,260 to zero in the fixed windows.
Slot-reuse scene-fence CPU fractions range 15.6–55.3% after versus 51.8–60.2% before;
remaining slot/WSI waits are intentional backpressure. GPU scene p99 is 2.894–4.358
ms after, so no reduced GPU-work claim is made. Two targets/UBO/descriptor/profiler
sets increase resident ownership; resource-preparation tests verify settled VMA
allocations, without claiming an exact runtime memory budget.

All seven after captures were visually inspected together, including world/HUD,
death/retry, pause and resized output (`after/visual-review.png`). Technical image
parity is established by the deterministic 134-image gate; subjective art/HDR,
native 1.3-only hardware, other GPUs and queue topologies remain unverified.
Evidence supports retaining points 2/3; rollback is not selected. Point commits:
`bf4e45b` (baseline), `9391903` (uploads), `166369f` (slots/UI/copy), followed by
the point 4 verification record. Later VK14 milestones are outside this task.

## Vulkan 1.4-only contract revision — 2026-10-12

The owner explicitly requested removal of Vulkan 1.3 compatibility to avoid the
second API implementation/test profile during subsequent VK14 work. Require
loader and selected physical-device API ≥1.4, request instance API 1.4 and pass
1.4 to VMA. Remove API negotiation, the forced-1.3 environment override and its
two CTest registrations. Keep existing feature/extension checks and SPIR-V 1.5;
optional rendering capabilities still require their own support checks.

This supersedes the original VK14-01 compatibility contract and all earlier
native/forced-1.3 acceptance gates. Historical two-profile evidence above remains
unchanged. Vulkan 1.3 hardware is outside the supported runtime baseline; no
performance gain is claimed.

**Verification:** Windows/MSVC, RTX 4060 Ti, loader/headers 1.4.357, physical API
1.4.329. `cmake --preset msvc-debug` and `cmake --preset msvc-dev` passed without
dependency version changes. The initial sandbox build failed with the previously
recorded MSVC C1902/PDB IPC restriction; the same targeted build passed outside
the sandbox in the VS x64 developer environment:

```powershell
cmake --build --preset graphics-debug --target relaycore_vulkan_device_tests relaycore_vulkan_presentation_tests relaycore_vulkan_upload_tests relaycore_vulkan_mesh_tests relaycore_player_ui_graphics relaycore_containment_loop_graphics relaycore_game_lifecycle relaycore_vulkan_toolchain_check -j 2
cmake --build --preset demo-dev --target relaycore_demo relaycore_vulkan_device_tests -j 2
```

`build/msvc-debug/relaycore_vulkan_toolchain_check.exe` passed, reporting loader
and VMA support 1.4 plus available validation/debug-utils. Graphics checks ran
on the ordinary desktop outside the sandbox with process-local
`DISABLE_VULKAN_OBS_CAPTURE=1`; installed/global hooks were unchanged:

```powershell
ctest --preset graphics-debug -R '^graphics\.(vulkan_(device|missing_validation|presentation|recreation_failure|upload|mesh|color_baseline)|mesh_pipeline|hdr_pipeline|msaa|containment_color|resource_preparation|player_ui|containment_loop(_hdr)?|game_lifecycle)$' --no-tests=error --output-on-failure --output-junit vk14-only-graphics.xml
ctest --test-dir build/msvc-dev -R '^graphics\.vulkan_device$' --no-tests=error --output-on-failure --output-junit vk14-only-device.xml
```

Debug: **15 passed / 1 failed / 0 skipped**; development/Tracy device: **1 passed /
0 failed / 0 skipped**. Passing cases cover device allocation/timeline/lifetime,
missing-validation rejection, presentation/recreation, uploads, prepared resource
churn, mesh pipeline, RmlUi, G-019 SDR/HDR/exposure, color/HDR/MSAA and lifecycle.
No validation warnings/errors occurred. JUnit reports are in the respective build
directories. The normal `RelayCore — Play` executable is updated.

**Separate baseline failure:** `graphics.vulkan_mesh` fails its existing
`power_restoration-0` image comparison: mask difference **0.00361111**, color
difference **0.306111**, mean RGB delta **5.48441**. A diagnostic executable using
the pre-change negotiation logic from HEAD `889cf3e` reproduces the identical
failure on the same current renderer, shaders, test object and staged assets,
with instance/effective API 1.4. The old source was adapted only to the current
capability ABI (removed the retired override-report field and localized its old
API constants). This isolates the API change; it does not diagnose the original
image discrepancy. Reproduction script, source, compiler/link commands and log
remain in `build/vk14-only/`; diagnostic binary is
`build/msvc-debug/tests/relaycore_vulkan_mesh_before_api.exe`. Baselines/tolerances
were preserved. No unrelated rendering correction is included.

`clang-format --dry-run --Werror` passed for the four changed C++ files;
`git diff --check` passed. Real older-loader/GPU rejection remains unexecuted
because that hardware/loader is unavailable. Optional-feature and other-GPU/human
gates remain independent. No commit, push or package was created.

## 1. Executive decision

1. **Minimum runtime:** Vulkan 1.4, retaining all existing playable/gameplay functionality, current visual features, editor/debug tools, HUD/menu and save/continue flows.
2. **Required API:** Both loader and selected physical device must support at least 1.4. Instance and VMA use API 1.4; missing support fails startup explicitly. There is no 1.3 fallback or developer override.
3. **One renderer:** Preserve the existing render-data boundary and Vulkan ownership. Do not restart the renderer, reintroduce OpenGL, or add a generic `IRenderBackend`.
4. **Core vs optional:** Vulkan 1.4 core improvements are independent from optional extensions (hardware ray tracing, mesh shaders, fragment shading rate, HDR presentation). Optional features must have explicit capability checks, bounded quality/cost settings, and viable fallback behavior.
5. **Full requested feature backlog, incremental delivery:** All technologies in this document are in scope for evaluation/implementation. Implement and verify them individually, with meaningful acceptance gates and rollbacks. The aspiration to support a feature is not grounds to enable it on every GPU or to compromise existing content.
6. **Actual user-facing value:** Prioritize known SDR/HDR color complaints, visible graphics deficiencies, first-use/session stutters and measured bottlenecks. Do not claim that API 1.4, ray tracing, or compute automatically improves frame rate or art quality.
7. **Compatibility test:** Test startup/rendering on Vulkan 1.4 and explicit rejection of older loaders/devices where available. Test optional features independently; an API version string alone does not establish extension support. No forced-1.3 suite is required.

**Definition of success:** A stable Vulkan 1.4 runtime, a selectable and observable optional feature/capability system, materially improved rendering where applicable, reliable optional-feature alternatives, validated GPU resource ownership, reproducible evidence and no regression to the G-019 gameplay/content pipeline.

---

## 2. Source-of-truth and baseline audit

### 2.1 Read before implementing

Read the **current** versions of:

- [`AGENTS.MD`](../AGENTS.MD) — mandatory repository workflow, ownership, anti-slop and verification policy.
- [`docs/PLAYABLE_SLICE_ROADMAP.md`](PLAYABLE_SLICE_ROADMAP.md) — active ticket/acceptance/evidence register, including G-019.
- [`docs/PROJECT_PLAN.md`](PROJECT_PLAN.md) — active development strategy and gameplay/content constraints.
- [`docs/VULKAN_MIGRATION_PLAN.md`](VULKAN_MIGRATION_PLAN.md) — completed OpenGL→Vulkan migration, synchronization decisions and test conventions. Do not rerun completed parity work.
- [`docs/ENGINE_ARCHITECTURE_REFERENCE.md`](ENGINE_ARCHITECTURE_REFERENCE.md) — long-term engine/asset architecture and the API-neutral scene submission boundary.
- [`docs/DEPENDENCY_STACK.md`](DEPENDENCY_STACK.md) — dependency ownership, pinned vcpkg policy and asset-product choices.
- [`docs/DEVELOPMENT.md`](DEVELOPMENT.md) — real Windows/MSVC presets, profiling, graphics validation and runtime diagnostics.
- [`docs/ART_DIRECTION.md`](ART_DIRECTION.md) — current visual direction; shader/features do not supersede authored aesthetic goals.

This document owns the separate renderer modernization milestones and is **not** another competing gameplay roadmap. Give Vulkan/render modernization its own staged work items and evidence, linked from existing development planning, rather than cloning every task/status into multiple files.

### 2.2 Existing working foundations — preserve

- C++20, Windows/MSVC, CMake/Ninja/vcpkg; SDL3 platform/window/input; **direct Vulkan 1.4** is the sole runtime graphics backend (required since 2026-10-12).
- `vk-bootstrap` for instance/physical-device/logical-device/swapchain selection; standard Vulkan loader; **VMA** for GPU buffers/images; Slang compiled **offline** to SPIR-V; Khronos validation, Tracy Vulkan and RenderDoc workflow.
- Required device/runtime contract: `dynamicRendering`, `synchronization2`, `timelineSemaphore`, swapchain/WSI maintenance dependencies; optional anisotropic filtering.
- Scene passes, MSAA, directional/spot/point shadow maps, FP16 HDR scene, exposure/tone mapping, existing SDR color-path variants, fog and post effects.
- PBR metallic/roughness, normal maps, AO textures, emissive data, GGX roughness refinement, SH irradiance and baked reflection-probe paths. These already have asset and shader consumers; **do not reimplement them**.
- Native GLB→RelayCore product pipeline, versioned `.rcmodel` readers (including earlier versions), validated staging/receipts, static geometry and separate collision data; `ModelAssetData` and `RenderItem` already carry AO/emissive/PBR data.
- **E-018** prepared-scene GPU pre-acquisition removes much first-render GPU acquisition from the playable frame, but underlying `uploads.wait()` calls still exist; measure whether their cost now occurs during loading/preparation rather than gameplay before describing a hitch.
- `WorldSceneRenderer` already performs **CPU-side chunk/frustum culling** and transparency ordering. This is not equivalent to GPU-driven culling, but culling is not wholly absent.
- RmlUi 6.3 provides player HUD/menus; Dear ImGui remains for editor, debug and diagnostic tools. Retain current SDL3/Vulkan ownership and RmlUi rendering integration.
- Existing gameplay loop, G-019 maps, editor, tests, content pipeline, save/continue/retry and packages are regression-critical.

### 2.3 Historical missing/incomplete backend mechanisms at the stated commit

This is the pre-VK14-01 source snapshot, not the current task list. Execution
evidence above and the 2026-10-12 API contract supersede completed/revised rows.

| Area | Code evidence | Current limitation |
|---|---|---|
| Runtime API selection | `vulkan_device.h`: `kVulkanApiVersion = VK_API_VERSION_1_3`; `vulkan_device.cpp`: `.require_api_version(kVulkanApiVersion)`, selector min 1.3 | No preferred-1.4 / baseline-1.3 negotiation or force-1.3 testing mode |
| Scene scheduling | `vulkan_mesh_renderer.cpp` `State`, `begin_commands()`, `submit_and_wait()` | Single scene pool/command buffer/fence/target, immediate completion waits; no independent concurrent scene frames |
| Presentation scheduling | `vulkan_swapchain.cpp` `draw_clear()` | Swapchain frames exist, but copy-source `scene_image` forces a host fence wait; presentation frames are **not** concurrent scene frames |
| Upload consumption | `vulkan_uploads.cpp`; `vulkan_mesh_renderer.cpp::mesh()`, material creation | Timeline-enabled producer with synchronous host waits at consumer creation; cannot claim nonblocking first-use uploads |
| Descriptor allocation | `vulkan_mesh_renderer.cpp` material allocation | Per-material `VkDescriptorPool`, `maxSets = 1` |
| Pipeline construction | `vulkan_mesh_renderer.cpp`, `vulkan_rml_renderer.cpp` | `vkCreateGraphicsPipelines(..., VK_NULL_HANDLE, ...)`; no persistent/shared `VkPipelineCache` |
| Formats | `vulkan_mesh_renderer.cpp` | Hard-coded D32 scene depth / X8_D24 shadow depth; limited fallback selection |
| WSI surface loss | `vulkan_swapchain.cpp` | `VK_ERROR_SURFACE_LOST_KHR` recognized, but no complete SDL surface recreation path in current owner |
| GPU budgets | `vulkan_device.cpp` VMA init | No persistent runtime heap-budget/usage observability; one device test calls `vmaCalculateStatistics()` as a lifetime probe, which is **not** a runtime memory dashboard |
| Draw submission | `vulkan_mesh_renderer.cpp` | CPU-recorded `vkCmdDrawIndexed(..., instanceCount = 1)`; no GPU-generated indirect draw batches / native GPU-instanced path |
| Presentation gamut | `vulkan_swapchain.cpp` lines ~220–234 | Explicit SDR BGRA8/RGBA8 UNORM and `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`; **FP16 HDR rendering is not HDR10/scRGB monitor output** |
| Shader workload | `CMakeLists.txt` | Existing Slang vertex/fragment shaders built to SPIR-V 1.5; no shipped compute/mesh/ray-tracing shader toolchain paths |

These are **static code findings, not measured FPS regressions**. Re-read latest code before every phase, especially if Codex has changed the repository since this snapshot.

---

## 3. Vulkan 1.4: what changes, what does not

Vulkan 1.4 (December 2024) promoted and standardized many previously optional capabilities, including `VK_KHR_push_descriptor`, `VK_KHR_dynamic_rendering_local_read`, `VK_KHR_maintenance5/6`, `VK_KHR_map_memory2`, line rasterization and other extensions; it increases guaranteed capabilities/limits and introduces stronger streaming-transfer requirements. **Promoted functionality is not automatically a performance optimization**: every new path still requires an actual consumer, correct synchronization and benchmarks.

### 3.1 Required version contract

- **Build-time SDK/headers:** require Vulkan 1.4-capable headers/loader and compatible vk-bootstrap/VMA/Slang. CMake requires Vulkan ≥1.4. Keep the existing dependency baseline unless a concrete blocker requires a separate update.
- **Instance version:** inspect `vkb::SystemInfo` / `vkEnumerateInstanceVersion`; reject loaders below 1.4 and request API 1.4. Distinguish loader/instance version from each physical GPU's version.
- **GPU version:** require `VkPhysicalDeviceProperties::apiVersion` ≥1.4 together with required features/extensions and SDL surface-compatible queues. Older devices are rejected with detailed selection diagnostics.
- **Enabled features:** use the proper `VkPhysicalDeviceFeatures2` / Vulkan 1.2/1.3/1.4 feature chains and optional extension feature structs. Existing 1.2/1.3 feature structs remain valid on 1.4; they are not fallback paths. Feature availability, device-extension promotion, function aliases, queue support and shader capabilities are separate checks.
- **Loader/function dispatch:** use supported core commands on 1.4 and independently enabled extensions where needed. Keep the existing standard loader policy unless there is a concrete reason to change it.
- **VMA:** pass API 1.4 after loader and physical-device checks succeed.
- **Shader compatibility:** preserve the current compatible SPIR-V 1.5 baseline. Introduce separate shader modules/variants for extensions or advanced capabilities; validate each variant on its intended Vulkan/device profile. Vulkan 1.4 does **not** mandate a new SPIR-V source format for every shader.
- **Debug/test profiles:** run the Vulkan 1.4 profile. The forced-1.3 override and its tests are removed; retain tests for optional capabilities and required-feature rejection.
- **Diagnostics:** log loader API, requested instance API, selected physical-device API, effective API, queue families, required/optional feature bits, enabled extension names, chosen formats, shader variant and reasons for optional-feature alternatives. Do not mistake a log line for proof that a path executed.

Vulkan 1.4 is an intentional compatibility requirement. This does not enable optional features automatically or justify unused feature chains, shader variants or rendering effects.

### 3.2 Vulkan 1.4-specific candidate consumers

| Vulkan 1.4 area | RelayCore use case | Adoption constraint / alternative |
|---|---|---|
| Improved streaming-transfer guarantee | Sustained texture/model transfers while drawing | Existing timeline/graphics-queue transfer model; extension/capability-aware upload path. Guarantees do not remove the app's own `uploads.wait()` calls. |
| Push descriptors | Very small pass-local transient bindings | Standard descriptor sets; do **not** replace persistent material descriptors without evidence. |
| Dynamic rendering local read | Suitable attachment-read rendering algorithms; check depth/stencil and multisampled local-read properties separately | Explicit image transitions and separate subpasses/passes; no mandatory deferred renderer conversion. |
| Scalar block layout (optional since Vulkan 1.2; mandatory support in 1.4) | Compact data layouts where correct shader interface demands them | Existing layouts/padding; reflect/validate host–GPU offsets. |
| Maintenance5/6, map-memory2 and other promoted features | Specific API simplifications and correctness improvements | Adopt through a concrete consumer with explicitly enabled required features. |
| Larger guaranteed limits | More MRTs/render targets, wider push constants and high-resolution targets | Query actual device limits and validate layouts; never hardcode assumptions into common shader interfaces. |

**Not provided by core Vulkan 1.4:** hardware RT, `VK_EXT_mesh_shader`, fragment shading rate, bindless resource architecture, true display HDR, sophisticated GI, motion vectors or FSR. Those require separate code and in several cases optional extensions and hardware.

---

## 4. Runtime capability model and compatibility behavior

Implement a **small renderer-owned** capability record, populated once after device selection; avoid a generic framework or global singleton. Candidate fields (exact type/name should follow current code conventions); introduce only fields consumed by the selected milestone, not the whole future backlog:

- `loader_api`, `instance_api`, `physical_api`, `effective_api`;
- required: dynamic rendering, sync2, timeline, swapchain maintenance, presentation/format support;
- optional: memory-budget reporting, depth/shadow format options, suitable compute queue, transfer queue, push descriptors/local read, descriptor indexing/update-after-bind, buffer device address, indirect-draw/mesh-shader capabilities, fragment shading rate, ray query/acceleration structures/ray tracing pipelines, HDR-capable surface formats/colorspaces/metadata;
- GPU limits: attachment counts, push constants, workgroup sizes, descriptor counts, maximum resolution, timestamp support, memory heaps;
- selected **backend technique** per feature: e.g. CPU draws vs indirect draws, raster reflections vs RT reflections.

Support **three distinct configuration layers**:

1. **Required baseline** — must function for the game to run; failure is diagnostic and explicit.
2. **Optional capability** — enable only if API version/extension, feature flags, queue/format support and shipped shader variant all satisfy the dependency.
3. **User graphics preference** — off/low/medium/high/ultra or auto, bounded by the detected capability. Unsupported settings should be disabled or produce an actionable message, not silently select an invalid GPU path.

**Fallback matrix (normative):**

| Feature | Preferred | Fallback / baseline |
|---|---|---|
| API | Required Vulkan 1.4 | Startup failure for unsupported loader/device; no API fallback |
| Mesh pipeline | Meshlets + mesh shaders where supported/beneficial | Existing indexed triangles via conventional vertex/fragment shaders |
| Submission | GPU culling + indirect (when it wins) | Existing CPU chunk/frustum culling + indexed draws |
| Material bindings | Descriptor indexing/bindless where capacity/scaling justifies it | Shared pooled conventional descriptors |
| Reflections | Optional RT/hybrid reflections | SSR where valid → existing reflection probes/SH fallback |
| Indirect diffuse lighting | Optional RT/SSGI | Existing authored/SH irradiance and baked probes |
| Ambient occlusion | GTAO/SSAO | Existing asset AO + authored lighting; avoid double-darkening |
| AA / upscale | Tunable TAA/temporal upscale | Existing MSAA and/or spatial AA/upscaler |
| Scene output | HDR10/scRGB where *OS+display+swapchain* support it | Existing tested SDR presentation with FP16 internal HDR + tone mapping |
| VRS | Hardware fragment shading rate | Full-rate shading |
| Async compute | Overlapped compute on validated queue setup | Graphics-queue compute or graphics-only fallback |
| Volumetrics | Full volumetric field | Existing distance fog or simplified fog volumes |
| Depth of field / motion blur | Optional effect | Off; preserve sharp/readable gameplay, HUD and viewmodel rules |

A fallback is only “supported” after testing. It is not sufficient to set a boolean and hope the alternative shader behaves correctly.

---

## 5. Vulkan backend hardening workstream

### B0 — Instrument and capture a reproducible baseline (first)

- Before modifying scheduling, collect **Tracy CPU + Vulkan GPU**, frame-time distribution, UI submit, blit/present, resize/reload, asset/session preparation, GPU memory and GPU driver/version on real G-019 scenes.
- Track p50/p95/p99, worst hitch and CPU-wait proportions. Separate first-load/preparation stalls from gameplay-frame stalls and VSync/presentation wait.
- Profile an idle/settled corridor, a populated battle/particles room, session entry, save/retry, shader reload, resize/minimize, and heavy material/mesh churn. Use the same optimized build/settings for before/after.
- Existing E-018 GPU prewarming is already a first-load mitigation; preserve and measure it.
- Gate: identify costs worth changing. If a rewrite adds complexity without improving measurements, prefer the simpler safe implementation.

### B1 — Explicit GPU completion and resource ownership

- Map every resource owner and last-user completion domain: scene commands, frame-local buffers, mesh buffers, textures/images/views/samplers, material descriptor allocations, pipelines/shaders, render targets, RmlUi/ImGui, upload staging, swapchain images and present fences.
- Introduce **small renderer-local completion tokens** and retirement queues only where GPU access can outlive CPU ownership.
- Replace resources only after successful new creation; defer destruction until their last real GPU use completes. Avoid global `vkDeviceWaitIdle()` during normal rendering/reloads.
- Cover lifecycle corner cases: failed pipeline hot reload, device teardown, swapchain recreate, UI resource reload, map replacement, aborted upload and exception/rollback paths.
- Gate: validation-clean outstanding references; intentional device idle at shutdown is acceptable.

### B2 — Genuine scene frames-in-flight

- Remove single-frame host serialization only after B1 makes lifetime safe.
- Start with **2 independent scene frame contexts**: command pool/buffer, completion, color/depth targets where necessary, frame-local mutable uniforms/descriptors and transient allocations.
- Model the **scene → optional UI → presentation source-image GPU dependency** explicitly. Swapchain's two presentation frames are **not** enough; its blit currently waits on a scene source-image host fence.
- Wait on the CPU only when reusing a specific in-flight frame resource, on intentional reconfiguration, or when backpressure is necessary.
- Validate CPU writes to per-frame data, frame-index reuse, transitions, readback-only paths, shutdown, hot reload, UI overlay, screen capture and resize with one/multiple outstanding frames.
- Gate: no read/write races, no early pool reset/target reuse, clean synchronization validation and measured impact. Roll back if practical cost/complexity outweighs benefit.

### B3 — Nonblocking GPU upload consumption

- Reuse existing `VulkanUploads` timeline completions; replace ordinary host waits in mesh/texture material construction with a **graphics submit waiting for the required GPU timeline value**.
- Batch uploads; inspect command pool/buffer churn, staging lifetime and a bounded outstanding upload-memory budget. Recycle only after timeline completion.
- Retain a deliberate backpressure wait when the staging budget is full. Keep synchronous readback/CPU-needed operations intentional and separately measured.
- Preserve the prepared-session acquisition contract; do not move asset decoding or unstable gameplay state into the render thread just to call the feature “async”.
- Gate: no incomplete resource sampling, no use-after-free, no first-use stalls on the normal graphics path due to redundant host waits, reproducible before/after.

### B4 — Material descriptor pooling and pipeline caching

- Replace one-pool-per-material with a small **persistent descriptor allocator** (growth, lifetime accounting, bounded reclamation); create transient frame pools **only** if B2 requires them.
- Test 1,000+ synthetic material allocations, material churn and hot reload without 1,000 descriptor pools or GPU use-after-free.
- Introduce one renderer-owned `VkPipelineCache` and route graphics/compute pipelines through it where supported. Measure compile/startup/reload costs first.
- Optional on-disk persistence must be invalidated for mismatched vendor/device/driver, `pipelineCacheUUID`, schema/shaders. Corrupt cache must safely fall back to a fresh cache.
- Maintain valid old pipelines on compilation errors; publish new pipelines atomically and retire previous ones after GPU completion.
- Gate: no new validation errors, proven lifetime and stable shader reload.

### B5 — Portability, WSI and memory diagnostics

- Explicit Required/Preferred/Optional feature table and device selection logs.
- Capability-driven scene depth / sampled shadow depth candidates; validate exact usage/format properties and per-format visual tolerances.
- Recover from `VK_ERROR_SURFACE_LOST_KHR` by correctly recreating the SDL `VkSurfaceKHR`, rechecking present support and rebuilding the swapchain; stop gracefully on unrecoverable `VK_ERROR_DEVICE_LOST` with diagnostics. Full device-hot-recovery is **not** a requirement.
- Add VMA heap budgets/allocation counts and reporting, using `VK_EXT_memory_budget` when available, plus upload staging outstanding bytes and pending GPU retirements; keep costly detailed snapshots out of the normal frame path.
- Add semantic Vulkan debug names and Tracy zones for wait points, allocations and pass boundaries; preserve RenderDoc compatibility.
- Gate: repeated window loss/resize/minimize/restore, device-incompatible conditions and resource churn on Vulkan 1.4.

---

## 6. GPU submission, compute and scalable geometry workstream

Implement this **after** a measured baseline and safe frame/resource-lifetime foundation. Reuse existing CPU frustum/chunk culling rather than discarding it prematurely.

| Feature | Actual implementation deliverable | Fallback / acceptance |
|---|---|---|
| **Compute shader infrastructure** | Offline Slang→SPIR-V compute targets, descriptor conventions, dispatch sizes, pass synchronization, GPU debug/capture/test harness | Graphics-only passes where adequate; start with one clear consumer, e.g. tile light list, AO or particles |
| **GPU instancing** | Combine identical mesh+material objects into indexed instanced draws; per-instance transforms/material indices, compatible opaque/transparent grouping | Existing per-object indexed draws; include transform, shadow and selection-ID parity |
| **Indirect drawing** | `VkDrawIndexedIndirectCommand` buffers, deterministic command ownership and checked draw counts | Conventional `vkCmdDrawIndexed` per drawable; measure CPU draw-call savings |
| **GPU-driven culling** | GPU-visible bounds, frustum + optional occlusion/LOD selection, compaction/indirect command generation | Existing CPU chunk/frustum culling; test camera cuts, fast motion, GPU command limits, false negatives |
| **Bindless/descriptor indexing** | Capability-gated descriptor arrays, stable texture/material indices, update-after-bind synchronization if enabled; capacity/error policy | Conventional pooled material descriptor sets; do not require this for current PBR compatibility |
| **Asynchronous compute** | Profile independent compute work; supported compute queue selection, semaphores/ownership transfers, scheduling without graphics starvation | Compute on graphics queue or original raster path; accept only measured improvement |
| **Mesh shaders** | Optional `VK_EXT_mesh_shader` path and offline meshlet products (meshoptimizer or equivalent); culling/LOD and shading parity | Preserve indexed vertex path and original assets; verify each supported device separately |
| **Variable Rate Shading** | Optional `VK_KHR_fragment_shading_rate`, supported attachment rates/regions, moving-camera behavior, visual quality controls | Full-rate shading; avoid degraded weapon, HUD, small text and thin-target legibility |
| **Hardware ray tracing** | Optional extension set (`VK_KHR_acceleration_structure`, ray query/pipeline as chosen, required dependencies), BLAS/TLAS from native meshes, update strategy, shaders, SBT if pipeline-based, denoising/compositing | Existing shadows/reflection probes plus SSR/SSGI as appropriate; gate on GPU time and memory; handle alpha masks and moving objects |

**Important engineering tradeoffs:**

- Existing native static GLB meshes can generally be converted **offline** into meshlets, instance streams, bounds and acceleration structures; no mass reauthoring of Blender source assets is required.
- Mesh shaders, bindless and hardware RT remain optional on Vulkan 1.4: selected physical-device extensions/features determine their availability.
- One workable ray-traced effect (e.g. selected reflections or shadows) must reach parity/fallback and profile gates before combining RT GI, RT AO, RT reflections and RT shadows.
- Do not infer that increasing GPU draw complexity, frame buffering or async queues helps the small present-day G-019 workload; demand a real consumer and profile.

---

## 7. Visual effects and rendering quality workstream

**Implementation policy:** Each effect starts as an isolated rendering experiment with a representative RelayCore fixture (industrial room, emissive device, reflective surface, static props, moving enemy, viewmodel, RmlUi/ImGui overlay). Then integrate into game settings and compare against existing SDR/HDR baselines. Every effect must have an **off** configuration and its own performance budget. Do not demand every effect be enabled simultaneously on modest hardware.

| Feature | Rendering inputs and pass work | Asset requirements / visual failure modes |
|---|---|---|
| **Color pipeline and SDR/HDR correctness** | Explicit scene-linear lighting, exposure, tone mapping, output transfer functions, UI compositing, screenshot/readback behavior; document `legacy_gamma`/`hardware_srgb` transitions | No new authored textures needed; carefully validate current highlights and SDR-darkness/HDR-brightness complaints first |
| **True HDR display output** | Enumerate surface formats/colorspaces; optional HDR10 (`VK_COLOR_SPACE_HDR10_ST2084_EXT` plus suitable format) and/or scRGB (`VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT`), Windows OS/display support, correct output transform, optional HDR metadata (`VK_EXT_hdr_metadata`), fallback SDR | Internal FP16 HDR is **not** proof of HDR monitor output; calibrate paper white, UI brightness and clipping on actual HDR hardware |
| **Bloom** | HDR bright-pass or energy-preserving bloom, downsample/upsample, stable exposure/threshold/composite | Existing native emissive maps/factors support selective glow; prevent UI bleed, halos, low-frequency washout |
| **SSAO / GTAO** | Depth and normals, sample/reprojection/filter, composite with indirect lighting | Existing AO maps already present: avoid AO double counting, screen-space halos and dark outlines; keep non-screen AO baseline |
| **SSR** | Hi-Z/depth tracing, normals, roughness, hit confidence, rejection/denoise, fallback to existing reflection probes | Existing PBR maps are usable; off-screen reflectors cannot appear in SSR, so probe fallback is essential |
| **SSGI** | Screen-space diffuse rays/accumulation, temporal denoise, rejection, probe/SH combination | Existing albedo/normals and probes usable; avoid light leaking, energy double-counting and unstable indirect illumination |
| **Volumetric fog/light shafts** | Volume/froxel representation where justified, injection of local lights, density/anisotropy, shadowing, spatial+temporal filtering, integration with distance fog | Usually no new mesh/texture needed; optional authored volume parameters and local density, gameplay sightline constraints |
| **TAA** | Previous/current matrices and per-object previous transforms, velocity buffer, jitter, camera cuts/resets, history validation and clamping/sharpening | No texture reauthoring; **moving enemies, animated props and viewmodel** need correct velocities or separate anti-ghosting policy |
| **TAAU / temporal upscaling** | Render scale, motion vectors, depth, exposure, jitter, reactive/transparent masks, stable UI/native-resolution composition | No PBR changes; compare native-resolution quality, fine geometry and fast camera movement |
| **FSR spatial** | Licensed official/current spatial upscaling implementation where useful | No history/velocity needed; compare against native and TAAU, do not mistake sharpening for reconstructed detail |
| **FSR temporal / optional frame generation** | Evaluate actual official AMD SDK/version, Vulkan compatibility, licenses, integration and validation requirements; temporal path requires motion/depth/exposure/reactive inputs; frame generation adds presentation/latency/UI complexity | Separate **temporal upscaling** from **frame generation**; frame generation is optional and must not be a minimum-mode dependency or be reported as real simulation FPS |
| **Motion blur** | Velocity-buffer blur, camera/weapon policy, sample/temporal stability | No texture changes; default off or conservative for readability and comfort |
| **Depth of field** | Depth-based blur/circle-of-confusion, focus/camera settings | No texture changes; typically disabled during active FPS play, optional for cinematics/menus |
| **Dynamic resolution** | GPU-time target and resolution controller, sizing-dependent targets/history invalidation, coherent HUD/viewmodel composition | No asset changes; avoid oscillation/hitches and require measurable GPU pressure |
| **RT reflections/shadows/optional GI** | See §6 RT foundations; hybrid compositor/denoiser, explicit fallback and reflection/lighting arbitration | Existing static triangle meshes are viable BLAS input; add alpha/double-sided/transparent material semantics deliberately; no automatic GI magic |

### 7.1 Frame data to add before temporal techniques

- Stable current and previous **jittered and unjittered** view-projection matrices and exposure history.
- Per-instance and per-animated-object previous transforms, including object creation/destruction/teleport cases.
- Screen velocity texture with documented units, sign, coordinate conventions and depth relation; distinguish camera motion from object motion.
- Depth/normal/color/material inputs with defined space and encoding; explicit history invalidation for scene switch, save/reload/respawn, teleports, camera cuts, window resize, FOV changes and render-scale changes.
- Defined treatment of first-person weapon, transparent particles, emissive highlights, RmlUi and developer overlays.
- Debug visualizers: velocity vectors, depth, normals, roughness, AO, reflection hit confidence, SSGI, history acceptance/rejection and light/fog volumes.

### 7.2 Effect composition and ordering

Establish a **documented frame dependency graph** before implementing several passes. One possible architecture (subject to measured complexity and artistic comparison):

```text
Prepared RenderQueue (unchanged game/engine boundary)
    -> depth/normal/velocity preparation as needed
    -> raster opaque + shadow passes (existing)
    -> optional GPU AO / indirect light / reflections / RT composites
    -> forward transparents, emissive and viewmodel (policy per effect)
    -> HDR volumetrics / bloom
    -> optional temporal AA / temporal upscale / spatial upscale
    -> exposure + tone map + explicit output color transform
    -> RmlUi + ImGui composition at the intended display stage
    -> SDR or optional HDR swapchain presentation
```

This is **not** a requirement to implement a generic render graph. Keep named passes, explicit dependencies, owned targets and predictable resource lifetimes until the actual dependency complexity warrants further abstraction. Test UI layering, shader reload, screenshots and gamma at each change.

---

## 8. Asset pipeline: what changes and what remains compatible

**Rule: do not reauthor or re-export all GLB models/textures just to enable renderer features.** Preserve existing `.rcmodel` readers and content staging. Introduce versioned new product data only if the next feature has a concrete consumer.

| Feature group | Existing assets reusable? | New data the compiler/runtime may need |
|---|---|---|
| Bloom, SSAO/GTAO, SSR, SSGI, volumetric fog, DoF, motion blur, TAA/FSR, HDR output | **Yes** | Depth/normals, velocity/history, exposure, lights, per-effect authored optional controls; existing PBR/AO/emissive textures remain valid |
| Reflection quality / PBR | **Yes** | Existing normal/metallic/roughness/AO/emissive map semantics; verify linear vs sRGB and tangent spaces; optional material-specific flags |
| Instancing / GPU culling | **Yes** | Reusable mesh IDs, bounding sphere/AABB, instance transform/batch data, per-instance metadata |
| Mesh shader path | **Yes, as source** | Offline meshlet/cluster bounds and indexes; fallback traditional indexed product path stays |
| Hardware ray tracing | **Yes, as source** | BLAS/TLAS build inputs, instance transforms, optional alpha-mask/two-sided/opacity semantics, dynamic update tracking |
| Temporal passes | **Yes** | Previous world transform and camera history for dynamic entities; camera cuts, motion masks, transparency exceptions |
| Texture compression/streaming | **Usually yes, as source** | Optional KTX2/compressed mip products when memory/bandwidth budget justifies them; retain existing texture readers |
| True HDR output | **Yes** | Output transforms/display calibration, not new PBR texture files |

**Existing current asset capabilities:** `src/relaycore/render/model_asset.h` has metallic/roughness maps, AO texture/strength and emissive factor/texture; roadmap documents completed technical IMG-01–IMG-06 work with pending visual acceptance. Verify exact native product version on the latest commit before editing serializers.

**Material issues requiring explicit tests:**

- Distinguish base-color/emissive **sRGB** from normal/roughness/metallic/AO **linear** data; no double encoding through SDR/HDR output.
- Alpha masks, transparent objects, two-sided materials, normal/tangent conventions and skinned/morphing meshes need deliberate support only where a real consumer exists (especially RT/velocity paths); avoid silently treating unsupported models as correct.
- Do not bake specific Vulkan handles, descriptor indexes or GPU acceleration structure layouts into portable serialized asset identity.
- Keep authoring source → compiler → versioned native product → staging/manifest → prepared render data → renderer-owned GPU objects.
- Reuse meshoptimizer where it materially benefits meshlet/LOD/vertex processing, and KTX2/library choices only after checking dependency ownership/actual versions under `docs/DEPENDENCY_STACK.md`.
- For rendering-only features, add screen-space/debug fixtures before touching the asset compiler. Only alter product schema for a feature requiring persistent, reproducible offline data.

---

## 9. Ordered milestone sequence and deliverables

This is a **multi-session program**. A coding agent must take **one bounded milestone at a time**, run its specified checks, and stop for evidence/review before starting the next. Independent features can be reordered when profiling, dependencies or owner preference make another order clearly better.

| ID | Milestone | Concrete deliverable | Non-negotiable gate |
|---|---|---|---|
| **VK14-00** | Source, hardware and performance baseline | Inventory current Vulkan code/SDK versions, selected GPUs and tests; screenshots/Tracy baseline; initial version/capability matrix | Exact commit, build commands, data and bottlenecks documented; no speculative performance claims |
| **VK14-01** | Required Vulkan 1.4 (owner revision 2026-10-12) | Loader/device minimum, instance/VMA 1.4, capability report, tests and docs; retire 1.3 fallback | Launch/render/resize/UI/content on 1.4; older loaders/devices fail explicitly |
| **VK14-02** | SDR/HDR correctness | Isolated color-pipeline corrections, output-stage contracts and test captures | Existing SDR/HDR scene-image comparison, UI colors, readback, no unintended regressions; physical HDR separate |
| **VK14-03** | Completion/lifetime foundations + profiler data | Explicit GPU-resource last-use/deferred retirement; wait-zone instrumentation. [Implemented; local desktop gate completed during VK14-04](#vk14-04-execution--2026-10-12) | Clean sync validation during reload, resize, resource churn, failed prepare and shutdown |
| **VK14-04** | Scene scheduling and upload fixes | Two scene slots and GPU timeline waits for resource consumption. [Local technical gate complete; measured benefit and tail limit](#vk14-04-execution--2026-10-12) | Measured first-use/frame benefit; no lifetime regressions; rollback if not beneficial |
| **VK14-05** | Descriptors/pipelines/portability | Shared allocator, pipeline cache, format candidates, surface-loss policy, VMA telemetry | Stable churn, tested cache invalidation, clear errors, validation-clean |
| **VK14-06** | Compute/pass inputs | Compute shader toolchain, depth/normals/motion/temporal history targets as needed, debug visualizers | Single correct compute consumer; known synchronization and history-reset behavior |
| **VK14-07** | Visual quality I | Bloom, GTAO/SSAO, optional volumetrics and SSR with probe fallback | Visible improvement on real G-019 scenes; effect-off parity and GPU budgets |
| **VK14-08** | Temporal and resolution | TAA, optional TAAU/spatial upscaler, dynamic resolution and evaluated FSR integration | Moving-enemy/viewmodel tests, camera cuts, ghosting/shimmer review, no UI blur, predictable quality/performance |
| **VK14-09** | Visual quality II | SSGI, improved reflections, bounded DoF/motion blur | Temporal stability, no energy/AO double counting; use off/low/high comparison |
| **VK14-10** | Scalable scene submission | CPU instancing → indirect draws → optional GPU culling | Image parity, correct object picking/shadows/transparency, real CPU/GPU improvement under stress |
| **VK14-11** | Optional advanced GPU | Capability-gated bindless, mesh shaders/meshlets, VRS and async compute | Every feature passes supported+unsupported device paths and budget; indexed renderer remains usable |
| **VK14-12** | Optional hardware RT | BLAS/TLAS, one RT consumer, denoiser/composite, later RT extensions as warranted | RT-off parity, tested fallback, resource churn, performance/memory and moving-object correctness |
| **VK14-13** | Physical HDR presentation | HDR10/scRGB opt-in, OS/display detection, output transform/metadata where supported | Actual HDR monitor verification; SDR fallback across ordinary displays |
| **VK14-14** | Cross-feature integration and release hardening | Presets, quality tiers, hardware matrix, end-to-end captures/performance, docs, user playtest | No GPU validation warnings/errors, stable feature toggles, no new save/editor/gameplay regressions |

**Dependencies:** `VK14-01` before any Vulkan 1.4-only path; `VK14-03` before removing host waits or implementing independent concurrent frames; `VK14-06` before temporal/screen-space effects; meshlets before mesh shaders; actual RT dependency/extension/geometry ownership before an RT effect; real HDR-capable output before claiming HDR10 support.

**Scope policy:** Every milestone should have an explicit task, affected files, feature contract, built fixture, tests, screenshots and profiling evidence; one new technique or one linked safe refactor at a time. Do not implement unused future scaffolding merely because a later milestone is listed here.

---

### Execution slices and dependency clarification

The parent milestone IDs above group a program; they are not single implementation patches. Use these bounded child tasks and keep status/evidence here rather than duplicating them in the gameplay queue. All child tasks remain pending until explicitly selected.

| Parent | Separate implementation tasks |
|---|---|
| VK14-05 | 05a persistent material descriptor pooling; 05b pipeline cache; 05c depth format selection; 05d surface-loss handling; 05e memory telemetry |
| VK14-06 | 06a compute toolchain with one consumer; 06b required depth/normal inputs; 06c velocity/history contract when the first temporal consumer needs it |
| VK14-07 | 07a bloom; 07b one AO technique; 07c SSR with probe fallback; 07d volumetrics |
| VK14-08 | 08a TAA; 08b one upscaler; 08c dynamic resolution if measured GPU pressure warrants it; FSR evaluation is separate from integration and frame generation |
| VK14-09 | 09a SSGI; reflection improvements, DoF and motion blur are separate optional tasks |
| VK14-10 | 10a CPU instancing; 10b indirect draws; 10c GPU culling if measurement justifies it |
| VK14-11 | 11a descriptor indexing; 11b meshlets/mesh shaders; 11c VRS; 11d async compute |
| VK14-12 | 12a acceleration structures plus one RT effect and its fallback; further RT effects require separate tasks |

Any SSR, AO or volumetric implementation using temporal accumulation depends on **06c**, including history reset and motion conventions, before integration. It does not require TAA itself. SSGI likewise requires the temporal inputs and rejection policy it actually consumes. Avoid allocating all optional buffers in 06a. Physical HDR can move earlier if display-output correctness is the selected priority; it need not wait for RT, mesh shaders or other unrelated effects.

For each selected child task record fixture, baseline settings, measurable budget, fallback, affected files and evidence. VK14-00/01 execution is recorded above; later child tasks remain pending. Routine verification does not create a ZIP or release; packaging requires an explicit packaging/release request.

## 10. Compatibility, validation and release test matrix

### 10.1 Mandatory test profiles

| Profile | What it verifies |
|---|---|
| **A — Loader/device below Vulkan 1.4** (when available) | Explicit startup rejection with an actionable diagnostic; not a rendering profile |
| **C — Vulkan 1.4** on supported GPU | Required core API, enabled features and performance |
| **D — Vulkan 1.4 lacking an optional RT/mesh/VRS/HDR capability** | Optional features do not become de facto hard requirements |
| **E — Debug validation + sync validation and targeted GPU-assisted validation** | Resource/barrier/layout/descriptor correctness |
| **F — Optimized real gameplay + editor + UI** | Performance, save/continue, continuous session, material reload, authoring workflow |
| **G — SDR display + genuine HDR10/scRGB monitor** | Output transfer, color space, UI luminance; capture not a substitute for physical HDR |

If specific physical hardware is unavailable, mark that test **blocked**; do not invent coverage or mark unsupported-path tests as passed merely because a software mock was exercised.

### 10.2 Minimum regressions per affected milestone

- Existing graphics baseline tests (color pipeline, HDR pipeline, MSAA/shadows, renderer materials, probes, RmlUi, viewmodel, G-019 route), content validation, unit tests, save/load/lifecycle and expected staging paths.
- Resize, window minimize/restore, display mode/VSync, swapchain out-of-date/surface-loss (with injection when possible), shader reload (success/failure), texture/material reload and entity/session/level replacement.
- Hard cases: release CPU owner before GPU completion; both frames in flight; burst uploads; memory budget pressure; render-scale changes; rapid camera motion/cuts; transparent/alpha masked assets; reflection-probe fallback; RT/mesh/VRS unsupported devices.
- Image comparisons in SDR and internal HDR; animated clips for shimmer, ghosting, flicker, light leaking, viewmodel motion and fog stability. Screenshots alone are insufficient for temporal effects.
- Collect measured GPU cost **per pass** and complete frame p50/p95/p99; record memory footprint, first-frame/session load and shader-pipeline compilation costs.
- Full in-game **G-019** human playthrough is a separate acceptance activity and must not be auto-closed by test-only evidence.

### 10.3 Build and verification procedure for Codex

1. Inspect latest `CMakePresets.json`, tests and `docs/DEVELOPMENT.md` before choosing actual commands. Existing MSVC development workflow includes `cmake --preset msvc-debug`, `cmake --build --preset demo-debug`, relevant `ctest --preset graphics-debug ...` and optimized `msvc-dev` / `msvc-relwithdebinfo` profiling. Use **only** presets and test patterns currently present.
2. Prefer the narrowest affected build/tests first, then validation and selected graphics/lifecycle runs; expand when shaders, device selection or submission contracts change.
3. Do not claim a performance gain from Debug/validation-enabled measurements; use same route, GPU, driver, resolution, content/quality settings and optimized build before/after.
4. Use RenderDoc for texture/depth/velocity/transition/pipeline validation, Tracy for host/GPU synchronization and actual driver/perf data; capture artifact paths, counts, warnings, GPU info and meaningful screenshots/videos.
5. Inspect the diff, run `git diff --check`, and report exact paths, commands, result counts, blocked checks, before/after metrics and acceptance status. Do not auto-commit, push or mark tasks done unless explicitly requested.

---

## 11. Major risks and explicit stop conditions

| Risk | Required mitigation or stop condition |
|---|---|
| Accidental optional-feature hard requirement | Vulkan 1.4 remains the API baseline; test unsupported optional RT/mesh/VRS/HDR paths independently |
| Synchronization regression during frame overlap | B1 owner/completion invariants **before** B2; sync validation, reload/resize tests and GPU-last-use recording |
| Feature explosion without product value | Each expensive renderer technique gets its own slice, visible fixture, measurable budget and rollback/off policy |
| Shader/pipeline/descriptor churn | Reuse current Slang pipeline, build/publish/retire policies; avoid speculative render graph/job-system rewrites |
| Temporal artifacts | Motion vectors, history invalidation, moving-enemy/viewmodel captures, camera-cut/reprojection tests |
| AO/GI/reflection double counting | Define light/material composition and fallback priority before combining GTAO, SSGI, SSR, probes and RT |
| HDR incorrect output | Separate scene HDR, SDR tone mapping and **actual** monitor HDR swapchain/color-space paths; physical display review |
| Memory / RT acceleration structures | Track heap budgets, BLAS/TLAS/update costs, residency, feature budgets and quality tiers |
| Models/asset compatibility | Preserve versioned `.rcmodel` readers/receipts; add optional compiler products not wholesale GLB reauthoring |
| Unavailable hardware or blocked external SDK | Mark features pending/blocked; verify SDK/source licenses/dependencies before integration; no invented test success |
| Gameplay quality displaced indefinitely | Keep G-019 gameplay/art acceptance a tracked, independent outcome; renderer advances do not demonstrate fun or compelling content |

A developer may **decline or defer a specific optimization** if measurement shows a clear regression or no plausible benefit on supported target hardware. This must be recorded as an explicit engineering decision with evidence, not silently skipped while claiming the entire feature set is complete.

---

## 12. Codex execution contract / concise handoff prompt

> You are implementing the RelayCore Vulkan 1.4 renderer modernization program. The current planning specification is `docs/RENDERER_MODERNIZATION_PLAN.md`. First inspect the current `dev` HEAD and repository rules (`AGENTS.MD`), then relevant existing Vulkan/architecture/dependency/roadmap documents and directly affected source/tests. Compare this file's baseline to the latest code; never implement already-completed work twice. Vulkan 1.4 is **required**, with no 1.3 fallback or forced-1.3 tests; optional advanced capabilities must not become baseline dependencies. Preserve existing engine/game boundaries, native models/PBR/assets, RmlUi player UI, ImGui diagnostics, editor, G-019, save/continue and build/validation workflows.
>
> Work on **one explicitly selected pending milestone**, not the whole backlog in one pass. Before changes, write the factual delta, precise scoped design, compatibility rules, tests, risks and rollback. Implement only the smallest complete patch. Run relevant real Windows/MSVC/Vulkan tests when available; where tools/GPU hardware are unavailable, record exact blockers. Capture evidence and per-feature cost instead of equating compiling with visual quality. Do not add generic managers, virtual backends, render graphs, ECS, dependencies or configuration scaffolding without a concrete consumer. Keep existing stable code; do not commit/push or silently change gameplay/art direction. Finish with changed files, executed commands, evidence, unverified gates and the next ready milestone.

**First implementation deliverable when implementation is explicitly requested without a narrower task:** recheck the current-state delta, required Vulkan 1.4 capability contract and acceptance matrix; select the next pending milestone using the execution evidence above. Do not repeat completed VK14-00/01 work or interpret this document alone as permission to implement the remaining backlog automatically.

---

## 13. Primary technical references

- Khronos — Vulkan 1.4 release announcement: https://www.khronos.org/news/press/khronos-streamlines-development-and-deployment-of-gpu-accelerated-applications-with-vulkan-1.4
- Vulkan core revisions and promoted features: https://docs.vulkan.org/spec/latest/appendices/versions.html
- Vulkan version / porting guide: https://docs.vulkan.org/guide/latest/versions.html
- Vulkan specification — instance/device versions, extension promotion and compatibility: https://docs.vulkan.org/spec/latest/chapters/extensions.html
- Vulkan 1.4 feature proposal and streaming-transfer rationale: https://docs.vulkan.org/features/latest/features/proposals/VK_VERSION_1_4.html
- Khronos Vulkan guide: https://docs.vulkan.org/guide/latest/
- Khronos Vulkan synchronization guide: https://docs.vulkan.org/guide/latest/synchronization.html
- Khronos Vulkan pipeline cache: https://docs.vulkan.org/guide/latest/pipeline_cache.html
- Khronos ray tracing extension overview: https://www.khronos.org/blog/vulkan-ray-tracing-final-specification-release
- Khronos ray tracing guide: https://github.khronos.org/Vulkan-Site/guide/latest/extensions/ray_tracing.html
- VMA statistics and budgets: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/statistics.html
- VMA staying within memory budget: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/staying_within_budget.html
- Khronos Vulkan samples (extension examples, not drop-in engine architecture): https://github.com/KhronosGroup/Vulkan-Samples
- AMD GPUOpen FidelityFX developer resources: https://gpuopen.com/fidelityfx-superresolution/

All links refer to **public technical references**, not evidence that a feature is present or validated in RelayCore. Recheck actual SDK versions, extension specifications, third-party license/API requirements and current local dependencies when implementing a milestone.

---

## Final acceptance checklist

- [ ] Runtime requires Vulkan 1.4 for loader/device/instance/VMA and explicitly rejects older loaders/devices.
- [ ] Optional RT/mesh/VRS/bindless/async/HDR feature paths are explicitly guarded by complete capability checks.
- [ ] GPU completion, uploads, scene scheduling, descriptors, pipelines, swapchain lifetime and memory telemetry have passing ownership and stress tests.
- [ ] Each requested rendering technique has a concrete, validated implementation or an explicitly evidence-backed blocked/deferred status; no nominal feature checkboxes.
- [ ] Native model/texture/product compatibility is preserved; generated optional data uses versioned products and documented fallback.
- [ ] SDR and physical HDR display outputs are separately verified where supported, without player HUD or gamma regressions.
- [ ] Performance and visual quality are documented with reproducible baseline/after data, frame-time distributions and motion captures.
- [ ] Gameplay/editor/session/save/continue/G-019 regression tests and author acceptance remain independently tracked.
- [ ] Current project documentation and development tickets reflect actual implementation **without duplicating the active gameplay roadmap**.

**Scope of this document:** Planning and static source/specification research only. **No repository modifications, builds, validation runs, hardware benchmarks or graphics captures were performed to produce this file.**
