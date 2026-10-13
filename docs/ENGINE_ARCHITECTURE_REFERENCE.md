# RelayCore — engine architecture and Source 2 capability reference

Status: long-term architecture/reference document, not an automatic implementation queue.

Basis: repository snapshot `1.3.4` supplied 2026-09-30. Current source, tests, runtime evidence, and the user's current task override this document when they disagree.

Current implemented contracts live in [ARCHITECTURE](ARCHITECTURE.md), [MODEL_ASSETS](MODEL_ASSETS.md), [RENDERING](RENDERING.md) and [COLLISION_ASSETS](COLLISION_ASSETS.md). This reference describes target capabilities and boundaries; implementation status belongs to the [roadmap](PLAYABLE_SLICE_ROADMAP.md) and [renderer-plan](RENDERER_MODERNIZATION_PLAN.md).

## 1. When to read this

Read this file for work that materially affects engine/world architecture, scene/level representation, asset import/compilation/loading, rendering, materials, lighting, collision/physics world, navigation, visibility/streaming, animation/FX, or Source/Source 2 compatibility.

For graphics-backend migration or Vulkan-specific implementation, also read `docs/VULKAN_MIGRATION_PLAN.md`.

Do not load these documents for an unrelated narrow gameplay/UI/release fix whose owner is already clear.

Precedence:

1. user's current request;
2. `AGENTS.MD`;
3. current code/tests/runtime/profiler evidence;
4. `docs/PLAYABLE_SLICE_ROADMAP.md` for the active gameplay/product queue;
5. `docs/VULKAN_MIGRATION_PLAN.md` for the graphics-backend migration;
6. this file for long-term engine direction;
7. historical audits/plans.

These documents constrain design and dependency order. They do not authorize broad implementation by themselves. Approved third-party responsibilities, staged vcpkg policy and rejected alternatives are centralized in `DEPENDENCY_STACK.md`.

## 2. Direction

RelayCore is a custom C++20 engine and playable indoor FPS. It is not a historical recreation and is not defined as an educational/tutorial engine. Optimize for a codebase one developer can direct, an agent can modify safely, and both can debug, profile, verify, and keep shipping.

Use the Source family as a major reference, but not as a single architecture blueprint:

- **GoldSrc** — FPS feel, compact readable spaces, environmental storytelling.
- **Source 1** — map-centric interactions, entity connections, surface/collision semantics.
- **Source 2** — primary capability target for a modern FPS world: compiled resources, arbitrary meshes/models, richer materials, independent physics data, visibility/world partition data, baked lighting/probes, navigation, animation/FX, and modern map content.

Long-term acceptance target: a representative Source 2 / CS2 map can be imported reproducibly into RelayCore and run as a normal RelayCore level with important geometry, placements, materials, collision, lighting/environment, and relevant metadata preserved.

Two invariants:

1. Source 2 support is an **offline importer**, never a Source 2 runtime dependency.
2. A capability added because of Source 2 becomes a **native RelayCore capability** usable by RelayCore-authored content and the game itself.

A Source 2 map port is therefore an integration test of RelayCore's own systems.

### 2.1 Graphics direction

Direct Vulkan is the runtime graphics API after VK-013 cutover; VK14-01 prefers 1.4 while retaining the required 1.3 baseline. The original OpenGL implementation supplied parity evidence and has been removed.

The migration must preserve visible behavior before adding the next generation of renderer features. In particular, do not combine backend parity work with an HDR/material/lightmap/probe redesign unless a small prerequisite is unavoidable.

Use SDL3 for platform/window/input integration. Do not introduce SDL_GPU as an additional rendering abstraction unless a future task demonstrates a concrete portability requirement that justifies changing this decision.

The preferred Vulkan support stack is deliberately small:

- Vulkan SDK for development tools, validation, headers/tools inspection and diagnostics;
- normal Vulkan loader linkage during the initial migration;
- `vk-bootstrap` for instance/device/queue/swapchain bootstrap;
- Vulkan Memory Allocator (VMA) for device-memory suballocation;
- Slang for shader authoring and SPIR-V compilation;
- Khronos Validation Layers for API/synchronization/GPU-assisted validation;
- Tracy Vulkan integration for GPU profiling;
- RenderDoc for frame capture and GPU inspection;
- fastgltf already fronts static GLB compilation. meshoptimizer, KTX2/libktx and BLAKE3 remain later asset-pipeline choices when their owning stages begin; Zstandard is reserved for intentional/measured product compression.
- Jolt Physics already owns the static native collision/query path; general dynamics and character replacement remain separate milestones. Recast/Detour is the approved future navigation foundation. Their implementation types stay behind RelayCore subsystem contracts.
- miniaudio remains the audio foundation; Steam Audio is an approved later spatial-acoustics extension, not a replacement for `AudioSystem`.

