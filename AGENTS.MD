# RelayCore

## Project contract

- RelayCore is a custom C++20 engine and GoldSource-inspired indoor FPS product, not a historical recreation or a general-purpose framework. Keep it understandable, debuggable, profileable, and shippable under one-developer direction even when implementation is agent-assisted.
- Prefer a small working vertical slice over framework-building, speculative generality, or architecture for hypothetical future features.
- Primary environment: Windows 11, Visual Studio/MSVC, CMake presets, Ninja, vcpkg manifest mode, CLion. Current runtime requires SDL3 and direct Vulkan 1.4; OpenGL was removed at VK-013. Tests use doctest; performance work uses Tracy.

## Evidence and scope

- For gameplay development, project direction, or milestone work, read the introduction, status table and selected ticket in `docs/PLAYABLE_SLICE_ROADMAP.md`. It is the active development queue; historical audits and plans retained in Git are evidence to recheck, not an automatic task list. Follow the user's requested scope, and update the roadmap task status and verification evidence when carrying out its tickets. The roadmap alone does not authorize implementation.
- For changes that materially affect engine/world architecture, asset pipeline contracts, rendering/physics capabilities, or Source/Source 2 compatibility, read the relevant sections of `docs/ENGINE_ARCHITECTURE_REFERENCE.md`. It is the long-term architecture/capability reference, not an automatic task queue.
- For Vulkan/backend migration work, also read `docs/VULKAN_MIGRATION_PLAN.md` and operate only within the requested phase/gate. Do not mix parity migration with new renderer architecture unless the phase explicitly requires it.
- For dependency selection, replacement, addition/removal, version/baseline changes, or third-party subsystem foundations, read `docs/DEPENDENCY_STACK.md`. It records approved responsibilities and staged integration; it is not permission to add future dependencies before their owning milestone.

- For build/runtime setup, graphics diagnostics, profiling or native asset production, read only the relevant section of `docs/DEVELOPMENT.md`. For editor/map transitions use `docs/EDITOR_AND_CAMPAIGN.md`; for native collision products use `docs/COLLISION_ASSETS.md`; for requested packaging/release/update work use `docs/UPDATES.md`.
- For gameplay/system/level design or architecture rationale, consult `docs/DESIGN_REFERENCES.md` as needed. It preserves the project design references and gameplay type guidance without making them mandatory for routine fixes.
- For complex, multi-session work, keep scope, milestones, decisions and verification in the existing owning plan/ticket. Add a task-specific plan only when no existing document fits; do not duplicate roadmap statuses or create plans for routine fixes.

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
- Refactor only when the requested change exposes a concrete structural obstacle. Keep it local and preserve existing behavior; verify it separately where practical.
- Preserve current module boundaries. Engine code must not depend on game-specific code; rendering consumes prepared data and must not own gameplay rules; debug/UI code must not become required runtime architecture.
- Prefer explicit ownership, RAII, typed runtime data, direct calls, clear update order, local state changes, and visible failure modes.
- Do not add a new dependency, ECS, render graph, editor framework, scripting runtime, networking layer, job system, or generalized abstraction unless the task demonstrates a concrete need. Dependencies assigned a responsibility by `docs/DEPENDENCY_STACK.md`, `docs/ENGINE_ARCHITECTURE_REFERENCE.md`, or `docs/VULKAN_MIGRATION_PLAN.md` are approved only when their owning milestone begins; do not add them early or replace a working foundation merely to make the stack look more modern.
- Keep third-party implementation types behind the subsystem/tool that owns them. Do not let `JPH::*`, Recast/Detour types, fastgltf types, Vulkan handles, or importer-library types become generic gameplay/world/save/asset-schema contracts.
- If a required vcpkg port is materially stale, prefer a repository-local overlay port pinned to an exact release/commit. Do not introduce unpinned `FetchContent` or global package state as a shortcut.
- Do not silently change public behavior, authored formats, runtime paths, presets, CI behavior, dependency baselines, or tool versions.
- Invalid authored/config data should fail with actionable diagnostics unless existing behavior explicitly defines a fallback. Do not hide bad data with new clamps or silent defaults.
- Match existing naming and C++ style. Keep headers light. Comments explain non-obvious intent or invariants, not syntax.

## Anti-slop rules

- Do not add abstractions merely to make the patch look designed. A helper/type/layer must remove real duplication, encode an invariant, clarify ownership, or isolate a changing concern.
- DRY applies to authoritative knowledge and invariants, not merely similar code. Keep similar code separate when it represents independently changing behavior.
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

- For visual/art decisions, use `.agents/skills/relaycore-art-direction/SKILL.md` and the authoritative `docs/ART_DIRECTION.md`. For source/product/export/staging work, use `.agents/skills/relaycore-assets/SKILL.md`. Match nearby authored conventions; proposed scenes in reference documents do not authorize content or pipeline work.
- For Blender/Python modeling, use `.agents/skills/relaycore-blender/SKILL.md` and its installed specialist skills. Review references, proportions, construction, materials and multiple rendered views; a primitive blockout is an intermediate stage unless explicitly requested as the deliverable. Project art direction and native importer constraints take precedence over generic Blender workflows.
- For external textures, materials, HDRIs and models, follow `docs/ASSET_SOURCES.md` for source sites, task-specific search routes and adaptation checks. The owner intends a private project; do not turn licensing audits or public-release preparation into routine asset-work gates.

## Build and verification

- Routine verification ends with the relevant build/tests. Create or recreate a ZIP/standalone package only when the user directly asks for packaging or a release; do not do it automatically because an older plan or ticket mentions delivery. Reuse existing artifacts when sufficient and avoid spending time, compute, or tokens on unnecessary archives.
- Use only presets/targets that exist in the current `CMakePresets.json` and CMake files. Never invent a command when the repository can provide the exact one.
- Use `.agents/skills/relaycore-verify/SKILL.md` when selecting nontrivial verification scope or diagnosing build/test failures. Run `git diff --check` after edits.
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
