---
name: relaycore-implement
description: Implement or modify RelayCore C++ runtime, gameplay, rendering, config, CMake, tests, or authored behavior. Use for concrete code changes and bug fixes; do not use for read-only review, pure asset creation, or performance investigation without an implementation request.
---

# RelayCore implementation

Use this workflow to keep changes narrow, evidence-driven, and free of generic agent-generated structure.

## 1. Establish the contract

Before editing, resolve from the request and repository:

- required observable behavior;
- files/subsystem that currently own it;
- compatibility or behavior that must not change;
- narrowest relevant verification;
- any explicit non-goals.

Keep this contract in working memory; do not create a planning document unless requested.

## 2. Investigate narrowly

Start with the named path/symbol, nearest tests, and direct callers. Use targeted searches. Expand only when a specific unresolved question requires it.

Prefer current code and test behavior over comments. If the implementation already has a suitable owner or pattern, extend it rather than introducing a parallel mechanism.

## 3. Patch minimally

Prefer the smallest complete change.

Reject a proposed helper/type/layer unless it does at least one of these now:

- removes meaningful duplication;
- encodes an invariant;
- makes ownership/lifetime clearer;
- isolates a real changing concern;
- makes the requested behavior directly testable.

Do not add speculative extension points, generic managers/services, configuration switches, compatibility shims, or framework-like abstractions for hypothetical future use.

Keep failure behavior explicit. Do not add silent recovery that hides malformed content or invalid state.

## 4. Tests

Add or change a test when it protects requested behavior or a demonstrated regression. Prefer public behavior/invariants over private implementation shape.

Do not add tests for trivial accessors, constructors, constant values, or line-by-line duplication of implementation logic unless they encode a real contract.

## 5. Anti-slop pass

Before verification, inspect only the diff and remove:

- comments that restate code;
- redundant wrappers/helpers;
- unnecessary `try/catch`, logging, fallback, or validation duplication;
- unused generality and one-use configuration knobs;
- unrelated formatting or renames;
- verbose names or prose introduced only to sound architectural;
- TODOs/placeholders that are not required for the requested behavior.

The final code should look like an incremental RelayCore change written by the existing maintainer.

## 6. Verify and report

Run `git diff --check`, then the narrowest build/test that exercises the change. Broaden only when the affected boundary justifies it.

Report only:

- what changed and why;
- paths changed;
- checks actually run and their result;
- remaining unverified risk/blocker.

Do not claim runtime correctness if only compilation or static inspection was performed.
