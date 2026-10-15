# RelayCore — Combat Hit Feedback: Codex Implementation Brief

> **Source-of-truth requirement (non-negotiable):** The live `dev` branch of the actual RelayCore GitHub repository, identified from the current checkout’s `origin` remote and verified before any work, is the ONLY authoritative source for current implementation details. Do not use `main`, local stale checkouts, historical zip archives, previous audits, or this brief as evidence of present code. First inspect `git remote -v` and confirm that `origin` points to RelayCore; then run `git fetch origin dev`, inspect `origin/dev`, and record the exact HEAD SHA. If no verified checkout is available, ask for the repository URL instead of guessing. If the repository or `dev` cannot be accessed, explicitly report the access failure and **do not implement or claim to have audited current code**. Do not switch to another repository or branch as a substitute. For edits, create a working branch/worktree from the verified `origin/dev`, preserving `dev` unless instructed to push.

**Project:** RelayCore (C++20 indoor FPS, Vulkan renderer)  
**Target:** Current `dev` branch, not a historical archive  
**Priority:** After the current H-008 performance baseline and investigation, unless the repo's current plan explicitly changes the order  
**Scope:** Improve the perceptual and gameplay response to confirmed weapon impacts without introducing a new combat framework.

## 1. Mission for Codex

Inspect the **current repository state** first, then implement the smallest coherent set of missing or inadequate combat hit-feedback behaviors. Do **not** assume the effects below are absent. Reuse existing damage, hit detection, weapon, material, particles/VFX, audio, camera, AI, animation, configuration, and debug facilities wherever practical.

The intended result is that shooting an enemy or a surface gives a clear, immediate, controlled response while preserving accuracy, frame pacing, gameplay determinism, and performance.

## 2. Why this work matters

Earlier manual feedback on RelayCore identified shooting as relatively dull and enemies as insufficiently compelling. Increasing enemy health or weapon damage alone does not address the underlying perception of hits. A responsive FPS communicates impact through several synchronized channels:

- **Visual confirmation:** a brief enemy-material flash and context-appropriate impact particles.
- **Auditory confirmation:** timely, varied impact sounds that differ by hit target/surface.
- **Physical/gameplay confirmation:** directional knockback or stagger when justified by weapon and enemy characteristics.
- **Player-side confirmation:** restrained camera/weapon impulse without compromising aiming comfort.

These signals should be driven by the **actual resolved hit**, not a visual guess or duplicate independently calculated hit. The work supports the existing G-019 chapter playtest and provides a concrete basis for combat tuning. It is not a request to emulate a specific game's entire animation or combat system.

## 3. Repository reconnaissance — mandatory before edits

1. Read `AGENTS.md` and the current relevant architecture, development plans, renderer, performance, and game design documents; check recent commits and open work items.
2. Establish the current branch/head and working-tree status. Do not overwrite user changes.
3. Trace the complete hit path: firing -> hit detection -> damage resolution -> AI reaction -> rendering/audio feedback. Identify ownership, threading, fixed-simulation boundaries, and event-dispatch conventions.
4. Inventory existing hit flash, particle/decal, material hit response, impact audio, recoil/camera response, hit reactions, staggering, knockback, and debug tuning tools.
5. Summarize **already implemented / partially implemented / missing / uncertain**, with file and symbol references. Do not build replacement systems for working implementations.
6. Review H-008 baseline/profiling status before introducing additional render work. If performance evidence is unavailable, record that limitation; avoid invented performance improvements.

## 4. Implementation direction (adapt to what the code actually supports)

### A. Single authoritative resolved-hit input

Reuse an existing combat event/result structure if available. If there is no suitable mechanism, add the minimum local data flow needed to provide **target/entity, impact point, impact normal/direction, surface or target category, weapon/attack context, and resolved damage or impact strength** to consumers. Avoid redundant raycasts and avoid introducing a general-purpose event bus solely for this feature.

Keep gameplay decisions in the fixed-step simulation and visual/audio presentation in their existing subsystems. Define event lifetime, ownership, and deduplication so a hit creates feedback exactly once. Respect any multithreading constraints already present.

### B. Enemy hit flash

Implement or refine a short, tunable flash triggered by a confirmed enemy hit. Prefer an existing material/shader parameter or equivalent compatible with the renderer over texture replacement, new material copies per hit, or per-frame allocations. Handle overlapping hits and enemy death/despawn safely. Preserve material/PBR appearance outside the flash interval. Do not assume an exact Vulkan shader layout until inspected.

**Initial tuning candidate:** ~50–90 ms, starting at 70 ms. This is a trial value, not an established design target.

### C. Impact VFX and audio

Use the authoritative contact point/normal and target classification for effects (for example, metal sparks, concrete dust, organic impact). Prefer existing effects and pooled resources. Give sounds controlled variation and appropriate attenuation; prevent excessive simultaneous instances during automatic fire or pellet weapons. Avoid hard-coding effects to every weapon independently if existing weapon/impact data already supports configuration. Avoid GPU stalls, runtime shader compilation, and uncontrolled particle counts.

**Initial tuning candidate:** ~5–12 particles for small impacts, subject to the current particle system and profiling.

### D. Knockback and stagger

Reuse current movement/AI/animation hit-reaction paths. Directional knockback should be consistent with resolved impact direction and game units; tune by weapon strength, enemy mass/role, and reaction cooldowns. Do not permanently interrupt AI or permit trivial repeated stagger-lock. Heavy and light enemy roles should remain distinct. If enemies lack the requisite animation/physics support, implement a minimal compatible response and clearly document remaining asset needs instead of building a full animation framework.

