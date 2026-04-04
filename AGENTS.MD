# AGENTS.md — RelayCore

## Project

RelayCore is a small C++20 retro-FPS / engine-programming project focused on building a playable indoor FPS slice rather
than a tech demo.

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

## Project direction

RelayCore is not a historical recreation of GoldSrc.

It should aim for the strongest qualities of a GoldSource-inspired indoor FPS, especially the atmosphere associated with
Half-Life 1:

- retro-industrial sci-fi
- liminal and technical spaces
- readable room/corridor-oriented layouts
- strong sense of place
- moody ambient presentation
- clear gameplay-facing structure
- memorable spatial identity through geometry, materials, lighting, and sound

Preserve the feel and design philosophy, but implement systems using modern engineering practices, modern tooling, and
maintainable architecture.

Target philosophy:

- classic FPS feel, modern codebase
- old-school atmosphere, modern implementation
- simple and inspectable systems, not archaic solutions
- practical performance and debuggability over historical imitation

Do not imitate legacy engine internals mechanically just for authenticity.

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

## Core design principles

Prioritize:

- simplicity
- debuggability
- small incremental changes
- explicit ownership and lifetime
- predictable runtime behavior
- practical engine architecture over clever abstractions
- solo-developer maintainability

Prefer:

- clear module boundaries
- RAII
- value types where reasonable
- explicit data flow
- localized state changes
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

## What to preserve from GoldSource-like design

Preserve these principles where they help the project:

- readable world layout
- strong gameplay-oriented spatial structure
- clear interaction logic embedded in the map/world
- tight and inspectable movement/query model
- simple and debuggable traces/collision logic
- map/data as a meaningful gameplay container
- strong world readability over visual noise
- disciplined materials and environmental storytelling

## What to modernize

Use modern implementations where they improve clarity, iteration speed, stability, and maintainability.

Modernization is encouraged for:

- code organization
- tooling
- diagnostics
- asset handling
- material systems
- lighting/shadowing approaches
- rendering architecture
- build/test workflow

Do not preserve historical constraints unless they meaningfully support the target feel.

## Repository mindset

This repository is primarily:

- a practical engine-learning codebase
- a GoldSource-inspired retro-FPS foundation
- a small systems project that should remain understandable and shippable by one developer

Optimize for a codebase that a solo developer can keep shipping.

## Typical repository areas

Treat the repository roughly as having these responsibilities:

- engine runtime / subsystems
- game layer / gameplay glue
- world + blockout/level data
- rendering
- input/platform/app bootstrap
- assets/config
- debug/overlay/profiling
- tests
- CI/build configuration

When changing code, respect existing boundaries instead of spreading responsibilities across unrelated modules.

## Working style

For non-trivial tasks:

1. First inspect the existing code and summarize how the relevant subsystem currently works.
2. Then propose a short plan.
3. Then implement the smallest viable patch.
4. Then run the most relevant validation steps.
5. Then report:
    - what changed
    - why
    - risks / follow-ups
    - next recommended step

Do not make broad speculative refactors unless explicitly requested.

If the task is ambiguous or large:

- prefer analysis and a short implementation plan first
- prefer staged delivery over a giant rewrite
- keep scope tight

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

- Use C++20, but prefer conservative and readable usage.
- Keep headers lightweight.
- Minimize include spread and rebuild cost.
- Prefer const correctness and narrow scopes.
- Avoid dynamic allocation unless needed.
- Prefer explicit types when they improve readability.
- Add comments only where intent is non-obvious.
- Preserve predictable lifetime and ownership.
- Avoid cleverness that hurts debugging.

When changing engine code:

- preserve debugability
- keep hot-path code simple
- prefer deterministic behavior
- add diagnostics when useful
- avoid hidden cross-system coupling

## Rendering guidance

This is an engine-learning project with a GoldSource-inspired retro indoor FPS target and a Half-Life-1-like atmospheric
direction, implemented with modern technology and maintainable engine architecture.

Prefer:

- simple explicit render paths
- stable frame debugging
- easy inspection in Tracy / logs / overlay
- render-world structures that are easy to reason about
- pragmatic material and lighting systems
- strong spatial clarity over visual excess

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

When implementing world systems:

- keep static world and dynamic objects conceptually distinct
- keep queries easy to debug
- keep gameplay logic understandable from data + code together
- prefer blockout-first iteration over premature content pipeline complexity

## Architecture guidance

Prefer clear boundaries between:

- platform/app bootstrap
- input
- render
- audio
- assets/config
- world representation
- collision/world queries
- gameplay logic
- debug/editor-facing tooling

Do not tightly couple gameplay logic directly to rendering details.
Do not let debug code silently become production architecture.
Do not collapse engine/game/data boundaries without a strong reason.

Prefer:

- explicit ownership
- simple interface boundaries
- predictable data lifetime
- direct calls over unnecessary messaging
- events/messages only where they genuinely reduce coupling

## Data and content guidance

Prefer data-driven behavior where it improves iteration and clarity, especially for:

- level/blockout description
- simple interactables
- triggers
- doors/buttons/lifts
- material assignment
- tuning values

Do not build a giant generic data framework too early.
Keep formats simple and inspectable.

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

## Editing constraints

Do not rewrite unrelated files.
Do not rename public-facing modules or move large subsystems without strong reason.
Do not silently change asset formats, build presets, CI behavior, or project structure without calling it out.

Prefer minimal diffs.

## Build/test/validation

Use the project’s existing build flow and presets.
Prefer small checkpoints and keep the repo in a runnable state.

After code changes, use the most relevant checks available, such as:

- configure/build with the existing CMake preset
- run relevant doctest targets if affected
- verify the app still launches if startup/runtime code changed
- verify no obvious regressions in overlay/logging if diagnostics were touched

If you cannot run a check, say so explicitly and explain why.

## Done means

A task is not done just because code was written.

For implementation tasks, done usually means:

- the requested behavior is implemented
- the change fits the existing architecture
- the diff is scoped and reviewable
- relevant checks/build steps were run when possible
- risks and follow-ups were called out

## Performance/debugging

Treat performance as important, but do not micro-optimize early.

Prefer:

- measuring before optimizing
- Tracy instrumentation for meaningful hot paths
- fixing correctness and architecture bottlenecks before low-value tweaks

When touching frame-critical code, briefly note:

- expected runtime impact
- debugging impact
- whether more instrumentation is needed

## Good task patterns

Good examples:

- “Analyze the door interaction path, find why close/open toggling fails, propose a minimal fix, then validate the
  build.”
- “Trace player collision against blockout geometry, explain the current algorithm, identify failure cases, and patch
  only the smallest necessary parts.”
- “Inspect rendering of blockout chunks, explain visibility/culling flow, and suggest the next incremental step toward
  room-scale indoor FPS content.”
- “Analyze how this subsystem supports or weakens the intended GoldSource/Half-Life-1-like feel, then propose the
  smallest practical improvement.”
