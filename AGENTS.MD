# AGENTS.md — RelayCore

## Project
RelayCore is a small C++20 retro-FPS / engine-programming starter focused on a playable indoor FPS slice rather than a tech demo.

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

## Product goal
Move the project toward a small playable indoor FPS slice with:
- spawn
- walking
- collision
- doors / buttons / triggers
- materials / textures
- lighting
- ambient/world audio
- one small polished level

Prefer vertical slices. Every step must end in a working build.

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

Do not make broad speculative refactors unless explicitly requested.

## Architecture priorities
Prioritize:
- simplicity
- debuggability
- small incremental changes
- explicit ownership and lifetime
- predictable runtime behavior
- practical engine architecture over clever abstractions

Prefer:
- clear module boundaries
- RAII
- value types where reasonable
- explicit data flow
- localized state changes
- straightforward error handling and logging

Avoid introducing:
- premature ECS complexity
- template-heavy abstractions without strong payoff
- hidden global state
- unnecessary inheritance hierarchies
- large “framework” refactors
- new third-party dependencies unless clearly justified

## Code guidelines
Match the existing project style and naming.

When editing C++:
- Use C++20, but prefer conservative, readable usage.
- Keep headers lightweight.
- Minimize include spread and rebuild cost.
- Prefer `const` correctness and narrow scopes.
- Avoid dynamic allocation unless needed.
- Prefer explicit types when they improve readability.
- Add comments only where intent is non-obvious.

When changing engine code:
- Preserve debugability.
- Keep hot-path code simple.
- Prefer deterministic behavior.
- Add diagnostics when useful.

## Rendering guidance
This is an engine-learning project with a retro indoor FPS target.

Prefer:
- simple explicit render paths
- data-oriented world/build steps where useful
- stable frame debugging
- easy inspection in Tracy / logs / overlay

Do not introduce advanced rendering architecture too early unless requested, including:
- render graph systems
- overly abstract material systems
- ECS-driven rendering rewrites
- Vulkan migrations
- complex visibility systems beyond current project needs

## Gameplay/world guidance
Near-term gameplay/world work should favor:
- reliable collision
- usable interaction
- simple doors/buttons/triggers
- stable player controller behavior
- level iteration speed
- easy-to-debug authoring through TOML/blockout data

When implementing interactions:
- prefer explicit state machines
- keep trigger conditions inspectable
- avoid hidden magic behavior
- expose useful diagnostics for overlay/logging

## Build/test/validation
Use the project’s existing build flow and presets.
Prefer small checkpoints and keep the repo in a runnable state.

After code changes, use the most relevant checks available, such as:
- configure/build with the existing CMake preset
- run relevant doctest targets if affected
- verify the app still launches if startup/runtime code changed
- verify no obvious regressions in overlay/logging if diagnostics were touched

If you cannot run a check, say so explicitly and explain why.

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

## Dependency policy
Do not add new dependencies unless:
- the benefit is concrete
- the integration cost is small
- the dependency fits the project scope
- the user asked for it or it is clearly justified

If suggesting a dependency, explain:
- why it is needed
- what problem it solves
- why standard library / current stack is insufficient

## Editing constraints
Do not rewrite unrelated files.
Do not rename public-facing modules or move large subsystems without strong reason.
Do not silently change asset formats, build presets, or project structure without calling it out.

Prefer minimal diffs.

## Communication style
Be concise, technical, and practical.
Explain engine/programming reasoning clearly.
For substantial tasks, include:
- current cause/problem
- chosen approach
- minimal patch summary
- validation performed
- next recommended step

Do not pad the response with generic advice.

## Good task patterns
Good examples:
- “Analyze the door interaction path, find why close/open toggling fails, propose a minimal fix, then validate the build.”
- “Trace player collision against blockout geometry, explain the current algorithm, identify failure cases, and patch only the smallest necessary parts.”
- “Inspect rendering of blockout chunks, explain visibility/culling flow, and suggest the next incremental step toward room-scale indoor FPS content.”

## Default mindset
This repository is primarily:
- a practical engine-learning codebase
- a retro-FPS foundation
- a small systems project that should remain understandable

Optimize for a codebase that a solo developer can keep shipping.
