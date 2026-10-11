# RelayCore — Vulkan 1.4 renderer modernization

This plan owns VK14 milestones, dependencies and acceptance gates. Execution reports are separate; current runtime ownership belongs to [ARCHITECTURE](ARCHITECTURE.md#graphics-backend-direction), dependency responsibilities to [DEPENDENCY_STACK](DEPENDENCY_STACK.md), and workflow to [AGENTS](../AGENTS.MD).

VK14-00–04 have covered local technical evidence. Since the owner revision of 2026-10-12, loader/GPU Vulkan 1.4 is required; 1.3 fallback and forced-profile gates are retired. Other-GPU coverage, human/physical-HDR review and the historical unexplained presentation discrepancy remain separate limitations. Reports describe their dated builds, not a fresh verification of current HEAD.

## Execution reports

[Baseline](renderer/VK14-00.md) · [API negotiation history](renderer/VK14-01.md) · [Color correctness](renderer/VK14-02.md) · [Resource lifetime](renderer/VK14-03.md) · [Scene scheduling](renderer/VK14-04.md) · [Material pooling](renderer/VK14-05a.md) · [Pipelines and portability](renderer/VK14-05.md) · [Required 1.4 revision](renderer/vulkan-1.4-contract-2026-10-12.md).

<a id="9-ordered-milestone-sequence-and-deliverables"></a>
## Milestones and acceptance

This is a **multi-session program**. A coding agent must take **one bounded milestone at a time**, run its specified checks, and stop for evidence/review before starting the next. Independent features can be reordered when profiling, dependencies or owner preference make another order clearly better.

| ID | Status / evidence | Milestone | Concrete deliverable | Non-negotiable gate |
|---|---|---|---|---|
| **VK14-00** | [Local technical gate](renderer/VK14-00.md) | Source, hardware and performance baseline | Inventory current Vulkan code/SDK versions, selected GPUs and tests; screenshots/Tracy baseline; initial version/capability matrix | Exact commit, build commands, data and bottlenecks documented; no speculative performance claims |
| **VK14-01** | [1.4-only revision](renderer/vulkan-1.4-contract-2026-10-12.md) | Required Vulkan 1.4 (owner revision 2026-10-12) | Loader/device minimum, instance/VMA 1.4, capability report, tests and docs; retire 1.3 fallback | Launch/render/resize/UI/content on 1.4; older loaders/devices fail explicitly |
| **VK14-02** | [Local technical gate](renderer/VK14-02.md) | SDR/HDR correctness | Isolated color-pipeline corrections, output-stage contracts and test captures | Existing SDR/HDR scene-image comparison, UI colors, readback, no unintended regressions; physical HDR separate |
| **VK14-03** | [Local technical gate](renderer/VK14-03.md) | Completion/lifetime foundations + profiler data | Explicit GPU-resource last-use/deferred retirement; wait-zone instrumentation. [Implemented; local desktop gate completed during VK14-04](renderer/VK14-04.md#vk14-04-execution--2026-10-12) | Clean sync validation during reload, resize, resource churn, failed prepare and shutdown |
| **VK14-04** | [Local technical gate](renderer/VK14-04.md) | Scene scheduling and upload fixes | Two scene slots and GPU timeline waits for resource consumption. [Local technical gate complete; measured benefit and tail limit](renderer/VK14-04.md#vk14-04-execution--2026-10-12) | Measured first-use/frame benefit; no lifetime regressions; rollback if not beneficial |
| **VK14-05** | [05a local technical gate](renderer/VK14-05a.md); [05b/c local technical gates](renderer/VK14-05.md); 05d/e in progress | Descriptors/pipelines/portability | Shared allocator, pipeline cache, format candidates, surface-loss policy, VMA telemetry | Stable churn, tested cache invalidation, clear errors, validation-clean |
| **VK14-06** | `pending` | Compute/pass inputs | Compute shader toolchain, depth/normals/motion/temporal history targets as needed, debug visualizers | Single correct compute consumer; known synchronization and history-reset behavior |
| **VK14-07** | `pending` | Visual quality I | Bloom, GTAO/SSAO, optional volumetrics and SSR with probe fallback | Visible improvement on real G-019 scenes; effect-off parity and GPU budgets |
| **VK14-08** | `pending` | Temporal and resolution | TAA, optional TAAU/spatial upscaler, dynamic resolution and evaluated FSR integration | Moving-enemy/viewmodel tests, camera cuts, ghosting/shimmer review, no UI blur, predictable quality/performance |
| **VK14-09** | `pending` | Visual quality II | SSGI, improved reflections, bounded DoF/motion blur | Temporal stability, no energy/AO double counting; use off/low/high comparison |
| **VK14-10** | `pending` | Scalable scene submission | CPU instancing → indirect draws → optional GPU culling | Image parity, correct object picking/shadows/transparency, real CPU/GPU improvement under stress |
| **VK14-11** | `pending` | Optional advanced GPU | Capability-gated bindless, mesh shaders/meshlets, VRS and async compute | Every feature passes supported+unsupported device paths and budget; indexed renderer remains usable |
| **VK14-12** | `pending` | Optional hardware RT | BLAS/TLAS, one RT consumer, denoiser/composite, later RT extensions as warranted | RT-off parity, tested fallback, resource churn, performance/memory and moving-object correctness |
| **VK14-13** | `pending` | Physical HDR presentation | HDR10/scRGB opt-in, OS/display detection, output transform/metadata where supported | Actual HDR monitor verification; SDR fallback across ordinary displays |
| **VK14-14** | `pending` | Cross-feature integration and release hardening | Presets, quality tiers, hardware matrix, end-to-end captures/performance, docs, user playtest | No GPU validation warnings/errors, stable feature toggles, no new save/editor/gameplay regressions |

**Dependencies:** `VK14-01` before any Vulkan 1.4-only path; `VK14-03` before removing host waits or implementing independent concurrent frames; `VK14-06` before temporal/screen-space effects; meshlets before mesh shaders; actual RT dependency/extension/geometry ownership before an RT effect; real HDR-capable output before claiming HDR10 support.

**Scope policy:** Every milestone should have an explicit task, affected files, feature contract, built fixture, tests, screenshots and profiling evidence; one new technique or one linked safe refactor at a time. Do not implement unused future scaffolding merely because a later milestone is listed here.

---

### Execution slices and dependency clarification

The parent milestone IDs above group a program; they are not single implementation patches. Use these bounded child tasks and keep status/evidence here rather than duplicating them in the gameplay queue. The owner selected all remaining VK14-05 tasks on 2026-10-12; execute and verify them in bounded slices.

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

For each selected child task record fixture, baseline settings, measurable budget, fallback, affected files and evidence. Execution and remaining child tasks are recorded in the table. Routine verification does not create a ZIP or release; packaging requires an explicit packaging/release request.

### Effort и экономия токенов — предпочтение автора, 2026-10-12

Стартовые уровни для реализации отдельных задач, а не всего parent milestone сразу.
Названия задач и зависимости находятся выше; эта таблица не меняет их статусы.
Для следующего шага **05b ставить `high`**. Effort выбирается в настройках запуска;
этот файл сам настройку не переключает.

| Пункты VK14 | `medium` | `high` | `xhigh` |
|---|---|---|---|
| 00–01 | — | 00 baseline; 01 Vulkan 1.4 | — |
| 02–04 | — | — | 02 SDR/HDR; 03 lifetime; 04 scheduling/uploads |
| 05 | 05c depth formats; 05e memory telemetry | 05a material pools; 05b pipeline cache; 05d surface loss | — |
| 06 | — | 06a compute consumer; 06b depth/normals | 06c motion/history |
| 07 | — | 07a bloom; 07b AO | 07c SSR; 07d volumetrics |
| 08 | — | 08c dynamic resolution; оценка FSR | 08a TAA; 08b upscaler; внедрение FSR |
| 09 | — | DoF; motion blur | 09a SSGI; улучшение отражений |
| 10 | — | 10a CPU instancing; 10b indirect draws | 10c GPU culling |
| 11 | — | 11c VRS | 11a descriptor indexing; 11b meshlets/shaders; 11d async compute |
| 12 | — | — | 12a BLAS/TLAS + первый RT effect/fallback |
| 13–14 | — | — | 13 physical HDR; 14 cross-feature integration |

- Отдельный запуск готовых проверок и запись evidence: `medium`. Исправление
  обнаруженной ошибки оценивается по её причине, а не по effort запуска тестов.
- Повышать `high` до `xhigh` при конкретном препятствии: неясная GPU lifetime,
  synchronization или temporal проблема. `max` оставлять для ограниченного разбора
  сложного сбоя, который не удалось разрешить на `xhigh`; заранее его не назначать.
- Соблюдать [AGENTS](../AGENTS.MD#evidence-and-scope): читать выбранный slice,
  нужные контракты и прямые call sites; переиспользовать собранное evidence и
  выполнять узкие проверки. Не пересказывать весь план и не повторять успешные
  дорогие проверки без новых изменений или нерешённого вопроса.
- Ответы и отчёты держать краткими: результат, значимый риск, фактически выполненные
  проверки, commit и ссылка на evidence. Экономия токенов не отменяет acceptance gates.

Это рабочая оценка сложности RelayCore, а не измеренное сравнение моделей.
Общие tradeoffs уровней описаны в
[OpenAI reasoning effort](https://developers.openai.com/api/docs/guides/reasoning#reasoning-effort).
Поддерживаемые уровни зависят от выбранной модели.

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

Use [documentation routing](README.md) to read the selected milestone, its dependencies, affected runtime contract and nearest source/tests. [AGENTS](../AGENTS.MD) owns workflow; this plan owns only VK14 scope, status and evidence.

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

### 2.3 Historical baseline

[Pre-change code findings and baseline](renderer/VK14-00.md#23-historical-missingincomplete-backend-mechanisms-at-the-stated-commit). Recheck affected source before implementing a pending slice.

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

Follow [AGENTS](../AGENTS.MD#build-and-verification), [DEVELOPMENT](DEVELOPMENT.md#узкие-проверки) and the verify/profile skills. Record exact commands, counts/skips, validation output, image comparisons and measured per-pass/frame cost in the selected milestone report. Use the same optimized workload/settings for before/after; Debug is not performance evidence.

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

## 12. Execution boundary

Work on one explicitly selected pending slice. Record its concrete consumer, scope, compatibility/fallback, measurable budget, tests and rollback here before implementation. Recheck completed reports instead of repeating their work. Repository workflow and automatic structured local commits belong to [AGENTS](../AGENTS.MD#git-and-handoff).

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

<details>
<summary>Earlier report links</summary>

<a id="current-local-delta--2026-10-11"></a>[Current local delta — 2026-10-11](renderer/VK14-00.md#current-local-delta--2026-10-11)

<a id="vk14-00-execution--2026-10-11"></a>[VK14-00 execution — 2026-10-11](renderer/VK14-00.md#vk14-00-execution--2026-10-11)

<a id="vk14-01-execution--2026-10-11"></a>[VK14-01 execution — 2026-10-11](renderer/VK14-01.md#vk14-01-execution--2026-10-11)

<a id="vk14-02-execution--2026-10-11"></a>[VK14-02 execution — 2026-10-11](renderer/VK14-02.md#vk14-02-execution--2026-10-11)

<a id="vk14-03-execution--2026-10-11"></a>[VK14-03 execution — 2026-10-11](renderer/VK14-03.md#vk14-03-execution--2026-10-11)

<a id="vk14-04-execution--2026-10-12"></a>[VK14-04 execution — 2026-10-12](renderer/VK14-04.md#vk14-04-execution--2026-10-12)

<a id="vulkan-14-only-contract-revision--2026-10-12"></a>[Vulkan 1.4-only contract revision — 2026-10-12](renderer/vulkan-1.4-contract-2026-10-12.md#vulkan-14-only-contract-revision--2026-10-12)

<a id="vk14-01-presentation-follow-up--2026-10-11"></a>[VK14-01 presentation follow-up — 2026-10-11](renderer/VK14-01.md#vk14-01-presentation-follow-up--2026-10-11)

</details>
