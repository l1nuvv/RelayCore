# AGENTS.md — RelayCore

## Project
RelayCore is a small C++20 retro-FPS / engine-programming project focused on building a playable indoor FPS slice rather than a tech demo.

Current stack:
- Windows 11
- MSVC
- CMake presets
- CLion
- SDL3
- OpenGL 4.6 Core
- glad2
- GLM
- spdlog
- Dear ImGui
- toml++
- miniaudio
- doctest
- Tracy
- vcpkg manifest mode
- GitHub Actions

## Identity and direction
RelayCore is not a historical recreation of GoldSrc.

It should aim for the strongest qualities of a GoldSource-inspired indoor FPS, especially the feel associated with Half-Life 1:
- retro-industrial sci-fi
- liminal and technical spaces
- readable room/corridor-oriented layouts
- strong sense of place
- moody ambient presentation
- clear gameplay-facing structure
- memorable spatial identity through geometry, materials, lighting, and sound

Preserve the feel and design philosophy, but implement systems using modern engineering practices, modern tooling, and maintainable architecture.

Target philosophy:
- classic FPS feel, modern codebase
- old-school atmosphere, modern implementation
- simple and inspectable systems, not archaic solutions
- practical performance and debuggability over historical imitation

Do not imitate legacy engine internals mechanically just for authenticity.

GoldSource / Half-Life-like influence should primarily guide:
- spatial composition
- interaction readability
- movement/collision feel
- atmosphere
- map-embedded gameplay logic
- environmental storytelling through space, sound, and material choices

Historical constraints are not goals unless they materially support the intended feel.

## Product goal
Move the project toward a small playable indoor FPS slice with:
- spawn
- walking
- collision
- room/corridor spaces
- doors / buttons / triggers
- materials / textures
- lighting
- ambient/world audio
- one small polished level

Prefer vertical slices.
Every step must end in a working build.

## Repository mindset
This repository is primarily:
- a practical engine-learning codebase
- a GoldSource-inspired retro-FPS foundation
- a small systems project that should remain understandable and shippable by one developer

Optimize for a codebase that a solo developer can keep shipping.

Judge architecture primarily by:
- cost of change
- ownership clarity
- ease of debugging
- iteration speed
- stability under incremental growth
- fit for the current project scope

## Core principles
Prioritize:
- simplicity
- debuggability
- explicit ownership and lifetime
- predictable runtime behavior
- small incremental changes
- practical engine architecture over clever abstractions
- solo-developer maintainability

Prefer:
- clear module boundaries
- RAII
- value types where reasonable
- explicit data flow
- localized state changes
- typed runtime data
- deterministic behavior where practical
- straightforward logging and error handling
- minimal diffs

Avoid introducing too early:
- premature ECS complexity
- template-heavy abstractions without strong payoff
- hidden global state
- unnecessary inheritance hierarchies
- large framework-style refactors
- full BSP pipelines
- full editor/tool suites
- advanced render graph systems
- network stacks
- Vulkan migrations
- new third-party dependencies unless clearly justified

## Current non-goals
Unless explicitly requested, do not optimize the architecture toward:
- full ECS migration
- generalized editor tooling
- scripting VM/runtime
- save/load framework
- multiplayer/network architecture
- generalized quest/state authoring frameworks
- AAA-style physically based rendering complexity
- procedural generation frameworks
- advanced job systems/fibers
- complex visibility systems beyond current project needs

## Dependency direction rules
Prefer one-way dependency flow.

Default direction:
- platform/app bootstrap may depend on engine subsystems
- gameplay may depend on engine/runtime services
- renderer may consume prepared world/runtime data
- debug/overlay may inspect runtime state
- asset/content loading may produce validated runtime data

Avoid reverse coupling:
- renderer should not own gameplay rules
- debug/overlay should not become a required production dependency
- asset/content loading should not depend on UI/debug presentation
- low-level engine subsystems should not depend on game-specific code
- runtime data models should not depend on renderer implementation details

If a dependency points upward in the stack, call it out explicitly and justify it.

