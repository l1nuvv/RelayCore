# RelayCore next steps

## Current state

RelayCore now has a **real engine/game runtime seam** instead of only a launcher split:

- the engine owns the app loop, window/context, renderer bootstrap, audio and config
- the game/demo owns camera input updates and scene rendering through `AppDelegate`
- the renderer no longer hardcodes the rotating cube inside the engine frame loop
- the demo now renders the cube from `src/relaygame/`

## Recommended order from here

### 1. Add `world/` and `collision/` to the engine
Introduce static room/blockout data, traces and sweeps on the engine side.

### 2. Add a real player motor on the game side
Replace the free-fly demo camera with a gameplay controller built on top of engine queries.

### 3. Split mesh abstractions
Move from `ColorCubeMesh` to reusable `VertexArray`, `VertexBuffer`, `Mesh` and later static-world mesh batches.

### 4. Add textures and materials
Load one texture, bind it through a small material path and move from vertex-colored geometry to authored surfaces.

### 5. Add a level/bootstrap path
Let the game spawn a small room/corridor blockout from data instead of hand-coded demo primitives.

## Build pipeline

- [x] vendored glad2
- [x] first 3D camera and cube
- [x] basic CI on GitHub Actions
- [x] real engine/game delegate seam for update + render
- [ ] room geometry and player collision
- [ ] input abstraction layer
- [ ] texture/material pipeline
- [ ] package/release workflow only after stable runtime assets and executable layout
