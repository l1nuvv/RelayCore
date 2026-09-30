---
name: relaycore-verify
description: Choose and run the narrowest correct RelayCore build/test/validation sequence after edits or while diagnosing a build/test failure. Use for CMake preset selection, focused tests, CI-like verification, and validation evidence; do not run expensive full workflows by default.
---

# RelayCore verification

Verification is a routing problem: use the cheapest check that can falsify the change, then broaden only when needed.

Read [references/check-matrix.md](references/check-matrix.md) only when selecting commands beyond the obvious targeted build/test.

## Rules

1. Inspect current `CMakePresets.json`, relevant `CMakeLists.txt`, and existing scripts before choosing command names.
2. Never invent a preset, target, or test regex.
3. Run `git diff --check` after edits.
4. Prefer one affected target/test group before a full suite.
5. Reconfigure only when CMake/configuration/dependencies require it.
6. Do not delete build trees or caches merely because a build failed.
7. For compiler failures, fix the first causal error rather than reacting to downstream noise.
8. For test failures, reproduce the smallest failing test before changing code.
9. Full debug/RelWithDebInfo/ASan or CI-like workflows are for cross-cutting changes, release confidence, or explicit requests—not every patch.
10. Never report a check as passed unless its command actually completed successfully in the current work.

## Handoff

State the exact commands run, their result, and what remains unverified. Summarize failures; do not paste large build logs unless requested.
