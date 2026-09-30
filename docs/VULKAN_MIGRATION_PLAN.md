# RelayCore — Vulkan migration plan

Status: approved graphics-backend direction and implementation plan. This is not permission to execute every phase in one task.

Basis: repository snapshot `1.3.4` supplied 2026-09-30. Current source/tests and `AGENTS.MD` override stale details. Long-term engine direction is in `docs/ENGINE_ARCHITECTURE_REFERENCE.md`.

## 1. Goal

Migrate RelayCore from its current OpenGL 4.6 renderer to a direct modern Vulkan renderer **before** the large material/HDR/lightmap/probe expansion, while preserving current game behavior and keeping engine/game/world boundaries intact.

This is a controlled backend migration, not a renderer rewrite for its own sake.

Success means:

- Vulkan is the only runtime graphics backend after parity;
- existing accepted rendering behavior is preserved unless a change is explicitly approved;
- no gameplay/world/physics code depends on Vulkan handles;
- validation is clean on covered paths;
- the renderer is ready to host later production material/HDR/lightmap/probe/instance work without another backend migration.

## 2. Current repository facts

Recheck before implementation. In the supplied snapshot:

- platform: SDL3;
- graphics: OpenGL 4.6 Core;
- GL loader: vendored glad2 in `extern/glad` and `RelayCore::glad`;
- renderer: `src/relaycore/render/*`;
- `Window` owns SDL window + OpenGL context;
- `Renderer` owns ImGui renderer integration, shaders, textures, shadow resources and other GPU resources;
- render flow already separates `PreparedWorldSceneData → WorldSceneRenderer → RenderQueue → Renderer`;
- Dear ImGui vcpkg features now include `opengl3-binding`, `sdl3-binding` and `vulkan-binding`; the runtime still uses the OpenGL backend;
- Tracy is optional through `RELAYCORE_ENABLE_TRACY`;
- vcpkg manifest mode now pins builtin baseline `a42757564758c60651ebc616d9b000b7b91249ef` (VK-D00, 2026-10-01); the pre-refresh snapshot used `40f3c709db80acf154ac4b17a1f83c564ebd022e`;
- Windows/MSVC/Ninja/CMake presets are authoritative for commands.
- repository `VERSION` and vcpkg `version-string` are `1.3.4`; current gameplay/graphics parity evidence must include the newer shotgun/weapon-selection path and `pressure_transfer.toml`, not only older `power_restoration.toml` captures.

Do not treat an older audit or this plan as proof that those facts remain true later.

## 3. Chosen stack

### 3.1 Runtime/build dependencies

| Responsibility | Choice | Policy |
| --- | --- | --- |
| platform/window/input | SDL3 with vcpkg `vulkan` feature | keep existing owner; use SDL Vulkan surface APIs |
| Vulkan API | direct Vulkan | no SDL_GPU abstraction |
| loader | normal Vulkan loader | simplest path with current vcpkg ImGui Vulkan backend |
| bootstrap | `vk-bootstrap` | instance/device/queue/swapchain selection only |
| memory | Vulkan Memory Allocator | allocation/suballocation/budget/mapping, not renderer architecture |
| shader source/compiler | Slang → SPIR-V | build-time/offline by default |
| UI | Dear ImGui SDL3 + Vulkan backends | keep UI/debug optional from runtime architecture |
| profiling | Tracy Vulkan | preserve existing profiling workflow |
| validation | Khronos Validation Layers | standard + focused sync/GPU-assisted modes |
| frame debugging | RenderDoc | external developer tool |
| post-migration engine dependencies | see `DEPENDENCY_STACK.md` | do not preinstall physics/asset/nav/audio libraries during Vulkan parity |

### 3.2 Why volk is not default initially

`volk` is useful when the application consistently uses a custom meta-loader and does not directly link the Vulkan loader. The current vcpkg `imgui[vulkan-binding]` package links `Vulkan::Vulkan`. Mixing the two models gives no useful architectural benefit during migration and creates symbol/build configuration hazards.

Therefore:

- **do not add volk in VK-000**;
- keep one normal-loader path for RelayCore + ImGui;
- reconsider volk only in a dedicated dependency/loader task if all Vulkan consumers can move to one consistent no-prototype/custom-loader configuration;
- do not use `VOLK_NAMESPACE` merely to make an otherwise redundant dependency coexist.

This is a deliberate simplification, not a rejection of volk generally.

## 4. Version and dependency policy

### 4.1 External developer tools

Install a current LunarG Vulkan SDK. At the time this plan was written, Windows Vulkan SDK `1.4.363.0` was current. The SDK supplies validation/tools and sets `VULKAN_SDK`; the project must not assume that exact version forever.

Install the latest stable RenderDoc separately. Do not link RenderDoc into RelayCore; normal capture works by injection. Add its in-app API only if a later workflow needs programmatic captures.

Keep GPU drivers current enough to expose the selected Vulkan feature baseline.

### 4.2 vcpkg baseline strategy

Do **not** update the whole vcpkg builtin baseline in the same diff that first introduces Vulkan code. Dependency churn and renderer churn must have separate causes and separate rollback points.

The baseline selected by VK-D00 on 2026-10-01 provides the following versions. Vulkan toolchain dependencies were installed and checked in VK-000; instance/device/allocator lifetime was validated in VK-002. Entries not yet used by the manifest are registry availability, not installed or verified integrations:

| Port | Version at current project baseline |
| --- | --- |
| `vulkan-headers` | `1.4.357.0` |
| `vulkan-loader` | `1.4.357.0` |
| `vk-bootstrap` | `1.4.357` |
| `vulkan-memory-allocator` | `3.4.0` |
| `shader-slang` | `2026.18.2` |
| `imgui` | `1.92.9` |
| `sdl3` | `3.4.16#1` |
| `tracy` | `0.14.1#1` |
| `meshoptimizer` | `1.3` |

The pre-refresh registry already provided the Vulkan dependencies needed to start migration; their runtime integration was not verified. Later engine dependencies are resolved when their owning milestone begins. Because this migration establishes a new long-lived graphics foundation, use a **separate dependency-refresh ticket (`VK-D00`) before `VK-000`**. The original 2026-09-30 research snapshot listed:

| Port | Current public vcpkg version |
| --- | --- |
| `vulkan-headers` | `1.4.357.0` |
| `vk-bootstrap` | `1.4.357` |
| `vulkan-memory-allocator` | `3.4.0` |
| `shader-slang` | `2026.18` |
| `imgui` | `1.92.9` |
| `sdl3` | `3.4.16#1` |
| `tracy` | `0.14.1` |
| `meshoptimizer` | `1.2` |
| `fastgltf` | `0.9.0` |
| `joltphysics` | `5.5.0` (upstream `5.6.0`; do not override the registry merely to chase the minor release) |
| `recastnavigation` | `1.6.0#1` |
| `zstd` | `1.5.7` |
| `blake3` | `1.8.5` |
| `ktx` | `4.4.2` |

Do not copy these numbers into per-package `overrides` merely to chase latest versions. Update the vcpkg registry baseline as one controlled operation, let the manifest resolve a coherent graph, build/test the unchanged OpenGL project, and record the resulting baseline commit plus resolved versions. If the refresh causes unrelated breakage, fix or defer it **before** Vulkan implementation begins; the existing baseline remains a valid fallback.

