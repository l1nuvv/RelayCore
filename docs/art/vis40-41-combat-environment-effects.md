# VIS-40–41 — combat and equipment effects

2026-10-09, direct request for VIS-40 and VIS-41. Both tickets are technically
implemented and remain `awaiting_playtest` for the owner's assessment in combat
and continuous movement. This pass uses the existing meshes, render passes,
query surfaces and environment targets.

![Before and after at matching cameras](vis40-41-effects-before-after.png)

## VIS-40

- The pistol's 45 mm travelling block becomes a fading 9 mm streak, capped at
  45 cm and visible for 65 ms. Hostile streaks are 11 mm / 55 cm / 80 ms;
  pellets are 4 mm / 16 cm / 55 ms. These are visual limits; hitscan, ranges,
  damage, cadence and the existing transient timers are unchanged.
- Pistol and shotgun flashes use the same recoil transforms and actual bore
  coordinates as their native viewmodels, for 45/55 ms. The turret receives a
  short barrel flash. Effects use the existing emissive material path and depth
  testing, with no new dynamic-light or particle system.
- Traces retain an optional current-query hit. Metal contacts produce small
  orange sparks; hard surfaces produce shorter pale flecks along the hit normal.
  Turret and rover contacts use their existing hit bounds. A range miss has no
  impact, and a subsequent miss clears the preceding shot's impact. The surface
  classification is shared with existing impact audio.
- Turret hit and destruction timers drive compact ballistic bursts; destruction
  no longer submits three full hostile shot traces. Transient impact data is not
  added to disk saves.

## VIS-41

- Ten soft puffs begin 12 mm from the source, move along its authored direction,
  rise and spread within a smaller plume. Their original exponential shutdown
  remains intact. Steam stays in the existing sorted transparent pass.
- Pressure steam now begins at a receiver isolation flange; cooling steam begins
  at the exchanger's supply union. Power's pump union and spill outlet remain the
  existing sources. Stop targets are preserved.
- Electrical bursts use small three-dimensional streaks instead of flat strips,
  preserving the 3.2 s rhythm. Two power sources move out of their panel housings;
  restoration/reconnection still stop them immediately. The sandbox source remains
  the existing blockout equipment fixture.
- All six existing fans receive a fixed open wire guard, mounting tabs and
  fasteners in front of the rotor. Three overlapping ventilation cassettes move
  beside their fans: two in cooling and one in foundation pump. Fan startup,
  rotation and coast-down retain their existing timing and power targets.

![Combat, isolation, rotation and transparent overlap](vis40-41-effects-states.png)

Only decorative effect origin/orientation and three noncolliding vent prop
positions change in maps. The four changed TOMLs retain their solids, gameplay
entities, entity order, mission/retry/save data, targets, lights and collision
products. No models, textures, shaders, dependencies or authored format change.
This does not add particle collision simulation; opaque depth testing and compact
placements handle the inspected surface contacts.

## Verification

The existing MSVC developer environment and `VCPKG_ROOT` were used. Debug has
Vulkan validation enabled. Final builds:

```powershell
cmake --build --preset graphics-debug --target relaycore_power_restoration_graphics relaycore_vulkan_mesh_tests relaycore_tests relaycore_demo relaycore_content_validation -j 2
cmake --build --preset demo-debug --target relaycore_demo relaycore_power_restoration_graphics -j 2
cmake --build --preset demo-relwithdebinfo -j 2
```

32 distinct selected checks pass: 20 unit regressions, eight affected-map content
checks and four graphics gates. The unit/content selection checks blocking doors,
nearest contacts, a hit followed by a miss, pellet behavior, enemy cadence,
persistence, environment timing, routes, repairs and snapshots:

```powershell
ctest --preset test-debug --output-on-failure --no-tests=error -R '^relaycore\.(combat |player fire |a missed shot|shotgun |weapon impacts |environment |lamp fixtures |rover |nonlethal hits)|^validation\.(cooling loop (service|gates|isolation)|pressure transfer (wall equipment|complete route|stops)|power restoration (requires|can finish))'
ctest --preset graphics-debug --output-on-failure --no-tests=error -R '^graphics\.(world_effects(_sdr)?|pistol_viewmodel|shotgun_viewmodel)$'
ctest --preset graphics-debug --output-on-failure --no-tests=error -R '^graphics\.world_effects(_sdr)?$'
```

