# Verification selection

Use current `CMakePresets.json` and affected `CMakeLists.txt` to confirm names and dependencies. `docs/DEVELOPMENT.md` owns setup and diagnostic procedures.

| Change | Start with | Expand when |
| --- | --- | --- |
| Local C++ behavior | Affected target and nearest discovered CTest/doctest cases | Shared API or integration behavior changes |
| Gameplay/map/config | Parser/session/content cases, affected map via `relaycore_level_check` | Continue/snapshot/editor/session transitions change: add relevant lifecycle checks |
| Model/import/staging | Importer/product cases and `relaycore.tooling.model_assets` | Runtime consumption changes: stage and check affected content/render view |
| Native collision | `relaycore_physics_tests` and relevant `relaycore.physics.*` / `relaycore.tooling.collision_assets` | Placement/movement/editor integration changes: add affected content/lifecycle checks |
| Shader/render/device/presentation | Affected target and selected `graphics.*` with validation | Cross-pass/lifetime/color contracts change: expand graphics coverage/capture |
| Hook/release scripts | Corresponding `tests/*_tests.ps1` isolated fixture | Entry points, candidate retry or installation contract changes |
| CMake/dependencies | Configure affected preset, affected target and tests | Inheritance, ABI or deployment changes: relevant optimized/ASan/CI configurations |
| Performance patch | Correctness checks plus comparable workload before/after | Noise or unmeasured scenario prevents a conclusion |
| Prose/skill-only | Link/frontmatter/routing inspection and `git diff --check` | Executable tooling is also changed |

## Non-obvious boundaries

- `tests-debug` and `test-debug` are different preset kinds. `test-debug` includes tooling and isolated physics cases; inspect dependencies before choosing a smaller build target. Tooling needs PowerShell 7 and some fixtures need `clang-format`.
- CTest normally sets staged `RELAYCORE_ASSET_ROOT`. Manual runtime checks need the complete staged root, not source `assets/`. `relaycore_level_check` reports warnings alongside success and does not prove human reachability/readability.
- Model/collision targets and isolated physics fixtures can avoid full runtime irradiance work. Demo/runtime staging can depend on expensive bakes; inspect the target graph before selecting it for a pure format/physics change.
- A full discipline workflow is broader than routine verification but is not necessarily every gate: the current RelWithDebInfo discipline workflow omits content validation; graphics execution is separate from default hosted CI. Inspect workflow steps rather than treating the name as proof.
- `scripts/run-clang-tidy.ps1` analyzes all production `.cpp` files, despite the CI step's word “targeted”. Use for relevant static-analysis work or a requested broader check, not every edit. ASan targets project boundaries; third-party binaries may be uninstrumented.
- Release scripts can change versions/create archives/publish. Their regression fixtures simulate these operations; running a fixture is different from invoking `scripts/release.ps1`. Packaging is outside routine verification.