Recommended PowerShell flow from the repository root, using the `VCPKG_ROOT` that matches the existing CMake cache:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" x-update-baseline --dry-run
& "$env:VCPKG_ROOT\vcpkg.exe" x-update-baseline
```

Inspect the exact proposed registry commit and version changes before applying it. `x-update-baseline` fetches the latest registry baseline; it does not require pulling or changing a shared local vcpkg checkout. Verify that the applied commit matches the reviewed dry-run result. Then configure/build/test the still-OpenGL repository with the actual current presets. Do not add Vulkan dependencies until that dependency-only change is accepted. The resulting manifest pins the registry commit so the selected graph stays reproducible rather than floating.

### 4.3 Manifest change when VK-000 begins

Do not add unused dependencies before implementation starts. At VK-000, evolve the manifest approximately as follows:

```json
{
  "dependencies": [
    "doctest",
    "glm",
    {
      "name": "imgui",
      "default-features": false,
      "features": [
        "opengl3-binding",
        "sdl3-binding",
        "vulkan-binding"
      ]
    },
    "miniaudio",
    {
      "name": "sdl3",
      "features": ["vulkan"]
    },
    {
      "name": "shader-slang",
      "host": true
    },
    "spdlog",
    "tomlplusplus",
    "tracy",
    "vk-bootstrap",
    "vulkan-memory-allocator"
  ]
}
```

During coexistence keep `opengl3-binding`; after Vulkan parity/removal, remove it and delete glad/OpenGL integration.

`imgui[vulkan-binding]` pulls the vcpkg Vulkan loader/headers transitively. Do not add a redundant explicit `vulkan` dependency unless CMake ownership/readability or another consumer demonstrates a need.

Do not add post-migration engine dependencies to VK-000. Their ownership and staged manifest entries are defined in `DEPENDENCY_STACK.md`. In particular, prefer base `ktx` first rather than `ktx[vulkan]` so texture product parsing/metadata does not bypass RelayCore renderer ownership; enable libktx Vulkan helpers only if a renderer-owned task later proves a concrete benefit.

### 4.4 Slang integration policy

First migration target: use `slangc` as a build tool producing SPIR-V. Do not link the Slang compiler/runtime into `relaycore_engine` solely to obtain shader compilation.

Preferred flow:

```text
*.slang
  ↓ custom CMake build step / tool invocation
*.spv
  ↓ package/copy with runtime assets
VkShaderModule
```

If developer hot reload later requires the compiler API in-process, add that separately and document/shipping-separate the compiler dependency.

## 5. Vulkan feature baseline

Develop with a current Vulkan 1.4 SDK and headers. The implementation should use modern Vulkan architecture, not Vulkan-1.0 tutorial patterns.

Required architectural facilities:

- dynamic rendering;
- synchronization2;
- debug utils/object labels in development;
- timeline semaphores where they actually simplify upload/deferred-destruction completion;
- standard presentation/swapchain support;
- `VK_KHR_swapchain_maintenance1` with `swapchainMaintenance1`, plus its instance extension dependencies, for presentation fences (VK-003);
- descriptor sets/push constants sufficient for current renderer parity.

Do not require descriptor buffers, mesh shaders, ray tracing, bindless resources, variable-rate shading or other optional modern features during migration.

Vulkan **1.3 is the minimum runtime API** for the current migration, agreed with the project owner during VK-002 on 2026-10-01. Require it for the instance and selected physical device and report it in startup diagnostics. Enable required feature bits explicitly; API version support alone does not enable device features. Vulkan 1.4 facilities may be introduced by their owning tasks after checking instance/device API, extension and feature support. On a 1.3 device, use a supported extension path or an implemented alternative; an unsupported mandatory facility must fail startup with a concrete diagnostic. Never call unsupported functionality based only on the developer GPU's version.

## 6. Architecture and patterns

### 6.1 Preserve the durable boundary

Target flow:

```text
Game/world state
      ↓
PreparedWorldSceneData / later PreparedRenderWorld
      ↓
WorldSceneRenderer / render preparation
      ↓
RenderQueue
      ↓
Renderer
      ↓
Vulkan
```

No gameplay/world object stores `VkBuffer`, `VkImage`, descriptor sets or pipeline handles.

Do not create `IRenderBackend` or a virtual one-to-one graphics API wrapper for the temporary OpenGL/Vulkan overlap. That would encode the wrong abstraction and then survive the migration as debt.

### 6.2 RAII owner pattern

GPU objects with meaningful destruction rules use small move-only owners where ownership is otherwise ambiguous, for example concepts such as:

```text
GpuBuffer
GpuImage
GpuSampler
Pipeline
SwapchainState
```

Names are not mandated. A wrapper is justified only when it owns lifetime/invariants. Do not wrap every plain Vulkan struct/value.

VMA allocations belong to the same owner as the associated buffer/image unless a concrete pooling design requires otherwise.

### 6.3 Device/renderer responsibility

Avoid a generic `VulkanManager`/`GraphicsService`. Prefer a small concrete device state owned by `Renderer` or one narrowly scoped renderer-internal owner.

It should make these relationships explicit:

```text
instance
  ↓
physical device + capabilities
  ↓
device + queues
  ↓
allocator
  ↓
