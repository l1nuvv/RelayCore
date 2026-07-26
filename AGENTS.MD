# RelayCore

## Project contract

- RelayCore is a small C++20 learning project: a GoldSource-inspired indoor
  FPS vertical slice, not a general-purpose engine or a historical recreation.
- Keep the codebase understandable for one developer. Prefer a small working
  vertical slice over new framework-level systems.
- Environment: Windows 11, Visual Studio 2022/MSVC, CMake 4.0+, Ninja, vcpkg
  manifest mode, CLion.
- Run `msvc-*` presets only from Developer PowerShell or Developer Command
  Prompt for VS 2022. `VCPKG_ROOT` must point to the vcpkg installation.

## Repository map

- `src/relaycore/` вЂ” engine runtime and subsystems.
- `src/relaygame/` вЂ” demo/game glue.
- `assets/` вЂ” shaders, audio, textures, and authored content.
- `tests/` вЂ” doctest unit and content-validation tests.
- `cmake/`, `CMakeLists.txt`, `CMakePresets.json` вЂ” build configuration.
- `vcpkg.json` вЂ” dependency manifest.

## Scope and investigation

- Start with files named by the task and their nearest tests/call sites.
- Do not scan the full repository, Git history, audit, or external sources
  unless a concrete unanswered question requires it. State that question before
  expanding scope.
- Treat code, presets, test output, and build logs as the source of truth;
  comments and README are secondary evidence.
- For audit work, first confirm the defect in the current source. An
  analysis-only task must not edit files. Keep confirmed facts, hypotheses, and
  recommendations separate.
- Do not create skills or subagents for a one-off task. Do not browse when the
  repository already contains the answer.

## Implementation rules

- Make the smallest patch that solves the requested problem. Do not mix it
  with cleanup, mass formatting, renames, or speculative refactoring.
- Preserve existing module boundaries: renderer consumes prepared data and does
  not own gameplay rules; low-level engine code does not depend on game code.
- Prefer explicit ownership, RAII, direct calls, typed runtime data, clear
  update order, and simple diagnostics over hidden globals or abstractions.
- Do not add dependencies, ECS, render graphs, editor tooling, networking,
  scripting, or a broad framework rewrite without an explicit request.
- Do not silently change public behavior, assets/formats, runtime paths,
  presets, CI, dependency baselines, or tool versions. Explain the impact and
  update the relevant test or documentation when the task requires it.
- Do not hide invalid authored data with clamping or silent fallback. New
  validation rules need evidence from current usage and tests.
- Match the existing C++ style. Keep headers light and comments limited to
  non-obvious intent.

## Build and verification

Run `git diff --check` after every edit. Then choose the narrowest relevant
check; do not run the full workflow for a documentation-only change.

```powershell
# Configure the Debug build
cmake --preset msvc-debug

# Build one target
cmake --build --preset demo-debug
cmake --build --preset tests-debug
cmake --build --preset validation-debug

# Run the matching tests
ctest --preset test-debug --output-on-failure
ctest --preset validation-debug --output-on-failure

# Use for CMake, CI, runtime-integration changes, or when explicitly requested
cmake --workflow --preset debug-discipline-workflow
```

- If a required check cannot run, state the exact command, blocker, and what
  remains unverified. Never claim a build, test, or runtime path works without
  its result.
- Run format checks only when C/C++ formatting is affected and the configured
  `clang-format` target/tool is available.
- Do not delete generated build directories, caches, or user presets unless
  the task explicitly requires it.

## Git and handoff

- Do not use `git add .`; stage explicit paths only.
- Do not commit unless asked. Before a requested commit, show the affected
  files, `git diff --check` result, and checks actually run.
- Keep the final report short: conclusion, changed files, verification, and
  remaining blocker or risk. Do not paste full logs or repeat the task.