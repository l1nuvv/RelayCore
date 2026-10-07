---
name: relaycore-profile
description: Investigate RelayCore frame-time regressions or measured CPU/GPU/query/upload/wait costs using Tracy and reproducible workloads. Use for profiling and requested measured optimization; not for generic cleanup, graphics correctness alone or estimating player experience.
---

# RelayCore profiling

Paths below are relative to the repository root. Read the profiling section of `docs/DEVELOPMENT.md`; inspect current presets, workload arguments and Tracy client version before selecting tools. Read only the relevant report in `docs/profiling/` or protocol linked by the requested roadmap ticket.

## Establish comparable evidence

- Use the optimized profiling configuration. Debug/ASan/validation FPS does not characterize the playable build. Record revision plus working-tree changes, executable/assets, GPU/driver, resolution, VSync, color pipeline/HDR, shadows, audio and validation state.
- Choose the workload that exercises the reported issue. Existing episode/bench/view workloads have different boundaries; inspect what they actually update/render/present. Stationary states and fixed simulation ticks do not establish continuous player-route performance or human duration.
- Capture a baseline before editing. Separate first-use from revisit and state whether OS/driver caches were controlled; a new process alone is not a cold-cache measurement. Avoid concurrent builds, GPU tests and other captures.
- Inspect CPU work, GPU intervals, uploads, queries and fence/present waits separately. Nested or overlapping CPU/GPU times cannot be added as independent costs. Missing allocation instrumentation is not evidence of zero allocations.

## Investigate and conclude

Rank measured contribution/frequency, form a specific hypothesis, then optimize only if requested. Prefer removing work or improving data shape before new scheduling/cache/render architecture. For a cache change identify ownership, invalidation and stale-data verification.

Run affected correctness checks and repeat the same workload/settings. Report sample count, frame-time distribution and spikes with measurement boundaries; expand sampling only when noise or a missing scenario prevents a conclusion. If there is no comparable after-capture, report the unverified hypothesis or structural change without a speedup claim. Preserve relevant evidence paths; do not add instrumentation without a diagnostic purpose.
