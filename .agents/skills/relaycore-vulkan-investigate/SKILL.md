---
name: relaycore-vulkan-investigate
description: Diagnose RelayCore Vulkan startup/device/presentation failures, validation or synchronization errors, resource lifetime faults and graphics regressions. Use for focused GPU/RenderDoc investigations; not for ordinary renderer edits with known checks, CPU-only bugs or performance claims without profiling.
---

# RelayCore Vulkan investigation

Paths below are relative to the repository root. Read the Vulkan capture/validation section of `docs/DEVELOPMENT.md` for current procedures. Read relevant architecture sections only if the investigation reaches an ownership/contract decision; consult `docs/VULKAN_MIGRATION_PLAN.md` for a named migration gate or historical parity comparison, not as a current task queue.

1. Fix the reproduction: executable/config/assets, GPU/driver, validation state, desktop/session conditions and first failing operation. Separate shader compilation, loader/layers, device features, SDL surface/presentation and rendered output. The toolchain probe proves only its loader/layer checks; Vulkan 1.3 alone does not satisfy the device contract.
2. Select the nearest registered graphics test through `relaycore-verify`. Inspect current `VulkanDevice` requirements and the failing owner's diagnostics before changing requirements or adding fallbacks. Exit 77, locked desktop/readback failure and an empty test selection are evidence gaps, not passes.
3. For lifetime/synchronization, trace the resource's producer/consumer, layout/access/stage, queue and completion condition through recreate/failure/shutdown. Follow existing VMA/device/presentation ownership rather than adding a second synchronization mechanism.
4. Start with normal core/synchronization validation. Use RenderDoc for a concrete attachment/barrier/descriptor/draw/pixel question. Use GPU-assisted validation only when it answers a remaining shader-access question; apply process-local settings from the developer guide and restore them. Do not weaken the strict CTest warning gate or measure performance under GPU-AV.
5. For visual regressions inspect the relevant accepted captures and color/HDR/material path. Do not update baselines to conceal unexplained changes. If the issue is timing rather than correctness, use `relaycore-profile` for measurement.

Conclude with the causal evidence, affected contract, narrow correction if authorized, repeated regression/validation result and any unavailable hardware/capture evidence. Stop when the reported failure is resolved or the missing environment prevents further diagnosis; do not resume completed migration phases.