swapchain/frame contexts/resources/pipelines
```

### 6.4 Frame contexts

Use a small fixed number of frames in flight. Each frame context owns only per-frame state required for safe reuse, such as:

- command pool/buffers;
- completion fence or timeline bookkeeping;
- acquire/render-complete synchronization as appropriate;
- per-frame descriptor/transient allocation if required.

Do not create hidden global synchronization state.

### 6.5 Deferred destruction

When CPU lifetime ends before GPU use completes, defer destruction until the known completion value/fence. Keep the queue local to the renderer/device lifetime domain.

Do not solve resource lifetime with routine `vkDeviceWaitIdle()` calls. Full idle waits are acceptable for shutdown and rare coarse recovery/reconfiguration where the stall is intentional and documented.

### 6.6 Upload path

Start with the simplest correct upload path using staging/transfer buffers and the graphics queue. Do not require a dedicated transfer queue/thread before profiling shows a need.

Track upload completion explicitly. Large asynchronous streaming is later asset/runtime work, not required to reach OpenGL parity.

### 6.7 Descriptors

Start explicit:

- stable descriptor layout conventions;
- per-frame/per-view data separated from material/object data where useful;
- push constants for genuinely small fast-changing data;
- descriptor pools/allocators scoped to their lifetime.

Do not start with a bindless or descriptor-buffer framework. Introduce advanced binding only from measured material/draw scaling needs.

### 6.8 Pipelines/shaders

Use dynamic rendering, explicit pipeline descriptions and Slang-generated SPIR-V. Keep shader interface/binding contracts inspectable.

Reflection is allowed to validate or generate repetitive metadata, but avoid a runtime system that silently invents layouts from arbitrary shaders and obscures ownership.

Cache pipeline data only where it reduces real startup/build cost and has a clear invalidation/version rule.

### 6.9 Swapchain rebuild

Stage swapchain-dependent CPU state, but do not assume swapchain recreation is a rollback-safe transaction:

1. detect resize/out-of-date/suboptimal state;
2. wait only for resources that must no longer be in flight;
3. create the replacement swapchain, then its dependent images/views/state;
4. publish the complete replacement set;
5. destroy old resources only after their GPU/presentation use completes.

Passing a non-null `oldSwapchain` to `vkCreateSwapchainKHR` retires it even if creation fails. Once retired, do not acquire new images from it or pass it again as a non-retired `oldSwapchain`. If replacement creation or dependent-resource preparation fails, stop normal acquire/present flow and explicitly retry recreation or report failure; preserving the old CPU owner does not restore a usable swapchain. Verify this path with an injected recreation failure as well as resize/minimize/restore.

Reuse the project's existing prepare/commit mindset; do not scatter resize repair across render passes.

### 6.10 Failure policy

Initialization failures must report the concrete missing requirement: Vulkan loader, instance/device extension, API version, queue, surface format/present mode or required feature.

Do not silently fall back to OpenGL after Vulkan becomes the supported renderer. During migration, backend selection is a developer/test mechanism, not a user-facing hidden fallback policy.

## 7. Migration sequence

Each phase must leave a reviewable, buildable state. A phase may span multiple implementation tasks.

### VK-D00 — Dependency baseline refresh

Status: **done** on 2026-10-01; execution evidence is recorded in section 14.

Scope:

- resolve and inspect the proposed registry baseline without changing a shared vcpkg checkout;
- inspect `x-update-baseline --dry-run`, then update `builtin-baseline`;
- do **not** add Vulkan-specific dependencies in this same diff;
- configure/build the existing OpenGL targets with the actual repository presets;
- run the checks appropriate for a dependency-wide change;
- record the resulting vcpkg baseline commit and resolved direct dependency versions.

Gate:

- unchanged OpenGL runtime/tests build on the refreshed dependency graph, or the refresh is explicitly reverted/deferred;
- no Vulkan implementation has started yet.

### VK-000 — Toolchain and dependency foundation

Status: **done** on 2026-10-01; execution evidence is recorded in section 14.

Scope:

- verify installed Vulkan SDK/driver with Vulkan tools;
- add vcpkg dependencies only when used;
- add Vulkan/Slang dependency discovery in `RelayCoreDependencies.cmake` without removing GL yet;
- add a minimal configure-time capability report if useful;
- establish Slang→SPIR-V build output directory and one trivial shader compile;
- establish validation/debug build switches without adding a second generic configuration framework.

Gate:

- existing OpenGL build still works;
- Vulkan headers/loader, vk-bootstrap, VMA and Slang tool are resolved deterministically;
- one SPIR-V artifact is produced by the build;
- no runtime Vulkan renderer is claimed yet.

### VK-001 — Platform/window separation

Status: **done** on 2026-10-01; execution evidence is recorded in section 14.

Scope:

- separate SDL window ownership from OpenGL context ownership;
- allow a Vulkan-capable SDL window configuration without changing gameplay/app ownership;
- obtain SDL's required Vulkan instance extensions for the next phase;
- preserve current OpenGL path temporarily.

Gate: existing GL startup still works and an isolated Vulkan-capable SDL window can be created/destroyed with its required instance extensions reported, without creating a Vulkan instance or surface yet.

### VK-002 — Instance/device/capabilities

Status: **done** on 2026-10-01; execution evidence is recorded in section 14.

Scope:

- create Vulkan instance/debug messenger with SDL's required instance extensions;
- create/destroy the Vulkan surface through SDL after instance creation; destroy the surface before its instance;
- select physical device with explicit requirements;
- create logical device/graphics+present queues;
- expose a small renderer-internal capability record;
- create VMA allocator after device function availability.

Use vk-bootstrap for selection/bootstrap, not a custom duplicate scoring framework unless project-specific policy exceeds it.

Gate: startup/shutdown is validation-clean; missing requirements produce actionable errors; no resource leaks on partial initialization failure.

### VK-003 — Swapchain and frame lifecycle

Status: done (2026-10-01); implementation and verification evidence below.

Scope:

- choose surface format/present mode/extent deliberately;
- create swapchain/images/views;
- fixed frame contexts;
- acquire → record → submit → present;
- resize/out-of-date recreation;
- debug labels and basic frame Tracy zones where appropriate.

Gate: clear-color window runs/resizes/minimizes/restores repeatedly without validation errors or device-wide idle in normal frame flow.

Presentation fences require `VK_KHR_swapchain_maintenance1` and `swapchainMaintenance1`; instance setup requires `VK_KHR_surface_maintenance1` and `VK_KHR_get_surface_capabilities2`. Check and enable these explicitly even on Vulkan 1.4 hardware. This narrows driver compatibility but provides a defined lifetime for presentation's semaphore and swapchain references without relying on device/queue idle. Missing support fails startup with a concrete diagnostic; runtime API minimum remains 1.3. See the [Khronos presentation fence contract](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html).

### VK-004 — GPU resources and upload

Status: done (2026-10-01); implementation and verification evidence below.

Scope:

- VMA-backed buffers/images;
- mapped/upload/staging path;
- image layout/state helpers only where they encode real invariants;
- deferred destruction tied to GPU completion;
- sampler ownership;
- minimal texture upload/mips sufficient for current content.

Gate: vertex/index buffers and a texture with required mips upload, reload and destroy cleanly; verify uploaded contents through readback and run focused sync validation. Visual acceptance follows in VK-005 after a graphics pipeline exists.

### VK-005 — Slang pipeline foundation

Status: **done** on 2026-10-01; joint VK-005/VK-006 execution evidence is recorded in section 14.

Scope:

- port first real vertex/fragment shader to Slang;
- build-time SPIR-V compilation;
- explicit descriptor/push-constant layout contract;
- dynamic-rendering graphics pipeline;
- development shader diagnostics with source path/entry point.

Do not port all shaders before the first working vertical slice.

Gate: one current RelayCore indexed textured mesh path renders through a Slang/Vulkan pipeline using the VK-004 upload path; reload/destruction and focused sync validation are clean.

### VK-006 — Current world/mesh parity

Status: **done** on 2026-10-01; joint VK-005/VK-006 execution evidence is recorded in section 14.

Scope:

- consume existing `RenderQueue`/prepared data;
- port blockout mesh/index buffers;
- camera/view/projection data;
- culling behavior stays CPU-side as currently accepted;
- port static OBJ model cache/texture behavior without inventing production MeshAsset yet.

Gate: representative current level geometry and static props match OpenGL placement/visibility/culling. Include at least one `pressure_transfer.toml` comparison so the newer level and shotgun/static-model presentation path are not missed.

### VK-007 — ImGui/debug HUD parity

Scope:

- enable `imgui[vulkan-binding]` while retaining SDL3 platform binding;
- initialize backend with Vulkan objects/dynamic rendering;
- port F2/debug UI/HUD integration;
- keep debug UI optional from renderer correctness.

Gate: UI renders/resizes/shuts down cleanly and does not own core renderer state.

### VK-008 — Transparency/overlays/state parity

Scope:

- opaque/overlay/transparent behavior;
- depth/cull/blend/scissor/viewport state;
- transparent ordering stays equivalent unless a separate bug is approved;
- decals/effects currently routed through render queues preserve behavior.

Gate: targeted graphics scenes match accepted ordering/depth/blend behavior.

### VK-009 — Lighting parity

Scope:

- existing directional/local light data;
- current nearest-light budget/selection;
- color-space behavior preserved;
- no clustered/bindless redesign in this phase.

Gate: current lit scenes match accepted OpenGL intent and validation stays clean.

### VK-010 — Shadow parity

Scope:

- directional cascades;
- spot shadow path;
- point-light cube shadow images/views;
- shadow culling and bias semantics;
- explicit image transitions/synchronization.

Gate: representative current shadow scenes match accepted behavior; RenderDoc confirms intended depth formats/usages/transitions.

### VK-011 — Color pipeline, fog and post parity

Scope:

- preserve `legacy_gamma`/verified sRGB semantics as required for migration comparison;
- fog/post behavior;
- swapchain color-space selection explicit;
- do **not** introduce the new HDR pipeline yet.

Gate: current graphics/color tests are replaced/ported with equivalent Vulkan evidence where tests depend on GL-specific default-framebuffer behavior.

### VK-012 — Tracy, validation and graphics regression parity

Scope:

- Vulkan Tracy GPU context/zones/collection;
- standard validation on normal debug runs;
- synchronization validation workflow;
- focused GPU-assisted workflow;
- RenderDoc capture checklist;
- replace GL-specific diagnostics/tests with Vulkan-specific equivalents rather than preserving obsolete GL assertions.

Gate: no known validation errors in covered normal gameplay/render paths; profiler capture works; graphics verification has a Vulkan baseline.

### VK-013 — Cutover and OpenGL removal

Prerequisites:

- all accepted current visual/runtime paths use Vulkan;
- user/runtime parity accepted;
- relevant tests/workflows pass;
- no required tool still depends on GL context ownership.

Remove:

- glad source/include/target;
- OpenGL loader/debug/context code;
- ImGui OpenGL binding feature;
- GL-specific shader files/helpers that have no remaining owner;
- obsolete GL graphics tests/probes after Vulkan replacements exist.

Update README/runtime requirements only here, when Vulkan is factually the supported renderer.

Gate: clean configure/build/test from a fresh build tree with no OpenGL/glad dependency.

### VK-014 — Post-migration architecture review

Read-only first:

- inspect ownership/lifetime/error paths;
- search for `Vk*` leakage into gameplay/world/public asset data;
- inspect duplicated temporary GL/VK adapters;
- inspect unnecessary waits/per-frame allocation/descriptors;
- capture a representative Tracy/RenderDoc baseline.

Only then schedule corrections. Do not combine this review with HDR/PBR work automatically.

## 8. What comes after Vulkan parity

Only after VK-013/014 should the large renderer capability program proceed normally. Non-renderer foundations follow `DEPENDENCY_STACK.md` and their own milestones rather than being bundled into Vulkan parity:

```text
production MeshAsset/ModelAsset
    ↓
