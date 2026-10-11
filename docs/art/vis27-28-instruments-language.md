# VIS-27–28 — instrument content and language

2026-10-08. Implemented at the owner's request; verification results below.

The CRT keeps a two-channel C2/R0 waveform with large Russian function labels,
scale units and a sample designation. The control console uses an electrical
distribution reference schematic. The surveillance monitor uses a retained B-17
observation frame marked АРХИВ / КАДР 014. These opaque albedos use the existing
material pipeline; no emission or live-display system is necessary.

Pressure and cooling have distinct matte printed circuit legends below the
interactive terminal and between the existing cabinet hinges. Their large
function names, supply/return paths and units distinguish the instruments;
the existing analog gauges retain ownership of readings. Cooling uses a local
variant of the pressure casing, preserving its placement and physical content.
The existing cooling-specific header remains in front of its generic base label.

The language standard and exact equipment/route terminology are authoritative
in [ART_DIRECTION.md](../ART_DIRECTION.md#язык-учреждения-и-терминология-vis-28).
Russian is mandatory for gameplay instructions; the existing English institutional
signage is intentional. Route numbers and equipment IDs are separate namespaces.
The seven maps' labels, locked/denied/success messages and procedure steps were
checked against actual target chains; no language or numbering correction was
needed in their gameplay data. VIS-26 navigation and VIS-25 procedures are retained.

Sources: scripts/build-instrument-graphics.cjs and
scripts/export-instrument-graphics-glb.py; content/graphics/instruments;
crt_terminal, control_console, surveillance_monitor, pressure_control_casing and
cooling_control_casing, electrical_notes_board, pressure_notes_board, cooling_notes_board and b17_observation_camera under content/models. The original lab and Poly Haven
exporters now reference the differentiated textures. After rebuilding a base
pressure casing, run the instrument exporter to apply both printed legends.
The instrument exporter is repeatable and preserves the authored geometry;
only one printed quad per casing is added; reference boards reuse their original geometry. Run the graphics generator with the existing
sharp dependency in NODE_PATH, then Blender with the existing export flags.

observation_frame.png is an original RelayCore runtime B-17 capture retained from
build/msvc-debug/tests/power-captures/hdr/shadows/b17-laboratory.bmp, cropped and
converted to monochrome. It is an archived world image, not a live camera feed.
SVG/PNG masters and editable packed BLEND sources are retained.

## Verification

Focused graphics.instruments / graphics.instruments_sdr capture sixteen fixed views per colour mode (32 total)
covering all five functions at approximately 1 and 3 metres, all seven playable maps and the physical camera/source view. Test cameras and
CTest registrations are in tests/power_restoration_graphics.cpp and tests/CMakeLists.txt.
Working baselines, export/build/check logs and inspection reports: build/vis27-28.
Executed 2026-10-08:

- Debug build: relaycore_demo, relaycore_power_restoration_graphics,
  relaycore_content_validation and relaycore_game_lifecycle passed.
- 42/42 focused CTest checks passed, including collision-aware chapter routes,
  the new campaign topology/save check, real DemoGame campaign transitions,
  both instrument colour modes and model-pipeline regressions. No skips.
- 5/5 affected checks passed after capture refinements; 3/3 passed after final
  first-map board placement (all shipped maps and both instrument colour modes).
- All 66 compiled/staged model receipts match current sources, importer and policy.
  All seven level-check reports are valid with zero authoring warnings.
- Eight authored/fresh-import models preserve baseline bounds and triangle counts;
  each casing adds only its two printed-surface triangles. The camera passes the
  native importer and was reviewed both in neutral views and in the actual room.
- Sixteen captures per colour mode were executed and visually reviewed (32 total).
  The instrument check rejects skipped views. Paper circuits, phosphor screens,
  clear reference-board mounting, CAM 02 and its source view were inspected.
  Final executed graphics logs contain no Vulkan validation warnings/errors or
  shutdown failures.

The B-17 lighting product was computed with the existing optimized baker and
accepted by the current Debug baker as an exact input match:
18 probes, signature 11915785073291801243. This is tool reuse, not an optimized
full-build or performance claim. Logs, JUnit results, geometry/content audits,
level reports and final-verification.json are retained in build/vis27-28;
full captures are in build/msvc-debug/tests/power-captures/{hdr,legacy}/shadows.
The final first-map cooling reference is below its existing pump route sign,
so neither the drawing nor the retained navigation is occluded.

These are automated gameplay checks and local visual review, not a human
end-to-end playthrough, a new GPU test or a performance comparison. Existing
external playtest gates remain open.

## Owner follow-up: physical camera, reuse and connected chapters

The owner explicitly requested the physical source camera, use of the instrument
graphics on other maps and continuous progression through the existing chapters.
CAM 02 is a compact analog box camera on a supported tripod at the original
archived-frame pose: eye (-0.7, 1.65, -3.4), target (2, 1.4, 1.2), vertical FOV 60°.
The durable pose is content/graphics/instruments/observation_camera.json and its
fitted source/export is scripts/export-observation-camera-glb.py. A tripod fits
the source point inside the laboratory; moving it to a wall would change the photo.
The archived frame and camera therefore refer to the same optical location and view.

Instrument coverage follows room function, avoiding duplicated whole instrument sets:

| Map | Instruments / reference graphics |
| --- | --- |
| power_restoration | Electrical console; printed cooling reference by the pump |
| foundation_intake | Printed electrical reference in the workshop |
| foundation_pump | Printed pressure reference by the pump approach |
| bench_relay | Waveform CRT and printed electrical reference |
| pressure_transfer | Analog gauges and printed pressure circuit |
| cooling_loop | Analog gauges and printed supply/return temperature and differential circuit |
| occult_lab_b17 | Waveform CRT, electrical console, archived observation monitors and physical CAM 02 |

The three new reference-board variants reuse the existing construction and staging.
Generic circuit legends carry no foreign sector ID; the existing local plates own
equipment identification. The waveform has a large primary 50 Hz value and a
period matching 10 ms/div. Printed electrical artwork has a paper/ink variant;
CRT artwork keeps the dark phosphor field.

The seven-map route is owned as G-018 in PLAYABLE_SLICE_ROADMAP.md. Existing
completion-gated level_exit and level_entry entities implement the chain; no new
campaign format or runtime framework was added. The default menu option names
the full campaign, and selecting a later chapter starts there and continues onward.
Only B-17 ends the connected route; the two-sector return link remains available.
Six forward transitions and safe arrival poses use the existing progress/history
mechanism. Existing chapter ending markers remain for source compatibility, but
completion exits suppress their intermediate finale through existing runtime logic.

tests/campaign_route_tests.cpp verifies forward topology/gating, entries and seven
disk snapshots. graphics.campaign_route exercises real DemoGame transitions,
continue after every transition, carried health/inventory/weapon/flashlight/time,
absence of arrival bounce and final B-17 completion. The transition check seeds
progress and invokes transitions; it does not claim a human walkthrough or replace
the existing collision-aware chapter gameplay tests.
