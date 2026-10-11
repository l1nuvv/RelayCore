---
name: relaycore-playtest
description: Carry out RelayCore playable-slice tickets, gameplay/content changes requiring player validation, or playtest preparation and evidence updates. Use for acceptance criteria, observer protocols and technical-versus-human results; not for isolated C++ mechanics fixes, art-only assets or profiling alone.
---

# RelayCore gameplay and playtest evidence

Paths below are relative to the repository root. Read the roadmap introduction, current status table and selected ticket in `docs/PLAYABLE_SLICE_ROADMAP.md`, then only its linked protocol in `docs/playtests/`. Current ticket scope takes precedence over old candidate instructions.

## Define and prepare the check

State the player decision/experience being tested and its observable acceptance criteria. Reuse the ticket's protocol rather than creating a second acceptance checklist. For design choices consult `docs/DESIGN_REFERENCES.md` as needed; for authored map/editor/session work use the relevant `docs/DEVELOPMENT.md` and `docs/EDITOR_AND_CAMPAIGN.md` sections.

Record the actual build/source/assets and scenario. Use an existing suitable build; an archive is needed only when requested. Rebuild/stage changed map assets before F7; editor user copies and disk progress may differ from repository maps. Do not erase user progress to obtain a clean test; use an isolated test save/profile or a user-authorized reset.

Run relevant deterministic/content/lifecycle checks using `relaycore-verify`. Map-check success, scripted progression, rendered captures and simulation tick counts each establish different facts; none proves first-player duration, readability, combat feel or enjoyment.

## Observe and record

Give a new player only the selected scenario's spoiler-free brief. `docs/PLAYTEST_GUIDE.txt` may describe a different ticket; verify it before reuse. Keep observer routes/solutions separate, record interventions and collect the protocol's times, deaths, decisions, confusion and continue checks. The agent's knowledge of the level does not qualify as independent-player evidence.

Update the existing ticket/protocol with actual observations, checks and limits. Technical completion with pending required human evidence remains `awaiting_playtest`; record explicit author acceptance and its limits if provided. A deferred playtest does not block independent engineering work already authorized by the user. End at the requested ticket; do not automatically implement feedback from unrelated tickets or manufacture missing player results.
