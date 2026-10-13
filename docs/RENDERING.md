# RelayCore — материалы, освещение и вывод изображения

Текущие настройки, authoring и проверки material/HDR/probe paths. GPU ownership и device contract — [ARCHITECTURE](ARCHITECTURE.md), capture/profiling — [DEVELOPMENT](DEVELOPMENT.md#vulkan-capture-и-validation), VK14 milestones/evidence — [renderer-план](RENDERER_MODERNIZATION_PLAN.md).

## Освещение материалов и сглаживание

Local lighting retains up to 32 active sources, ordered by camera distance divided
by radius. The nearest eight keep point shadow maps (six faces each); sources beyond
the shadow budget still contribute direct and volumetric light. Maps exceeding 32
sources emit the existing budget warning and select the nearest 32. This avoids
dropping small visible lights in the current rooms merely because they leave the
eight-source shadow budget. Emissive materials are independent of this selection.

`render.msaa_samples` принимает `1`, `2`, `4`; все три игровые конфигурации
(`engine`, `native_lab_hdr`, `occult_lab_b17`) используют `4`. Отсутствующий ключ
сохраняет `1` для прежних конфигураций и изолированных fixtures. Неподдерживаемый
GPU sample count отклоняется с диагностикой; смена MSAA требует перезагрузки
renderer. Цвет и depth сцены multisampled, shadows остаются single-sample.
Resolve выполняется для мира и viewmodel до HDR tone mapping и UI;
HUD/RmlUi/ImGui остаются в финальном single-sample target.

Metallic/roughness материалы получают мягкую environment specular составляющую:
hemisphere ambient и восстановленные низкочастотные SH radiance из существующих
irradiance probes, с roughness-фильтром и аналитической
[environment BRDF](https://www.unrealengine.com/blog/physically-based-shading-on-mobile).
Это fallback вне authored reflection probe. IMG-02 добавляет статический cubemap
с GGX mip chain и box projection; внутри bounds он заменяет hemisphere/SH specular.
Diffuse probes, прямой GGX, shadows и authored texture data
сохраняются. MSAA сглаживает покрытие геометрии. Опциональные TAA, spatial upscale
и dynamic resolution: [контракт, настройки и проверки VK14-08](renderer/VK14-08.md#selected-contract).

IMG-05 broadens GGX roughness on native normal-mapped PBR surfaces using the
screen-space variance of the mapped normal. The addition to squared GGX alpha is
bounded at .04; flat normals and non-PBR materials retain their existing response.
This filters specular lighting and reflection LOD, without filtering base color,
text or UI and without storing frame history. Matched 48-frame camera arcs are
captured by `relaycore_containment_loop_graphics --hdr --motion` (omit `--hdr` for SDR).

Проверки: `graphics.msaa` (1/2/4×, HDR/SDR, диагональное покрытие, viewmodel,
resize и settled allocations), `graphics.hdr_pipeline`, `graphics.irradiance_probes`,
`graphics.player_ui`, `graphics.containment_loop(_hdr)` и engine config cases.
[Одинаковые игровые кадры до/после](art/g019-lighting-aa-before-after.png).


## HDR scene rendering with SDR output — N3.1

В игровом меню и паузе: «Настройки» → «Режим изображения» → SDR / HDR.
Выбор применяется сразу и сохраняется вместе с настройками игрока; перезапуск
и F7 его сохраняют. Старые settings без `hdr_enabled` наследуют startup config.
При включении HDR меню само выбирает hardware_srgb и подготавливает сцены игры,
редактора и сохранённой editor-return сессии; аргументы запуска не нужны.
При возврате в SDR восстанавливается цветовой путь, выбранный при запуске, и
отключается отдельный FP16/fullscreen pass. Игра, редактор и editor-return сцены подготавливаются
заново при смене color pipeline. Явный startup hardware_srgb также сохраняется.
Ошибка подготовки сохраняет рабочий режим и отображается в меню.

`assets/config/native_lab_hdr.toml` явно включает `render.hdr_enabled = true` и
`exposure_ev = 0`. HDR требует `hardware_srgb` (теперь default запуска); с `legacy_gamma`
startup завершается понятной ошибкой. Default `engine.toml` сохраняет SDR;
обычный запуск теперь выбирает `hardware_srgb`, а `--color-pipeline legacy_gamma`
остаётся явным режимом совместимости. Exposure задаётся в stops: +1 удваивает linear radiance; допустимы
finite значения [-10,10]. Это ручная экспозиция, auto-exposure не добавлен.

```powershell
./build/msvc-debug/src/relaygame/RelayCore.exe --config assets/config/native_lab_hdr.toml --color-pipeline hardware_srgb
```

World/transparent/viewmodel пишутся в `R16G16B16A16_SFLOAT`; lighting/emissive не
обрезаются до 1. Прозрачность смешивается в linear space. Отдельный fullscreen pass
применяет exposure и тот же highlight shoulder, что линейный SDR, затем
существующий postfx в display brightness space (contrast/saturation/grain),
затем возвращает linear RGB и пишет
в sRGB attachment над RGBA8 UNORM storage. UI загружается после tone mapping через
UNORM view в прежних display-space semantics; exposure/postfx не затрагивают его.
Readback/presentation используют encoded RGBA8 без второй sRGB-конверсии. HDR здесь
относится к scene radiance; swapchain/display остаётся SDR.

SDR сохраняет свой lighting path, но плавно сжимает значения выше 0.65 до postfx,
вместо обрезания lighting до 1: яркие поверхности сохраняют градации и детали.
Общий shoulder оставляет значения до 0.65 неизменными, непрерывно по значению и
производной сжимает более яркие значения к 1. HDR больше не добавляет ACES toe и
скрытый множитель 0.6, затемнявшие слабые сигналы; exposure 0 остаётся штатным.
В SDR classic hardware-sRGB и MR postfx также работают после linear → sRGB,
с обратным преобразованием перед sRGB attachment. Legacy classic сохраняет
encoded lighting и UNORM attachment; его арифметика не служит linear reference.

Ambient и `local_light_gain` — скалярные коэффициенты освещения, а не sRGB-цвета;
они одинаковы в SDR/HDR. Их прежнее HDR-only декодирование удалено: оно затемняло
поверхности вне прямого света. Postfx contrast применяется после linear → display
conversion, чтобы не обрезать слабую linear radiance. `graphics.hdr_pipeline`
проверяет тёмные значения и diffuse hemisphere для classic/MR материалов.
`graphics.vulkan_color_baseline` проверяет разделение ярких SDR-значений выше 1,
`graphics.hdr_pipeline` — разделение теней/полутонов и ярких diffuse-значений.
`graphics.hdr_settings` запускает эпизод и сравнивает enabled/disabled shadows
при одинаковой камере; пустое изображение без заметного влияния теней считается ошибкой.
HDR должен сохранять минимум половину суммарного hardware-sRGB SDR shadow contrast.
Legacy SDR записывается отдельно, его lighting arithmetic отличается.
`graphics.containment_color` (`relaycore_containment_loop_graphics --color-audit`)
сравнивает три фиксированных вида G-019 в hardware-sRGB SDR/HDR при EV 0 и
проверяет HDR EV −1/0/+1 через prepared reload. Метрики печатаются как `color_view`,
кадры — `containment-captures/color-*-ev*-hdr.bmp`; это encoded SDR readback.
Captures: `hdr-settings-shadow-{SDR,HDR,hardware-SDR}.bmp`,
`hdr-settings-{door,fan,ceiling}.bmp` в test working directory.

Путь вывода во всех режимах — `B8G8R8A8_UNORM`/`R8G8B8A8_UNORM` с
`VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`. HDR10/PQ/scRGB presentation, BT.2020 и HDR
metadata отсутствуют. Включение Windows HDR оставляет RelayCore SDR-приложением;
DWM выполняет системное преобразование и SDR white mapping. Дополнительная
компенсация white level в renderer не нужна. См. [Microsoft Advanced Color](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range)
и [численный аудит цветового пути](color_pipeline_baseline.md#vulkan-sdrhdr-audit--2026-10-08).

HDR format attachment/blend/sampling support проверяется при startup. Изменение
`hdr_enabled` требует prepared backend reload; exposure можно обновлять через
`set_config`. Shader reload атомарно включает mesh/shadow/tone pipelines; resize
пересоздаёт обе color images и descriptor после завершения GPU work. Synchronous
ownership и submission policy сохранены. Fullscreen shader использует
[`SV_VulkanVertexID`](https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/a2-01-spirv-target-specific.html#using-sv_instanceid-and-sv_vertexid-with-spir-v-target),
поэтому новые device feature bits для DrawParameters не нужны.

```powershell
ctest --preset graphics-debug -R '^graphics\.(hdr_pipeline|hdr_episode)$' --output-on-failure --no-tests=error
ctest --preset graphics-debug -R '^graphics\.(hdr_settings|game_lifecycle)$' --output-on-failure --no-tests=error
# Из build/msvc-debug/tests, RELAYCORE_ASSET_ROOT — staged assets:
./relaycore_vulkan_mesh_tests.exe --profile-native-hdr
```

Irradiance `.rcprobe` и статический reflection `.rcreflection` уже используются;
их bake/runtime contracts описаны выше и в разделе Static reflection probes.


## Spatial quality effects — VK14-07

Bloom, SSAO, SSR and homogeneous volumetric lighting are optional renderer
effects, disabled by default and selectable in the saved player settings.
Bloom and SSR use the internal HDR path; output remains SDR. Selecting SDR
disables those two effects. SSAO combines with authored AO on indirect light,
SSR retains reflection probes on rejected/missing hits, and volumetrics replace
opaque distance fog. Player/developer UI is composed after the effects.
The authored controls, resource/pass contract, diagnostic views, captures,
budgets and limitations belong to [VK14-07](renderer/VK14-07.md).

## Advanced lighting and lens effects — VK14-09

Optional `[render]` controls `ssgi_quality`, `reflection_quality`, `dof_quality`
and `motion_blur_quality` accept integer 0/1/2. All default to 0. For reflections,
0 retains the existing SSR technique; 1/2 select roughness cone sampling and
require `ssr_enabled`. SSGI and lens effects require internal HDR/hardware sRGB.
Player settings persist these qualities and the menu applies them through a
prepared renderer reload. Selecting SDR disables them; disabling SSR resets its
quality. `dof_focus_distance` is authored in world units, default 8, range
[0.25,100]; quality changes require reload, focus changes reset history.

SSGI replaces the covered fraction of ambient/SH diffuse using four/six
cosine-weighted rays up to three world units, a half-resolution history and
bilateral reconstruction. The source contains current opaque radiance without
SSGI/SSR feedback or volumetrics. Misses retain ambient/probes; material and
screen AO are combined by minimum and applied once to indirect illumination.
Geometry/eligibility rejection, current radiance/coverage clipping and the
existing history-reset contract bound temporal reuse. It cannot recover hidden
or off-screen light and does not replace baked irradiance.

Reflections retain probes for missing/unreliable hits; low/high use two/four
deterministic cone rays. Existing optional TAA filters their final composition.
DoF and motion blur run after TAA, before bloom/tone mapping, with four/eight
depth/normal/motion-compatible taps. DoF radius is capped at three/six internal
pixels; half-frame shutter motion at six/twelve. Invalid history/unknown motion,
transparent/overlay/weapon pixels retain the sharp source. UI is appended at
native resolution afterwards. Small kernels deliberately preserve depth edges
instead of producing cinematic foreground bokeh. Controls default off pending
owner visual preference; [execution evidence](renderer/VK14-09.md) owns local
budgets, checks, captures and limitations.

## Static reflection probes (IMG-02)

The engine accepts one `reflection_probe` per level. Its `origin` is the capture
position; `mins`/`maxs` define world-space influence and box projection. Required
`resolution` is a power of two from 8 to 256, `samples` is 32–1024, and `product`
is a relative `.rcreflection` asset path. The first authored consumer is the central
containment hall. Other levels use SH/ambient specular until a probe is authored
and declared in `cmake/RelayCoreReflectionAssets.cmake`.

`relaycore_reflection_bake level.toml render_authoring.toml output.rcreflection`
uses prepared static opaque geometry, mean linear diffuse reflectance,
unconditional local lights and ambient floor. Six linear radiance faces are GGX
prefiltered into a complete roughness mip chain. Dynamic actors/doors, conditional
lights, transparent geometry, directional light, emission maps and recursive
specular transport are excluded. It is a coarse static room snapshot.

RREF v1 stores position/bounds, resolution/sample count, a canonical input signature
and mip-major +X,-X,+Y,-Y,+Z,-Z RGBA32F radiance. The GPU uploads authored mips
without ordinary box-filter regeneration. The baker reuses matching inputs and
settings; runtime rejects stale products. `relaycore_reflection_assets` generates
under the native asset root; `relaycore_demo_assets` stages the product, and the
existing `runtime-assets.txt` manifest owns its path.

Verification: `graphics.material_maps_reflections`, `graphics.vulkan_upload`,
`graphics.irradiance_probes`, `graphics.hdr_pipeline`, containment HDR/SDR and native
material/import/reflection cases. [Source and visual evidence](art/img01-03-materials-reflections-ao.md).
