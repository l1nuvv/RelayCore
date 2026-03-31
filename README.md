# RelayCore

Small C++20 retro-FPS / engine-programming starter built around SDL3, OpenGL and a clean incremental architecture.

## Current milestone

- SDL3 window + OpenGL 4.6 core context
- `vendored glad2` loader
- structured engine/launcher split
- `spdlog` logging
- OpenGL debug output
- shader loading from `assets/`
- rotating 3D color cube with depth testing
- perspective camera + MVP matrix path
- Dear ImGui diagnostics overlay
- TOML engine config (`assets/config/engine.toml`)
- miniaudio-backed audio system foundation
- doctest-based unit tests

## Build requirements

- Windows 11
- CLion or another CMake-capable IDE
- MSVC toolchain
- vcpkg manifest mode dependencies (`sdl3`, `glm`, `spdlog`, `imgui`, `doctest`, `miniaudio`, `tomlplusplus`)

## Build notes

This project uses a vendored generated `glad2` loader under `extern/glad/` and no longer needs network access during configure/build.

Recommended presets:
- `msvc-debug` for day-to-day iteration
- `msvc-relwithdebinfo` for profiling (`PCH + Unity Build + Tracy`)
- `msvc-release` for fully optimized builds
- `msvc-asan` for memory bug hunting

## Architecture

- `src/app` - application loop, frame timing and camera controller
- `src/audio` - miniaudio audio system foundation
- `src/core` - logging, paths, engine config, SDL error helpers
- `src/debug` - Dear ImGui overlay
- `src/platform` - SDL system init and window/context lifetime
- `src/render` - GL loading, debug output, shaders, camera, mesh, renderer
- `assets` - runtime shader files, config, and later textures/sounds

## Controls
- Mouse: look around
- W/A/S/D: move
- Q/E: move down/up
- Shift: sprint
- F1: toggle relative mouse capture
- Esc: quit

## Config

Runtime config lives in `assets/config/engine.toml` and currently controls:
- window title/size/vsync/high-DPI
- clear color
- camera spawn position/target/FOV/clip planes
- ImGui enable/demo window
- fly camera speed/sensitivity
- audio enable/master volume/startup sound path

## CI

В проекте добавлен GitHub Actions workflow `.github/workflows/ci.yml`:
- `format-check` — проверка `.clang-format`;
- `msvc-debug` и `msvc-relwithdebinfo` — configure/build/ctest;
- `clang-tidy` — статический анализ на compile database;
- `msvc-analyze` — native MSVC `/analyze`;
- `msvc-asan` — AddressSanitizer на ручном запуске и на `main/master`;
- `RelWithDebInfo` build directory публикуется как артефакт.

Для CI используется `lukka/get-cmake` для установки CMake 4.x и `lukka/run-vcpkg` для инициализации vcpkg и кэширования его бинарных артефактов.

## Profiling

См. `docs/profiling.md`.
