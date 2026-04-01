# RelayCore

Small C++20 retro-FPS / engine-programming starter built around SDL3, OpenGL and a clean incremental architecture.

## Current milestone

- SDL3 window + OpenGL 4.6 core context
- vendored glad2 loader
- explicit **engine/game split**
- spdlog logging
- OpenGL debug output
- shader loading from `assets/`
- rotating 3D color cube with depth testing
- perspective camera + MVP matrix path
- Dear ImGui diagnostics overlay
- TOML engine config (`assets/config/engine.toml`)
- miniaudio-backed audio system foundation
- doctest-based unit tests
- game-side update/render hook through `AppDelegate + EngineServices`

## Targets

- `relaycore_engine` — reusable engine library
- `relaycore_demo` — runnable sandbox/demo application (binary name remains `RelayCore`)
- `relaycore_tests` — unit tests

## Architecture

The codebase is split physically and semantically:

- `src/relaycore/` — engine code and reusable runtime systems
- `src/relaygame/` — demo/game entrypoint and game-side delegate
- `cmake/` — shared CMake helpers, options and dependency discovery

Inside `src/relaycore/`:

- `app/` — application runtime, frame loop, delegate seam, bootstrap runner, engine services
- `audio/` — audio system and miniaudio integration
- `core/` — config, logging, paths, profiling helpers
- `debug/` — ImGui debug overlay
- `platform/` — SDL system and window/context lifetime
- `render/` — camera, GL setup, mesh, shader, renderer

Inside `src/relaygame/`:

- `main.cpp` — demo executable entrypoint
- `demo_game.*` — current game-side delegate and demo scene logic
- future gameplay systems like player motor, doors, triggers and level bootstrap

## Boundary rule

The engine owns:

- the application loop
- platform/window/context lifetime
- rendering infrastructure
- audio/config/bootstrap/runtime services

The game owns:

- camera control policy
- scene submission
- gameplay rules
- future entities / player logic / level bootstrap

That means the split is no longer only “library + exe” on paper: the demo now actually drives input/update/render through the delegate seam.

## Presets

Recommended configure presets:

- `msvc-debug`
- `msvc-relwithdebinfo`
- `msvc-release`
- `msvc-asan`

Recommended build presets:

- `demo-debug`
- `demo-relwithdebinfo`
- `tests-debug`

## Notes

This snapshot keeps one clear engine root and one clear game root:

- `src/relaycore`
- `src/relaygame`

That keeps target names, folder names and include paths aligned.
