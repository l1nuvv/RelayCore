# Verification routing

Always confirm names against the current repository before running them. This file describes selection logic, not immutable preset names.

| Change type | Start with | Broaden when |
| --- | --- | --- |
| One C++ implementation/header | affected target + nearest doctest filter/group | shared API, multiple subsystems, or failure suggests wider impact |
| Gameplay/runtime behavior | tests covering that behavior, then demo build | integration/startup/content path changed |
| Renderer/shader path | renderer/demo target + focused tests/validation | GPU state, startup, framebuffer, asset loading, or cross-pass behavior changed |
| Config/TOML/content validation | parser/validation tests + validation target | defaults, shipped content, startup, or runtime reload behavior changed |
| CMake/presets/vcpkg | configure + affected target | preset inheritance, dependency, CI, or packaging behavior changed |
| Test infrastructure | smallest affected test binary/group | shared fixture/logger/temp-path behavior changed |
| Documentation/comments only | `git diff --check` | generated docs or tooling is itself part of the change |
| Performance optimization | correctness tests + same workload/capture before/after | result is noisy or hot path spans multiple systems |

Known historical RelayCore presets have included `msvc-debug`, `demo-debug`, `tests-debug`, `validation-debug`, `test-debug`, `debug-discipline-workflow`, RelWithDebInfo, and ASan variants. Treat these as hints only; current repository files are authoritative.
