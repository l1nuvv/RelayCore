# RC-022g1 — Pipeline-Specific Authoring Overlay Contract

Historical RC-022 reference. Original measurements, authored values and GL-era commands below retain their original scope; they are not current Vulkan verification evidence. Use [DEVELOPMENT.md](DEVELOPMENT.md) for current commands and [the Vulkan SDR/HDR audit](color_pipeline_baseline.md#vulkan-sdrhdr-audit--2026-10-08) for the later colour-path measurements.

## Scope

RC-022g1 establishes the data model, strict TOML parser, validation rules, and pure application helpers for a
pipeline-specific visual authoring overlay. It deliberately does not select or apply an overlay in the runtime.

The legacy asset path and startup default remain unchanged.

## Schema

```toml
[global]
indirect_bounce_strength = 0.12
fog_enabled = true
fog_color = [0.08, 0.09, 0.11]
fog_start = 16.0
fog_end = 42.0
shadow_light_color = [0.46, 0.50, 0.54]
shadow_light_intensity = 1.00
shadow_strength = 0.80
shadow_contact_strength = 0.10

[[materials]]
name = "glass"
base_color = [0.40, 0.50, 0.60]
ambient_bias = 0.90
bounce_mix = 0.25

[[levels]]
path = "levels/test_room.toml"

[[levels.solids]]
id = "relay_hall.glass_probe"
tint = [0.90, 1.00, 1.10]
brightness = 0.82

[[levels.render_lights]]
id = "relay_hall.key_light"
color = [0.20, 0.34, 0.42]
radius = 4.4
intensity = 0.68
```

## Validation contract

- the overlay file is mandatory when explicitly selected;
- an empty overlay is rejected;
- duplicate material entries are rejected;
- duplicate normalized level paths are rejected;
- duplicate solid or render-light IDs inside a level are rejected;
- unknown material names are rejected;
- invalid types, non-finite values, and invalid ranges are rejected;
- level paths must be relative asset paths;
- empty per-entry overrides are rejected;
- level-local targets are keyed by stable authored IDs, never by array position;
- unknown or stale target IDs fail validation.

## RC-022g1 boundary

RC-022g1 does not yet:

- add stable IDs to shipped level authoring;
- extract identity indices from loaded levels;
- select an overlay from `hardware_srgb` startup mode;
- mutate a loaded `BlockoutLevel`;
- ship production retuning values.

Those responsibilities belong to subsequent RC-022g increments. This boundary keeps the parser and validation
contract independently testable before runtime integration.