## Typical repository areas
Treat the repository roughly as having these responsibilities:
- platform/app bootstrap
- engine runtime / subsystems
- game layer / gameplay glue
- world + blockout/level data
- collision/world queries
- rendering
- audio
- assets/config
- debug/overlay/profiling
- tests
- CI/build configuration

Respect existing boundaries instead of spreading responsibilities across unrelated modules.

## Frame/update phase guidance
Prefer a stable and explicit frame pipeline.

Typical order:
1. gather raw input
2. resolve commands/actions
3. update gameplay/world state
4. resolve collision and world queries
5. process queued events at explicit points
6. submit audio/debug/render-facing results
7. build render submission data
8. render
9. draw diagnostics/overlay

Rules:
- avoid hidden side effects outside the intended frame phase
- do not mutate gameplay state deep inside rendering code
- do not let debug/UI code silently become part of gameplay update flow
- keep update order explainable in a few sentences
- prefer controlled points of event delivery over ad hoc callbacks

## Runtime and data-shape guidance
Keep these layers conceptually separate:
- authored content data
- validated typed runtime data
- renderer-facing submission data

Do not let raw authored formats or stringly-typed property maps leak deep into runtime logic or hot paths.

Parse early, validate early, and convert to typed structures as soon as practical.

Prefer:
- typed runtime data
- explicit IDs/handles where useful
- subsystem-owned storage
- contiguous storage for frequently updated or query-heavy data
- hot/cold data separation when useful
- simple transforms from one data form to another
- minimal per-frame allocations in hot paths

Avoid:
- giant runtime objects that own unrelated domains at once
- string-keyed runtime logic in frequently executed code
- hidden ownership or ambiguous lifetime
- excessive pointer chasing in update/render critical paths
- mixing authored data concerns with runtime simulation logic

## Authored data and content contract
Prefer data-driven behavior where it improves iteration and clarity, especially for:
- level/blockout description
- simple interactables
- triggers
- doors/buttons/lifts
- material assignment
- tuning values

Keep formats simple and inspectable.

Rules:
- validate authored data early
- fail loudly and clearly on malformed critical content
- use explicit defaults where reasonable
- do not silently “fix” broken content unless that behavior is intentional and documented
- distinguish load-time validation errors from runtime simulation errors
- keep content schemas narrow and practical
- authored data should be easy to inspect by hand

Avoid:
- giant generic data frameworks
- weakly validated runtime dependence on loose content
- silent clamps/fallbacks that hide authoring mistakes
- schema complexity that exceeds current project needs

## Ownership and lifetime guidance
Prefer:
- explicit ownership
- predictable lifetime
- narrow interfaces
- subsystem-local reasoning
- data owned by the subsystem that updates it

Do not treat entities/objects as giant owners of unrelated behavior.

Prefer:
- lightweight identity/handle concepts where useful
- clear separation between identity, state, and presentation
- runtime structures that make invalid states hard to represent
- direct dependencies over hidden service access when practical

When a design choice is unclear, prefer the version with:
- simpler ownership
- fewer hidden dependencies
- clearer update order
- easier validation and debugging

## Preferred implementation patterns
Prefer these patterns when they fit:
- command/action layer between raw input and gameplay logic
- explicit state machines for doors, buttons, locomotion, and interaction modes
- component-style decomposition instead of deep inheritance
- direct calls by default
- events/messages only when they clearly reduce coupling or improve frame-phase control
- dirty flags for expensive derived data only when caching is justified

Message/event policy:
- default to direct calls
- use events for cross-subsystem notification or explicit frame-phase delivery
- every event should have a clear producer, consumer, and processing point
- avoid “everything through an event bus” architecture

Do not introduce full ECS architecture by default.
Use ECS-style ideas only when current composition/query/update problems clearly justify the added complexity.

Avoid:
- god objects
- director-everything objects
- deep inheritance trees for gameplay entities
- global singletons
- large message-bus-first architectures without clear benefit