**Initial tuning candidate:** ~100–180 ms stagger for eligible heavier impacts; not every bullet should interrupt an enemy.

### E. Camera and weapon impulse

Use existing recoil/camera facilities where possible. Make the effect brief, additive, tunable, and independent of mouse look. Preserve aim readability and comfort; avoid unbounded shake stacking in automatic fire. Distinguish recoil caused by **firing** from feedback caused by a **confirmed impact**, if both are used.

**Initial tuning candidate:** ~0.15–0.35 degrees for a small impulse, to be evaluated in playtests.

### F. Hitstop — explicitly non-default

**Do not freeze the entire game's fixed simulation.** Global hitstop risks inconsistent AI, physics, timers, networking assumptions (if any), and frame pacing. A tiny presentation-only pause of a particular reaction animation (~20–40 ms) may be evaluated only if the current animation model supports it cleanly. It is optional and should be omitted rather than introducing time-scale architecture for this work.

## 5. Boundaries and non-goals

- Maintain compatibility with the renderer's **actual** Vulkan capability/fallback structure as discovered in the repository; do not assume it is still Vulkan 1.3-only. Avoid feature requirements that break supported Vulkan 1.3 devices.
- Respect existing RmlUi game UI and ImGui developer tooling boundaries; no gameplay UI migration is part of this task.
- Do not undertake unrelated renderer modernization, physics replacement, ECS redesign, new Manager/Service hierarchies, new frameworks, or a sweeping refactor.
- Do not change balance globally merely to make effects more visible; keep damage/health changes separate and justified.
- Do not add external assets or dependencies without checking licenses, packaging, and repo conventions. Reuse or create simple native assets as appropriate.
- Do not silently touch unrelated files or commit/push unless explicitly authorized by the active agent workflow.

## 6. Execution plan

**Phase 0 — baseline and gap report.** Inspect relevant code and docs; establish the actual missing functionality; review H-008 status and baseline data. Produce a concise implementation plan referencing concrete files and functions.

**Phase 1 — impact confirmation.** Integrate missing flash, impact VFX, and audio into existing hit flow. Add tunables using the project's established data/config patterns.

**Phase 2 — gameplay response.** Add or refine restrained knockback/stagger by enemy role and weapon category. Preserve AI state transitions.

**Phase 3 — player-side feel.** Refine camera/weapon impulses where necessary. Do not add global hitstop.

**Phase 4 — verify and tune.** Validate the result on the current G-019 playable chapter or an equivalent existing deterministic/test map, including repeated hits, automatic fire, enemies dying during an effect, misses, surface impacts, save/load or restart, and low-frame-rate behavior.

Adapt or skip phases already satisfactorily implemented. Small incremental commits are preferable where workflow permits, but do not create commits unless authorized.

## 7. Definition of done

- Confirmed hits produce appropriate synchronized feedback; misses do not generate enemy-hit effects.
- Surface/target classification selects sensible effects without duplicating combat resolution.
- Repeated/overlapping hits do not leak resources, explode particle/audio counts, or trap AI in stagger.
- Enemy death/despawn and level restart leave no dangling references or orphaned effects.
- Fixed simulation remains stable, and no global hitstop or render-thread simulation mutation is introduced.
- Supported Vulkan paths and existing UI/tooling remain functional.
- Existing automated tests pass; add focused regression coverage where test architecture allows. Record exact commands and results.
- Compare CPU/GPU time, allocations, and frame pacing against available H-008 baseline when reproducible; otherwise explicitly state that performance has **not** been verified.
- A direct playtest demonstrates improved hit readability, without excessive visual noise or camera disturbance. If manual playtest was not run, label it **not tested**, not passed.
- Update only relevant documentation/config examples and identify remaining art/audio asset or animation requirements.

## 8. Required final Codex report

Return, in this order:

1. **Current-state findings** — evidence-backed inventory with paths/symbols and branch/head.
2. **Changes implemented** — file-by-file explanations and why each edit was necessary.
3. **Configuration/tuning** — parameter names, defaults, and how to change them.
4. **Verification** — exact build/test/profile/playtest commands, environment, results, and untested cases. Distinguish static inspection from executed checks.
5. **Performance** — measured before/after only if evidence exists; otherwise identify the profiling gap.
6. **Remaining risks / deferred work** — prioritised and specific.

Do not claim success because code compiles; successful combat feedback requires a validated runtime scenario. If the task conflicts with current higher-priority H-008 work, finish the audit/gap report and clearly flag the implementation as deferred rather than bypassing the performance baseline.

## Mandatory final report

- GitHub repository, verified source branch `dev`, exact inspected HEAD SHA, and whether the working branch was based on that SHA.
- Existing systems and specific source paths confirmed in `dev`; separate present features from genuine gaps and optional polish.
- Changed files and the reason for each change; describe integration at actual call sites, not invented architectural diagrams.
- Build/test commands executed and their observed results, plus any untested cases.
- Performance evidence for incremental effects, if benchmark data exists; otherwise explicitly mark measurements as pending.
- Gameplay verification steps for G-019 (only if that chapter is present in the verified source).

**Stop condition:** If the actual RelayCore repository or its `dev` branch cannot be identified and accessed, report the specific remote/ref check that failed and stop implementation. Never substitute a guessed repository, `main`, or historical archives.
