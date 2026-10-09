# RelayCore — dependency stack and integration policy

Status: architecture reference. This document records approved third-party responsibilities and when they may enter the repository. It is **not** permission to add every library to `vcpkg.json` immediately.

Read this file when a task adds, replaces, upgrades, removes, or exposes a third-party dependency, or when choosing the foundation for rendering, physics, asset processing, navigation, animation, audio, compression, or tooling.

## 1. Policy

RelayCore should own engine policy and product-specific behavior, but it should not reimplement mature commodity infrastructure without a concrete reason.

Rules:

- a dependency enters only when code in the same milestone owns and uses it;
- prefer one focused library per responsibility over overlapping frameworks;
- prefer vcpkg manifest mode and the repository `builtin-baseline` as the reproducible source of versions;
- dependency baseline updates are separate changes from subsystem migrations/features;
- third-party implementation types stay behind the owning subsystem boundary unless the type is already an intentional project-wide convention;
- do not add generic wrappers around a dependency merely to hide its name; add a RelayCore boundary where it expresses engine policy, ownership, lifetime, data conversion, or stable public semantics;
- do not let importer/tool dependencies become required game-runtime dependencies unless runtime use is independently justified;
- prefer stable releases. Do not chase upstream HEAD or release candidates without a confirmed blocker or required capability;
- if an official vcpkg port is materially stale and blocks required work, prefer a repository-local **overlay port pinned to an exact release/commit** over ad-hoc `FetchContent`, global installs, or an unpinned git dependency;
- record the exact baseline/overlay version and verification evidence whenever a dependency changes.

Version numbers below are a **2026-09-30 research snapshot**, not permanent pins. The checked-in vcpkg baseline is authoritative after each accepted dependency update.

Current repository snapshot: RelayCore `1.3.4`, vcpkg manifest baseline `a42757564758c60651ebc616d9b000b7b91249ef`. Vulkan is the sole runtime backend after VK-013; VK-014 review is complete. The VK-D00 OpenGL builds below are historical dependency-refresh evidence. Current Vulkan build/runtime verification is recorded in `VULKAN_MIGRATION_PLAN.md`, sections 27–28. Recheck the manifest and current source before changing dependencies.

## 2. Keep the current foundations

These libraries already fit RelayCore. Do not replace them merely for perceived prestige.

| Responsibility | Choice | Boundary / reason |
| --- | --- | --- |
| platform, window, events, input, controllers, Vulkan surface | **SDL3** | Keep. It is the platform layer, not the renderer. The manifest enables its `vulkan` feature. |
| audio device, playback, mixing/resource plumbing | **miniaudio** | Keep. `AudioSystem` owns it; gameplay/world audio semantics stay RelayCore data. |
| math | **GLM** | Keep. Vulkan does not make GLM obsolete; define and test RelayCore coordinate/clip-space conventions explicitly. |
| logging | **spdlog** | Keep. No demonstrated reason to migrate logging. |
| authored TOML/config parsing | **toml++** | Keep for human-authored data/config; do not turn TOML into the compiled product-asset format. |
| tests | **doctest** | Keep the existing test investment. |
| debug/editor UI | **Dear ImGui** | Keep as optional tooling/UI; it must not become required gameplay architecture. |
| profiling | **Tracy** | Keep Vulkan GPU instrumentation and frame/CPU timings. |

The verified VK-D00 manifest resolves SDL3 `3.4.16#1`, miniaudio `0.11.25`, GLM `1.0.3`, spdlog `1.17.0#1`, toml++ `3.4.0#1`, doctest `2.5.3`, ImGui `1.92.9`, and Tracy `0.14.1#1`. Do not hard-code these versions individually unless a dedicated version constraint is required.

### Player-facing UI decision — 2026-10-09