## World and gameplay guidance
Near-term gameplay/world work should favor:
- reliable collision
- usable interaction
- simple doors/buttons/triggers
- stable player controller behavior
- level iteration speed
- room/corridor/facility-like blockout spaces
- easy-to-debug authoring through TOML/blockout data

When implementing interactions:
- prefer explicit state machines
- keep trigger conditions inspectable
- avoid hidden magic behavior
- expose useful diagnostics for overlay/logging
- prefer data + code together to explain behavior clearly

When implementing world systems:
- keep static world and dynamic objects conceptually distinct
- keep queries easy to debug
- keep gameplay logic understandable from data + code together
- prefer blockout-first iteration over premature content pipeline complexity
- keep world queries deterministic and inspectable where possible

When evaluating gameplay/world changes, ask:
- does this improve readability of space?
- does this support the intended Half-Life-like feel?
- is the interaction/collision model easier or harder to reason about?
- does this improve iteration speed or only add abstraction?

## Rendering guidance
This is an engine-learning project with a GoldSource-inspired retro indoor FPS target and a Half-Life-1-like atmospheric direction, implemented with modern technology and maintainable engine architecture.

Prefer:
- simple explicit render paths
- stable frame debugging
- easy inspection in Tracy / logs / overlay
- render-world structures that are easy to reason about
- pragmatic material and lighting systems
- strong spatial clarity over visual excess
- renderer-facing data prepared from runtime/world state
- explicit ownership of GPU resources and render-time state

Do not introduce advanced rendering architecture too early unless explicitly requested, including:
- render graph systems
- overly abstract material systems
- ECS-driven rendering rewrites
- heavyweight generalized scene frameworks
- complex visibility systems beyond current project needs

When making rendering decisions:
- preserve readability of space
- avoid over-glossy or over-busy results that weaken the intended atmosphere
- favor coherent industrial sci-fi presentation over generic modern FPS aesthetics
- prefer solutions with strong results per unit complexity

Treat the renderer as a consumer of prepared data, not the owner of gameplay rules.

## Performance and data locality guidance
Treat hot-path performance primarily as a data-shape problem before treating it as an abstraction problem.

Prefer:
- measuring before optimizing
- Tracy instrumentation for meaningful hot paths
- fixing correctness and architecture bottlenecks before low-value tweaks
- optimizing data layout before inventing elaborate abstractions
- simple iteration over tightly packed data in hot paths
- predictable memory access patterns
- minimal per-frame allocations in frequently executed systems

Only add more advanced systems when profiling justifies them, including:
- spatial partitioning
- job systems / fibers
- advanced visibility systems
- render graph / frame graph architecture

Do not add complexity because it “sounds engine-like”.

## Derived data and caching guidance
Use cached/derived data only when it reduces meaningful repeated work.

Prefer dirty flags when:
- source data changes less often than derived data is queried
- recomputation is non-trivial
- invalidation rules are easy to reason about

Avoid dirty-flag complexity when:
- derived data is cheap to recompute
- every change is immediately followed by use anyway
- stale-data bugs would be hard to detect

Be explicit about:
- who owns cached data
- what invalidates it
- when it is rebuilt
- how it is verified in debugging

## Debug and diagnostics contract
Debugability is a feature, not a luxury.

For new runtime systems, prefer at least one of:
- useful structured logs
- overlay-visible state
- deterministic test coverage
- Tracy zones/counters for meaningful hot paths
- simple ways to inspect important transitions or queries

Important state changes should be easy to observe.
If a system is difficult to inspect, treat that as an architectural weakness.

Do not let debug code silently become required production architecture.
Do not hide critical system behavior behind opaque helper layers.

## Temporary code / hack policy
Temporary code is allowed only when it is explicit.

If a workaround or hack is introduced:
- say that it is temporary
- state why it exists
- keep its scope small
- note the follow-up that would replace it cleanly

Avoid silent “just for now” architecture.
Temporary code that is not clearly marked tends to become permanent.

