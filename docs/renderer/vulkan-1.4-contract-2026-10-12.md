# vulkan-1.4-contract-2026-10-12 — execution evidence

Датированный отчёт. Действующий scope и gates — в [renderer-плане](../RENDERER_MODERNIZATION_PLAN.md); прежние 1.3/forced-profile проверки сохраняются как история, а не текущие требования. Команды, метрики и failures относятся к идентичности конкретного запуска.

## Vulkan 1.4-only contract revision — 2026-10-12

The owner explicitly requested removal of Vulkan 1.3 compatibility to avoid the
second API implementation/test profile during subsequent VK14 work. Require
loader and selected physical-device API ≥1.4, request instance API 1.4 and pass
1.4 to VMA. Remove API negotiation, the forced-1.3 environment override and its
two CTest registrations. Keep existing feature/extension checks and SPIR-V 1.5;
optional rendering capabilities still require their own support checks.

This supersedes the original VK14-01 compatibility contract and all earlier
native/forced-1.3 acceptance gates. Historical two-profile evidence above remains
unchanged. Vulkan 1.3 hardware is outside the supported runtime baseline; no
performance gain is claimed.

**Verification:** Windows/MSVC, RTX 4060 Ti, loader/headers 1.4.357, physical API
1.4.329. `cmake --preset msvc-debug` and `cmake --preset msvc-dev` passed without
dependency version changes. The initial sandbox build failed with the previously
recorded MSVC C1902/PDB IPC restriction; the same targeted build passed outside
the sandbox in the VS x64 developer environment:

```powershell
cmake --build --preset graphics-debug --target relaycore_vulkan_device_tests relaycore_vulkan_presentation_tests relaycore_vulkan_upload_tests relaycore_vulkan_mesh_tests relaycore_player_ui_graphics relaycore_containment_loop_graphics relaycore_game_lifecycle relaycore_vulkan_toolchain_check -j 2
cmake --build --preset demo-dev --target relaycore_demo relaycore_vulkan_device_tests -j 2
```

`build/msvc-debug/relaycore_vulkan_toolchain_check.exe` passed, reporting loader
and VMA support 1.4 plus available validation/debug-utils. Graphics checks ran
on the ordinary desktop outside the sandbox with process-local
`DISABLE_VULKAN_OBS_CAPTURE=1`; installed/global hooks were unchanged:

```powershell
ctest --preset graphics-debug -R '^graphics\.(vulkan_(device|missing_validation|presentation|recreation_failure|upload|mesh|color_baseline)|mesh_pipeline|hdr_pipeline|msaa|containment_color|resource_preparation|player_ui|containment_loop(_hdr)?|game_lifecycle)$' --no-tests=error --output-on-failure --output-junit vk14-only-graphics.xml
ctest --test-dir build/msvc-dev -R '^graphics\.vulkan_device$' --no-tests=error --output-on-failure --output-junit vk14-only-device.xml
```

Debug: **15 passed / 1 failed / 0 skipped**; development/Tracy device: **1 passed /
0 failed / 0 skipped**. Passing cases cover device allocation/timeline/lifetime,
missing-validation rejection, presentation/recreation, uploads, prepared resource
churn, mesh pipeline, RmlUi, G-019 SDR/HDR/exposure, color/HDR/MSAA and lifecycle.
No validation warnings/errors occurred. JUnit reports are in the respective build
directories. The normal `RelayCore — Play` executable is updated.

**Separate baseline failure:** `graphics.vulkan_mesh` fails its existing
`power_restoration-0` image comparison: mask difference **0.00361111**, color
difference **0.306111**, mean RGB delta **5.48441**. A diagnostic executable using
the pre-change negotiation logic from HEAD `889cf3e` reproduces the identical
failure on the same current renderer, shaders, test object and staged assets,
with instance/effective API 1.4. The old source was adapted only to the current
capability ABI (removed the retired override-report field and localized its old
API constants). This isolates the API change; it does not diagnose the original
image discrepancy. Reproduction script, source, compiler/link commands and log
remain in `build/vk14-only/`; diagnostic binary is
`build/msvc-debug/tests/relaycore_vulkan_mesh_before_api.exe`. Baselines/tolerances
were preserved. No unrelated rendering correction is included.

`clang-format --dry-run --Werror` passed for the four changed C++ files;
`git diff --check` passed. Real older-loader/GPU rejection remains unexecuted
because that hardware/loader is unavailable. Optional-feature and other-GPU/human
gates remain independent. No commit, push or package was created.
