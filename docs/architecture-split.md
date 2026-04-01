# RelayCore clean split architecture

## Goal

Keep the repo small, but make the boundary between **engine** and **game/demo** explicit in both build targets and runtime ownership.

## Physical layout

### Engine target

`relaycore_engine` owns `src/relaycore/`:

- `app/` — application runtime bootstrap, frame loop, delegate seam, `EngineServices`
- `audio/` — audio system lifetime and playback services
- `core/` — config, logging, paths, profiling helpers
- `debug/` — diagnostics overlay and future debug tools
- `platform/` — window, SDL lifetime, context management
- `render/` — render-facing runtime systems
- future: `world/`, `collision/`, `assets/`

### Game target

`relaycore_demo` owns `src/relaygame/`:

- `main.cpp`
- `demo_game.*`
- future gameplay systems like:
  - `player_motor.*`
  - `demo_level_bootstrap.*`
  - `door_system.*`
  - `trigger_system.*`

## Runtime ownership rule

The engine answers:

- how the app runs
- how the window/context/audio/render runtime works
- what services are exposed to game code

The game answers:

- what input policy drives the camera/player
- what gets rendered each frame
- what gameplay rules define the sandbox

## Why this is cleaner

The previous transition solved only the **filesystem** split.
The main weak spot was that the engine still owned too much demo behavior:

- the free-fly camera update lived inside `Application`
- the rotating cube draw lived inside `Renderer`
- the game delegate had too little access to engine state

The new split fixes that by introducing `EngineServices` and a render hook:

- `Application` owns the loop
- `AppDelegate` owns `on_update()` and `on_render()` policy
- `Renderer` exposes frame primitives instead of hardcoding the demo scene
- `DemoGame` now owns free-camera handling and cube drawing

## Next recommended step

Build the next milestone on top of this split:

### Engine side

Add:

- `src/relaycore/world/`
- `src/relaycore/collision/`

### Game side

Add:

- `src/relaygame/player_motor.*`
- `src/relaygame/demo_level_bootstrap.*`

That keeps GoldSrc-inspired world/query infrastructure reusable while keeping gameplay-specific behavior out of the engine library.
