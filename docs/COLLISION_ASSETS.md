# Native collision products

E-012 adds independent `.rccollision` products. Existing gameplay maps keep their AABB backend until E-013.

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

```powershell
cmake --build --preset demo-debug --target relaycore_collision_assets relaycore_physics_tests relaycore_physics_fixture -j 2
ctest --preset test-debug -R '^relaycore\.(physics\.|tooling\.collision_assets$)' --output-on-failure --no-tests=error
build/msvc-debug/src/relaygame/relaycore_physics_fixture.exe --collision build/msvc-debug/physics-fixture/assets
```

`LevelSessionBuilder::build_collision_fixture` prepares an immutable physics world from a product before activation.
The focused physics targets stage model/collision products into `build/<preset>/physics-fixture/assets`
using the same manifest synchronizer, without depending on shader compilation or irradiance baking.
Loading failures leave the active session intact. The bench fixture exercises rays through the opening and against
metal supports/tile worktop. Player-controller parity and authored level placement are separate E-013 work.
