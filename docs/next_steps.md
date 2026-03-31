# RelayCore next steps

## Current state

RelayCore now renders a real 3D object instead of a triangle in clip space:

- OpenGL is loaded through `glad`
- the renderer enables depth testing and back-face culling
- shaders are read from `assets/shaders`
- a perspective camera builds view/projection matrices
- the frame loop feeds time into the renderer for model animation

## Recommended order from here

### 1. Vendor generated glad2
Remove network dependence from configure/build and keep the GL loader inside the repository.

### 2. Add camera controls
Introduce keyboard + mouse driven movement and keep the camera as a small self-contained render-side object.

### 3. Split mesh abstractions
Move from `ColorCubeMesh` to reusable `VertexArray`, `VertexBuffer`, and `Mesh` classes.

### 4. Add textures
Load one texture, bind it in a material/shader path, and move from colored geometry to textured test geometry.

### 5. Add a scene delegate or render world
At that point the renderer should stop owning a hardcoded cube and start consuming scene data.


## Build pipeline

- [x] vendored glad2
- [x] first 3D camera and cube
- [x] basic CI on GitHub Actions
- [ ] room geometry and player collision
- [ ] input abstraction layer
- [ ] texture/material pipeline
- [ ] package/release workflow only after stable runtime assets and executable layout
