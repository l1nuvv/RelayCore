# RelayCore

## Project contract

- RelayCore is a custom C++20 engine and GoldSource-inspired indoor FPS product, not a historical recreation or a general-purpose framework. Keep it understandable, debuggable, profileable, and shippable under one-developer direction even when implementation is agent-assisted.
- Prefer a small working vertical slice over framework-building, speculative generality, or architecture for hypothetical future features.
- Primary environment: Windows 11, Visual Studio/MSVC, CMake presets, Ninja, vcpkg manifest mode, CLion. Current runtime requires SDL3 and direct Vulkan 1.4; OpenGL was removed at VK-013. Tests use doctest; performance work uses Tracy.

## Token economy

- Use one agent unless the user explicitly requests delegation or parallel agents. Keep output near 2000 tokens per ordinary tool call; save long logs and return relevant failures/results.
- Start from named files, direct call sites and nearby tests. Search the owning directory first; widen only for a concrete unanswered question. Reuse known paths and findings, including after compaction. Reread only needed changed/missing ranges.
- Respect `.gitignore`/`.rgignore`. No repository-wide `rg --no-ignore`/`rg -uuu` or recursive PowerShell enumeration from the root. Access ignored files through specific paths; scope any ignore bypass narrowly.
- Complete the requested patch and relevant checks, then stop. Avoid unsolicited audits, speculative alternatives, repeated confidence checks and unrelated cleanup. Keep updates and handoffs concise.
- Reasoning effort remains the user's choice in Codex. Recommend Low for mechanical edits, Medium for ordinary implementation, and High for ambiguous engine/debugging work when warranted. Do not pin effort in project configuration or trade required verification for a lower setting.

## Evidence and scope

- Before editing, identify the requested result, scope and narrow verification from the request and existing ticket. Resolve routine choices autonomously; ask only for missing information that materially affects the result. Review/analysis requests are read-only.
- Source, current presets/tests, runtime output and captures are primary evidence. Separate facts from hypotheses; do not patch suspicions. Plans and historical audits do not authorize extra work.
- Use the task routing in [the guide](docs/AGENT_TASK_GUIDE.md#evidence-and-scope) only when the task concerns gameplay/milestones, architecture, rendering, dependencies, setup, design or documentation ownership. Read the selected owner/section, not the entire guide or documentation set. Use [docs index](docs/README.md) only if the owner is unclear.
- Keep status, decisions and evidence in the existing owning ticket/plan. Do not duplicate statuses or create plans for routine fixes. Preserve acceptance criteria, provenance and verification limits when shortening/moving docs; update links and useful anchor redirects.

## Implementation

- Make the smallest complete patch in existing project style. Preserve module boundaries, explicit ownership/RAII, authored behavior and actionable validation; no silent format/path/preset/tool/dependency changes.
- Before code changes, read [Implementation](docs/AGENT_TASK_GUIDE.md#implementation) and [Anti-slop rules](docs/AGENT_TASK_GUIDE.md#anti-slop-rules). These retain dependency/type boundaries and detailed constraints; apply them without expanding scope.

## Anti-slop rules

Use existing idioms. Add abstractions, configuration, comments, logging and tests only for a concrete need. Protect behavior/invariants rather than implementation structure; no speculative framework work. Detailed requirements are in [the guide](docs/AGENT_TASK_GUIDE.md#anti-slop-rules).

## Performance

Measure before optimizing or claiming speedups. For performance work, read [Performance](docs/AGENT_TASK_GUIDE.md#performance) and use the applicable profiling workflow.

## Authored content and assets

For art, assets or Blender work, read [Authored content and assets](docs/AGENT_TASK_GUIDE.md#authored-content-and-assets) for the required project skills and authoritative documents. Proposed reference scenes do not authorize content work.

## Build and verification

- Finish a coherent patch before checking it. Use the existing build tree, incremental affected targets and narrow relevant tests. Reconfigure only for a concrete configuration/dependency need or diagnostic; no routine clean/rebuild-all.
- Repeat successful checks only after relevant edits or new evidence. Broaden only for crossed build/config/runtime boundaries or an explicit request. Use actual current presets/targets.
- Docs/instructions need diff/link checks as applicable; tool config needs its own validation, not an engine build. Always run `git diff --check` and inspect the task diff.
- For nontrivial build/test selection or failures, use `.agents/skills/relaycore-verify/SKILL.md` and [Build and verification](docs/AGENT_TASK_GUIDE.md#build-and-verification). Report exact blocked commands and unverified behavior; inspection alone is not a passing check.
- Never delete build trees/caches/user presets/generated data as a first troubleshooting step. Create archives/packages/standalone candidates only when the user directly requests packaging or release, regardless of older ticket criteria.

## Git and handoff

- After implementation and relevant verification, automatically stage explicit task-owned paths/hunks and make coherent local commits with concise imperative subjects. No `git add .`; preserve unrelated changes. Report blocked checks honestly. Do not push or rewrite history unless asked.
- Final handoff: result, changed paths, checks actually run and remaining blocker/risk. Do not repeat logs or the prompt.
- For work continuing in another chat, update the existing owning ticket with the compact [continuation format](docs/AGENT_TASK_GUIDE.md#task-and-continuation-formats). Do not create a handoff file or extra report for every small task.