The last two environment gates were rerun after the final placement/camera fixes.
Graphics JUnit reports have zero failures and skips, and the validation warning/error
failure expression is clean. Pistol/shotgun gates cover HDR/SDR, FOV 50/70/90°,
640×360 and 800×600, reticle clearance, recoil recovery and prepared weapon switching.

Each final effects mode records 141 frames at 1280×720: 13 sources with off,
four phase samples, side, coast and stopped states; 34 combat/contact/miss states;
three glass/steam overlap views. The existing pixel comparison verifies that
side-view steam/glass blend in the expected order. Source visibility and stopping
also have executable assertions. Fixed cameras and source/side contact sheets were
reviewed in HDR and SDR; screenshots do not establish moving-player readability.

Check Map reports `valid=true`, zero warnings for power, pressure, cooling and foundation pump.
The four changed maps' Debug and Play staged hashes match source. A semantic TOML
comparison against the starting Git versions permits only the decorative spatial
changes described above. `git diff --check` passes.

Local logs, JUnit, baseline images and review scripts are in
`build/vis40-41`; final raw captures are in
`build/msvc-debug/tests/power-captures/effects-hdr` and `effects-sdr`.
The initial baseline capture helper quit SDL before renderer destruction and
reported a teardown warning; that helper was fixed before the final gates.
Baseline images are visual evidence, not a passed validation gate.

## Changed owners and remaining acceptance

`combat_runtime.{h,cpp}` retain query contacts; `enemy_runtime.cpp` supplies robot
contacts; `player_feedback.{h,cpp}` owns the shared material classification.
`world_scene_renderer{,_interactives,_environment}.{h,cpp}` owns the visuals.
The four map TOMLs own placement. Existing combat, enemy and weapon tests plus
`power_restoration_graphics.cpp` / `tests/CMakeLists.txt` own regressions/captures.

The owner still needs to judge threat direction, hits versus misses, effect strength
and duration at combat distance, and leak/repair readability while moving. External
playtest/GPU acceptance and measured performance are not claimed. VIS-42 and broader
equipment state presentation remain outside this request.

## Combat hit-feedback audit and implementation plan — 2026-10-14

### Scope and inspected source

This section records the requested audit and plan based on the supplied
`RELAYCORE_COMBAT_HIT_FEEDBACK_CODEX_PROMPT.md`. The brief is proposal input,
not authorization to implement its instructions. No gameplay or renderer changes
were made during the audit.

Verified repository: <https://github.com/l1nuvv/RelayCore>. `git fetch origin dev`
succeeded; local `dev` and `origin/dev` matched
`1bce0d6595bc3b2dc7fa307d3940374b24ebb72d`, with a clean working tree.
The audit inspected that revision; no implementation branch was created.

The recommended scope is to improve enemy-hit presentation and preserve complete
resolved-hit data while reusing existing surface effects, audio, recoil and combat
rules. Implementation follows the performance checkpoint selected by the owner;
the [roadmap](../PLAYABLE_SLICE_ROADMAP.md) owns priorities and status.
[H-008](../tickets/H-008.md) and its
[current profiling report](../profiling/H-008-2026-10-14.md) own performance evidence.

### Current-state findings

| Area | Confirmed implementation | Gap or constraint |
| --- | --- | --- |
| Hit resolution | Fixed-step firing; `CombatFireResult`, `PlayerAttackResult`, contact-bearing traces | Mobile-enemy identity is not preserved per pellet; applied damage is absent from the result |
| Enemy flash | Turret hit/death timers | Timers drive sparks, not a material flash; mobile enemies lack dedicated damage-flash state |
| Impact effects | Metal sparks, hard-surface flecks, tracers, muzzle flashes and persistent bullet marks | Turrets also emit a burst from their origin toward the camera, independently of the contact burst |
| Audio | Positional surface cues, generic enemy-hit cue, nearest contact per surface family for shotgun audio | No impact variation in this path; replaying a cue restarts its cached sound |
| Gameplay reactions | Awareness/death transitions, protected windup/recovery/lunge behavior, guardian suppression | No general directional knockback or damage stagger |
| Player feedback | Hit confirmation, damage numbers, weapon animation, shotgun camera recoil | Camera recoil directly changes aiming pitch; it cannot be reused blindly for cosmetic impact shake |

