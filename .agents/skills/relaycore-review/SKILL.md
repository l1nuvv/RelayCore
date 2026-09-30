---
name: relaycore-review
description: Perform a read-only defect-first review or audit of RelayCore code, a diff, commit, branch, subsystem, or proposed patch. Use when asked to review, audit, inspect correctness, find regressions, or remove AI-generated code slop; do not edit unless the user separately asks for fixes.
---

# RelayCore review

Review for actionable defects, regressions, project-fit problems, and generated-code slop. Stay read-only unless implementation is explicitly requested.

## Scope

1. Read applicable repository instructions.
2. Inspect the requested diff/paths completely enough to understand changed behavior.
3. Inspect nearest tests and direct call sites for each material finding.
4. Expand beyond that only to prove or disprove a concrete concern.

Do not run a broad architecture critique when the request is a patch review.

## Finding threshold

Report a finding only when a maintainer would likely act on it. Prefer confirmed issues over speculative “could be cleaner” suggestions.

For each finding include:

- exact path/symbol or diff location;
- manifestation or failure scenario;
- cause;
- impact;
- smallest practical correction;
- focused verification.

Label uncertainty explicitly when evidence is incomplete.

## Anti-slop review

Treat these as findings only when they create real maintenance, correctness, performance, or consistency cost:

- duplicate abstraction for an existing concept;
- wrapper/helper used once with no invariant or ownership value;
- generic `Manager`/`Service`/`Utils` layer that obscures the actual owner;
- speculative extensibility, unused configuration, callback/interface/template generality;
- comments/docblocks that restate obvious code at scale;
- defensive fallback that masks invalid state;
- redundant logging/exception handling/validation;
- tests coupled to implementation rather than behavior;
- broad unrelated rename/format churn;
- generated visual/content changes that drift from nearby project conventions.

Do not report personal style preferences that have no concrete cost.

## Output

Lead with findings ordered by severity. If no actionable defects are confirmed, say so directly and state the main residual risks or unverified areas in one short section.

Do not pad the review with praise, generic best practices, or a rewrite proposal when the current design is adequate.