TextureAsset / MaterialAsset
    ↓
production collision/world queries
    ↓
linear HDR scene rendering + tone mapping
    ↓
lightmaps
    ↓
reflection/irradiance probes
    ↓
scalable lighting/instances based on profiling
    ↓
Source 2 visual-content acceptance
```

This order is the reason for migrating now: those systems are then built once on the intended backend.

## 9. Verification matrix

Use existing preset names from the checkout; do not invent commands. At minimum, each implementation task should choose the narrowest applicable evidence from this matrix.

| Change | Required evidence |
| --- | --- |
| CMake/vcpkg/toolchain | clean configure + affected target build; record resolved package/tool versions |
| instance/device/swapchain | standard validation; repeated startup/shutdown/resize |
| barriers/synchronization/upload | synchronization validation; focused RenderDoc inspection |
| shader/pipeline | successful Slang compile; validation; frame capture for bindings/attachments |
| visual parity | same scene/camera/settings comparison; record intentional differences |
| GPU resource lifetime | validation; repeated reload/recreate/shutdown; ASan where CPU ownership changed |
| performance claim | same workload Tracy before/after; do not use validation-enabled timings |
| final cutover | debug + affected graphics/lifecycle tests + RelWithDebInfo representative run; broader workflow as current repo defines |

Validation modes are diagnostic tools, not performance configurations. GPU-assisted validation may materially change behavior/performance and should be enabled only for targeted checks.

## 10. Agent rules for Vulkan work

The implementing agent must:

- read the current phase and stop at its gate unless explicitly told to continue;
- preserve user changes and current project conventions;
- use `vk-bootstrap` and VMA for their assigned commodity responsibilities instead of reimplementing them;
- not add volk unless a task explicitly changes the loader policy consistently;
- not create a generic backend abstraction for temporary dual rendering;
- not introduce render graph, bindless, descriptor buffer, job system or async streaming as migration prerequisites;
- keep Vulkan handles below render/platform boundaries;
- make partial-initialization cleanup correct through RAII/explicit ownership;
- name/label Vulkan objects/regions enough for RenderDoc/validation diagnostics without decorative logging;
- classify every GPU resource lifetime against CPU owner and GPU completion;
- keep normal frame flow free of blanket `vkDeviceWaitIdle()`;
- use current code and actual validation output as evidence instead of copying tutorial patterns;
- inspect the diff for generated-code slop before handoff;
- report exact checks run and remaining parity work.

## 11. Developer machine setup (Windows 11)

Do this before VK-000 implementation/testing.

### Required external installs

1. **GPU driver** — install a current NVIDIA/AMD/Intel driver with Vulkan support.
2. **LunarG Vulkan SDK** — install the current Windows x64 SDK. The installer should set `VULKAN_SDK` and place SDK tools on `PATH`.
3. **RenderDoc** — install the latest stable public build separately.
4. Existing project prerequisites remain: Visual Studio/MSVC developer environment, CMake, Ninja, Git, PowerShell and vcpkg.

After SDK installation, in a new PowerShell terminal verify at least:

```powershell
$env:VULKAN_SDK
Get-Command vulkaninfo
vulkaninfo --summary
```

Exact `vulkaninfo` output depends on the driver. The implementation agent should record the device name, driver, API version and required feature support when VK-000 starts.

### vcpkg

Keep using manifest mode. Do not globally install separate copies of the project's libraries. When `vcpkg.json` is changed, normal CMake configure through the existing toolchain installs the pinned manifest dependencies.

If manually prewarming the manifest is desired, from the repository root with `VCPKG_ROOT` set:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install --triplet x64-windows
```

Do not use classic `vcpkg install <port>` commands as the canonical project state; the manifest is authoritative.

Perform the initial dependency refresh only in `VK-D00`. After that, do not change `builtin-baseline` inside ordinary Vulkan parity tickets; dependency upgrades remain separate, reviewable changes.

### Post-Vulkan dependency installation

Use `DEPENDENCY_STACK.md` as the source of truth for Jolt, fastgltf, meshoptimizer, KTX2/libktx, BLAKE3/Zstandard, Recast/Detour, Steam Audio and animation-library decisions. Do not add them all at once. Each dependency enters with its owning milestone, CMake linkage, focused tests and rollback point.

## 12. Dependency-upgrade rule after VK-D00

After the initial `VK-D00` refresh, dependency versions remain baseline-managed and should move only in dedicated dependency changes:

1. update the local vcpkg registry intentionally and inspect `x-update-baseline --dry-run`;
2. update the pinned baseline as its own reviewable change;
3. inspect important direct/transitive version changes;
4. configure/build targeted presets and run relevant tests/graphics validation;
5. inspect compiler warnings and runtime validation;
6. record the new known-good baseline.

Do not pin one transitive library to a random HEAD commit while the rest of the manifest remains baseline-managed unless a confirmed blocker requires it. Do not routinely update dependencies in the middle of a Vulkan parity ticket.

## 13. Definition of done

The migration is complete when:

- Vulkan is the normal and sole supported renderer;
- OpenGL/glad/context-specific runtime code is removed;
- current accepted game/world rendering, UI, lighting, shadows, fog/post and color behavior have Vulkan parity or explicitly accepted differences;
- Vulkan validation is clean for the documented tested paths;
- sync/resource lifetime has focused validation evidence;
- Tracy Vulkan GPU profiling and RenderDoc capture are usable;
- world/game/physics/public asset data contain no Vulkan object ownership;
- no long-lived fake backend abstraction was introduced solely for migration;
- future HDR/material/lightmap/probe work can proceed directly on Vulkan.

## 14. Execution evidence

### VK-D00 — 2026-10-01

Updated `vcpkg.json` from `40f3c709db80acf154ac4b17a1f83c564ebd022e` to `a42757564758c60651ebc616d9b000b7b91249ef` (registry commit dated 2026-09-30). The applied commit matches `x-update-baseline --dry-run`. Manifest dependency entries and C++ sources are unchanged; no Vulkan dependencies or runtime code were added.

Changed direct versions: SDL3 `3.4.12 → 3.4.16#1`, ImGui `1.92.8#1 → 1.92.9`, Tracy `0.13.1 → 0.14.1#1`. Other direct versions are listed in `DEPENDENCY_STACK.md`; fmt remains `12.2.0#1`, and the vcpkg CMake helper changes from `2024-04-23` to `2025-08-07`. Reviewed port license metadata: SDL3 `Zlib AND MIT AND Apache-2.0 AND BSD-3-Clause`, ImGui `MIT`, Tracy `BSD-3-Clause`; retain the existing license notices/deployment process. CMake continues to copy runtime DLLs through the existing rules; package size was not measured.