## Refactor trigger guidance
Prefer small patches by default, but refactor when one or more of these become true:
- a file mixes parsing, runtime logic, rendering, and debug concerns
- a simple feature requires changes across too many unrelated modules
- ownership/lifetime is difficult to explain clearly
- debugging a bug requires stepping through too many layers
- a subsystem API no longer reflects the real data flow
- stringly-typed or weakly validated logic has spread into runtime-critical code
- a module has become a bottleneck for iteration

When refactoring, prefer the smallest split that restores clarity.

## Working style
For non-trivial tasks:
1. inspect the existing code and summarize how the relevant subsystem currently works
2. propose a short plan
3. implement the smallest viable patch
4. run the most relevant validation steps
5. report:
    - what changed
    - why
    - risks / follow-ups
    - next recommended step

Rules:
- do not make broad speculative refactors unless explicitly requested
- prefer staged delivery over giant rewrites
- keep scope tight
- prefer a small correct patch over a large theoretically cleaner rewrite
- when blocked by ambiguity, analyze current code and propose the narrowest practical next move

## Communication style
Be concise, technical, practical, and implementation-focused.

For substantial tasks, include:
- current cause/problem
- chosen approach
- minimal patch summary
- validation performed
- remaining risks
- next recommended step

Do not pad the response with generic advice.
Do not restate basic engine concepts unless they are directly relevant to the current code/task.

## Code guidelines
Match the existing project style and naming.

When editing C++:
- use C++20, but prefer conservative and readable usage
- keep headers lightweight
- minimize include spread and rebuild cost
- prefer const correctness and narrow scopes
- avoid dynamic allocation unless needed
- prefer explicit types when they improve readability
- add comments only where intent is non-obvious
- preserve predictable lifetime and ownership
- avoid cleverness that hurts debugging

When changing engine code:
- preserve debugability
- keep hot-path code simple
- prefer deterministic behavior
- add diagnostics when useful
- avoid hidden cross-system coupling
- prefer data-shape clarity over abstraction density

## Editing constraints
Do not rewrite unrelated files.
Do not rename public-facing modules or move large subsystems without strong reason.
Do not silently change asset formats, build presets, CI behavior, or project structure without calling it out.

Prefer minimal diffs.

If a file mixes too many responsibilities, prefer a small targeted split rather than a sweeping rewrite.

## Dependency policy
Do not add new dependencies unless:
- the benefit is concrete
- the integration cost is small
- the dependency fits the project scope
- the user asked for it or it is clearly justified

If suggesting a dependency, explain:
- why it is needed
- what problem it solves
- why the standard library / current stack is insufficient

## Build/test/validation
Use the project’s existing build flow and presets.
Prefer small checkpoints and keep the repo in a runnable state.

After code changes, use the most relevant checks available, such as:
- configure/build with the existing CMake preset
- run relevant doctest targets if affected
- verify the app still launches if startup/runtime code changed
- verify no obvious regressions in overlay/logging if diagnostics were touched

Prefer tests around:
- parsing/loading
- validation
- collision/math
- interaction resolution
- deterministic runtime transforms
- content-to-runtime conversion

If you cannot run a check, say so explicitly and explain why.

## Definition of done
A task is not done just because code was written.

For implementation tasks, done usually means:
- the requested behavior is implemented
- the change fits the existing architecture
- the diff is scoped and reviewable
- relevant checks/build steps were run when possible
- risks and follow-ups were called out
- the result is still understandable by a solo developer maintaining the codebase

## Good task patterns
Good examples:
- “Analyze the door interaction path, find why close/open toggling fails, propose a minimal fix, then validate the build.”
- “Trace player collision against blockout geometry, explain the current algorithm, identify failure cases, and patch only the smallest necessary parts.”
- “Inspect rendering of blockout chunks, explain visibility/culling flow, and suggest the next incremental step toward room-scale indoor FPS content.”
- “Analyze how this subsystem supports or weakens the intended GoldSource/Half-Life-1-like feel, then propose the smallest practical improvement.”
- “Inspect how authored level data becomes runtime data, identify stringly-typed or weakly validated areas, and propose the smallest practical conversion toward typed structures.”
