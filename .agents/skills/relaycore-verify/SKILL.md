---
name: relaycore-verify
description: Select and run RelayCore CMake/CTest, content, tooling or GPU checks after changes or build/test failures. Use when verification scope, presets, staging or Skipped results need interpretation; not for read-only prose review or automatic release/packaging.
---

# RelayCore verification

Read [references/check-matrix.md](references/check-matrix.md) for scope selection. Repository paths there are relative to the root. Current `CMakePresets.json`, affected CMake targets and test registrations own command names; the matrix does not replace them.

For MSVC builds confirm the developer environment and `VCPKG_ROOT`. Reuse the configured tree; configure only for missing/changed configuration. Discover the selected tests with CTest `-N` before expensive work when the filter is uncertain, then use `--no-tests=error` when executing a selection. Do not interpret an empty selection as verification.

Build the target that provides the selected tests and their current dependencies. CTest filters select registered test names, while doctest filters select cases inside a binary. Do not assume a build preset builds every test selected by the similarly named test preset.

Graphics exit code 77 is `Skipped`, even when CTest exits zero. A graphics gate requires executed cases without failures or skips and clean expected validation output; inspect the summary or JUnit report. Compiling graphics targets and passing the loader/layer probe do not prove device/presentation compatibility. Diagnose graphics failures with `relaycore-vulkan-investigate` when needed.

Stop after the affected checks pass; broaden for changed shared contracts, unresolved failures or explicitly requested full validation. Do not manufacture a full-CI, bake or package requirement. Run `git diff --check` and report exact commands, executed/skipped/failed results and remaining blockers. Old build/capture evidence does not validate a changed working tree.