Environment: Windows x64, MSVC `19.51.36257.0`, existing `x64-windows` triplet. `VCPKG_ROOT` was set per process to `C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg`, matching both existing CMake caches. The unrelated default vcpkg checkout and user presets were not changed. Commands ran from a Visual Studio developer environment:

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" x-update-baseline --dry-run
& "$env:VCPKG_ROOT\vcpkg.exe" x-update-baseline
cmake --preset msvc-debug
cmake --build --preset tests-debug --target relaycore_tests relaycore_audio_lifetime relaycore_content_validation relaycore_demo relaycore_window_lifetime_tests relaycore_game_lifecycle relaycore_power_restoration_graphics relaycore_graphics_baseline relaycore_default_framebuffer_probe
ctest --preset test-debug --output-on-failure --no-tests=error
ctest --preset validation-debug --output-on-failure --no-tests=error
ctest --preset graphics-debug --output-on-failure --no-tests=error --output-junit vk-d00-graphics.xml
cmake --preset msvc-relwithdebinfo
cmake --build --preset tests-relwithdebinfo --target relaycore_tests relaycore_audio_lifetime relaycore_content_validation relaycore_demo relaycore_game_lifecycle
ctest --preset test-relwithdebinfo --output-on-failure --no-tests=error
ctest --preset validation-relwithdebinfo --output-on-failure --no-tests=error
ctest --test-dir build/msvc-relwithdebinfo -R '^graphics\.game_lifecycle$' --output-on-failure --no-tests=error
git diff --check
```

Results:

- Before refresh, existing Debug binaries passed 250 tests and all 5 graphics checks.
- Refreshed Debug configure/build passed; 250 tests, 31 content checks and 5 graphics checks passed. The graphics JUnit report contains 5 cases and no skips.
- Refreshed RelWithDebInfo configure/build passed with `RELAYCORE_ENABLE_TRACY=ON`; 250 tests, 31 content checks and the game lifecycle check passed.
- The first Debug configure failed when vcpkg could not rename the downloaded SDL archive (`Access is denied`). A retry used the cached archive and completed without deleting caches or changing ports.
- Compilation reports C4456/C4100 in existing game sources and C4834 in existing overlay tests. No warning cleanup is bundled into this dependency change.

Logs are in `build/vk-d00-*.log`; the graphics report is `build/msvc-debug/vk-d00-graphics.xml`. ASan, remote CI, a human playthrough, packaging size and a new Tracy capture were not checked in VK-D00. The gate for the existing OpenGL dependency refresh is satisfied.

### VK-000 — 2026-10-01

Enabled SDL3 Vulkan support and the ImGui Vulkan binding while retaining OpenGL. Added vk-bootstrap `1.4.357`, VMA `3.4.0` and host-tool Slang `2026.18.2`; the transitive Vulkan headers/loader resolve to `1.4.357.0` from the unchanged VK-D00 baseline. CMake resolves the loader and headers from the manifest prefix and `slangc` from that prefix's `tools/shader-slang` directory. Slang is not linked into engine/game targets.

`relaycore_vulkan_shaders` builds `generated/shaders/toolchain_check.vert.spv` from one vertex shader. The demo depends on this target; a second shader build reports no work. `relaycore_vulkan_toolchain_check` consumes the normal loader, vk-bootstrap and VMA headers, and reports loader/layer/extension availability. `RELAYCORE_ENABLE_VULKAN_VALIDATION` requires Khronos validation and debug-utils in this Debug/Asan development check; RelWithDebInfo leaves that requirement disabled. No Vulkan instance, surface, device, allocator or runtime renderer is created in this phase.

Environment: the same MSVC/vcpkg environment as VK-D00. The inherited `VULKAN_SDK=C:\VulkanSDK\1.4.350.0` points to a missing directory; commands used the installed `C:\VulkanSDK` per process without changing global settings. System `vulkaninfo` reports loader `1.4.363`, NVIDIA GeForce RTX 4060 Ti, driver `596.49`, device API `1.4.329`, `dynamicRendering`, `synchronization2`, `timelineSemaphore` and `VK_KHR_swapchain`. Khronos validation and the RenderDoc capture layer are available. The development executable uses the manifest loader `1.4.357`.

Commands run successfully from the developer environment:

```powershell
vulkaninfo --summary
vulkaninfo
cmake --preset msvc-debug
cmake --build --preset demo-debug --target relaycore_vulkan_toolchain_check relaycore_vulkan_shaders
.\build\msvc-debug\relaycore_vulkan_toolchain_check.exe
& 'C:\VulkanSDK\Bin\spirv-val.exe' --target-env vulkan1.3 build\msvc-debug\generated\shaders\toolchain_check.vert.spv
cmake --build --preset demo-debug --target relaycore_demo relaycore_tests relaycore_audio_lifetime relaycore_content_validation relaycore_window_lifetime_tests relaycore_game_lifecycle relaycore_power_restoration_graphics relaycore_graphics_baseline relaycore_default_framebuffer_probe
ctest --preset test-debug --output-on-failure
ctest --preset validation-debug --output-on-failure
ctest --preset graphics-debug --output-on-failure --output-junit vk-000-graphics.xml
cmake --preset msvc-relwithdebinfo
cmake --build --preset demo-relwithdebinfo --target relaycore_demo relaycore_vulkan_toolchain_check relaycore_vulkan_shaders relaycore_game_lifecycle
.\build\msvc-relwithdebinfo\relaycore_vulkan_toolchain_check.exe
& 'C:\VulkanSDK\Bin\spirv-val.exe' --target-env vulkan1.3 build\msvc-relwithdebinfo\generated\shaders\toolchain_check.vert.spv
ctest --test-dir build/msvc-relwithdebinfo -R '^graphics\.game_lifecycle$' --output-on-failure --no-tests=error
cmake --build --preset demo-debug --target relaycore_vulkan_shaders
dumpbin /DEPENDENTS build\msvc-debug\src\relaygame\RelayCore.exe
git diff --check
```

Results:

- Debug: demo and verification targets build; 250 tests, 31 content checks and 5 graphics checks pass. The graphics JUnit report contains 5 cases and no skips.
- RelWithDebInfo: demo/toolchain/shader/lifecycle targets build with Tracy enabled; game lifecycle passes. Development validation requirement is disabled as intended.
- Both SPIR-V artifacts pass `spirv-val` for Vulkan 1.3; disassembly confirms SPIR-V 1.5, a vertex entry point, input location 0 and position output.
- With a temporary empty `VK_LAYER_PATH`, the Debug check returns 1 and names the missing validation layer and remedy; the environment is restored afterward. An intentionally invalid shader under `build/` returns a Slang error with source path, line and column.
- Demo imports still contain SDL3/spdlog/fmt and system/runtime DLLs, with no Slang compiler dependency. Existing deployment rules remain in place. Port notices cover Vulkan loader/headers, vk-bootstrap, VMA and Slang, including Slang's bundled third-party notices.
- The first Debug build failed copying runtime DLLs with `Permission denied`; the same build succeeded on retry without deleting outputs. The source of the transient file lock was not established. Existing C4456/C4100/C4834 warnings remain outside this patch.

Logs and shader diagnostics are in `build/vk-000-*.log`; the graphics report is `build/msvc-debug/vk-000-graphics.xml`. ASan, a RenderDoc capture, remote CI, package size, Vulkan rendering and allocator lifetime remain unverified here. The VK-000 gate is satisfied.

### VK-001 — 2026-10-01

Separated SDL window ownership from GL context resource ownership: `Window` owns an SDL window through RAII and composes an optional `OpenGlContext` owner, destroyed before the window. GL attributes, context creation, sRGB capability queries, swap interval and presentation now live in `OpenGlContext`. Existing `Window` calls and application/game ownership remain compatible; the default path is still OpenGL.

The constructor's explicit `WindowGraphicsApi::vulkan` path requests `SDL_WINDOW_VULKAN` without GL attributes or context creation. `vulkan_instance_extensions()` returns SDL-owned names for consumption while the Vulkan window is alive. GL presentation operations on a Vulkan window fail explicitly rather than querying or changing another window's current GL context. No runtime backend selector, authored configuration change, Vulkan instance/surface or new dependency is added.

SDL handles Vulkan loader lifetime through window creation/destruction; required extensions are queried after creation, following the [SDL window contract](https://wiki.libsdl.org/SDL3/SDL_CreateWindow) and [extension query contract](https://wiki.libsdl.org/SDL3/SDL_Vulkan_GetInstanceExtensions).

Commands run successfully in the same developer environment as VK-000:

```powershell
cmake --preset msvc-debug
cmake --build --preset graphics-debug --target relaycore_window_lifetime_tests relaycore_vulkan_window_tests
ctest --test-dir build/msvc-debug -R '^graphics\.(vulkan_window|window_lifetime)$' --output-on-failure --no-tests=error --output-junit vk-001-window.xml
cmake --build --preset graphics-debug --target relaycore_demo relaycore_tests relaycore_window_lifetime_tests relaycore_vulkan_window_tests relaycore_game_lifecycle relaycore_power_restoration_graphics relaycore_graphics_baseline relaycore_default_framebuffer_probe
ctest --preset test-debug --output-on-failure --no-tests=error
ctest --preset graphics-debug --output-on-failure --no-tests=error --output-junit vk-001-graphics.xml
cmake --preset msvc-relwithdebinfo
cmake --build --preset demo-relwithdebinfo --target relaycore_demo relaycore_window_lifetime_tests relaycore_vulkan_window_tests relaycore_game_lifecycle
ctest --test-dir build/msvc-relwithdebinfo -R '^graphics\.(window_lifetime|vulkan_window|game_lifecycle)$' --output-on-failure --no-tests=error --output-junit vk-001-window.xml
git diff --check
```

Results:

- Debug: demo and affected verification targets build; all 250 tests and 6 graphics checks pass. RelWithDebInfo: demo builds with Tracy; all 3 selected window/lifecycle checks pass. Both graphics JUnit reports contain zero failures and zero skips.
- The standalone Vulkan window check runs without initializing the RelayCore logger, confirms no GL context, reports `VK_KHR_surface` and `VK_KHR_win32_surface`, exercises resizing/hiding, and leaves no SDL windows or current GL context after each of three create/destroy cycles.
- An injected missing Vulkan loader makes window construction fail with an SDL diagnostic naming the missing DLL; the hint is reset and subsequent window creation succeeds without retained windows.
- A Vulkan window can coexist with an active GL window; querying its GL swap interval is rejected, and GL context/presentation survive its destruction. Existing partial-construction failures in both GL color modes and delegate shutdown/destruction ordering still pass.
- Changed C++ files pass the repository's clang-format check. Existing C4456/C4100/C4834 warnings remain outside this patch.

Logs are in `build/vk-001-*.log`; reports are `build/msvc-debug/vk-001-graphics.xml` and `build/msvc-relwithdebinfo/vk-001-window.xml`. ASan, remote CI, a human playthrough, RenderDoc capture and Vulkan rendering remain unverified here. The VK-001 gate is satisfied.

### VK-002 — 2026-10-01

Added renderer-internal `VulkanDevice`, a move-only owner of vk-bootstrap instance/debug messenger, SDL surface, device/queues and VMA allocator. Its private state releases partial construction as well as normal lifetime in the order device idle → allocator → device → SDL surface → instance/debug messenger. The Vulkan window/SDL must outlive this owner. Engine/game service interfaces and the OpenGL application path remain unchanged; the owner is exercised by an isolated integration executable.

Runtime baseline is Vulkan 1.3 with explicitly enabled `dynamicRendering`, `synchronization2`, `timelineSemaphore`, swapchain support and surface-compatible graphics/present queues. Selection failures retain vk-bootstrap's detailed rejection reasons and list the requirements. SDL extensions are enabled before surface creation. Debug/Asan require validation/debug-utils and fail explicitly when missing. VMA uses device/instance function lookup through the normal loader, API 1.3, and one implementation translation unit excluded from unity compilation. Vulkan 1.4 facilities are not called in this phase; future owning tasks must check their API/extension/feature requirements as described in section 5.

Commands run in the same developer environment as VK-000:

```powershell
cmake --preset msvc-debug
cmake --build --preset graphics-debug --target relaycore_vulkan_device_tests
ctest --test-dir build/msvc-debug -R '^graphics\.vulkan_(device|missing_validation)$' --output-on-failure --no-tests=error --output-junit vk-002-device.xml
cmake --build --preset graphics-debug --target relaycore_demo relaycore_tests relaycore_window_lifetime_tests relaycore_vulkan_window_tests relaycore_vulkan_device_tests relaycore_game_lifecycle relaycore_power_restoration_graphics relaycore_graphics_baseline relaycore_default_framebuffer_probe
ctest --preset test-debug --output-on-failure --no-tests=error
ctest --preset graphics-debug --output-on-failure --no-tests=error --output-junit vk-002-graphics.xml
cmake --preset msvc-relwithdebinfo
cmake --build --preset demo-relwithdebinfo --target relaycore_demo relaycore_vulkan_device_tests relaycore_game_lifecycle
ctest --test-dir build/msvc-relwithdebinfo -R '^graphics\.(vulkan_device|game_lifecycle)$' --output-on-failure --no-tests=error --output-junit vk-002-device.xml
cmake --preset msvc-asan
cmake --build --preset demo-asan --target relaycore_vulkan_device_tests
ctest --test-dir build/msvc-asan -R '^graphics\.vulkan_(device|missing_validation)$' --output-on-failure --no-tests=error --output-junit vk-002-device.xml
git diff --check
```

Results:

- NVIDIA GeForce RTX 4060 Ti, physical API `1.4.329`, selected instance/VMA API `1.3`, graphics/present queue family `0`. Debug and Asan validation are enabled; RelWithDebInfo validation is disabled as intended.
- Integration checks cover two late constructor failures after allocator creation, three successful create/move/destroy cycles on the same window, actual VMA buffer allocation/release, allocator/device ownership, timeline semaphore creation/counter query and surface presentation support. CTest rejects any validation warning/error or device shutdown error, including teardown output; none were reported.
- The missing-validation test runs in a child process with an empty `VK_LAYER_PATH` and verifies the actionable startup error.
- Debug demo/affected targets build; 250 tests and all 8 graphics checks pass. RelWithDebInfo demo builds with Tracy; Vulkan device and game lifecycle checks pass. The focused ASan device and missing-validation checks pass. All selected reports contain zero failures and zero skips.
- Initial RelWithDebInfo runtime DLL copying failed transiently; the same build succeeded on retry without removing outputs.
- MSVC ASan initially failed linking against prebuilt vk-bootstrap with mismatched `annotate_string/vector/optional`. Asan targets now consistently use `_DISABLE_STL_ANNOTATION`, matching vcpkg libraries, while `/fsanitize=address` instrumentation stays enabled. This removes STL logical-container-boundary checks, retaining address/allocation-boundary checking. The invalid `/fsanitize=address` argument to `link.exe` was removed; compiler-produced objects select the ASan runtime. This follows the [MSVC annotation compatibility contract](https://learn.microsoft.com/en-us/cpp/sanitizers/error-container-overflow?view=msvc-170).

Logs are in `build/vk-002-*.log`; reports are `build/msvc-debug/vk-002-graphics.xml`, `build/msvc-relwithdebinfo/vk-002-device.xml` and `build/msvc-asan/vk-002-device.xml`. No full ASan suite, unsupported GPU hardware, remote CI, human playthrough or RenderDoc capture was tested. Swapchain/rendering are VK-003 work. The VK-002 gate is satisfied.

### VK-003 — 2026-10-01

Status: done.

Added the renderer-local `VulkanSwapchain` owner in `src/relaycore/render/vulkan_swapchain.{h,cpp}` and its integration checks in `tests/vulkan_presentation_tests.cpp`. vk-bootstrap selects the swapchain surface configuration: SDR BGRA8/RGBA8 UNORM with SRGB nonlinear color space, FIFO for vsync, IMMEDIATE with FIFO fallback otherwise, desired image count three bounded by surface capabilities, and extent from the surface/window. Image views have partial-construction cleanup. Two fixed frame contexts own command pools/buffers, binary semaphores and acquire/render/present fences. Dynamic rendering clears the color attachment; synchronization2 transitions and submission connect acquire to rendering and presentation. Debug command labels and CPU Tracy zones cover frame waits, acquire, record, submit, present and recreation.

VK-003 enables and requires `VK_KHR_swapchain_maintenance1` / `swapchainMaintenance1`, `VK_KHR_surface_maintenance1` and `VK_KHR_get_surface_capabilities2`. Startup checks these separately from the API 1.3 baseline. Present fences protect semaphore reuse and swapchain destruction; submission fences alone are insufficient for those presentation references. Resize drains these fences before replacement. Hidden/minimized/zero-size windows suspend presentation; out-of-date and suboptimal results request recreation. No device/queue idle call exists in `VulkanSwapchain`; the device owner retains its shutdown-only idle wait.

Recreation removes the old owner from the active path before passing `oldSwapchain`. Failure destroys both incomplete replacement and retired old resources, stops acquisition and requires explicit retry with no retired handle. Native Vulkan allocation callbacks inject actual host allocation failure; the test verifies stopped presentation, discarded extent, successful retry, resumed presentation and zero live host allocations after shutdown. A failed draw also recreates frame synchronization before recovery, so an unconsumed acquire semaphore cannot be reused.

Commands run from the existing VS developer environment with the same per-process `VCPKG_ROOT` and `VULKAN_SDK` as VK-002:

```powershell
cmake --preset msvc-debug
cmake --build --preset graphics-debug --target relaycore_vulkan_presentation_tests
ctest --preset graphics-debug -R '^graphics\.vulkan_(presentation|recreation_failure)$' --output-on-failure --no-tests=error --output-junit vk-003-presentation.xml
cmake --build --preset graphics-debug --target relaycore_vulkan_device_tests relaycore_vulkan_presentation_tests relaycore_vulkan_window_tests relaycore_window_lifetime_tests relaycore_game_lifecycle relaycore_power_restoration_graphics relaycore_graphics_baseline relaycore_default_framebuffer_probe relaycore_tests relaycore_demo
ctest --preset test-debug --output-on-failure --no-tests=error
ctest --preset graphics-debug --output-on-failure --no-tests=error --output-junit vk-003-graphics.xml
cmake --preset msvc-relwithdebinfo
cmake --build --preset tests-relwithdebinfo --target relaycore_vulkan_device_tests relaycore_vulkan_presentation_tests relaycore_game_lifecycle relaycore_demo
ctest --preset test-relwithdebinfo -R '^graphics\.(vulkan_device|vulkan_presentation|vulkan_recreation_failure|game_lifecycle)$' --output-on-failure --no-tests=error --output-junit vk-003-presentation.xml
cmake --preset msvc-asan
cmake --build --preset tests-asan --target relaycore_vulkan_presentation_tests
ctest --preset test-asan -R '^graphics\.vulkan_(presentation|recreation_failure)$' --output-on-failure --no-tests=error --output-junit vk-003-presentation.xml
git diff --check
```

Results and limits:

- Debug development startup now enables synchronization validation in addition to core validation. It caught the initial acquire/layout-transition dependency error; the first barrier now includes COLOR_ATTACHMENT_OUTPUT in its source stage, chaining the semaphore wait into the transition. The final tested paths emit no validation warnings/errors, including teardown.
- On Windows/RTX 4060 Ti, both FIFO and non-vsync configurations run through two window/device/swapchain lifetimes, three resize/minimize/restore cycles per lifetime, hide/show and 120 sustained frames. Tests observe actual red/green/blue presented client pixels through Win32, rather than inferring visibility from return codes. The temporary test window stays on top to avoid unrelated window occlusion. This check requires an interactive Windows desktop; it skips on other platforms.
- The installed OBS implicit Vulkan hook repeats failed swapchain creation with the same, already retired `oldSwapchain`. Loader-layer logging and comparison with only `DISABLE_VULKAN_OBS_CAPTURE=1` isolated that external failure. `graphics.vulkan_recreation_failure` sets that variable for its child process; the normal presentation check keeps installed hooks. No runtime/global layer setting was changed. Native allocation failure with OBS active remains an external compatibility limitation.
- Debug demo and affected targets build; all 250 main tests and all 10 graphics checks pass, with zero skips. RelWithDebInfo demo builds with Tracy enabled; device, presentation, recreation failure and game lifecycle checks pass (4). Focused ASan presentation/failure checks pass (2), with no sanitizer diagnostics. The VK-002 STL annotation tradeoff still applies.
- Formatting and `git diff --check` pass. No full ASan suite, GPU limited to API 1.3, separate graphics/present queue hardware, other OS, remote CI, RenderDoc or Tracy capture was tested. Resource uploads and scene rendering remain later tickets; the main game still uses the OpenGL reference path.

Logs are `build/vk-003-*.log`; final reports are `build/msvc-debug/vk-003-graphics.xml`, `build/msvc-relwithdebinfo/vk-003-presentation.xml` and `build/msvc-asan/vk-003-presentation.xml`. The VK-003 gate is satisfied for these tested paths.

### VK-004 — 2026-10-01

Status: done.

Added backend-local `VulkanBuffer` / `VulkanTexture` owners in `src/relaycore/render/vulkan_resources.{h,cpp}`, `VulkanUploads` in `src/relaycore/render/vulkan_uploads.{h,cpp}`, and `tests/vulkan_upload_tests.cpp`. Buffer allocations use VMA device, mapped upload or mapped readback memory. VMA copy helpers perform the necessary flush/invalidate operations; offsets and ranges are checked before host access. Texture ownership includes the VMA image/allocation, full-mip image view and linear/repeat/trilinear sampler. Texture creation checks RGBA8 UNORM/sRGB format capabilities and image limits before allocation.

Uploads use staging buffers and the graphics queue. Each batch has its own command pool and a timeline completion value; submissions wait for the preceding value and signal completion after ALL_COMMANDS. CPU batch ownership is allocated before submitting. Staging/command resources and explicitly retired buffers/textures remain alive until the timeline reaches their value. Retirement rejects foreign timelines, values before the resource's upload, and unsubmitted values before taking ownership. CPU resource release and move assignment still require completed GPU references or explicit retirement; upload retirement does not imply completion of later frame rendering. The renderer must track those references when drawing is introduced. No generic garbage collector, transfer thread, dedicated transfer queue, new dependency or version change was added.

RGBA uploads create a fresh image, copy the base level, generate the full mip chain with checked linear blits, and transition every mip to shader-read layout. Reload replaces resources instead of overwriting GPU-referenced images. Readback copies mesh bytes or all tightly packed texture mips to host memory, waits for its own completion, and restores texture shader-read layout. Per-mip transitions encode the known upload/readback states; this is not a general image-state framework. sRGB filtering follows the [Khronos blit conversion rules](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBlitImage.html); mapped access uses the [VMA copy/flush/invalidate contract](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/memory_mapping.html).

Commands run in the same per-process VS/vcpkg/Vulkan SDK environment as VK-003:

```powershell
cmake --preset msvc-debug
cmake --build --preset graphics-debug --target relaycore_vulkan_upload_tests
ctest --preset graphics-debug -R '^graphics\.vulkan_upload$' --output-on-failure --no-tests=error --output-junit vk-004-upload.xml
cmake --build --preset graphics-debug --target relaycore_vulkan_upload_tests relaycore_vulkan_device_tests relaycore_vulkan_presentation_tests relaycore_vulkan_window_tests relaycore_window_lifetime_tests relaycore_game_lifecycle relaycore_power_restoration_graphics relaycore_graphics_baseline relaycore_default_framebuffer_probe relaycore_demo
ctest --preset graphics-debug --output-on-failure --no-tests=error --output-junit vk-004-graphics.xml
cmake --preset msvc-relwithdebinfo
cmake --build --preset tests-relwithdebinfo --target relaycore_vulkan_upload_tests relaycore_vulkan_presentation_tests relaycore_game_lifecycle relaycore_demo
ctest --preset test-relwithdebinfo -R '^graphics\.(vulkan_upload|vulkan_presentation|game_lifecycle)$' --output-on-failure --no-tests=error --output-junit vk-004-upload.xml
cmake --preset msvc-asan
cmake --build --preset tests-asan --target relaycore_vulkan_upload_tests
ctest --preset test-asan -R '^graphics\.vulkan_upload$' --output-on-failure --no-tests=error --output-junit vk-004-upload.xml
git diff --check
```

Results:

- Existing box-mesh vertex/index bytes round-trip exactly through GPU buffers. Three reloads per encoding verify texture base bytes, linear/sRGB checkerboard mip filtering (within one byte of expected UNORM quantization), opaque alpha and repeated readback/layout restoration. Non-power-of-two 7x3 textures reach 3x1 and 1x1 mips and preserve constant color.
- Mapped upload offset writes round-trip correctly. Empty mesh data, nonpositive texture dimensions, pixel-size mismatch, out-of-range writes, unsubmitted waits, early retirement and foreign upload timelines fail explicitly without validation errors.
- A host-signaled test timeline blocks actual GPU execution. After retirement and collection, all six destination/staging VMA allocations remain live while completion is pending; after release/wait/collection the count is zero. Pending upload-owner shutdown also releases all allocations without requiring an explicit caller wait. The test gate uses queue idle only for exception/shutdown cleanup; production upload/resource owners use timeline completion and contain no device/queue idle calls.
- Debug demo/graphics targets build and all 11 graphics checks pass. RelWithDebInfo demo builds with Tracy enabled; upload, swapchain presentation and OpenGL game lifecycle pass (3). Focused ASan upload/readback/reload/retirement passes (1), with no sanitizer diagnostics. Debug core/synchronization validation reports no warnings/errors, including teardown. Final reports have zero failures and zero skips. Existing unrelated save/debug-command compiler warnings remain.
- Formatting and `git diff --check` pass. This phase does not rerun the full main/content/ASan suites. Other GPU vendors, non-coherent host memory hardware, API-1.3-only devices, other OS, remote CI and profiler/RenderDoc capture remain untested. Visual textured-mesh acceptance belongs to VK-005; the main game remains on the OpenGL reference path.

Logs are `build/vk-004-*.log`; final reports are `build/msvc-debug/vk-004-graphics.xml`, `build/msvc-relwithdebinfo/vk-004-upload.xml` and `build/msvc-asan/vk-004-upload.xml`. The VK-004 gate is satisfied for these tested paths.

### VK-005 / VK-006 — 2026-10-01

Status: done for the mesh/geometry gates. The main game retains the OpenGL reference renderer until the subsequent parity gates.

`assets/shaders/vulkan/mesh.slang` ports the existing scene vertex transforms, OBJ diffuse sampling, surface UV selection and blockout atlas sampling/mix. CMake compiles `vertex_main` and `fragment_main` offline to SPIR-V 1.5 with explicit column-major layout and retained entry-point names. Generated shaders are copied into the demo's runtime shader assets; no Slang runtime compiler or new dependency is linked. Compilation identifies the source path/entry point; shader-module/pipeline failures identify the SPIR-V path and source/entry label. Both modules and a replacement pipeline are staged before shader reload publishes them. Missing vertex/fragment output and malformed SPIR-V preserve the working pipeline.

The explicit shader interface follows [Slang's SPIR-V binding/layout rules](https://github.com/shader-slang/slang/blob/master/docs/user-guide/a2-01-spirv-target-specific.md): set 0 binding 0 is a material UBO with 32 atlas scale/mix entries, binding 1 is a combined image sampler. The push block is 116 bytes: a 64-byte MVP, three 16-byte affine model rows, and a 4-byte textured-model flag; C++ asserts the offsets/size. Vertex inputs use the current `ColorVertex` offsets and uint32 indices. `VulkanMeshRenderer` uses dynamic rendering, RGBA8 UNORM color and D32 depth, CCW backface culling and the reference LEQUAL depth test. The CPU camera/frustum convention remains unchanged; the vertex stage remaps clip Z from [-w,w] to [0,w], and a negative-height viewport preserves top-down image orientation. A separate depth clear preserves shotgun self-occlusion without clipping the viewmodel against world surfaces.

`StaticColorMesh`, `Texture2D` and `StaticModelCache` now support explicit CPU resource creation. This mode retains shared immutable CPU data without GL calls; the default OpenGL mode retains its existing GPU ownership without extra CPU copies. `WorldSceneRenderer::prepare_render_queue` separates the existing queue preparation from the GL draw call; `render` delegates to it. Static props, model transforms, CPU frustum culling and OBJ/BMP cache invalidation/encoding policy remain in their existing owners. The shotgun queue is prepared once for either consumer. Gameplay/world contracts acquire no Vulkan handles.

The Vulkan renderer uploads these resources through VK-004 and caches GPU meshes/materials by CPU resource identity. It waits for its submission fence before readback, reuse, replacement and expired-cache cleanup. The initial path deliberately serializes mesh submissions; it is a correctness/parity foundation, not a measured throughput optimization. `VulkanSwapchain::draw_scene` presents the completed color image through a GPU blit, without CPU readback. Its render fence protects the source image until the blit completes, including error cleanup; presentation references only swapchain images. Device identity, target limits, depth/color/blit format support and swapchain transfer-destination usage are checked. No routine queue/device idle wait was added. Lighting, procedural material shading/postfx, transparency and UI remain in their owning later gates; the comparison reference excludes those effects and uses the current GL vertex/surface/atlas code.

Commands ran in the same per-process VS/vcpkg/SDK environment as VK-004:

```powershell
cmake --preset msvc-debug
cmake --build --preset graphics-debug --target relaycore_vulkan_mesh_tests
ctest --preset graphics-debug -R '^graphics\.vulkan_mesh$' --output-on-failure --no-tests=error --output-junit vk-006-mesh.xml --test-output-size-passed 20000
cmake --build --preset graphics-debug --target relaycore_vulkan_mesh_tests relaycore_vulkan_upload_tests relaycore_vulkan_device_tests relaycore_vulkan_presentation_tests relaycore_vulkan_window_tests relaycore_window_lifetime_tests relaycore_game_lifecycle relaycore_power_restoration_graphics relaycore_graphics_baseline relaycore_default_framebuffer_probe relaycore_demo relaycore_tests
ctest --preset graphics-debug --output-on-failure --no-tests=error --output-junit vk-006-graphics.xml --test-output-size-passed 20000
ctest --preset test-debug -R '^relaycore\.(camera|blockout mesh|OBJ|Static props|render authoring config)' --output-on-failure --no-tests=error --output-junit vk-006-unit.xml
cmake --preset msvc-relwithdebinfo
cmake --build --preset tests-relwithdebinfo --target relaycore_vulkan_mesh_tests relaycore_vulkan_presentation_tests relaycore_game_lifecycle relaycore_demo
ctest --preset test-relwithdebinfo -R '^graphics\.(vulkan_mesh|vulkan_presentation|game_lifecycle)$' --output-on-failure --no-tests=error --output-junit vk-006-mesh.xml --test-output-size-passed 20000
cmake --preset msvc-asan
cmake --build --preset tests-asan --target relaycore_vulkan_mesh_tests
ctest --preset test-asan -R '^graphics\.vulkan_mesh$' --output-on-failure --no-tests=error --output-junit vk-006-mesh.xml --test-output-size-passed 20000
git diff --check
```

After adding the no-OpenGL-context lifecycle and device-limit cases, the mesh target was rebuilt and `graphics.vulkan_mesh` rerun in all three configurations. Final optimized/ASan reports are `vk-006-mesh-final.xml`. `C:\VulkanSDK\Bin\spirv-val.exe --target-env vulkan1.3` accepted both mesh modules in Debug, RelWithDebInfo and ASan. Changed C++ files pass clang-format checks; packaged Debug SPIR-V files match the generated products.

Results:

- Debug demo/graphics/unit targets build. All 12 graphics checks and 14 focused camera/blockout/OBJ/static-prop/authoring tests pass. RelWithDebInfo demo builds with Tracy enabled; mesh, presentation and GL game lifecycle pass (3). The final mesh test also passes in Debug, RelWithDebInfo and ASan, with zero skips and no sanitizer or core/synchronization-validation warnings/errors, including teardown. Existing unrelated save/debug-command compiler warnings remain.
- Sixteen GL/Vulkan image comparisons cover three linear/sRGB texture/mesh reload cycles, two views each of `power_restoration.toml` and `pressure_transfer.toml` with and without a cycling shotgun, a rotated/nonuniform OBJ prop, and four model-cache source/encoding revisions. All coverage masks agree exactly; no pixels differ by more than eight RGB byte values. The largest mean RGB-byte error is 0.001651/255. Debug captures were visually inspected for the pressure-transfer room/shotgun and the static prop.
- Tests exercise visible and culled chunks, repeated scene presentation, level-resource replacement, swapchain/target resize, valid and failed shader reload, model-cache reuse and changed OBJ/BMP sources, separate linear/sRGB cache entries, stale GPU allocation reclamation, invalid indices/dimensions and final zero VMA allocations. A CPU-mode world with its shotgun OBJ resources prepares and destroys successfully before any GL context or function loading exists.
- Full main/content/ASan suites, other GPUs/OS, API-1.3-only hardware, remote CI, throughput measurements and RenderDoc/Tracy GPU captures were not run. Complete lighting/material/color-pipeline equivalence is not claimed by these mesh gates.

Evidence: `build/vk-005-*.log`, `build/vk-006-*.log`, `build/msvc-debug/vk-006-{graphics,unit,mesh}.xml`, and optimized/ASan `vk-006-mesh-final.xml`. Paired BMPs are under each build's `tests/vulkan-mesh-captures/`. The VK-005 and VK-006 gates are satisfied for these tested paths.