Concrete owners and implications:

1. [`fire_player_weapon`](../../src/relaygame/enemy_runtime.cpp) retains seven
   pellet results, but their `CombatFireResult` entries lack mobile-enemy identity
   and applied damage. [`apply_player_attack`](../../src/relaygame/game_world_runtime.cpp)
   compares temporary before/after health arrays to generate damage numbers.
   A small game-owned result extension can support feedback and remove this
   reconstruction. This is a structural observation, not a measured speedup.
2. [`render_turrets`](../../src/relaygame/world_scene_renderer_interactives.cpp)
   uses hit timers for sparks. Mobile-enemy rendering in
   [`world_scene_renderer.cpp`](../../src/relaygame/world_scene_renderer.cpp)
   selects live/dead models and attack-state signals, without a damage flash.
   Existing per-draw emissive factors in
   [`RenderQueue`](../../src/relaycore/render/render_queue.h) are a candidate
   integration path without material copies or a new shader interface; visual
   suitability and integration across enemy model paths remain unverified.
3. [`enemy_runtime_tests.cpp`](../../tests/enemy_runtime_tests.cpp) explicitly
   protects windup and recovery from nonlethal hits, and committed lunge targeting
   from retargeting on impact.
   [`ContainmentEncounter::suppress`](../../src/relaygame/containment_encounter.cpp)
   already supplies conditional 650 ms suppression. Generic stagger must not
   silently override these contracts.
4. Existing impact bursts use six metal streaks or four hard-surface flecks.
   [`weapon_impact_feedback`](../../src/relaygame/player_feedback.cpp) bounds
   surface audio per discharge.
   [`AudioSystem::play_one_shot`](../../src/relaycore/audio/audio_system.cpp)
   supplies attenuation and cached playback. More particles or a new pooling
   system are not justified by this static audit.
5. [`process_fire_input`](../../src/relaygame/game_world_runtime_feedback.cpp)
   consumes resolved attacks and dispatches audio inside the fixed-step path.
   [`game_world_runtime_update.cpp`](../../src/relaygame/game_world_runtime_update.cpp)
   latches discrete input until simulation consumes it. Presentation must preserve
   this ownership and avoid duplicate gameplay resolution or audio dispatch.

### Corrections to the supplied brief

- Replace its Vulkan 1.3 compatibility requirement with the current
  [Vulkan 1.4 contract](../RENDERER_MODERNIZATION_PLAN.md), retaining optional-feature
  fallbacks. Do not restore retired 1.3 paths.
- Reuse implemented VIS-40 effects and surface audio instead of treating them as
  missing. G-019 acceptance does not establish the separate VIS-40 moving-player
  readability gate; consult the roadmap for current status.
- Treat 70 ms flash, particle counts, stagger duration and camera angles as trial
  values, not accepted defaults. Existing counts should remain the first baseline.
- Preserve current enemy roles and attack commitments. Separate presentation work
  from balance-changing stagger or knockback.
- H-008 records existing runtime bottlenecks, not the incremental cost of these
  proposed effects. Before/after measurements remain pending.
- Follow the active repository workflow for future implementation commits and
  verification. Routine work does not require archives or release packaging.

### Implementation sequence

1. **Establish the comparison scenario after the H-008 checkpoint.** Record current
   pistol/shotgun feedback against a turret, rover, mobile shooter, guardian,
   metal and concrete, with sound and ordinary gameplay cameras. Keep visual and
   gameplay evidence in this report; keep profiling evidence under H-008. Preserve
   damage, health, cadence, quality settings and existing behavior for comparison.