Implementation selection: **RmlUi 6.3**, standard manifest port at the existing
`a42757564758c60651ebc616d9b000b7b91249ef` baseline, default FreeType font engine.
Lua/SVG/Lottie and sample graphics backends are not enabled. `extern/rmlui`
contains the official 6.3 SDL platform/input adapter and MIT notice. RelayCore's
small `VulkanRmlRenderer` implements geometry, generated font textures,
premultiplied alpha, rectangular scissor and transforms on the existing device/VMA.
The upstream Vulkan demo owns its device/swapchain/frame machinery, so importing
that backend would duplicate runtime ownership. Authored UI uses no images,
filters, layers, transformed clipping or rounded overflow; these optional features
are outside the adapter's current contract. `assets/fonts/RobotoMono-Regular.ttf`
and its upstream font license are staged with RML/RCSS. See
[6.3 release](https://github.com/mikke89/RmlUi/releases/tag/6.3) and
[baseline port](https://github.com/microsoft/vcpkg/blob/a42757564758c60651ebc616d9b000b7b91249ef/ports/rmlui/vcpkg.json).

The owner requires replacing ImGui for game HUD and player-facing menus/screens. ImGui remains the debug/editor/developer-tools foundation. The current ImGui HUD/menu integration below describes the Vulkan migration baseline, not the target game UI contract. VIS-43 owns selection and integration of a focused game UI solution; VIS-44–46 complete screen migration. See [scope and gates](PLAYABLE_SLICE_ROADMAP.md#vis-43--переход-на-игровой-hud-и-ui).

RmlUi types remain within UI implementation; manifest/version policy is unchanged. Gameplay presentation data and actions are RelayCore-owned and independent of ImGui and diagnostics contracts. The implementation uses the selected release's headers/backends rather than master documentation for API signatures.

## 3. Vulkan migration dependencies — approved for VK-000

The ordered migration and loader policy remain in `VULKAN_MIGRATION_PLAN.md`.

| Responsibility | Choice | Ownership |
| --- | --- | --- |
| Vulkan loader/headers | normal Vulkan loader through vcpkg/SDK-compatible CMake path | render/platform only |
| instance/device/queue/swapchain bootstrap | **vk-bootstrap** | initialization only; do not turn it into renderer architecture |
| Vulkan memory allocation | **Vulkan Memory Allocator (VMA)** | GPU allocation/suballocation/mapping/budget only |
| shader language/compiler | **Slang → SPIR-V** | build/offline tool first; runtime compiler optional dev tooling later |
| UI backend at Vulkan cutover | ImGui SDL3 + Vulkan | current renderer-owned HUD/menu/debug/editor presentation; game UI replacement is planned in VIS-43–46 |
| validation | Khronos Validation Layers | development verification |
| frame debugging | RenderDoc | external developer tool |

Do **not** add `volk` in the first migration while the chosen ImGui/vcpkg path directly uses the normal Vulkan loader. Revisit loader policy only as a dedicated change.

VK-000 enabled `sdl3[vulkan]` and `imgui[vulkan-binding]` alongside the existing OpenGL binding on 2026-10-01. The manifest resolves Vulkan headers/loader `1.4.357.0`, vk-bootstrap `1.4.357`, VMA `3.4.0` and host-tool Slang `2026.18.2`. The build produces and validates SPIR-V. VK-002 integrates vk-bootstrap instance/device/queue selection, SDL surface lifetime and a VMA allocator using explicit function lookup with runtime API 1.3; Debug, RelWithDebInfo and focused ASan lifecycle checks pass. MSVC Asan targets disable STL container annotations to match prebuilt libraries while retaining address instrumentation. Commands, tradeoffs and verification results are recorded in `VULKAN_MIGRATION_PLAN.md`, section 14.

VK-003 uses vk-bootstrap for swapchain configuration and adds the clear/presentation lifecycle with explicit synchronization. Presentation fences require checked/enabled swapchain-maintenance extensions and feature support; the runtime API minimum remains 1.3. Debug synchronization validation, RelWithDebInfo/Tracy and focused ASan presentation checks pass. No dependency versions changed in this phase; the game retains the OpenGL reference path.

VK-004 integrates VMA-backed GPU buffers/images, mapped staging/readback, mip upload and completion-based retirement. Native image views/samplers and graphics-queue submissions remain renderer-owned. Debug synchronization validation and focused RelWithDebInfo/ASan readback/reload/lifetime checks pass; dependency versions remain unchanged. Slang rendering is the VK-005 gate.

VK-013 removes the ImGui OpenGL binding and vendored glad. The runtime uses SDL Vulkan windows, vk-bootstrap presentation/device ownership, VMA uploads and Slang-generated SPIR-V. Dependency versions and baseline are unchanged.

## 4. Physics — Jolt Physics

**Approved foundation:** `joltphysics` when the production collision/physics milestone begins.

Why:

- mature C++ rigid-body/collision system;
- static triangle meshes, convex shapes, casts/queries, sensors and dynamic bodies cover the Source 2-class world requirements;
- avoids growing the current AABB collision code into a home-grown general physics engine;
- multithread-capable without requiring RelayCore to invent an engine-wide job system first.

Architecture:

```text
Gameplay / GameWorldRuntime
          │
          ▼
RelayCore PhysicsWorld + typed query/results
          │
          ▼
       Jolt Physics
```

Rules:

- `JPH::*` types do not leak into gameplay, level formats, renderer data, save formats, or generic asset contracts;
- RelayCore owns collision layers, surface/material semantics, stable handles/IDs, map tags and conversion from compiled collision products;
- preserve FPS movement policy (acceleration, stepping, grounding, gameplay feel) in RelayCore. Evaluate Jolt `CharacterVirtual` or other character helpers only against movement-parity tests; do not silently replace player feel;
- start without optional Jolt profiler/RTTI features. Use Tracy as the project profiler. Add Jolt debug rendering only if a physics-debug task needs it;
- use Jolt's local job support for physics if required before creating any engine-wide job system.

E-011 integration (2026-10-04): the existing baseline resolves `joltphysics` `5.6.0#1`. `vcpkg-configuration.json` selects the repository overlay at `vcpkg-overlays/joltphysics`, copied from that port and pinned to upstream `v5.6.0` with its SHA512. The overlay changes CPU options, not the release: SSE4.1/4.2, AVX/AVX2/AVX512, FMA, F16C, LZCNT and TZCNT are disabled to retain the x64 SSE2 floor. Single precision, the shared DLL and the existing dynamic MSVC CRT remain in use; optional profiler, C++ RTTI and Jolt debug renderer are disabled. Do not replace the overlay just to chase a newer minor version; keep these CPU/ABI choices explicit when upgrading.

`physics::PhysicsWorld` owns a static query scene behind a Pimpl; `JPH::*` appears only in its implementation. `LevelSessionBuilder::build_physics_fixture` explicitly prepares the isolated fixture. Immutable worlds can be shared by copies of prepared sessions and transferred to `GameWorldRuntime`; ordinary map builds keep the AABB backend. `WorldQuery` borrows either AABB records or the fixture for ray queries. Box casts/overlaps use RelayCore-owned inputs/results on `PhysicsWorld`; FPS axis movement on the fixture fails explicitly until E-013 parity. There is no physics update/job system, dynamics or `CharacterVirtual` integration in E-011. See the E-011 roadmap card for current verification evidence.

## 5. Authoring interchange — glTF 2.0 + fastgltf

**Approved foundation:** `fastgltf` when the production model/scene importer starts.

```text
Blender / DCC
     │
 glTF 2.0 / GLB
     │
 fastgltf
     │
 Relay import IR
     │
 Asset Compiler
```

Rules:

- glTF is an authoring/interchange input, not the runtime scene format;
- `fastgltf::*` types stop at importer/tool boundaries;
- convert immediately to RelayCore-owned import data;
- do not add Assimp merely to support many legacy formats. Add a format-specific importer only when a real content source requires it;
- glTF 2.0 is the accepted interchange baseline until a task proves a need for 2.1 or a specific extension.

Research snapshot: public vcpkg `fastgltf 0.9.0`.

E-003 (2026-10-02) starts the static GLB importer. The unchanged manifest baseline resolves fastgltf `0.9.0` and its simdjson `5.0.1` dependency. They link privately to `relaycore_model_importer` and its CLI/tests, not the runtime engine/game. Embedded PNG/JPEG are decoded by Windows Imaging Component in the tool; the game reads native RGBA/model products. Meshoptimizer/KTX/BLAKE3 are not added by this first slice.

E-004 attaches this importer to CMake's ordinary file dependencies and runtime staging. SHA256 build receipts extend the existing packaging integrity checks; they are not a content-addressed asset cache or logical asset identity. No new dependency or baseline change is needed for this build step.

## 6. Mesh processing — meshoptimizer

**Approved foundation:** `meshoptimizer` with the production `MeshAsset` compiler.

Use it for applicable offline/compile-time operations such as:

- vertex remap/deduplication;
- vertex-cache and vertex-fetch optimization;
- overdraw optimization where useful;
- tangent-frame generation;
- mesh simplification/LOD generation;
- meshlets/cluster data only when the renderer has a demonstrated consumer;
- mesh/index compression only when a product format and measured loading/memory goal require it.

Do not let meshoptimizer dictate the runtime renderer architecture. Its output is compiled into RelayCore-owned mesh products.

Research snapshot: `meshoptimizer 1.2` is the current stable release and vcpkg port.

## 7. Texture products — KTX2/libktx

**Approved foundation:** KTX2 as the preferred production texture container when `TextureAsset`/texture compilation starts.

Suitable uses include:

- mip chains;
- BC-compressed textures;
- normal/data textures;
- cubemaps and arrays;
- HDR environment/probe/lightmap products;
- Basis Universal/supercompression where it is actually useful.

Architecture:

```text
PNG/TGA/HDR/etc. or imported Source 2 texture
                  │
          texture compiler
                  │
                 KTX2
                  │
      Relay TextureAsset metadata
                  │
       renderer upload path
```

Prefer the base `ktx` library first. Do **not** automatically enable `ktx[vulkan]` and let libktx own `VkImage` creation, because GPU image ownership belongs to RelayCore's renderer/upload system. Enable the Vulkan feature only if a later renderer-owned adapter demonstrates concrete value without leaking Vulkan through the asset layer.

KTX already uses Zstandard internally for supported supercompression; that does not make general RelayCore package compression architecture depend on KTX.

Research snapshot: stable `KTX 4.4.2`; 5.0 is still release-candidate territory and should not be the production baseline yet.

## 8. Compiled-asset compression and fingerprints

### Zstandard

**Approved on demand:** `zstd` when RelayCore has compiled asset bundles/chunks whose I/O/storage measurements justify general compression.

Do not compress every product by default. GPU-native compressed texture data, already compressed media, and tiny products may gain nothing or become slower.

Research snapshot: public vcpkg `zstd 1.5.7`.

### BLAKE3

**Approved foundation:** `blake3` for stable content fingerprints/cache keys when the asset compiler introduces dependency caching and product identity.

Prefer BLAKE3 over adding both a non-cryptographic fast hash and a separate integrity hash. It is fast enough for asset hashing while providing a strong content digest suitable for cache keys, manifests and integrity checks.

Do not use a hash as the only semantic asset ID when an authored stable logical ID is required; content digest and logical identity are different concepts.

Research snapshot: public vcpkg `blake3 1.8.5`; upstream may move faster. Baseline-manage it rather than overriding for every patch release.

## 9. Navigation — Recast + Detour

**Approved foundation when navigation work begins:** Recast Navigation.

Responsibility split:

- **Recast** — offline/build-time navmesh generation from RelayCore collision/walkable geometry;
- **Detour** — runtime navmesh representation and path queries;
- Detour Tile Cache / Crowd — only if concrete streaming/dynamic-obstacle/crowd requirements appear.

Architecture:

```text
Relay collision/world geometry
            │
          Recast
            │
     compiled nav product
            │
      Relay NavigationWorld
            │
          Detour
```

Rules:

- `dt*`/`rc*` types stay in navigation implementation/tooling;
- gameplay uses RelayCore path/query results and agent semantics;
- Source 2 navigation may provide metadata/input, but do not deserialize foreign navigation binaries directly into live Detour state without an explicit validated conversion path;
- for the first Source 2-class map, rebuilding nav from imported Relay collision geometry is acceptable and often safer than reproducing Source 2 runtime nav internals.

The public vcpkg port is `recastnavigation 1.6.0#1` and is older than active upstream development. When this milestone starts, re-evaluate the port. If required fixes are absent, use a pinned repository overlay port rather than unpinned `FetchContent`.

## 10. Spatial acoustics — Steam Audio, later

Keep **miniaudio** as the playback/mixing/device foundation.

**Approved future extension:** Steam Audio when arbitrary world geometry/material semantics are mature enough to drive acoustics.

Potential responsibilities:

- HRTF/binaural spatialization;
- geometry-aware occlusion/transmission;
- reflections/reverb;
- baked acoustic probes/pathing where useful.

Architecture:

```text
AudioSystem / miniaudio  ← playback, streams, mixing
          │
Relay spatial-audio bridge
          │
      Steam Audio        ← acoustic simulation/spatial effects
          │
Relay world geometry + acoustic material data
```

Steam Audio must not own gameplay sound-event semantics or replace the existing audio system wholesale. Introduce it only with an audible native RelayCore test case and performance budget.

Steam Audio is open source and actively maintained, but it is not part of the normal vcpkg stack assumed by this project. Pin a stable upstream release in a dedicated integration task if/when adopted; do not add it during unrelated audio work.

## 11. Skeletal animation — evaluate ozz-animation at the milestone

`ozz-animation` is the preferred **candidate** for skeletal sampling/blending/runtime data because it is a focused data-oriented C++ animation library with offline tooling. It is not an approved immediate dependency because the current standard vcpkg registry does not provide a normal `ozz-animation` port and RelayCore has no skeletal-animation consumer yet.

When animated models become a real milestone:

1. compare a small RelayCore-owned runtime against ozz for the required animation set;
2. prefer ozz if it removes meaningful sampling/blending/runtime work without forcing its scene/tool architecture into RelayCore;
3. integrate through a pinned overlay port or another repository-controlled reproducible mechanism, not an unpinned network fetch;
4. keep skeleton/model/animation product IDs and gameplay animation state owned by RelayCore.

Do not implement a Source 2 AnimGraph clone.

## 12. Dependencies intentionally not selected

Do not add these without a new evidence-based decision:

| Library/approach | Current decision |
| --- | --- |
| GLFW | no; SDL3 already provides the broader platform layer RelayCore needs |
| SDL_GPU | no; direct Vulkan is the approved renderer backend |
| OpenAL Soft | no; no demonstrated benefit over miniaudio for RelayCore's audio foundation |
| FMOD/Wwise | no by default; proprietary middleware/workflow is unnecessary for current goals |
| Bullet/PhysX | no; Jolt is the selected physics foundation |
| Assimp | no by default; glTF/fastgltf is the primary DCC interchange path |
| Draco | no until an actual imported asset requires Draco compression; do not support formats speculatively |
| volk | no in initial Vulkan loader policy; see `VULKAN_MIGRATION_PLAN.md` |
| Vulkan-Hpp | no; keep raw Vulkan handles with small explicit RelayCore RAII owners |
| shaderc/glslang as app dependencies | no; Slang is the shader toolchain target |
| SPIRV-Reflect | no by default; use Slang reflection when reflection is required |
| EnTT/Flecs | no; no ECS migration is justified |
| TBB/Taskflow/enkiTS | no engine-wide job library until profiling/current systems require a job system |
| mimalloc/tcmalloc | no allocator replacement without allocation-profile evidence |
| FlatBuffers/Cap'n Proto/cereal | no compiled-asset serialization framework before actual product-format requirements are designed |
| generic render-graph library | no before render-pass/resource complexity demonstrates the need |

## 13. Staged vcpkg manifest policy

Do not produce one giant future manifest. Evolve the real manifest at the milestone that first owns each dependency.

### Vulkan migration (`VK-000`)

Conceptually add/enable:

```text
sdl3[vulkan]
imgui[sdl3-binding, vulkan-binding]
shader-slang (host build tool)
vk-bootstrap
vulkan-memory-allocator
```

Keep the existing miniaudio/GLM/spdlog/toml++/Tracy/doctest dependencies.

### Physics milestone

Added for the isolated E-011 fixture:

```text
joltphysics
```

Do not enable optional Jolt profiler/RTTI features by default.

### Production asset compiler

Add as the corresponding code lands:

```text
fastgltf
meshoptimizer
ktx
blake3
```

Add `zstd` only when RelayCore itself starts compressing general compiled products/bundles; KTX may already bring zstd transitively for its own format.

### Navigation milestone

Add `recastnavigation` only after the version/overlay review described above.

### Steam Audio / ozz-animation

These are dedicated integration decisions rather than immediate vcpkg manifest entries.

## 14. Dependency change acceptance

A dependency change is complete only when the agent records:

- why the dependency is needed now;
- owning module/tool and forbidden leakage boundary;
- exact vcpkg baseline/overlay/source version used;
- license suitability;
- configure/build result on the actual Windows/MSVC presets;
- nearest tests/runtime validation;
- package/runtime-size impact when material;
- any new DLL/runtime deployment requirement;
- removal/replacement of superseded dependency/code when applicable.

Do not call a dependency migration complete merely because CMake configures.
