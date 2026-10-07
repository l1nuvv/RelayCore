# Native collision products

E-012 adds independent `.rccollision` products; E-013 connects explicit authored placements to gameplay.
Existing maps retain their AABB backend unless a native prop opts in.

Declare a source basename and its model basename in `cmake/RelayCoreCollisionAssets.cmake`.
The model must already be declared in `RelayCoreModelAssets.cmake`; the collision source must repeat the
same `models/<name>.rcmodel` reference. Render triangles are never used as implicit collision geometry.
`content/collision/lab_bench.toml` is an explicit three-box simplification that retains the opening under the bench.

Source v1 requires `version = 1`, `units = "metres"`, `axes = "right-handed-y-up"`,
`winding = "ccw"` and `model`. `[[boxes]]` entries contain `mins`, `maxs`, `surface`;
`[[triangles]]` entries contain `a`, `b`, `c`, `surface`. Surface names use the existing
`SurfaceMaterial` vocabulary. Empty geometry, degenerate triangles, invalid surfaces and non-finite positions fail compilation.
An optional 16-number column-major `transform` bakes an invertible affine transform into model-local positions.
Nonuniform scale is supported; negative determinant swaps triangle vertices to retain outward CCW winding.
Runtime fixture instance transforms use the same operation before constructing `PhysicsWorld`.

Product v1 is backend-independent: little-endian `uint32` magic `0x4c4f4352` (RCOL), version,
model-reference byte count, triangle count, six float32 bounds coordinates, UTF-8 model reference,
then triangles. Each triangle has nine float32 position coordinates and a uint32 `SurfaceMaterial` value.
Counts are bounded before allocation; the loader checks exact payload length, bounds, geometry, surfaces and pairing.
Changing the serialized surface numbering requires a product version change. Jolt handles/cooked state are not serialized.

Products share the existing per-configuration `generated/model-assets` staging root, under `collision/`.
`generated/collision-receipts` records SHA256 hashes for source, compiler executable, compile rule,
declaration, paired model product and collision product. Staging checks these hashes before changing runtime assets;
timestamp-preserving edits therefore fail with a rebuild diagnostic. A failed compile leaves the preceding
product, receipt and staged collision intact. Removed declarations disappear from the runtime manifest and staging.
Runtime loading needs only the product and expected model reference, with no source/DCC files.

## Editor and inspection

In the existing level workshop, select a `prop_static` and choose **Collision product** or the separate
box checkbox. Assigning a product validates its model pairing before changing the document and removes
the box fields; enabling a box removes the product reference. Undo/redo and save/reload preserve this choice.

Enable **Collision overlay / queries** to draw collision over the rendered scene. Boxes are cyan;
native triangles are orange, tile yellow and hazard red; selected mesh triangles are white.
The panel casts a 24 m ray and an axis-aligned box with 0.2 m half extents along the camera forward vector.
It reports hit position, normal, surface, layer and authored identity. Green markers show contacts/normals;
**Select hit** or clicking scene collision selects the authored object, including through actual mesh openings.
An invalid edit disables collision queries/overlay until the preview rebuild succeeds.

For a selected native prop, the panel shows product/model references and transformed collision bounds.
**Check build receipt / staleness** runs the existing read-only CMake checker on that product, showing
source, pairing, receipt version and SHA-256 validation, including both staged collision and paired model.
Repeat the check after external edits. Receipt inspection requires the developer build and checkout;
missing metadata/tool paths are reported as errors. Ordinary gameplay only loads runtime products.
The gameplay Inspector also reports native mesh hits, normals, surfaces, bounds and authored source;
pinned selection can be restored by authored source after reload.

```powershell
cmake --build --preset demo-debug --target relaycore_collision_assets relaycore_physics_tests relaycore_physics_fixture -j 2
ctest --preset test-debug -R '^relaycore\.(physics\.|tooling\.collision_assets$)' --output-on-failure --no-tests=error
build/msvc-debug/src/relaygame/relaycore_physics_fixture.exe --collision build/msvc-debug/physics-fixture/assets
```

`LevelSessionBuilder::build_collision_fixture` prepares an immutable physics world from a product before activation.
The focused physics targets stage model/collision products into `build/<preset>/physics-fixture/assets`
using the same manifest synchronizer, without depending on shader compilation or irradiance baking.
Loading failures leave the active session intact. The bench fixture exercises rays through the opening and against
metal supports/tile worktop.

## Authored placement and movement

A native `prop_static` opts in by declaring `collision_asset = "collision/lab_bench.rccollision"`
alongside `model = "models/lab_bench.rcmodel"`. Its origin, angles and scale transform both products.
The collision reference must name a staged `collision/<name>.rccollision` paired with that model;
legacy `collision_mins/maxs/material` and `collision_asset` are mutually exclusive.
Session preparation loads and validates the geometry before replacing the active session.
Maps without this property retain the existing AABB path.

The player remains a feet-origin, world-aligned box with the existing configurable dimensions,
axis order, substeps, step height, ground probe/snap and acceleration/friction/jump rules at 120 Hz.
`WorldQuery` preserves endpoint clipping for authored boxes, including its epsilon behavior;
only mesh geometry uses Jolt box sweeps. Sweeps reduce the orthogonal half extents by epsilon
and stop epsilon before contact. This keeps box trajectories identical while retaining mesh holes.
Rays use the same immutable static world plus live door/actor records, with original masks,
surface materials and authored identity. No capsule, `CharacterVirtual`, dynamics or slope policy is introduced.

The existing fixed-step phases are retained: movement, focus and player fire see the preceding
completed door state; use and trigger entry then request door changes, doors advance with overlap
reopening, and enemy movement/LOS and combat threats see the advanced state. Every query view
collects door bounds from the current fraction; actor-specific movement overlap filtering is retained.
Door geometry is never baked into the immutable mesh world. Snapshot/continue restore gameplay
state while session preparation/activation owns geometry and rebinds the controller's borrowed view.

`tests/native_collision_fixture.h` supplies the shared lifecycle/gameplay fixture. Its bench is
scaled vertically to provide standing clearance; it is test geometry, not a new shipped level.
