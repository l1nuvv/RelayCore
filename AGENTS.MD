# RelayCore

## Project contract

- RelayCore is a small C++20 engine-learning project: a GoldSource-inspired indoor FPS vertical slice, not a general-purpose engine and not a historical recreation.
- Optimize for a codebase one developer can understand, debug, profile, and keep shipping.
- Prefer a small working vertical slice over framework-building, speculative generality, or architecture for hypothetical future features.
- Primary environment: Windows 11, Visual Studio/MSVC, CMake presets, Ninja, vcpkg manifest mode, CLion. Runtime stack includes SDL3/OpenGL; tests use doctest; performance work uses Tracy.

## Evidence and scope

- For work on gameplay development, project direction, or the next milestone, read `docs/PLAYABLE_SLICE_ROADMAP.md`. It is the active development queue; historical RC/RF/status documents are evidence to recheck, not an automatic task list. Follow the user's requested scope, and update the roadmap task status and verification evidence when carrying out its tickets. The roadmap alone does not authorize implementation.

- Start with the files named by the task, their nearest tests, and direct call sites.
- Expand scope only to answer a concrete unresolved question. Do not scan the whole repository, Git history, docs, or web by default.
- Treat current source, CMake presets, tests, runtime output, and profiler captures as primary evidence. README/comments are secondary and may be stale.
- Reuse evidence already gathered in the current task. Do not reread unchanged large files or rerun an expensive command without a reason.
- Prefer targeted search (`rg`, specific paths, specific symbols) over broad dumps. Summarize large logs; quote only the lines needed to establish the cause.
- Separate confirmed facts, likely risks, hypotheses, and taste. Do not convert a suspicion into a patch without evidence.
- Analysis/review-only requests are read-only unless the user explicitly asks for changes.

## Implementation

- Make the smallest patch that fully solves the requested problem.
- Do not mix feature/fix work with unrelated cleanup, renames, reformatting, dependency changes, or speculative refactors.
- Preserve current module boundaries. Engine code must not depend on game-specific code; rendering consumes prepared data and must not own gameplay rules; debug/UI code must not become required runtime architecture.
- Prefer explicit ownership, RAII, typed runtime data, direct calls, clear update order, local state changes, and visible failure modes.
- Do not add a new dependency, ECS, render graph, editor framework, scripting runtime, networking layer, job system, or generalized abstraction unless the task demonstrates a concrete need.
- Do not silently change public behavior, authored formats, runtime paths, presets, CI behavior, dependency baselines, or tool versions.
- Invalid authored/config data should fail with actionable diagnostics unless existing behavior explicitly defines a fallback. Do not hide bad data with new clamps or silent defaults.
- Match existing naming and C++ style. Keep headers light. Comments explain non-obvious intent or invariants, not syntax.

## Anti-slop rules

- Do not add abstractions merely to make the patch look designed. A helper/type/layer must remove real duplication, encode an invariant, clarify ownership, or isolate a changing concern.
- Avoid vague buckets such as `Utils`, `Helpers`, `Manager`, `Service`, `Context`, or generic factories unless that role already exists and is the correct owner.
- Do not create configuration knobs, extension points, callbacks, interfaces, templates, or indirection for a single known use case without evidence they reduce current complexity.
- Do not add comments, docblocks, TODOs, markdown, logging, tests, or error handling just to increase apparent completeness.
- Do not write comments that paraphrase the next line of code. Do not add decorative section comments to small files.
- Do not catch exceptions only to log and rethrow, duplicate validation already owned elsewhere, or add defensive fallback paths that make invalid state harder to diagnose.
- Tests should protect behavior, invariants, regressions, parsing/validation, math, or deterministic transforms. Avoid tests that only mirror implementation structure or trivial getters/setters.
- Prefer existing project idioms over generic “best-practice” rewrites. Generated code should look like it belongs in RelayCore, not like a tutorial or framework sample.

## Performance

- Measure before optimizing. Use Tracy or reproducible timing evidence for hot-path claims.
- Fix algorithm/data-shape problems before micro-optimizing syntax.
- Avoid per-frame allocation, repeated parsing, avoidable string work, and unnecessary pointer chasing in demonstrated hot paths.
- Do not claim a speedup without measurements. If only complexity or allocation behavior improved, describe that instead.

## Authored content and assets

- Preserve the project direction: restrained retro-industrial sci-fi, technical/liminal indoor spaces, strong spatial readability, functional materials, and atmosphere over visual noise.
- Match nearby assets and authored data before inventing a new visual or schema convention.
- Avoid generic AI sci-fi slop: random neon, excessive greebles, meaningless fake labels, uniform edge wear, over-grunge, over-glossy surfaces, unnecessary micro-detail, and decoration that weakens gameplay readability.
- Do not bake lighting, shadows, AO, text, or perspective into an asset unless the current pipeline or task explicitly requires it.
- For generated visual assets, require a clear gameplay purpose, intended scale/use, tiling/alpha requirements when relevant, and an in-engine or pipeline-level validation step when available.

## Build and verification

- Use only presets/targets that exist in the current `CMakePresets.json` and CMake files. Never invent a command when the repository can provide the exact one.
- Run `git diff --check` after edits.
- Start with the narrowest relevant build/test. Run broader workflows only when the change crosses build/config/runtime boundaries or the user requests full validation.
- Known baseline flow is preset-based, for example `cmake --preset msvc-debug`, targeted `cmake --build --preset ...`, and matching `ctest --preset ... --output-on-failure`; verify current preset names before use.
- MSVC presets require a Visual Studio developer environment and valid `VCPKG_ROOT`.
- Do not delete build directories, caches, user presets, or generated data as a first troubleshooting step.
- If a check cannot run, report the exact command, blocker, and what remains unverified. Never claim success from code inspection alone.

## Git and handoff

- Inspect the diff before finishing. Keep it scoped and reviewable.
- Do not use `git add .`; stage explicit paths only when staging is requested.
- Do not commit, push, rewrite history, or modify unrelated working-tree changes unless asked.
- Final handoff should be compact: result, changed paths, checks actually run, and any remaining blocker/risk. Do not paste full logs or repeat the prompt.
