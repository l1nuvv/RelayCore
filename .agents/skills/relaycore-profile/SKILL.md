---
name: relaycore-profile
description: Analyze RelayCore performance using Tracy captures, timing data, frame diagnostics, or a reproducible workload, and implement optimizations only when requested. Use for profiling, frame-time regressions, allocation/hot-path work, or performance reviews; do not use for generic code cleanup.
---

# RelayCore performance workflow

Performance claims require evidence.

## Investigate

1. Identify the exact workload/capture and whether CPU, GPU, synchronization, allocation, or I/O is suspected.
2. Use the profiler capture and current source together. Do not infer a hot path from code appearance alone.
3. Rank issues by measured contribution and frequency, not by how “low-level” an optimization sounds.
4. Distinguish total-frame impact from an expensive-but-rare zone.

## Optimization order

Prefer, when supported by evidence:

1. eliminate unnecessary work;
2. reduce frequency of work;
3. improve data shape/locality and allocation behavior;
4. remove avoidable synchronization/state churn;
5. only then consider micro-optimizations.

Do not introduce jobs/fibers, complex caches, custom allocators, SIMD, spatial structures, or render architecture solely because they are common engine techniques.

For caches/dirty flags, state owner, invalidation condition, rebuild point, and stale-data verification.

## Verification

Preserve correctness tests. Compare the same workload before/after when possible and report the measured metric. If measurement is unavailable after a code change, describe it as a hypothesis or structural improvement, not a confirmed speedup.

Keep Tracy instrumentation only when it answers a recurring diagnostic question; do not fill hot paths with decorative zones.
