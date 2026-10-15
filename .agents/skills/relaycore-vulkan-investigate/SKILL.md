---
name: relaycore-vulkan-investigate
description: Diagnose RelayCore Vulkan startup/device/presentation failures, validation or synchronization errors, resource lifetime faults and graphics regressions. Use for focused GPU/RenderDoc investigations; not for ordinary renderer edits with known checks, CPU-only bugs or performance claims without profiling.
---

# RelayCore Vulkan investigation

Paths below are relative to the repository root. Read the Vulkan capture/validation section of `docs/DEVELOPMENT.md` for current procedures and relevant `docs/ARCHITECTURE.md` sections for runtime ownership/contracts. Current VK14 milestones, acceptance gates and evidence belong to `docs/RENDERER_MODERNIZATION_PLAN.md`; consult only the selected milestone when needed. Use `docs/VULKAN_MIGRATION_PLAN.md` only for completed migration, historical parity or decision rationale.

1. Fix the reproduction: executable/config/assets, GPU/driver, validation state, desktop/session conditions and first failing operation. Separate shader compilation, loader/layers, device features, SDL surface/presentation and rendered output.
2. Inspect current `VulkanDevice` requirements and the failing owner's diagnostics. Loader and selected GPU must support API 1.4; unsupported loaders/devices must fail startup with actionable diagnostics. There is no Vulkan 1.3 fallback, and the removed `RELAYCORE_VULKAN_FORCE_1_3` override no longer changes startup. API 1.4 alone does not prove the required feature/queue/WSI/presentation contract; check optional graphics capabilities independently. The toolchain probe proves only its loader/layer checks.
3. For shader/pipeline failures inspect Slang/SPIR-V compilation, staged shader paths and the failing pipeline's diagnostics. Select the nearest registered graphics test through `relaycore-verify`. Exit 77, locked desktop/readback failure and an empty test selection are evidence gaps, not passes.
4. For resource lifetime, trace the producer/consumer, queue and completion condition through recreate/failure/shutdown. Follow existing VMA/device/presentation ownership rather than adding a second synchronization mechanism.
5. Trace layout/access/stage and start with normal core/synchronization validation. Use GPU-assisted validation only when it answers a remaining shader-access question; apply process-local settings from the developer guide and restore them. Do not weaken the strict CTest warning gate or measure performance under GPU-AV.
6. Use RenderDoc for a concrete attachment/barrier/descriptor/draw/pixel question when validation and source evidence leave it unresolved.
7. For visual regressions inspect the relevant accepted captures and color/HDR/material path in `docs/RENDERING.md`. Do not update baselines to conceal unexplained changes.
8. If the issue is measured timing rather than correctness, use `relaycore-profile` for measurement. Do not expand a focused diagnosis into a full renderer audit.

Conclude with the causal evidence, affected contract, narrow correction if authorized, repeated regression/validation result and any unavailable hardware/capture evidence. Stop when the reported failure is resolved or the missing environment prevents further diagnosis; do not resume completed migration phases.
