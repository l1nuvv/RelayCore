# RC-022f — Production Authored Values Audit

## Scope

The audit covers the shipped TOML configuration and level assets that can influence the visual result of
`legacy_gamma` and `hardware_srgb`:

```text
assets/config/engine.toml
assets/config/rc022_capture.toml
assets/config/rc022_capture_debug.toml
assets/config/rc022_capture_srgb_retuned.toml
assets/config/render_authoring.toml
assets/levels/rc022_reference.toml
assets/levels/rc022_reference_srgb_retuned.toml
assets/levels/stress_room_01.toml
assets/levels/stress_room_02.toml
assets/levels/test_room.toml
```

The deterministic reference assets are evidence for the pipeline and retuning method. The migration decision,
however, is based on the three production-authored levels and the shared configuration/material tables.

## Inventory Summary

### Shared global configuration

`assets/config/engine.toml` owns the production global lighting contract:

- indirect bounce strength;
- fog colour, start, and end;
- directional shadow-light colour and intensity;
- shadow strength and contact strength;
- shadow projection distances and radii.

The deterministic hardware-sRGB retune already demonstrated that several of these values must differ from the
legacy-tuned reference to recover shadow depth and material separation.

### Shared material authoring

`assets/config/render_authoring.toml` contains one shared table with 22 material `base_color` entries plus common
ambient, bounce, and render-light defaults. There is no pipeline-specific material table in the audited assets.

Changing these values in place would change both pipelines because `legacy_gamma` remains the production default.

### Production levels

| Level | Material uses | Tint overrides | Brightness overrides | Local-light radius/intensity pairs |
| --- | ---: | ---: | ---: | ---: |
| `stress_room_01.toml` | 32 | 22 | 22 | 12 |
| `stress_room_02.toml` | 63 | 11 | 11 | 7 |
| `test_room.toml` | 117 | 96 | 96 | 16 |
| **Total** | **212** | **129** | **129** | **35** |

The levels use broad authored ranges rather than a small set of isolated exceptions:

- `tint` values include neutral, cool, warm, sub-unity, and above-one multipliers;
- `brightness` values span deliberately dark surfaces through emissive/highlight probes;
- local lights vary substantially in colour, radius, and intensity;
- glass, hazard, divider, light-strip, platform, panel, and structural materials all receive local overrides.

## Decision

### Rejected: shared retuning

A shared retune would modify the current `legacy_gamma` presentation across all production scenes. The breadth of
inline level overrides also means that changing only global lighting or only the shared material table cannot
produce a controlled migration.

Shared retuning is therefore rejected while `legacy_gamma` remains the production default.

### Selected: pipeline-specific authoring profiles

The migration must support explicit `hardware_srgb` overrides while preserving the current legacy values.

The required overlay has three scopes:

1. **Global render overrides**
   - indirect bounce;
   - fog;
   - directional-light and shadow controls.

2. **Material-profile overrides**
   - visual base colours;
   - emissive/material-specific scalar values where needed.

3. **Level-local overrides**
   - local-light radius/intensity;
   - selected solid `tint` and `brightness`;
   - glass and other scene-specific probes.

The base level remains authoritative for geometry, gameplay entities, identifiers, and unmodified authored values.
The overlay must contain only visual differences.

## Constraints for RC-022g

The implementation must satisfy all of the following:

- `legacy_gamma` loads exactly the current assets and remains the default;
- `hardware_srgb` selects an explicit overlay by startup mode;
- absence of an expected hardware profile is visible in diagnostics and must not silently select unrelated data;
- gameplay geometry and entities are not duplicated;
- overrides are keyed by stable authored identity, not array position;
- unknown, duplicate, or stale override targets fail validation;
- the deterministic reference profile remains a test fixture rather than an automatic production profile;
- renderer colour mathematics and framebuffer verification remain unchanged.

## Next Work

RC-022g should first establish the data model and validation contract. Production value retuning should begin only
after the overlay can be loaded deterministically and tested independently of visual judgment.
