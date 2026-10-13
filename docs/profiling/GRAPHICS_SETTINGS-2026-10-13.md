# Графические настройки — замер 1440p, 2026-10-13

## Follow-up: distance lighting and reported spikes

The owner's Play/max-settings screenshots showed the small red source disappearing.
World selection capped all lighting at eight camera-ranked sources. Direct/volume
lighting now retains 32 sources while the point-shadow budget remains eight; the
contract is in [RENDERING](../RENDERING.md#освещение-материалов-и-сглаживание).
`graphics.local_light_selection` passed in `msvc-dev`: a tenth GPU light contributes,
target deactivation clears it, and the authored pentagram source contributes at both
camera z=-1 and z=4. `graphics.point_shadow_receiver` passed; validation was false.
`cmake --build --preset demo-dev` and `git diff --check` passed.

A fresh before-patch `--graphics-settings-profile` retained 200 warm samples per
mode/view at 2560x1440. Entrance combined GPU p50/p95 was 11.141/11.767 ms;
maintenance 15.183/16.018 ms; corridor 13.696/14.549 ms. Evidence:
`build/light-distance-2026-10-13/before/{workload.log,summary.json,vulkan-mesh-captures/graphics-settings-profile.csv}`.
This fixture excludes gameplay/presentation and the newer advanced quality controls.
After the owner requested a quicker handoff, no after-patch timing or long live-game
capture was run. The isolated FPS spikes remain unexplained; no speedup is claimed.

По запросу владельца после падения с запомнившихся ~400 FPS до 48 FPS
при включении новых эффектов. Измерения подтверждают большую стоимость
SSR и объёмного света на GPU. Полное время игрового кадра 18–21 мс
со скриншота этим сценовым workload не воспроизведено.

## Условия и метод

- База исходников `e53ac57`, единственное изменение executable — новый
  `--graphics-settings-profile` в `tests/vulkan_mesh_tests.cpp`.
  Runtime/shaders и пользовательские settings/progress не изменены.
- Windows, NVIDIA RTX 4060 Ti; loader 1.4.357, GPU API 1.4.329,
  Vulkan driver field `2500608000`; validation=false.
- Оптимизированный `msvc-dev` (RelWithDebInfo, /O2, Tracy enabled),
  staged assets `build/msvc-dev/src/relaygame/assets`.
  Hardware sRGB, internal HDR в соответствующих вариантах,
  authored lights/probes/shadows, pistol viewmodel, MSAA 4 кроме отдельного 1×.
- 2560×1440; нет swapchain/present/VSync, HUD, ImGui, audio или simulation.
  Карта `power_restoration` подготовлена один раз. Движение, бои,
  состояние objective 4/9 и пользовательские открытые двери не воспроизводятся.
- На вариант 240 кадров, первые 40 исключены; 200 warm CPU/GPU samples.
  CSV и readback/BMP записываются после измеряемого цикла.
  OS/driver caches не очищались. Сборки, другая игра и GPU tests
  не запускались одновременно с измерением; прочие desktop apps не изолированы.
- CPU — wall time `prepare_frame`, включая подготовку/submission и ожидание
  двух scene slots; это стоимость steady throughput, не изолированная CPU работа.
  GPU — существующая пара Vulkan timestamps от начала до конца scene commands,
  прочитанная после завершения предыдущего slot. CPU и GPU не складываются.
- Чтобы получить timestamps без Tracy, HDR-варианты включают resolution queries
  через dynamic mode с target=50 мс. Масштаб проверяется каждый кадр и остаётся
  строго заданным; downscale/переаллокаций нет. Это диагностический способ
  измерения fixed resolution, а не тест обычного dynamic-resolution controller.
  Для SDR GPU timestamps отсутствуют; пустое поле CSV не означает нулевую стоимость.
- p50 — медиана, p95/p99 — nearest rank. 200 кадров достаточны для этого
  сравнения, но не устанавливают длительные игровые tails или другой-GPU бюджет.

Камеры `(position → target)`:
`entrance (0,1.65,-3.5) → (0,1.65,4)`;
`maintenance (14.3,1.65,20.2) → (14.3,1.65,34)` смотрит на рубильник;
`maintenance_corridor (13,1.65,34) → (13,1.65,16)` смотрит вдоль прохода.
Последний BMP визуально проверен: это близкий участок, не точная камера скриншота.

## Результат повторного запуска

Все числа — мс. Таблица содержит GPU p50 для всех трёх видов; крайний столбец
сохраняет p95/p99 для коридора. `HDR` означает остальные новые эффекты off.
`Все` — bloom + AO + SSR + volumetrics + TAA. Вариант 75% рендерит 1920×1080,
выход остаётся 2560×1440.

| Настройки | Вход p50 | Рубильник p50 | Коридор p50 | Коридор p95 / p99 |
| --- | ---: | ---: | ---: | ---: |
| HDR | 2.376 | 3.126 | 3.557 | 4.089 / 4.390 |
| HDR + bloom | 2.349 | 3.057 | 3.116 | 4.037 / 4.263 |
| HDR + AO | 3.747 | 4.744 | 4.834 | 5.672 / 6.202 |
| HDR + SSR | 5.651 | 8.938 | 7.929 | 8.691 / 9.121 |
| HDR + объёмный свет | 6.495 | 8.069 | 8.371 | 9.344 / 9.800 |
| HDR + TAA | 4.204 | 5.033 | 5.340 | 6.196 / 6.454 |
| Все без TAA | 10.275 | 14.283 | 13.051 | 15.280 / 16.192 |
| Все | 11.595 | 14.742 | 13.912 | 14.952 / 15.986 |
| Все без объёмного света | 7.573 | 10.812 | 9.279 | 10.326 / 10.820 |
| Все без SSR | 9.174 | 10.602 | 10.489 | 11.337 / 11.930 |
| Все, scale 75% | 6.813 | 9.671 | 8.551 | 9.716 / 10.369 |
| Все, MSAA 1× | 9.908 | 15.572 | 12.319 | 13.493 / 14.010 |

SDR/off CPU p50/p95: вход **2.055 / 2.434**, рубильник **3.021 / 3.576**,
коридор **2.884 / 3.675**. Для всех эффектов CPU p50/p95 в коридоре
**13.907 / 14.969**, GPU max **16.696**.

В коридоре отключение объёмного света уменьшило GPU p50 на 4.633 мс,
SSR — на 3.423 мс; scale 75% — на 5.361 мс. Это сравнения вариантов,
не независимые pass costs, и их нельзя суммировать. Bloom находится около
baseline в пределах разброса; отрицательная разница не означает ускорения от bloom.
MSAA 1× не дал устойчивого выигрыша на всех видах, поэтому считать его
основным решением нельзя. Эффекты остаются дорогими для бюджета 4.17 мс / 240 Гц.

Первый запуск включал только вход и рубильник. Вход repeat GPU p50:
HDR без остальных эффектов **2.249 → 2.376**, все **11.784 → 11.595**.
У рубильника все **25.987 → 14.742**, p95 **33.279 → 15.521**:
этот большой межзапусковый разброс не объяснён и не скрыт выбором второго запуска.
Результат первого запуска не доказывает отдельную регрессию TAA/MSAA;
для точной причины худшего интервала нужны clocks/scheduling или live-game capture.
Новые эффекты объясняют значительную часть просадки, но точное соответствие
48 FPS и вклад gameplay/UI/presentation остаются неизмеренными.

## Воспроизведение и evidence

```powershell
cmake --build --preset demo-dev --target relaycore_vulkan_mesh_tests -j 4
$env:RELAYCORE_ASSET_ROOT = (Resolve-Path build/msvc-dev/src/relaygame/assets).Path
$env:DISABLE_VULKAN_OBS_CAPTURE = '1'
# Запустить из отдельной папки для сохранения CSV/BMP, без других GPU workloads:
build/msvc-dev/tests/relaycore_vulkan_mesh_tests.exe --graphics-settings-profile
ctest --test-dir build/msvc-dev -R '^graphics.visual_effects$' --output-on-failure --no-tests=error
```

Локальные данные: `build/fps-2026-10-13/settings-run-{1,2}/` содержат
`workload.log`, `summary.json`, `vulkan-mesh-captures/graphics-settings-profile.csv`
и BMP каждого варианта. Анализ: `build/fps-2026-10-13/analyze-settings.py`.
Raw CSV сохраняет все 240 кадров, включая first-use; summary исключает 40.
Tracy TCP capture не подключился ни к 8086, ни к обнаруженному listener 8087;
свежих Tracy CPU/pass zones нет. Прямые timestamps успешно измерены.

Повторный executable SHA-256:
`FAF4BE5289F09B69A3C881B676D61C30ABAF42470B53E4F534EA4F9B93EE80D3`;
staged engine.toml:
`954AC433F9506F053D2DDCE0596AA8C7F16C849D9D91A1D0E0CEA04A52978241`.
Target build pass, оба workload pass, существующий `graphics.visual_effects`
**1/1 pass, без skip**; `git diff --check` pass. Пакет не создавался.
