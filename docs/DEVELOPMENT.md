# Разработка RelayCore

Этот файл владеет окружением, запуском, узкой верификацией и диагностикой. Текущие статусы находятся в [roadmap](PLAYABLE_SLICE_ROADMAP.md) и [renderer-плане](RENDERER_MODERNIZATION_PLAN.md).

| Задача | Раздел |
| --- | --- |
| Первичная настройка / CLion | [Сборка с нуля](#сборка-с-нуля) |
| Выбор build/tests | [Узкие проверки](#узкие-проверки), [автоматические проверки](#контракт-автоматических-проверок) |
| CPU/GPU стоимость | [Профилирование](#профилирование) |
| Vulkan capture / validation | [Диагностика](#vulkan-capture-и-validation) |
| GLB/native asset production | [MODEL_ASSETS](MODEL_ASSETS.md) |
| Material/HDR/probe authoring | [RENDERING](RENDERING.md) |
| RML/RCSS и UI | [Игровой UI](#игровой-ui-rmlui-63) |
| Сценарии и level authoring | [Полигон](#испытательный-полигон), [runtime objects](#где-объекты-во-время-игры), [editor/campaign](EDITOR_AND_CAMPAIGN.md) |

## Сборка с нуля

Нужны Windows, Visual Studio с C++ workload и Windows SDK, CMake 4+, Ninja,
CLion (если нужен) и vcpkg. Репозиторий содержит `vcpkg.json`, поэтому зависимости
устанавливаются при настройке CMake. Перед настройкой откройте терминал Visual
Studio Developer PowerShell либо запустите `VsDevCmd.bat` для x64.

```powershell
$env:VCPKG_ROOT = 'C:\путь\к\vcpkg'
Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --preset msvc-debug
cmake --build --preset demo-debug
```

`Test-Path` должен вернуть `True`. Debug используется для отладки и validation.
Для повседневных правок, игры и плейтеста выполните configure `msvc-dev` и
`cmake --build --preset demo-dev`, затем запустите
`build/msvc-dev/src/relaygame/RelayCore.exe` или **RelayCore — Play** в CLion.
Этот профиль сохраняет `/O2`, символы и Tracy, использует PCH и отдельные `.cpp`,
отключает IPO/LTO и разрешает incremental linking. Первый переход требует полной
сборки нового дерева; следующие правки используют его инкрементально.
`msvc-relwithdebinfo` остаётся профилем измерения производительности с IPO и Unity,
а `msvc-release` — финальной сборкой. Для проверки кода профиля разработки есть
`tests-dev` / `test-dev` и `validation-dev` (build/test presets).
Assets по умолчанию находятся рядом с executable; `RELAYCORE_ASSET_ROOT` задаёт
абсолютный override. Для renderer он должен включать собранные SPIR-V.

Для отдельного прохода `RELAYCORE_USER_DATA_ROOT` задаёт абсолютный каталог
игровых progress/settings; пустой или относительный путь — ошибка запуска.
Без override используется прежний каталог SDL. Конфигурация CLion
**RelayCore — G-019 Playtest** использует `build/g019/player-e`, сохраняя обычный
пользовательский progress. [Протокол G-019](playtests/G-019-2026-10-09.md)
описывает новый проход и Continue без удаления существующих сохранений.

### CLion

Откройте корень репозитория как проект CMake. Выберите профиль **MSVC Debug**
(`msvc-debug`) из `CMakePresets.json` и toolchain Visual Studio. Укажите
`VCPKG_ROOT` в окружении CMake и убедитесь, что `cl.exe` и `rc.exe` доступны CLion.
Создайте **CMake Application** с target `relaycore_demo` и рабочим каталогом корня
репозитория. Исполняемый файл называется `RelayCore.exe`. Не выбирайте
`relaycore_demo_assets`: этот target собирает и синхронизирует ресурсы, но не запускает игру.

### Видимые элементы светильников

`render_light.fixture_width` / `fixture_depth` задают размер светящейся плоскости
в метрах, `fixture_offset` — её высоту относительно источника, `angles[0]` —
поворот вокруг +Y. Корпус остаётся отдельной статической моделью или authored solid.
Плоскость смотрит вниз; её размеры и высота должны соответствовать внутреннему
проёму корпуса. `fixture_width = 0` оставляет источник без плоскости (например,
локальный свет от аппаратуры B-17). Без явной ширины сохраняется прежний режим:
0.56 × 0.18 м для switched/damaged ламп, без плоскости для обычного источника.
Видимый элемент использует ту же `strength`, что и освещение, включая startup,
`required_target`, `stop_target` и damaged flicker; нулевой уровень не emissive.
Не оставлять на том же месте безусловный `LightStrip` solid.

### Узкие проверки

Проверки коммита и автоматизации выпуска входят в CTest; для узкой диагностики их можно запускать непосредственно:

```powershell
pwsh -NoProfile -File tests/pre_commit_tests.ps1
pwsh -NoProfile -File tests/release_workflow_tests.ps1
```

Первый тест использует временный Git-репозиторий, второй — тестовую копию с имитацией сборки и публикации. Они не меняют индекс проекта и не загружают выпуск на GitHub. Pre-commit проверяет staged-содержимое: после исправления всех перечисленных файлов включите изменения в коммит через CLion.

```powershell
cmake --build --preset tests-debug
ctest --preset test-debug --output-on-failure --no-tests=error
cmake --build --preset validation-debug
ctest --preset validation-debug --output-on-failure --no-tests=error
# Если затронут lifecycle игры:
cmake --build --preset demo-debug --target relaycore_game_lifecycle
ctest --preset graphics-debug -R '^graphics\.game_lifecycle$' --output-on-failure --no-tests=error
```

Для графического теста нужны Vulkan 1.4 и поддерживаемые device contract возможности. Дополнительные пресеты и полный
порядок проверок перечислены в корневом README; их не требуется запускать после
каждого редактирования карты.

### Контракт автоматических проверок

На Windows `tests-debug` собирает unit/audio targets, а через зависимости — physics,
updater и CLI-проверку карт. `test-debug` запускает также tooling-проверки hooks,
release scripts/workflow, level/model/collision/irradiance assets. Нужны PowerShell 7
и `clang-format`; CMake добавляет найденный formatter в PATH pre-commit теста.
Release-тесты используют изолированные fixtures и имитацию публикации. Дополнительная
Java-проверка интерпретатора CLion остаётся ручной:
`pwsh -NoProfile -File tests/release_workflow_tests.ps1 -Java <java.exe>`.

`graphics-debug` build preset собирает базовые window/device/presentation/upload/mesh
и основные игровые targets. Одноимённый test preset выбирает все зарегистрированные
`graphics.*`, включая отдельные player UI и containment executables. Для полной
выборки дополнительно соберите `relaycore_player_ui_graphics` и
`relaycore_containment_loop_graphics`; для узкой — executable выбранного теста.
Проверьте выборку `ctest --preset graphics-debug -N` до запуска. Сборка базового
пресета сама по себе не доказывает готовность каждого зарегистрированного executable.

В `.github/workflows/ci.yml` обычные GitHub-hosted jobs проверяют сборку, unit/content/tooling и компиляцию graphics targets. Это не подтверждает работу GPU. Полное выполнение `graphics.*` запускается через `workflow_dispatch` с `run_graphics=true` на заранее настроенном runner с метками `self-hosted`, `Windows`, `X64`, `relaycore-graphics`. На нём нужны рабочий интерактивный Windows desktop, драйвер Vulkan, Vulkan SDK с validation layers и build tools. Метки не создают runner: зарегистрируйте подходящую машину перед запуском job.

Graphics job выполняет configure/build с validation=ON, проверяет loader/layers через существующий `relaycore_vulkan_toolchain_check`, затем результат CTest/JUnit: report должен содержать хотя бы один test case, без failure/error/skipped. Количество тестов не фиксировано. Доступность features, создание instance/device, presentation, uploads и validation проверяются самими Vulkan-тестами. Локальная полная выборка:

```powershell
cmake --build --preset graphics-debug
cmake --build --preset graphics-debug --target relaycore_player_ui_graphics relaycore_containment_loop_graphics
ctest --preset graphics-debug --no-tests=error --output-junit graphics-results.xml
```

После CTest проверьте отсутствие skip в report. Текущий CI job также использует базовый build preset; на чистом дереве его одного недостаточно для двух отдельных targets выше. Удалённый graphics gate подтверждается фактическим report, а не локальной сборкой или названием workflow.

### Профилирование

Профили `msvc-dev` и `msvc-relwithdebinfo` включают Tracy. Для сравнимых с прежними
измерений используйте `msvc-relwithdebinfo` или **RelayCore — Profile** в CLion.

Для обычной игры в CLion выбирайте **RelayCore — Play**: эта конфигурация собирает `relaycore_demo` с `msvc-dev` и использует runtime assets рядом с executable. Debug/ASan оставлены для отладки и Vulkan core/synchronization validation; их FPS не характеризует игровую сборку. Локальное сравнение после VK-013, RTX 4060 Ti, 1280×720, VSync off, один scripted entrance: Debug p50 42,79 мс / p95 44,16 мс, RelWithDebInfo p50 1,63 мс / p95 2,57 мс. Captures: `build/vk-013-regression-{debug-before,before}.tracy`; полный workload — `build/vk-013-regression-episode-{debug-before,before}.log`. Это сравнение конфигураций, не ускорение renderer патчем и не замер нового `msvc-dev`.

Для сравнимого захвата:

```powershell
cmake --preset msvc-relwithdebinfo
cmake --build --preset demo-relwithdebinfo
```

Запустите собранный `build/msvc-relwithdebinfo/src/relaygame/RelayCore.exe` и
подключите [Tracy 0.14.1](https://github.com/wolfpld/tracy/releases/tag/v0.14.1), соответствующий текущему manifest client.
Для H-008 после приёмки G-011 запишите непрерывный маршрут «Перепуск давления»:
обычное движение, все встречи, взаимодействия и финал; меню/редактор выделяйте отдельно.
Согласуйте бюджет кадра и слабый ПК, запишите GPU/драйвер, архив, разрешение, VSync
и настройки. First-use uploads сравнивайте с повторным проходом того же участка;
существующие frame zones включают scene/UI fence waits и не измеряют их отдельно.
Сравнивайте CPU/GPU зоны на одном разрешении, одном маршруте и одинаковых настройках;
разделяйте CPU work, GPU и present/VSync wait. Выделения памяти сейчас отдельно не
размечены; этот профиль не позволяет оценить их количество.

Для повторяемого локального сравнения шести видов можно собрать
`cmake --build --preset demo-relwithdebinfo --target relaycore_power_restoration_graphics`
и запустить этот test executable с `--profile-views`, указав абсолютный `RELAYCORE_ASSET_ROOT`
на ресурсы кандидата. Это 1280×720, VSync off, 20 кадров прогрева и 180 измеряемых кадров
на вид. `view_submit_ms` измеряет CPU-подготовку очереди; после VK-013
`view_completed_ms` дополнительно включает Vulkan scene submission и ожидание GPU.
Оба показателя исключают color readback, presentation и gameplay update. Исторические GL-измерения и ограничения —
в [протоколе G-006/H-002](playtests/G-006-H-002-2026-09-28.md).

Для сравнения новых графических настроек при 2560×1440 используйте
`relaycore_vulkan_mesh_tests --graphics-settings-profile` из оптимизированного
`msvc-dev` с его staged `RELAYCORE_ASSET_ROOT`. CSV/BMP записываются в
`vulkan-mesh-captures` рабочей папки; границы CPU/GPU измерений, fixed-scale
timestamp mode и результаты — в [отчёте 2026-10-13](profiling/GRAPHICS_SETTINGS-2026-10-13.md).

Для интегрированного scripted workload используйте отдельный target
`relaycore_containment_profile` для актуальной G-019:

```powershell
cmake --build --preset demo-relwithdebinfo --target relaycore_containment_profile -j 2
$env:RELAYCORE_ASSET_ROOT = (Resolve-Path build/msvc-relwithdebinfo/src/relaygame/assets).Path
build/msvc-relwithdebinfo/tests/relaycore_containment_profile.exe
```

Это два прохода семи состояний по 4 с, 1920×1080, VSync off, звук on,
scene HDR, изолированные Continue/бой/respawn/пауза/resize. Он сохраняет captures
в `containment-profile` относительно рабочей папки и wall-clock p50/p95/p99/max;
первые 0,5 с и readback исключены. Resize меняет разрешение; camera sweeps
не заменяют непрерывный grounded-проход. Evidence/рабочие бюджеты — VK14-00 в
[renderer plan](renderer/VK14-00.md#vk14-00-execution--2026-10-11).

The same executable accepts `--quality-preset low`, `balanced` or `high` to
exercise the player presets on this workload. No argument retains the original
internal-HDR baseline. Run each selection from a separate working directory to
preserve its log and captures. Add `--motion-captures` after the preset argument
to write numbered observation/workshop BMP frames at a target eight frames per
second during seconds 1–3 of the first pass. Readback perturbs this capture run;
use a separate run without that flag for performance measurements. These short
camera-sweep/combat sequences do not constitute a continuous human playthrough.
Commands, local budgets and hardware limits: [VK14-14](renderer/VK14-14.md).

Для прежнего эпизода используйте
`cmake --build --preset demo-relwithdebinfo --target relaycore_episode_profile`.
Запустите `build/msvc-relwithdebinfo/tests/relaycore_episode_profile.exe` с абсолютным
`RELAYCORE_ASSET_ROOT` на ресурсы кандидата. Он использует обычные `Application` и
`DemoGame`, 1280×720, VSync off, и подготавливает в отдельных временных сохранениях
вход, три встречи, питание, охлаждение, смерть/respawn, паузу, финал и редактор.
Начальные 0,5 с каждого состояния исключены из сводных значений. Tracy-показатели
`cpu_work_ms` и `present_ms` разделяют работу приложения и presentation; GPU-зоны
пишутся в Tracy через Vulkan. Этот сценарий не заменяет ручной непрерывный проход;
исторические результаты — в [протоколе H-002](playtests/H-002-2026-09-28.md).

Исторически для 1.0 снят воспроизводимый захват на графическом сценарии
`graphics.game_lifecycle` в RelWithDebInfo, 640×480, Tracy 0.13.1. Он проходит
переход между двумя картами, возврат после перезапуска и редактор. Файл захвата:
`build/profiling/relaycore_1_0_editor.tracy` (создаётся локально, не хранится в Git).

| CPU-зона | Вызовов | Среднее | Максимум |
| --- | ---: | ---: | ---: |
| `DemoGame::transition_to` | 3 | 4,31 мс | 5,51 мс |
| `LevelEditor::enter` | 4 | 13,59 мс | 19,76 мс |
| `LevelEditor::rebuild_scene` | 13 | 4,49 мс | 8,15 мс |
| `WorldSceneRenderer::render` | 5 | 0,56 мс | 1,49 мс |

Это короткий синтетический маршрут с пятью вызовами отрисовки сцены и двумя
отметками кадра, а не оценка кадровой частоты длительной игры. Времена включают
вложенную работу зон. По такому захвату нет основания менять hot path или вводить
новые кэши. Для решения о производительности долгой игровой сессии нужен отдельный
захват живого прохождения и проверка повторяемости результата.

### Vulkan: capture и validation

HDR tone mapping logs the selected compute/raster technique at backend creation.
For a process-local raster comparison set
`RELAYCORE_VULKAN_DISABLE_COMPUTE_TONE_MAPPING=1`; unset/0 uses compute when the
selected graphics queue and storage target support it. Other values fail. This
diagnostic override does not change authored/player settings; SDR is unchanged.
Compute shader compilation/staging and local evidence: [VK14-06a](renderer/VK14-06.md).

For pass-input diagnostics set process-local `RELAYCORE_VULKAN_PASS_INPUT_VIEW`
to `scene`, `depth`, `normals`, `velocity`, `history` (green accepted/red rejected),
or `previous_color` (raw reprojected history). `off`/unset disables the extra
targets/work. Values are strict; these modes do not alter saved graphics settings.
UI remains over the selected view. Encodings, conservative dynamic/viewmodel
rejection, history resets and checks: [VK14-06](renderer/VK14-06.md#06bc-contract--2026-10-12).

`temporal_source` compares the raw jittered source with TAA and requires TAA.
Temporal/resolution controls, motion conventions and profiling fixtures:
[VK14-08](renderer/VK14-08.md).

The spatial effects also expose `ao`, `reflection_confidence` and
`volume_transmittance` through this override. Enable the corresponding effect
first; invalid combinations fail explicitly. Controls, composition and measured
local limits: [VK14-07](renderer/VK14-07.md).

`ssgi` displays raw GI radiance times screen coverage and requires nonzero
`ssgi_quality`. Advanced lighting/lens controls and composition live in
[RENDERING](RENDERING.md#advanced-lighting-and-lens-effects--vk14-09); moving
fixtures and timing evidence live in [VK14-09](renderer/VK14-09.md).

В Advanced Diagnostics окно `Vulkan memory` обновляется кнопкой `Refresh GPU memory`.
Оно показывает heap usage/budget, VMA allocations/blocks, retained upload staging,
pending retirements и material descriptor pools. Снимок сохраняется до следующего
нажатия; ordinary frame loop не запрашивает heap statistics. `VK_EXT_memory_budget`
используется при наличии, иначе окно явно показывает оценочные VMA budgets.
Driver heap usage включает другие приложения; VMA allocations относятся к этому
allocator. Retained staging/retirements включают завершённые, но ещё не collected
ресурсы. Проверки и ограничения — [VK14-05](renderer/VK14-05.md).

Runtime требует минимум Vulkan 1.4 для loader и GPU; instance/effective/VMA API — 1.4.
Лог отдельно показывает loader, instance, physical/effective API, драйвер,
queue families и включённые device extensions. Fallback и developer override
для Vulkan 1.3 удалены; старый `RELAYCORE_VULKAN_FORCE_1_3` больше не влияет на запуск.
Loader/GPU ниже 1.4 отклоняются с диагностикой. SPIR-V 1.5 и
существующие feature chains сохраняются. Runtime также проверяет dynamic rendering, synchronization2, timeline semaphore,
graphics/present queues для SDL surface, `VK_KHR_swapchain`, `VK_KHR_swapchain_maintenance1` /
`swapchainMaintenance1`, `VK_KHR_surface_maintenance1` и `VK_KHR_get_surface_capabilities2`.
Tracy требует также host query reset и graphics timestamp queries. Presentation fences
используют maintenance feature для lifetime semaphore/swapchain; это осознанное
ограничение совместимости. На новом GPU/драйвере проверяйте настоящий запуск игры и
`graphics.vulkan_device`, `graphics.vulkan_presentation`, `graphics.game_lifecycle`.
`relaycore_vulkan_toolchain_check` проверяет loader/headers/layers и не подтверждает
поддержку device features или SDL presentation. Для оптимизированного standalone
кандидата Vulkan SDK не нужен; Debug/Asan development checks требуют validation layers.

Основной `RelayCore.exe` использует Vulkan. Отдельный Vulkan workload
отрисовывает `power_restoration` и `pressure_transfer`, тени, viewmodel и ImGui,
480 кадров в 640×360 с hardware sRGB. Он проверяет profiling/presentation,
а не gameplay lifecycle или скорость: задержка 16 мс намеренно задаёт темп захвата.

```powershell
cmake --build --preset demo-relwithdebinfo --target relaycore_vulkan_mesh_tests
$env:RELAYCORE_ASSET_ROOT = (Resolve-Path build/msvc-relwithdebinfo/src/relaygame/assets).Path
build/msvc-relwithdebinfo/tests/relaycore_vulkan_mesh_tests.exe --profile-views
```

Перед запуском подключите Tracy 0.14.1 либо запустите его `tracy-capture.exe`
с `-o build/vulkan.tracy -s 15`. `tracy-csvexport.exe -g build/vulkan.tracy`
проверяет GPU events. Контексты сцены и presentation имеют отдельные query pools;
коллекция выполняется после scene timeline waits и presentation fence waits.
VK14-03 сохраняет синхронные host waits; Tracy отдельно показывает scene
completion, acquire, presentation copy, present и retirement/drain. Последний use
mesh/material cache entries принадлежит scene timeline; upload и presentation
имеют отдельные completion domains. Подробности и ограничения —
[VK14-03](renderer/VK14-03.md#vk14-03-execution--2026-10-11).
`hostQueryReset` и timestamp queries
graphics queue требуются только при `RELAYCORE_ENABLE_TRACY=ON`. При выключенном
Tracy query pools не создаются. В кадре profiling не добавляет queue/device idle;
startup calibration Tracy использует queue waits.

Обычный Debug/Asan уже включает core и synchronization validation. Все Vulkan
CTest checks отклоняют неожиданные validation warnings/errors. Для screen-pixel
presentation/recreation checks нужен обычный Windows desktop без заставки.

```powershell
cmake --build --preset graphics-debug
ctest --preset graphics-debug --output-on-failure
```

Проверить Tracy вместе с validation можно в отдельной директории без изменения
основного preset/cache:

```powershell
cmake --preset msvc-debug -B build/vk-012-validation-profile -DRELAYCORE_ENABLE_TRACY=ON
cmake --build build/vk-012-validation-profile --target relaycore_vulkan_mesh_tests relaycore_vulkan_presentation_tests relaycore_vulkan_device_tests
ctest --test-dir build/vk-012-validation-profile -R '^graphics\.(vulkan_device|vulkan_mesh|vulkan_color_baseline)$' --output-on-failure
```

Для focused GPU-assisted прогона сначала добейтесь чистой обычной validation.
Создайте локальный `build/vk-012-gpuav/vk_layer_settings.txt`:

```ini
khronos_validation.validate_core = false
khronos_validation.gpuav_enable = true
khronos_validation.gpuav_shader_instrumentation = true
khronos_validation.gpuav_descriptor_checks = true
khronos_validation.gpuav_force_on_robustness = false
khronos_validation.gpuav_validate_trace_ray = false
khronos_validation.gpuav_mesh_shading = false
khronos_validation.gpuav_debug_validate_instrumented_shaders = true
khronos_validation.gpuav_debug_dump_instrumented_shaders = true
```

Применяйте настройки только к диагностическому процессу и восстанавливайте прежнее
значение переменной. Пример из корня проекта; `RELAYCORE_ASSET_ROOT` уже задан выше:

```powershell
$previousLayerSettings = $env:VK_LAYER_SETTINGS_PATH
try {
    $env:VK_LAYER_SETTINGS_PATH = (Resolve-Path build/vk-012-gpuav).Path
    Push-Location build/vk-012-validation-profile/tests
    try {
        ./relaycore_vulkan_mesh_tests.exe --color-baseline
        if ($LASTEXITCODE -ne 0) { throw 'Vulkan color baseline failed' }
        ./relaycore_vulkan_mesh_tests.exe
        if ($LASTEXITCODE -ne 0) { throw 'Vulkan mesh regression failed' }
    } finally { Pop-Location }
} finally { $env:VK_LAYER_SETTINGS_PATH = $previousLayerSettings }
```

Сверяйте имена настроек с установленным SDK: GPU-AV меняется между версиями.
Просмотрите весь validation output: в проверенном SDK GPU-AV сообщает
`WARNING-Setting-Limit-Adjusted` при включении своих instrumentation features.
Это ожидаемое сообщение настроек, а не разрешение игнорировать другие warnings,
VUID или shader-access errors. Такие запуски выполняются напрямую: строгий CTest
warning gate намеренно не ослаблен. `dump_*_after.spv` подтверждают instrumentation;
после диагностики dump options можно выключить. Не измеряйте производительность
с GPU-AV. Настройки не применяются глобально и не включаются в обычный runtime.

Для RenderDoc запускайте этот же executable с `--profile-views` либо
`--color-baseline`; engine API integration не нужна. Проверяйте в capture:

- SDR swapchain `B8G8R8A8_UNORM`/`R8G8B8A8_UNORM` и `SRGB_NONLINEAR`;
- scene attachment view: UNORM для legacy, SRGB для hardware; UI view всегда UNORM
  над тем же image. Blit читает UNORM storage и не кодирует готовые bytes повторно;
- порядок world/overlay/transparent → viewmodel с очисткой depth → RmlUi → developer ImGui →
  transfer/present, attachment reuse barriers и переходы layout/access/stage;
- D24 shadow writes → fragment reads, directional/spot/point layers и отсутствие
  validation messages. Сверяйте scene/UI и swapchain pixels, а не только успешный present.

`graphics.vulkan_color_baseline` не создаёт GL context и не использует GL
framebuffer assertions. Он проверяет transfer function, texture decode/filtering,
clear, UI и presentation/teardown. Исторические GL comparison captures остаются migration reference; GL runtime tests удалены в VK-013.

## Игровой UI (RmlUi 6.3)

Player documents are `assets/ui/hud.rml`, `menu.rml`, `finale.rml`; shared layout,
palette and control states live in `assets/ui/player.rcss`. The explicit Cyrillic
font is `assets/fonts/RobotoMono-Regular.ttf`. Existing asset synchronization stages
these files alongside the compiled `player_ui.vert/frag.spv`; runtime paths resolve
through the usual executable-relative root or `RELAYCORE_ASSET_ROOT`.
After editing RML/RCSS/font or UI shader files, build `relaycore_demo` to synchronize runtime
assets and restart the game: persistent documents are loaded at UI initialization.

Runtime ownership, action application and GPU lifetime are described in [ARCHITECTURE](ARCHITECTURE.md#player-ui-ownership-rmlui-63).

The adapter supports geometry, generated font textures, BMP images, premultiplied alpha, rectangular scissor and transforms. Authored images resolve relative to their RML document; SDL decodes BMP RGBA and the adapter premultiplies it before the existing texture upload. Invalid images fail with the source path and SDL diagnostic. PNG/SVG remain authoring formats, not runtime UI formats. Filters, layers, transformed clipping and rounded overflow are outside the current authored contract. The font and upstream license are staged with RML/RCSS; RmlUi implementation types do not enter gameplay presentation/action contracts.

Context dimensions, geometry and scissor use framebuffer pixels; the official SDL translator converts mouse coordinates by window pixel density. Authored `dp` layout scales from a 1280×720 reference with a 0.75 compatibility minimum. Menus scroll on short windows. Escape DOM text and retain unchanged text; screen changes clear stale UI actions and held gameplay inputs.

UI is drawn after scene display conversion and before ImGui, using encoded premultiplied alpha on the final UNORM view. It bypasses exposure, fog and scene grading; the HDR scene setting still presents SDR. Readback, ImGui and intentional reconfiguration remain synchronous.

Use existing `demo-debug`/`graphics-debug` presets; relevant checks are
`graphics.game_hud`, `graphics.player_ui`, `graphics.game_lifecycle`, `graphics.hdr_settings` and
`graphics.vulkan_color_baseline`. Review actual captures before changing graphics
baselines. ImGui's config switch controls developer tools; player UI remains enabled.

## Испытательный полигон

После перезагрузки CMake в CLion выберите **RelayCore — Sandbox** и нажмите Run.
Запустится меню полигона; «Начать испытания» открывает `assets/levels/development_sandbox.toml`.
Редактор из этого меню также получает полигон. Основной `relaycore_demo` открывает
игровое меню со сквозной кампанией по умолчанию; отдельная глава выбирается там же.
Сохранения и настройки полигона находятся в `%APPDATA%/RelayCore/Sandbox`, отдельно от игры.

Центральный зал безопасен. Запад — движение (ступени по 20 см, мост шириной 90 см,
прыжки и опасный участок); восток — лаборатория (карта со стола, дверь допуска,
питание и датчик двери). Северо-запад — стрельбище (мишени на 3/7/12 м,
турель и укрытие); северо-восток — арена двух rover с дополнительной турелью справа. Боевые двери открываются
с обеих сторон. Миссии и обязательного порядка нет.

F7 перечитывает карту и сбрасывает её состояние. F9 сохраняет локальную встречу,
F10 возвращает её; F3/F4/F8 — noclip, инспектор и исходник. Для правок в репозитории
конфигурации Play/Sandbox используют runtime assets своей сборки, включая SPIR-V.
После изменения исходного TOML пересоберите соответствующий target для синхронизации
ресурсов, затем нажмите F7. Override на исходный `assets/` подходит для headless
проверки карты, но не для запуска renderer.
Редактор сохраняет пользовательскую копию; изменения в исходном TOML делаются отдельно.

Для G-008 добавлены три стартовые полосы на полу: за укрытием стрельбища,
перед левым rover и в правой части арены перед новой турелью. Слева можно
проверить уклонение от замаха без огня турели; справа — выход из прострела
за высокий блок и бой с rover. F9 фиксирует выбранное начало, F10 повторяет
его, F7 возвращает весь полигон к исходному состоянию. Именованные точки
`cover`, `rover`, `mixed` доступны в проверке карты через `--entry`; этот
аргумент не переключает стартовую точку игры. [Сценарии и протокол](playtests/G-008-2026-09-28.md).

Для G-009 на столе в центральном зале, рядом с картой допуска, лежит дробовик.
Подойдите, наведитесь на него и нажмите E. Клавиша 1 выбирает точное оружие,
2 — дробовик, Mouse1 стреляет. Дробовик рассчитан на близкое сближение;
точное оружие удобнее на дистанции. Переключение сохраняет текущую паузу между
атаками. F9/F10 и выход в меню сохраняют выбор вместе с инвентарём и состоянием
мира. F7 начинает полигон заново, оружие нужно подобрать ещё раз.
Модель дробовика видна на столе и от первого лица, цевьё двигается после выстрела.
Native viewmodel VIS-35 включает перчатки и рукава; мировой pickup VIS-36 остаётся
без рук. После обновления расположения предмета использовать F7
или новый запуск полигона, затем подобрать E и выбрать 2.
[Проверка роли оружия](playtests/G-009-2026-09-28.md).

**RelayCore — Check Map** запускает проверку без окна и выдаёт JSON: размеры и
число объектов, сущности с исходными строками, разрешённые связи, задачи и предупреждения.
В аргументах можно выбрать другую карту. В Developer PowerShell:

```powershell
cmake --build --preset demo-debug --target relaycore_sandbox relaycore_level_check
$env:RELAYCORE_ASSET_ROOT = (Resolve-Path build/msvc-debug/src/relaygame/assets).Path
build/msvc-debug/src/relaygame/relaycore_level_check.exe assets/levels/development_sandbox.toml
build/msvc-debug/src/relaygame/relaycore_level_check.exe assets/levels/development_sandbox.toml --entry arena
pwsh -NoProfile -File tests/level_check_tests.ps1 -Executable build/msvc-debug/src/relaygame/relaycore_level_check.exe
```

Код 0 означает успешную подготовку сессии; предупреждения остаются в отчёте.
Код 1 — ошибка карты/ресурса/назначения перехода; 2 — неверные аргументы.
Проверка не заменяет игровой проход и не доказывает достижимость всего контента.

## Изолированный physics fixture

E-011 проверяет static boxes и triangle wall с проёмом без смены backend игровых карт. В Developer PowerShell:

```powershell
cmake --build --preset tests-debug --target relaycore_physics_tests relaycore_physics_fixture -j 2
ctest --preset test-debug -R '^relaycore\.physics\.' --output-on-failure --no-tests=error
build/msvc-debug/src/relaygame/relaycore_physics_fixture.exe build/msvc-debug/physics-fixture.svg
cmake --preset msvc-asan
cmake --build --preset tests-asan --target relaycore_physics_tests -j 2
ctest --preset test-asan -R '^relaycore\.physics\.' --output-on-failure --no-tests=error
```

CLI выводит hit distance/point/normal/surface/identity и box cast fraction; необязательный SVG показывает геометрию проёма, rays, точки и нормали. Координаты — метры, +Y вверх, right handed; CCW normal triangles сохраняется и при ray hit с обратной стороны. Shape queries двухсторонние. Tests проверяют masks, clearance, malformed data, staged/active lifetime и failed prepare. ASan инструментирует RelayCore boundary; vcpkg Jolt DLL использует обычную Debug-сборку. CPU/ABI options локального overlay описаны в `DEPENDENCY_STACK.md`. Fixture не заменяет human playtest или движение игрока по mesh.

## Где объекты во время игры

Файл карты TOML читается в `BlockoutLevel`: `solids` и `entities` — отдельные
`std::vector`. `EntitySpawnData` содержит класс, позицию, углы и набор текстовых
свойств. Это исходное описание карты, а не обновляемый на каждом кадре объект.

`LevelSessionBuilder` превращает описание в типизированные данные. В
`GameWorldRuntime` живут отдельные `std::vector<RuntimeDoor>`, `RuntimeTrigger`,
`RuntimeUsable`, объёмы опасности, противники и выходы. Реестр объектов хранит
стабильный ID, тип и индекс элемента в соответствующем векторе. Игровые правила
обновляют эти структуры, не перечитывая TOML каждый кадр.

Рендер подготавливает меши стен и экземпляры статических native/OBJ-моделей отдельно;
экземпляр содержит преобразование и ссылку на общий ресурс модели. При смене
карты новая сессия сначала собирается целиком, затем заменяет старую. Для уже
посещённых карт сохранение содержит снимки изменяемого состояния дверей,
предметов, заданий и других игровых объектов; геометрия остаётся в файлах карт.

Unit/content/graphics tests и episode profile используют staged runtime assets с native моделями и собранными SPIR-V; при ручном запуске задайте `RELAYCORE_ASSET_ROOT` на `build/<preset>/src/relaygame/assets`. CTest делает это автоматически. Изолированный mesh workload использует свой build-time SPIR-V path, но для native моделей также получает staged assets.

### Chapter enemy routes (G-019-C)

`enemy_rover` optionally names a `route`; `enemy_shooter` requires one.
Matching `enemy_route_point` entities use the same `route` string and feet
origins on one flat floor (1–16 points, within 8 cm of the enemy's floor).
The game tests connections against the full rover footprint, floor support,
and current doors; these authored encounter routes do not climb stairs or
replace the future native Recast/Detour capability. Enemies without a route
retain their existing direct pursuit. Missing/oversized/nonflat routes fail
session construction with the enemy index and reason.

Shooters use the existing saved enemy states: windup 0.7 s with fixed aim,
one 12 m shot, and recovery 0.95 s. A lost target prompts a reachable firing
position and then a search of the last-known position. Path/search/trace are
transient and reset on restore; enemy health/position/aim/mode/timer retain
the version-5 save contract. The editor displays shooters and route points
and preserves their properties through save/load.

### Pickup presentation (VIS-36)

`gameplay_entity` with `kind = pickup` may specify a finite `pickup_surface_y`
world height for the existing shotgun/fuse presentation. This is the visible
support plane, separate from the interaction `origin`/`size`. The renderer rests
the bare shotgun on its side and the fuse on its end-cap radius, 2 mm above that
plane. When absent, the bottom of the authored visual bounds is the support.
The field is excluded from save fingerprints, like `panel_style`; collision,
focus bounds, reach and consumed state retain their existing owners. Bench,
cooling, pressure and power placements use their actual authored worktop heights.

## Статические модели: GLB → native product

[Настройки и workflow](MODEL_ASSETS.md#статические-модели-glb--native-product).


## Освещение материалов и сглаживание

[Настройки и workflow](RENDERING.md#освещение-материалов-и-сглаживание).


## HDR scene rendering with SDR output — N3.1

[Настройки и workflow](RENDERING.md#hdr-scene-rendering-with-sdr-output--n31).


## Static reflection probes (IMG-02)

[Настройки и workflow](RENDERING.md#static-reflection-probes-img-02).