2. **Preserve complete resolved hits locally.** Extend `PlayerAttackResult` with
   bounded per-ray target identity, contact/direction and applied damage, populated
   at existing damage-resolution sites without additional raycasts. Keep game-only
   target classification out of engine contracts. Aggregate pellets per target for
   damage numbers and reaction eligibility. Preserve nearest-hit ordering,
   occlusion, kill events and the rule that an early pellet kill cannot expose a
   target behind it. Use copied values and session-local identities rather than
   retaining raw enemy pointers. Avoid a general event bus.

3. **Implement the smallest presentation improvement.** Add transient enemy flash
   state and evaluate a 70 ms emissive pulse through existing draw parameters.
   Define repeated-hit behavior and clear transient state on death and session
   restoration. Reuse contact bursts; evaluate removing the redundant
   camera-oriented turret burst. Retain current particle counts and audio cues
   initially. Keep short feedback visible across fixed-step catch-up without
   replaying audio. Preserve material appearance outside the pulse and verify
   both simple and multipart enemy model paths.

4. **Evaluate gameplay reactions separately.** Start with presentation-only
   flinch. Add collision-aware knockback or stagger only if playtesting shows a
   need. Define weapon/role eligibility, interruption rules, cooldowns and
   save/retry behavior first. Preserve guardian suppression and committed enemy
   attacks by default. Do not reuse attack-recovery timers in a way that changes
   cadence or permits stagger-lock; do not add an animation framework.

5. **Tune audio and camera against observed problems.** Introduce bounded sound
   variation if repeated cues are audibly monotonous. Keep firing recoil unchanged
   initially. Any confirmed-hit camera impulse needs a separate bounded
   presentation offset that respects `camera_motion` and does not alter shot
   direction. Omit hitstop. Reuse existing assets; identify any demonstrated asset
   gap before expanding production scope.

### Verification and acceptance plan

Use the current presets and the narrowest affected targets. Confirm an MSVC
developer environment and valid `VCPKG_ROOT` per
[DEVELOPMENT](../DEVELOPMENT.md#сборка-с-нуля); reuse configured builds.
The following commands were checked against preset/target/test registrations,
but were **not executed during this audit**:

```powershell
cmake --build --preset tests-debug --target relaycore_tests
cmake --build --preset graphics-debug --target relaycore_power_restoration_graphics relaycore_game_lifecycle
ctest --preset graphics-debug --output-on-failure --no-tests=error -R '^graphics\.(world_effects(_sdr)?|weapon_impacts(_hdr)?|game_lifecycle)$'
```

Discover affected unit cases with `ctest --preset test-debug -N` before selecting
them; execute the selected cases with `--output-on-failure --no-tests=error`.
Start with combat, enemy, weapon-impact and containment regressions. Add audio
lifetime or viewmodel targets only when changes affect those owners.

Required regression cases:

- Mixed-target pellets, actual applied damage, occlusion, early kills and one
  target event per kill; misses and rejected cooldown shots produce no enemy-hit
  feedback.
- Repeated/overlapping hits, lethal hits, transient expiry and low-frame-rate
  catch-up; bounded effects and no duplicate audio dispatch.
- Restart, Continue and encounter snapshot restoration clear stale presentation
  without altering persistent gameplay state.
- Existing windup, recovery, lunge targeting and guardian suppression remain
  unchanged unless a separately accepted gameplay change defines new behavior.
- Add flash samples to existing graphics fixtures. Check HDR/SDR and affected
  model paths, require executed cases without skips and clean Vulkan validation.

Compare identical optimized combat workloads before/after, recording source/build
identity, hardware, settings, CPU/GPU time, frame tails and relevant allocations.
Existing H-008 measurements are context, not proof that added effects are cheap.
Do not claim an improvement without comparable measurements.

Conduct a sound-on G-019 playtest for hit/miss readability, repeated-fire noise,
threat cues, aiming comfort and enemy-role preservation. Fixed-camera graphics
captures cannot establish those qualities. Record human acceptance separately
from technical test results. Run `git diff --check` after implementation and
commit coherent, verified changes according to the repository workflow.

**Audit verification actually performed:** remote/ref verification and static
inspection of source, tests, presets and owning documents. Builds, runtime tests,
profiling and playtesting were not run. No incremental performance or gameplay
acceptance is claimed. Implementation remains subject to the current H-008
priority and a subsequent implementation request.
