# VIS-26 — route cues and physical navigation

Implemented 2026-10-08. **Done — accepted by the owner after a walkthrough.**

Nine existing arrow/01–03 cues in pressure_transfer, cooling_loop and
power_restoration now belong to mounted enamel signs. Their glyphs remain
world_effect decals, sized and positioned in the left field of a native carrier;
the destination and equipment wording use the existing native mesh lettering.
Four additional signs explain the forward turn or return destination.

No existing cue was deleted. Bench/foundation/B-17 navigation was not changed.
All original solids, gameplay entities, missions, entries, existing props and
warning marks were preserved. The power switchgear cue moved to the corridor's
visible side wall and changed facing accordingly. Its portal and distribution
labels now consistently say 03; pump remains P-02, filtration F-04. These are
room/equipment identities, separate from the operating sequence on the VIS-25
procedure board.

## Construction and source

The reference is the existing facility kit and
docs/art/relaycore-design-systems-board.png: matte sage enamel, galvanized folded
edges, pale industrial lettering and captive fixings. A 2.4 × 0.72 m carrier
keeps the cue and destination together at a typical 2–5 m approach. The airlock
uses a 0.9 × 0.9 m carrier fitted to its narrow pier. Steel supports provide wall
contact; two carriers stand in front of structural posts. No collider is added.
The old seven-segment glyph shape and its renderer colour are retained.

- scripts/export-route-signage-glb.py
- content/graphics/route_signage/placements.json
- content/models/{pressure,cooling,power}_route_signage.{blend,glb}
- scripts/export-power-restoration-glb.py and the service/switchgear exports
- assets/levels/{pressure_transfer,cooling_loop,power_restoration}.toml
- cmake/RelayCoreModelAssets.cmake
- tests/power_restoration_graphics.cpp, tests/pressure_transfer_tests.cpp,
  tests/CMakeLists.txt

Blender 5.2.2 LTS. The three new room-fitted assemblies have 3,473 / 7,012 /
9,150 triangles respectively, three material batches each. Native opaque
materials and metre UVs reuse the current importer; no texture, shader,
dependency or runtime format was added.

The first game review exposed cable, instrument, turret and post occlusion.
The final pass moved signs to clear wall areas, lowered the cooling approach
sign below its pipe, raised the return arrow above its rail, fitted the airlock
carrier to its pier, and added stand-off mounts in front of two posts.

## Executed verification

- Debug: cmake --build --preset demo-debug --target
  relaycore_power_restoration_graphics relaycore_content_validation -j 2.
- Optimized assets: cmake --build --preset demo-relwithdebinfo --target
  relaycore_demo_assets -j 2.
- Five affected models passed authored BLEND and fresh GLB import inspection:
  the three new assemblies, power_service and power_switchgear. No issues.
- Neutral perspective/front/back/left/right/top views of the common carrier
  were inspected. Runtime views supplement this isolated construction review.
- 27 selected CTest cases covered pressure/cooling/power validation, all shipped
  maps and relaycore.tooling.model_assets. Initially 26 passed and the pressure
  mount test failed because it required each glyph to touch an authored solid.
  The test now checks actual native carrier triangles as an alternative support
  and applies the authored decal scale. It passed after rebuilding.
- Final CTest selection: pressure mounting, all shipped maps,
  graphics.route_signage and graphics.route_signage_sdr — **4/4 passed**, zero
  skips, 31.88 seconds. Both graphics cases cover 14 fixed views in standby and
  powered/open states: **56 final captures** across the two colour pipelines.
- Check Map: all three changed maps valid, warnings empty.
- scripts/check-model-assets.ps1 passed for Debug and RelWithDebInfo; all
  **61 native products** match between profiles, and staged hashes/receipts pass.
- B-17 irradiance was reused only after its current signature was checked by
  the baker: 14708618083590780887.
- Current capture log has Vulkan validation enabled and no error/warning lines.
- Source comparison verifies preserved physical/gameplay content and cue counts.
  git diff --check passed.

CTest selections used:

```text
^validation\.(pressure transfer|cooling loop|power restoration|all shipped maps)|^relaycore\.tooling\.model_assets$
^validation\.(pressure transfer wall equipment|all shipped maps)|^graphics\.route_signage(_sdr)?$
```

The 25 other content/gameplay cases and the model-tooling case were not repeated
after the mounting-only repair; their relevant gameplay/pipeline inputs stayed
unchanged. Final model receipts, imports, map loading and both colour modes were
checked again.

## Visual evidence and remaining acceptance

[Before/after](vis26-before-after.png) shows unchanged cameras for representative
pressure, cooling, airlock and distribution signs. Final views explicitly update
environment state; the baseline capture predates that diagnostic correction, so
the sheet is a structural comparison rather than a pixel-level lighting test.

The native lettering remains clearer than the retained yellow glyphs in the
legacy SDR pipeline. Destination wording, interaction signs and route numbers
were reviewed in fixed views, but this does not establish new-player navigation
quality or acceptance from every moving viewpoint. Duplicate legacy signs/cues
remain until a new player has checked the route, as required by VIS-26.

No archive, release, commit or push was created.

## Owner walkthrough — 2026-10-08

The owner reported completing a walkthrough, then replied “вроде все ок”
when asked about forward/return navigation and unclear or incorrect signs.
This records owner acceptance of the implemented VIS-26 pass. It does not
claim an independent first-time player test. Existing duplicate cues/signs
remain; no runtime content changed as part of this acceptance update.
