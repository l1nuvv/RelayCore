---
name: relaycore-rmlui
description: Author and debug the implemented RmlUi game HUD, menus and screens in RelayCore with SDL3 and Vulkan. Use for RML/RCSS, C++ presentation data, UI input/focus and rendering. Exclude ImGui editor/diagnostics changes except coexistence checks.
---

# RelayCore RmlUi

Use RmlUi for player-facing UI work requested by the owner. RmlUi 6.3 and the VIS-43–46 technical integration are implemented; human visual acceptance remains a separate gate in the roadmap. Retain ImGui for editor, diagnostics and developer tools. This skill does not authorize new UI features or repeating the migration.

## Scope and evidence

For routine HUD/RML/RCSS corrections start with the affected authored files, player UI source, direct callers and nearest tests; read the UI authoring/staging section of `docs/DEVELOPMENT.md` as needed. For ownership, rendering or input changes read the relevant `docs/ARCHITECTURE.md` and `docs/DEVELOPMENT.md` sections plus [references/integration.md](references/integration.md). Read the roadmap introduction/status and `docs/tickets/VIS-43-46.md` with its linked protocol for VIS-43–46 migration regressions, acceptance or changes to those contracts; routine layout edits do not require the migration history. Read `docs/DEPENDENCY_STACK.md` for dependency changes, and `docs/ENGINE_ARCHITECTURE_REFERENCE.md` for new capability boundaries. For visual decisions use `relaycore-art-direction` and `docs/ART_DIRECTION.md` when needed. Repository paths here are relative to the repository root.

Start at the affected screen and direct callers. Current ownership anchors, to recheck against source:

- `assets/ui/{hud,menu,finale}.rml` and nearby RCSS: authored player screens.
- `src/relaycore/render/player_ui.h`, `.cpp` and `player_ui_data.h`: persistent RmlUi context/documents, RelayCore-owned presentation/actions and input ownership.
- `src/relaycore/render/vulkan_rml_renderer.cpp`: geometry/textures, draw order and UI GPU lifetime.
- `src/relaygame/demo_game_menu.cpp`: menu presentation and existing actions/settings.
- `src/relaycore/render/renderer.cpp`: frame and presentation integration.
- `src/relaycore/debug/imgui_overlay.cpp`: SDL events and ImGui capture; inspect event dispatch callers before changing arbitration.

VIS-43 owns foundation/HUD; VIS-44–45 own further screens; VIS-46 records ordinary play without ImGui tools. These are completed technical slices with their recorded acceptance limits. Fix or extend only the requested screen/behavior.

## Dependency and backend

Read [references/integration.md](references/integration.md) for dependency adoption, lifecycle, backend adaptation or rendering/input debugging. Use the selected release's headers, CMake and backend source as API authority: online documentation follows master and may differ from the installed version.

Verify the actual vcpkg baseline/port, SDL3/Vulkan support and Windows/MSVC build. Record the exact selected version, features, integration cost and limitations in the owning ticket and `docs/DEPENDENCY_STACK.md`. Follow manifest/overlay policy. Upstream sample backends are adaptation sources; a standalone sample does not prove integration into RelayCore. SDL GPU is a separate path from the project's direct Vulkan renderer.

For a requested backend or ownership change, verify the real HUD and an interactive screen with long Russian text. Reuse the implemented foundation and preserve RelayCore window/device/swapchain ownership. Do not add Lua, a browser runtime, an event bus or a general UI framework merely to use RmlUi.

## Data and actions

Keep `Rml::*`, documents, model handles and callbacks within UI implementation. Supply minimal RelayCore-owned presentation data independent of diagnostics structs. Game owns state/actions; render/UI owns layout, GPU resources and submission. UI bindings must not become another gameplay state authority.

Prefer persistent documents and small data bindings to rebuilding markup every frame. Register types and bind variables before loading the document. Bound storage and callback targets must outlive their model; never bind temporary frame snapshots. Mark changed top-level variables dirty before `Context::Update()`. Remove models/listeners and invalidate document references during screen/session teardown. See the official [model contract](https://mikke89.github.io/RmlUiDoc/pages/data_bindings/model.html).

Translate callbacks into explicit existing game actions at a safe update boundary. Avoid loading levels, replacing the renderer or destroying the active screen within event dispatch. Preserve Continue/new game/save/load/settings semantics and errors.

## RML/RCSS and fonts

Author structure in RML and layout/style in RCSS. Consult the official [RML manual](https://mikke89.github.io/RmlUiDoc/pages/rml.html) and [RCSS manual/property index](https://mikke89.github.io/RmlUiDoc/pages/rcss.html) for supported syntax; browser HTML/CSS/JavaScript behavior is not a guarantee.

Use nearby authored conventions and share colors, spacing and typography only where actual screens need reuse. Show focus, hover, pressed, disabled and error states. Preserve sightlines and avoid duplicate large objective/event cards. Do not invent ammunition displays, minimaps or inventory grids without a gameplay need.

Load explicit font assets with Cyrillic coverage and required weights; RmlUi has no default font. Check load results and family names. Use UTF-8 and test long labels, multiline objectives and missing glyphs. Route RML, RCSS, textures and fonts through the actual asset-root/staging mechanism. Use `relaycore-assets` when changing delivery.

Treat variable strings as text; escape them before APIs that interpret markup. Do not manually restructure elements controlled by `data-for`; update the model. Attach binding attributes before document attachment. See [binding limitations](https://mikke89.github.io/RmlUiDoc/pages/data_bindings.html).

Author effects only when the renderer supports them: transforms, rounded clipping, shadows and blur may require optional callbacks. Successful parsing is not visual evidence.

## Verification

Use `relaycore-verify` for affected build/tests and run `git diff --check`. Record actual automated results and before/after frames in the owning ticket. Check HUD states, menu actions, input ownership, long Russian text, 1280×720/1920×1080/2560×1440, high DPI, resize, pause/resume and game/editor/diagnostics transitions. Verify ordinary game UI with ImGui tools disabled, developer access with them enabled, and SDR/HDR composition after scene tone mapping.

Exercise GPU retirement, presentation recreation and repeated screen/session transitions when their ownership changes. Report skipped checks and missing runtime evidence. Readability in motion remains a human playtest gate; compilation is not visual acceptance. Update `docs/ARCHITECTURE.md` for implemented contracts only. Do not package unless requested.
