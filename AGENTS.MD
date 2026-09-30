# RelayCore

## Project contract

- RelayCore is a custom C++20 engine and GoldSource-inspired indoor FPS product, not a historical recreation or a general-purpose framework. Keep it understandable, debuggable, profileable, and shippable under one-developer direction even when implementation is agent-assisted.
- Optimize for a codebase one developer can understand, debug, profile, and keep shipping.
- Prefer a small working vertical slice over framework-building, speculative generality, or architecture for hypothetical future features.
- Primary environment: Windows 11, Visual Studio/MSVC, CMake presets, Ninja, vcpkg manifest mode, CLion. Current runtime uses SDL3/OpenGL 4.6; direct Vulkan is the approved target backend and OpenGL is the temporary migration reference. Tests use doctest; performance work uses Tracy.

## Design and engineering knowledge

- Apply relevant game design, systems design, level design, engine architecture, mathematics, graphics, collision detection, and tools UX knowledge to concrete project decisions. Use the references below as a foundation, not an exhaustive list or a mandatory reading sequence for every task.
- Draw on other relevant books, primary research, official documentation, production experience, and project evidence when they answer a concrete question. Do not assume a technique fits RelayCore merely because a respected source describes it.
- Game design references: Jesse Schell, *The Art of Game Design*; Tynan Sylvester, *Designing Games*; Richard Lemarchand, *A Playful Production Process*; Steve Swink, *Game Feel*; Katie Salen and Eric Zimmerman, *Rules of Play*.
- Systems design references: Michael Sellers, *Advanced Game Design: A Systems Approach*; Ernest Adams and Joris Dormans, *Game Mechanics: Advanced Game Design*; Christopher Barney, *Pattern Language for Game Design*; Geoffrey Engelstein and Isaac Shalev, *Building Blocks of Tabletop Game Design*.
- Level design references: Benjamin Bauer, *A Practical Guide to Level Design*; Christopher W. Totten, *An Architectural Approach to Level Design*; Scott Rogers, *Level Up! The Guide to Great Video Game Design*.
- Engine and tools references: Jason Gregory, *Game Engine Architecture* (foundations/core systems and graphics/motion/sound); Robert Nystrom, *Game Programming Patterns*; David Lightbown, *Designing the User Experience of Game Development Tools*.
- Mathematics, graphics, and collision references: Eric Lengyel, *Foundations of Game Engine Development*, Volumes 1 and 2; Steve Marschner and Peter Shirley, *Fundamentals of Computer Graphics*; Tomas Akenine-Moller et al., *Real-Time Rendering*; Matt Pharr, Wenzel Jakob, and Greg Humphreys, *Physically Based Rendering*; Christer Ericson, *Real-Time Collision Detection*.
- Start gameplay design from the intended player experience and observable player decisions. Evaluate mechanics together with resources, feedback loops, enemy behavior, and level layout; verify their interactions through a working slice and focused playtests.
- Treat control response, acceleration, camera motion, action timing, and audiovisual feedback as part of gameplay behavior. Physical correctness alone does not establish good game feel.
- Establish routes, sightlines, cover, landmarks, encounter pacing, and learning opportunities during blockout. Validate spatial readability and combat behavior before adding visual detail.
- Use explicit ownership, update order, coordinate conventions, units, and numerical assumptions in technical work. Choose architecture and patterns to solve demonstrated problems at the project's current scale.
- When a principle materially affects a decision, briefly explain the concrete benefit and tradeoff. Validate with the appropriate code checks, runtime observation, playtests, or measurements. Do not invent quotations, page references, or edition-specific claims; verify exact attribution when needed.

## Gameplay types and interfaces

- Shared operations do not automatically require inheritance or a virtual interface. Start with a concrete implementation; represent weapon or enemy variants that differ only in parameters through typed data using existing project conventions.
- Separate behavior when actual variants require different logic. Introduce a common interface when callers need to use those implementations through a meaningful shared contract. For a small closed set of variants, an enum and direct dispatch may be simpler; choose based on current call sites and ownership.
- Keep contracts specific to capabilities and responsibilities. Weapon firing and reloading belong to weapon behavior; enemy decision-making belongs to enemy behavior. Do not force every enemy or gameplay object to implement unrelated methods, or create a separate interface for each individual weapon or enemy.
- Add abstractions incrementally when they remove real duplication, encode an invariant, or clarify ownership. Do not build a hierarchy or extensibility framework for hypothetical future content.

## Evidence and scope

- For work on gameplay development, project direction, or the next milestone, read `docs/PLAYABLE_SLICE_ROADMAP.md`. It is the active development queue; historical RC/RF/status documents are evidence to recheck, not an automatic task list. Follow the user's requested scope, and update the roadmap task status and verification evidence when carrying out its tickets. The roadmap alone does not authorize implementation.
- For engine/world architecture, asset pipeline, rendering/physics world capabilities, or Source/Source 2 compatibility work, read `docs/ENGINE_ARCHITECTURE_REFERENCE.md`. It is the long-term architecture/capability reference, not an automatic task queue.
- For Vulkan/backend migration work, also read `docs/VULKAN_MIGRATION_PLAN.md` and operate only within the requested phase/gate. Do not mix parity migration with new renderer architecture unless the phase explicitly requires it.
- For dependency selection, replacement, addition/removal, version/baseline changes, or third-party subsystem foundations, read `docs/DEPENDENCY_STACK.md`. It records approved responsibilities and staged integration; it is not permission to add future dependencies before their owning milestone.

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
- Do not add a new dependency, ECS, render graph, editor framework, scripting runtime, networking layer, job system, or generalized abstraction unless the task demonstrates a concrete need. Dependencies assigned a responsibility by `docs/DEPENDENCY_STACK.md`, `docs/ENGINE_ARCHITECTURE_REFERENCE.md`, or `docs/VULKAN_MIGRATION_PLAN.md` are approved only when their owning milestone begins; do not add them early or replace a working foundation merely to make the stack look more modern.
- Keep third-party implementation types behind the subsystem/tool that owns them. Do not let `JPH::*`, Recast/Detour types, fastgltf types, Vulkan handles, or importer-library types become generic gameplay/world/save/asset-schema contracts.
- If a required vcpkg port is materially stale, prefer a repository-local overlay port pinned to an exact release/commit. Do not introduce unpinned `FetchContent` or global package state as a shortcut.
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