`volk` is **not** a default dependency for the first migration. The current vcpkg Dear ImGui Vulkan binding links the normal Vulkan loader; mixing that with volk's no-direct-loader model creates unnecessary build/loader complexity. Reconsider volk only if the project intentionally moves all Vulkan consumers, including ImGui integration, to one consistent custom-loader path.

### 2.2 Core dependency policy

Do not replace a working library merely to make the stack look more heavyweight. Prefer a small dependency with a clear owner over a broad framework. Keep dependencies phase-gated: an approved library enters `vcpkg.json` only when code in the current milestone owns it.

Keep the current foundations unless a concrete defect changes the decision:

| Concern | Foundation | Policy |
| --- | --- | --- |
| platform/window/input | SDL3 | keep; use its Vulkan surface/platform integration |
| audio playback/mixing | miniaudio | keep as the native audio foundation |
| math | GLM | keep; do not replace for naming/style reasons |
| logging | spdlog | keep |
| authored config | toml++ | keep for human-authored data, not compiled binary assets |
| tests | doctest | keep existing suite |
| diagnostics UI | Dear ImGui | keep; Vulkan backend after cutover |
| profiling | Tracy | keep; migrate GPU zones to Vulkan |

**Owner decision and implementation — 2026-10-09:** RmlUi 6.3 replaces ImGui for player-facing HUD, menus and game screens through VIS-43–46; retain ImGui for diagnostics/editor/developer tools. Dependency rationale is recorded in `DEPENDENCY_STACK.md`, implemented ownership in `ARCHITECTURE.md`. Use minimal RelayCore-owned presentation data and explicit game actions; do not expose UI-library types in gameplay/world/save contracts or require diagnostics for ordinary play. Renderer/UI owns GPU resources, submission and display scaling; game owns state and actions. Menu focus captures player input, passive HUD does not, and developer ImGui retains its existing access. Preserve current scene/presentation lifetime and SDR/HDR composition. Do not rewrite the editor or introduce an unrelated framework. [Implementation evidence and pending acceptance](tickets/VIS-43-46.md#vis-43--переход-на-игровой-hud-и-ui).

Approved additions, introduced only at their owning milestone:

| Concern | Preferred dependency | Responsibility |
| --- | --- | --- |
| production physics/collision | Jolt Physics | rigid/static/convex/triangle collision, queries and future dynamics behind `PhysicsWorld` |
| standard model interchange | fastgltf | glTF 2.0/GLB import front end for Blender/native asset workflow |
| mesh compilation | meshoptimizer | vertex/index optimization, simplification/LOD, compression and later meshlets when justified |
| texture products | KTX2/libktx | product texture container/read-write tooling; keep Vulkan image ownership in RelayCore |
| asset fingerprints/cache keys | BLAKE3 | fast strong content digests for source/settings/tool-version cache identity and integrity |
| product/package compression | zstd | compression where measured size/I/O benefit justifies it |
| navigation | Recast/Detour | offline navmesh generation + runtime path queries behind Relay navigation contracts |
| advanced spatial acoustics | Steam Audio | later HRTF/occlusion/reflections/propagation layer; keep miniaudio as playback/mixing foundation |
| skeletal animation | ozz-animation (evaluate at milestone) | preferred candidate for sampling/blending/runtime data; not an immediate dependency |

Do **not** add Assimp, Bullet/PhysX, OpenAL Soft, GLFW, EnTT, Catch2, generic task libraries or another renderer framework merely because they are common. Reconsider only when a current problem is not well served by the chosen foundations.

## 3. Pre-migration baseline: preserve behavior before replacing

This table records the supplied 2026-09-30 `1.3.4` snapshot before cutover. It is historical evidence, not current GL ownership. After VK-013/014, `Window` owns the SDL Vulkan window; `Renderer` owns device/swapchain/backend and ImGui, and CPU assets remain API-neutral. Current ownership and frame flow are in `ARCHITECTURE.md`.

| Area | Current state | Consequence |
| --- | --- | --- |
| Lifetime | `Application` owns engine subsystems/delegate; renderer dies before `Window`/GL | preserve explicit ownership/shutdown under Vulkan |
| Engine/game boundary | engine does not include game headers; game borrows `EngineServices` | preserve one-way dependency |
| Platform | SDL3 window/event/input; `Window` currently owns the OpenGL context | evolve `Window` so platform window lifetime is not synonymous with GL context lifetime |
| Level authoring | TOML `BlockoutLevel`: AABB solids, entities/string properties, source locations, templates/groups/editor; current editor also authors models, lights, effects/decals and gameplay interactives | keep working during migration; evolve it into one source-authoring path rather than the universal runtime representation |
| Session load | level/runtime/scene prepared before live replacement | use staged prepare/commit for richer levels and GPU rebuilds |
| Render boundary | `PreparedWorldSceneData → WorldSceneRenderer → RenderQueue → Renderer` | preserve; make render-facing data API-neutral |
| Submission | explicit opaque/overlay/transparent passes | port behavior first; redesign only with measured/maintenance evidence |
| Visibility | blockout render chunks + frustum culling | valid foundation for arbitrary chunks/instances |
| Mesh | indexed `StaticColorMesh`; position/color/normal/material tag/UV0 | port current contract; production path later needs tangent, UV1, submeshes/material slots |
| Models | triangulated OBJ `v/vt/vn`, one BMP diffuse, shared immutable renderer cache | proof of sharing; not production model/material pipeline |
| Texture | 2D RGBA8/sRGB8A8, generated mips, fixed repeat/filter | preserve semantics during parity; general formats/HDR/cubes/arrays later |
| Color | explicit `legacy_gamma` and opt-in verified `hardware_srgb` | Vulkan parity must preserve explicit color-space behavior before HDR redesign |
| Lights | up to 32 local lights; nearest 8 have point shadows; [current contract](RENDERING.md#освещение-материалов-и-сглаживание) | scalable assignment remains a future capability |
| Shadows | directional cascades, spot path, point cube-array resources | parity target, not a mandate to preserve internal GL organization |
| Physics | player/world collision uses AABB world queries | independent of Vulkan; arbitrary maps still need triangle/convex collision |
| Asset deployment | CMake copies `assets/`; runtime consumes authored files | no general source→product compiler/catalog/dependency cache yet |
| Verification | unit/content/graphics/lifecycle tests, ASan workflows, Tracy | extend with Vulkan validation/RenderDoc evidence |

Existing shape to evolve:

```text
Authored level
    ↓ validate / prepare
GameWorldRuntime ──────────────┐
    ↓                         │
PreparedWorldSceneData        │
    ↓                         │
WorldSceneRenderer            │
    ↓                         │
RenderQueue → Renderer/GPU    │
                              │
Physics/world queries ◀───────┘
```

Do not restart the engine from zero to satisfy the target architecture.

## 4. External-engine reference policy

No single engine is the RelayCore blueprint. Borrow a principle only where it solves a current RelayCore problem.

| Reference | Borrow | Do not copy by default |
| --- | --- | --- |
| GoldSrc | level/FPS feel, readability, compact map logic | legacy renderer/BSP constraints, historical internals |
| Source 1 | entity connections, map logic, surface/collision semantics | legacy architecture for authenticity |
| Source 2 | capability set, compiled-resource decomposition, model instances, physics/light/nav/visibility data | inferred internal architecture; shader/particle/runtime clones |
| Godot | high-level scene/world → subsystem → platform separation | Node/SceneTree/Variant/ClassDB/server model literally |
| O3DE | source→intermediate→product asset pipeline, builders, dependencies, deterministic cache products | Gems/ECS/background-processor complexity initially |
| Unity | source assets vs imported artifacts, dependency invalidation, stable import identity | GameObject/MonoBehaviour architecture |
| Bevy | simulation/world data extracted/prepared for rendering | ECS migration merely to imitate Bevy |
| Unreal | mature asset IDs/references; world partition/streaming ideas at scale | UObject/reflection/GC, Actor framework, Blueprint, AAA-scale machinery |
| id Tech source | readable commercial C/C++ FPS implementation study | modern feature target |
| Khronos Vulkan guidance | explicit GPU ownership/synchronization, modern Vulkan primitives | tutorial architecture copied mechanically |

### 4.1 Why Source 2 is a capability reference, not the architecture master

Public Source 2 format knowledge is largely reverse-engineered. It is strong evidence for **what data/capabilities a modern Valve map needs**, but weak evidence for Valve's real ownership, threading, lifetime, renderer layering, or engine/game boundaries.

Translate Source 2 concepts into RelayCore concepts. Never shape runtime APIs around VRF/Source 2 classes merely because an importer exposes them.

### 4.2 Reference priority by subsystem

| Concern | Primary reference | Secondary |
| --- | --- | --- |
| FPS/level philosophy | GoldSrc | Source 1 |
| Map interactions | Source 1 | Source 2 |
| Modern FPS-world capabilities | Source 2 | Unreal |
| Asset/content decomposition | Source 2 | O3DE |
| Asset build pipeline | O3DE | Unity |
| Engine layering | current RelayCore | Godot |
| Simulation→renderer boundary | current RelayCore | Bevy |
| GPU API/ownership | Vulkan specification/Khronos guidance | current RelayCore ownership conventions |
| Asset IDs/dependency loading | O3DE | Unreal/Unity |
| Visibility/chunks/streaming | Source 2 | Unreal |
| Material feature target | Source 2 | glTF/PBR conventions |
| Collision semantics | Source 1/2 | current RelayCore |

When references disagree, choose the smallest design that satisfies current RelayCore behavior and the concrete requested capability.

## 5. Target architecture

```text
                       AUTHORING / IMPORT

 RelayCore data       Blender/glTF       Source 2 / CS2
      │                    │                  │
      └─────────────┬──────┴──────────┬───────┘
                    ▼                 ▼
               import/build front ends
                    │
                    ▼
           common RelayCore-oriented IR
                    │
                    ▼
              Relay asset compiler
          validate / derive / optimize
                    │
          deterministic product assets
                    │
        ┌───────────┼────────────┐
        ▼           ▼            ▼
     meshes      textures     materials
     models      collision    lighting
     audio       nav          level data
                    │
                    ▼
            runtime asset catalog

----------------------------------------------------
                         RUNTIME

                     LevelAsset
                         │
                  staged preparation
                         │
       ┌─────────────────┼────────────────┐
       ▼                 ▼                ▼
 GameWorldRuntime    PhysicsWorld    PreparedRenderWorld
       │                 │                │
       │                 │                ▼
       │                 │             Renderer
       │                 │                │
       └─────────────────┴────────────────┤
                                         ▼
                                 Vulkan + SDL3 platform
```

Names are responsibilities, not mandatory class names. Reuse current owners when they already fit.

### Runtime boundaries

- A serialized level may aggregate references, but do not create a runtime god-object that owns gameplay, physics, rendering, audio, and nav state.
- Renderer consumes prepared render data; gameplay/runtime does not expose mutable gameplay objects to Vulkan code.
- Physics owns collision/query representation independent from render meshes.
- Asset loading produces typed validated runtime data; renderer creates GPU resources from renderable products.
- Platform/window lifetime is distinct from Vulkan device/swapchain lifetime.
- Source 2 tooling ends before runtime product loading.

## 6. Vulkan ownership and render architecture rules

The detailed migration sequence lives in `docs/VULKAN_MIGRATION_PLAN.md`. Long-term rules belong here.

### 6.1 Ownership

Prefer move-only RAII owners for Vulkan objects that have non-trivial lifetime. Raw `Vk*` handles may be borrowed locally, but ownership must be obvious from type/location.

Expected responsibility split, adjusted to existing code rather than forced class names:

```text
Renderer
 ├─ device/queues/capabilities
 ├─ swapchain + presentation state
 ├─ frame contexts
 ├─ GPU resource owners
 ├─ pipeline/shader state
 ├─ upload path
 ├─ render passes expressed with dynamic rendering
 └─ diagnostics/profiling hooks
```

Do not add a generic `IRenderBackend` merely because OpenGL and Vulkan coexist temporarily. The durable abstraction boundary is render-facing data above `Renderer`, not a virtual clone of graphics API calls.

### 6.2 Modern Vulkan baseline

Use modern core facilities instead of Vulkan-1.0-era architecture:

- dynamic rendering rather than creating a render-pass framework around `VkRenderPass`/`VkFramebuffer`;
- synchronization2 for barriers/submission descriptions;
- timeline semaphores where they simplify cross-frame/upload completion; do not add them where a fence is simpler;
- explicit debug labels/object names;
- validation layers in development;
- SPIR-V produced offline/build-time from Slang;
- pipeline/cache handling kept explicit and measurable.

Vulkan 1.4 is the minimum loader and physical-device API, requested by the project owner on 2026-10-12. Instance and VMA use API 1.4; there is no 1.3 fallback or forced-1.3 mode. Required device feature bits remain explicit. Introduce additional facilities through concrete owning tasks with extension and feature checks; an unsupported mandatory facility must fail startup with a concrete diagnostic. Optional RT/mesh/VRS/HDR capabilities retain their own checks and alternatives.

VK-003 additionally requires `VK_KHR_swapchain_maintenance1` with `swapchainMaintenance1` and its instance extension dependencies. Present fences give swapchain and presentation semaphore references a defined destruction/reuse point without device-wide idle during frames or resize. This requires supporting drivers; missing extensions/features fail startup explicitly. API 1.4 alone does not establish presentation compatibility.

### 6.3 Frame/resource lifetime pattern

Use a small fixed number of frame contexts. Each frame context owns only resources that are actually frame-local: command pool/buffers, transient synchronization, and per-frame allocations/descriptors as required.

Do not call `vkDeviceWaitIdle()` in normal frame flow. Reserve full-idle waits for shutdown or rare recovery paths where simpler targeted synchronization cannot safely express the transition.

GPU resources destroyed while work may still reference them use deferred destruction tied to known GPU completion (frame fence/timeline value). Avoid a universal garbage collector; keep destruction queues scoped to the renderer/device owner.

### 6.4 Descriptor/resource-binding policy

Start with explicit descriptor set/layout conventions sufficient for current renderer parity. Do not begin with descriptor-buffer/bindless architecture merely because Vulkan supports it.

If descriptor indexing, bindless resources or descriptor buffers later solve measured CPU submission/material-scaling problems, adopt them as a separate architecture ticket with hardware requirements and RenderDoc/validation evidence.

### 6.5 Shader policy

Slang is the shader source language/toolchain target. Prefer build-time/offline Slang→SPIR-V for shipped builds. Runtime compilation is optional developer tooling, not a required game-runtime dependency.

Keep shader interface contracts explicit. Reflection may validate or generate repetitive metadata, but must not create opaque runtime magic where binding ownership cannot be inspected.

## 7. Asset pipeline contract

Source assets and runtime products are different layers:

```text
source/authoring files
       ↓ importer/builder
RelayCore-oriented intermediate data
       ↓ compiler/validation/optimization
versioned product assets
       ↓ runtime loader
validated typed runtime data
```

Rules:

- stable asset identity does not depend on raw pointer/GPU handle values;
- runtime formats are versioned and deterministic;
- dependencies are explicit and diagnosable;
- renderer-specific GPU objects are not serialized as asset identity;
- source/import metadata does not leak into gameplay/render APIs unless it has native meaning;
- product compilation owns mesh optimization, texture conversion/compression, tangent/LOD derivation and similar offline work when possible.

Preferred product/tool choices:

- **fastgltf** is the standard glTF 2.0/GLB front end for Blender/native model interchange. It translates into RelayCore-oriented intermediate data; runtime code does not depend on fastgltf objects.
- **meshoptimizer** owns offline mesh optimization/LOD/compression work when the production `MeshAsset` compiler exists. Do not perform these transformations every launch.
- **KTX2/libktx** is the preferred texture product/container direction for mip chains, compressed textures, cubemaps/arrays and HDR products. Prefer the base `ktx` package first so asset/texture code does not bypass RelayCore GPU ownership; enable `ktx[vulkan]` only if a renderer-owned adapter later proves useful.
- **BLAKE3** identifies source/settings/tool-version inputs for incremental asset compilation and can also serve integrity/manifests. A cache hit must still be versioned by importer/compiler semantics; a content digest is not the authored logical asset ID.
- **zstd** may compress compiled products/packages where profiling shows worthwhile storage/I/O benefit. Do not compress already GPU-compressed texture payloads blindly.

None of these are mandatory dependencies before their owning asset-pipeline milestone starts.

## 8. Capability targets

This section describes capability targets, including implemented subsets. Current native v5 models already carry indexed geometry/tangents, material parts, base-color/normal/MR/AO/emissive maps and instance transforms. FP16 HDR, dynamic lights/shadows, baked SH irradiance and one bounded static reflection probe are implemented; lightmaps, dynamic reflections, alpha-native materials, skeletal animation and general navmesh remain outside that subset. Static native collision is integrated behind `WorldQuery`; authored encounter routes are bounded gameplay data, not Recast/Detour navigation.

### Mesh/model

Production mesh data eventually needs:

- indexed positions/normals/tangents;
- UV0 plus UV1/lightmap UV where required;
- optional vertex color;
- submeshes/material slots;
- bounds;
- instances referencing shared mesh/model resources;
- LOD representation when content requires it;
- skeletal data/animation only when animated content enters scope.

Do not discard current blockout/static mesh paths until equivalent content works through the replacement.

### Texture/material

Native texture semantics must distinguish at least color/sRGB, linear data, normal maps and HDR/environment data. Add cube/array/compressed formats when concrete assets require them.

Material representation should support a small useful set before specialized Source 2 translations: base color, normal, roughness/metalness or the chosen RelayCore equivalent, AO/emissive as needed, alpha mode/cutoff, two-sided and UV transform.

Importer translations report `exact`, `approximated`, or `unsupported`; do not silently discard shader semantics.

### Lighting/environment

Preserve current dynamic-light/shadow behavior through Vulkan parity first. After parity, production rendering can add:

- linear HDR scene targets, exposure and tone mapping;
- scalable local-light assignment as justified by actual scenes;
- baked lightmaps and directional information;
- reflection probes/cubemaps;
- irradiance/light probes;
- sky/environment and fog/volumes needed by native/Source 2 content.

Do not reproduce Source 2's renderer exactly; preserve visual intent through native RelayCore systems.

### Collision/physics world

Render geometry and collision are independent products. **Jolt Physics is the preferred production physics/collision foundation** once this milestone begins; do not grow the current AABB code into a home-grown general rigid-body engine.

Preserve a thin RelayCore-owned `PhysicsWorld`/query boundary:

```text
Gameplay / world semantics
          ↓
     PhysicsWorld
          ↓
         Jolt
```

Jolt types (`JPH::*`) must not leak into game-facing authored data or unrelated gameplay APIs. The current AABB collision path remains valid during migration and can be retained as blockout/fast-path data where useful.

Initial Jolt scope should be driven by real content: static triangle/mesh shapes, raycasts and shape casts/sweeps, collision layers/masks, triggers/sensors and surface semantics. Add convex/dynamic rigid bodies when content requires them. Preserve accepted player-controller feel; evaluate Jolt's character facilities versus the current controller with explicit movement-parity tests instead of replacing movement behavior automatically.

A Source 2 physics resource maps into RelayCore collision products/Jolt-compatible cooked data; it does not become a Source 2 runtime object.

### Visibility/submission/world size

Keep current frustum/chunk logic while it works. Add instance batching, spatial chunking, occlusion, indirect/GPU culling or streaming only when full scenes expose a concrete CPU/GPU/memory problem.

Vulkan makes these architectures possible; it does not make them mandatory.

### World objects/map logic

Preserve the useful Source-like qualities already present: explicit entities, doors/buttons/triggers, readable state and target connections. Keep game-specific CS2 classes outside engine core. Unknown imported entities remain metadata/report entries until a native mapping is justified.

### Animation/FX/audio/navigation

Add native capabilities in slices driven by game content or the chosen acceptance map. Do not import Source 2 animation graphs, particle systems or nav runtime wholesale. Translate supported data into native RelayCore contracts.

Use **Recast** for native navmesh generation and **Detour** for runtime path queries when navigation enters scope. Rebuild navigation from RelayCore collision/walkable geometry first; imported Source 2 navigation may contribute metadata or validation evidence, but it should not dictate runtime nav internals.

Keep **miniaudio** as the playback/mixing/resource foundation. If geometry-aware acoustics becomes a real requirement, add **Steam Audio** behind the existing audio/world boundary for HRTF, occlusion, reflections and propagation; do not replace miniaudio merely to adopt Steam Audio. Steam Audio should enter only after stable arbitrary world geometry/collision exists, because acoustic geometry and bake/update ownership must be explicit.

For skeletal animation, evaluate **ozz-animation** at the first real animated-model milestone. It is a preferred candidate for sampling/blending/runtime data, not an immediate dependency; integrate it only through a reproducible pinned mechanism and keep RelayCore-owned model/skeleton/animation IDs and gameplay animation state.

## 9. Source 2 importer boundary

Preferred high-level pipeline:

```text
CS2 installation / VPK / compiled resources
                  ↓
       Relay Source 2 extractor
       (VRF isolated in tooling)
                  ↓
       RelayCore-oriented IR
                  ↓
          common asset compiler
                  ↓
        native runtime products
```

The extractor may be C#/.NET because ValveResourceFormat is C#. That does not justify embedding .NET/VRF in the C++ runtime.

Responsibilities:

- dependency traversal from the selected map;
- deterministic coordinate/unit conversion;
- meshes/models and instances;
- material/texture translation;
- independent collision/physics data;
- lights, baked-light data, probes/reflections/environment where extractable;
- entity metadata/mappings;
- navigation/visibility data where useful;
- import diagnostics and provenance.

### Coordinate contract

Define one canonical RelayCore world convention and one tested Source 2→RelayCore conversion layer. Conversion tests cover points, directions, normals, winding, transforms, rotations, AABBs and any tangent handedness/UV-origin adjustments.

### Import report

Every import records counts and fidelity status for important resource classes. Unknown/unsupported semantics are visible failures or warnings, not silent replacements.

Do not commit Valve copyrighted map/content assets unless licensing explicitly allows it. Full-map acceptance should use locally installed content.

## 10. Dependency roadmap

This is long-term dependency order, not the current sprint queue.

### A — Baseline and API-neutral render contracts

Inventory current GL/window/renderer/resource ownership and lock parity evidence. Make only the minimum CPU/render-facing data changes required so world/game code does not depend on OpenGL-specific handles or context behavior.

**Gate:** current OpenGL renderer still builds/runs and render-facing contracts can be consumed without exposing GL ownership above renderer/platform boundaries.

### B — Vulkan migration

Execute `docs/VULKAN_MIGRATION_PLAN.md`: bootstrap/toolchain → device/swapchain/frame lifecycle → GPU resources/upload → Slang pipelines → current world/models/UI → lighting/shadows/post/color → diagnostics/profiling → parity → OpenGL removal.

**Gate:** Vulkan is the sole supported runtime renderer, current levels/gameplay preserve accepted behavior, graphics validation is clean for tested paths, and OpenGL/glad are removed only after parity evidence exists.

### C — Native production assets

Historical scheduling decision (2026-10-02): the owner deferred blind playtesting/another GPU and explicitly authorized E-003, the first static MeshAsset/ModelAsset slice. GLB is compiler input; `.rcmodel` is the versioned native runtime product. Subsequent implemented capabilities are summarized at the top of this document; their scope and evidence belong to the roadmap. Further asset/rendering scope remains tied to demonstrated content needs and measurements; the original slice did not authorize the rest of this program.

E-004 completed the declared GLB build/staging slice: sources compile into per-configuration products, with SHA256 receipts checked during packaging. It used CMake file dependencies without a general asset cache. Later material/product versions are separate completed tickets; E-004 is not the next task.

Add production mesh/model data, texture products, material representation, resource references/dependencies as actually needed, and a richer native level/instance path that coexists with `BlockoutLevel`.

Use **fastgltf** for glTF/GLB source import and **meshoptimizer** in the compiler for derived mesh products. Adopt **KTX2/libktx** as the preferred texture product path. Prefer the base `ktx` package first so texture parsing/product logic stays separate from Vulkan image ownership; enable libktx Vulkan helpers only if a renderer-owned task later demonstrates a concrete benefit. Use **BLAKE3** for content fingerprints/cache keys when incremental compilation appears.

**Gate:** a project-authored test room uses arbitrary mesh instances/materials through the production path while existing levels still work.

### D — Production rendering

Build new rendering capabilities on Vulkan: normal/material shading, HDR, then baked lighting/probes/environment in small slices; add culling/instancing improvements from profiler evidence.

**Gate:** native content demonstrates every rendering primitive intended for the first Source 2 import.

### E — Production collision/world semantics

Integrate **Jolt Physics** behind RelayCore's physics boundary. Add independent collision products, static arbitrary-geometry collision, sweeps/raycast, layers/tags/triggers and required map semantics such as ladders/water. Preserve player movement behavior through explicit parity checks before adopting a different character-controller implementation.

**Gate:** a native arbitrary-mesh level is traversable without converting its world to blockout AABBs, and gameplay code does not depend directly on `JPH::*` types.

### F — Rich world features

Add only those required by game content or the chosen Source 2 acceptance map: decals, animation, particles, navmesh/path following, richer visibility/streaming and advanced spatial acoustics. **Recast/Detour** is the navigation foundation. Keep miniaudio; add **Steam Audio** only when HRTF/geometry-aware occlusion/reflections/propagation have a concrete owner and stable world geometry input. Evaluate ozz-animation only when skeletal animation has a real consumer.

**Gate:** each feature works from native RelayCore content without Source 2 tooling.

### G — Common asset compiler

Add explicit CLI build, versioned product formats, deterministic output, dependency manifest/catalog, validation and diagnostics. Use **BLAKE3** for content fingerprints/cache keys and **zstd** for selected product/package compression when they produce measurable build/storage/I/O value. Cache keys include importer/compiler version and settings, not just file bytes. Logical asset identity remains separate from a content digest.

**Gate:** clean native sources rebuild runtime products without manual edits; repeated unchanged builds can prove deterministic cache behavior when caching is enabled.

### H — Source 2 front end

Validate incrementally:

1. one prop: model + material/textures + transform + collision;
2. small room/section: world geometry + instances + collision + lights;
3. required baked/probe/reflection/sky/fog fidelity;
4. relevant triggers/ladders/audio/nav/FX;
5. full representative map.

`de_dust2` is a practical candidate, but map choice is an acceptance decision, not an architectural dependency.

### I — Full-map acceptance/hardening

Accept when:

- import from a clean local CS2 install is reproducible with one documented workflow;
- scale/orientation/placements are correct;
- repeated models remain instances where sensible;
- render/collision stay independently represented;
- material/lighting/environment intent is preserved and fidelity gaps are reported;
- the RelayCore player traverses the map through native world queries;
- supported map semantics work;
- full-map performance is profiled before optimization;
- reimport requires no manual scene-file repair;
- removing the Source 2 importer would remove no native engine capability.

## 11. Migration strategy

Avoid big-bang rewrites.

General rule:

1. preserve a working reference path;
2. add one complete replacement slice;
3. compare behavior/evidence;
4. migrate callers/content;
5. remove the old path only after no required content depends on it.

For Vulkan specifically, OpenGL may coexist temporarily only to provide parity/reference coverage. Do not maintain a long-lived dual-backend abstraction. Freeze new OpenGL renderer features during migration except fixes required to preserve the reference baseline.

For content/world evolution:

- do not delete `StaticColorMesh` before the production mesh path can carry current content;
- do not replace TOML authoring merely because product assets exist;
- do not replace `WorldSceneRenderer` just to obtain a generic scene architecture;
- do not introduce ECS to support imported props;
- do not introduce a render graph merely because pass count grows; require a demonstrated ordering/resource-lifetime maintenance problem.

## 12. Hard architecture rules

General engine/game/tooling ownership, anti-slop and dependency rules belong to [AGENTS](../AGENTS.MD#implementation). Additional importer constraints:

- Keep Source 2 decoding/conversion in offline tooling; no Source 2 classes/file parsing or special branches in generic runtime APIs.
- Import output must be native RelayCore data usable without the source engine. Unsupported source features require an import report, not a silently degraded contract.
- GPU handles and third-party implementation types are not gameplay/world/save/asset identity contracts.
- Preserve explicit Vulkan device/window/resource lifetime; a new capability must prove its native consumer and compatibility before expanding architecture.

## 13. Decision checklist for a new engine feature

Before implementing a Source 2-inspired or "modern engine" feature:

1. What current RelayCore/game/import scenario is blocked?
2. Which existing subsystem should own it?
3. Can native RelayCore content use it? If not, it likely belongs in importer tooling.
4. What is the smallest useful representation now?
5. What existing path must remain compatible?
6. What deterministic test/native scene can falsify it?
7. What evidence would justify the next, more complex architecture step?

For Vulkan work add:

8. Is this parity, required infrastructure, or a new rendering feature? Do not mix categories without necessity.
9. Who owns the `Vk*` object and what completion condition permits destruction/reuse?
10. Can validation/RenderDoc/Tracy disprove the intended behavior?

Copy the problem-solving principle, not a reference engine/framework.

## 14. Verification ladder

For foundational work, increase scope only as needed:

1. **Unit/format contract** — deterministic transforms/serialization/resource-state helpers where pure logic exists.
2. **Vulkan API validation** — standard validation; synchronization validation for sync/resource-state work; GPU-assisted validation for focused investigations.
3. **Native RelayCore fixture** — proves the feature is not Source 2-specific.
4. **RenderDoc/graphics comparison** — resource formats, attachments, barriers, draw state and visible parity.
5. **Tracy/reproducible performance capture** — only for performance claims.
6. **Small imported content** — proves translation.
7. **Full local map** — records import report, visual/collision gaps, unsupported features and relevant load/CPU/GPU/VRAM evidence.

Do not claim correctness from compilation alone. Do not optimize without measured evidence.

## 15. Agent workflow

Follow [AGENTS](../AGENTS.MD) and read only the relevant capability/owning contract. Classify the selected work as parity, native capability, importer tooling or measured optimization. Keep Source 2 conversion in tooling; extend the existing owner before creating a parallel mechanism. Use the verification ladder above for the actual affected boundary and record evidence in the owning ticket/report.

## 16. Research references

Dependency-specific package/version evidence and integration decisions are centralized in `DEPENDENCY_STACK.md`; keep this file focused on durable architecture. Recheck exact APIs/current behavior before relying on them in an implementation task.

- ValveResourceFormat / Source 2 Viewer: https://github.com/ValveResourceFormat/ValveResourceFormat
- VRF compiled-resource guide: https://github.com/ValveResourceFormat/ValveResourceFormat/blob/master/docs/guides/read-resource.md
- Vulkan specification: https://registry.khronos.org/vulkan/
- Khronos Vulkan Guide: https://github.khronos.org/Vulkan-Site/guide/latest/
- Khronos Vulkan Tutorial: https://docs.vulkan.org/tutorial/latest/
- Vulkan Memory Allocator: https://gpuopen.com/vulkan-memory-allocator/
- vk-bootstrap: https://github.com/charles-lunarg/vk-bootstrap
- Slang: https://github.com/shader-slang/slang
- Jolt Physics: https://github.com/jrouwe/JoltPhysics
- fastgltf: https://github.com/spnda/fastgltf
- meshoptimizer: https://github.com/zeux/meshoptimizer
- KTX-Software / KTX2: https://github.com/KhronosGroup/KTX-Software
- BLAKE3: https://github.com/BLAKE3-team/BLAKE3
- Recast Navigation: https://github.com/recastnavigation/recastnavigation
- ozz-animation: https://github.com/guillaumeblanc/ozz-animation
- zstd: https://github.com/facebook/zstd
- Steam Audio: https://valvesoftware.github.io/steam-audio/
- Dear ImGui Vulkan backend: https://github.com/ocornut/imgui/tree/master/backends
- RenderDoc: https://github.com/baldurk/renderdoc
- Godot architecture overview: https://docs.godotengine.org/en/latest/engine_details/architecture/godot_architecture_diagram.html
- O3DE asset pipeline: https://docs.o3de.org/docs/user-guide/assets/pipeline/
- Unity Asset Database: https://docs.unity3d.com/6000.0/Documentation/Manual/AssetDatabase.html
- Bevy render extraction: https://docs.rs/bevy/latest/bevy/render/struct.Extract.html
- Unreal asset management: https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-management-in-unreal-engine
- Unreal World Partition: https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine
- Doom 3 BFG source: https://github.com/id-Software/DOOM-3-BFG

## 17. End-state definition

The direction is successful when:

- Vulkan is the normal RelayCore renderer and OpenGL-specific runtime code/dependencies have been removed after verified parity;
- native RelayCore levels can use production mesh/material/collision/light/world capabilities without Source 2 tooling;
- source/import formats build deterministic versioned RelayCore runtime products;
- gameplay, physics, rendering, audio/nav and tools keep explicit ownership boundaries;
- a representative CS2 map imports reproducibly, with important approximations/unsupported features reported instead of hidden;
- the imported map uses the same normal runtime systems as native content;
- performance changes remain evidence-driven;
- the codebase remains maintainable under agent-driven implementation and human review;
- Source 2 is a capability/reference target, not the identity or internal architecture of RelayCore.
