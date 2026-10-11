# Разработка RelayCore

## Игровой UI (RmlUi 6.3)

Player documents are `assets/ui/hud.rml`, `menu.rml`, `finale.rml`; shared layout,
palette and control states live in `assets/ui/player.rcss`. The explicit Cyrillic
font is `assets/fonts/RobotoMono-Regular.ttf`. Existing asset synchronization stages
these files alongside the compiled `player_ui.vert/frag.spv`; runtime paths resolve
through the usual executable-relative root or `RELAYCORE_ASSET_ROOT`.
After editing RML/RCSS/font or UI shader files, build `relaycore_demo` to synchronize runtime
assets and restart the game: persistent documents are loaded at UI initialization.

`Renderer` owns `PlayerUi`, its context, official SDL system/IME adapter and Vulkan
render interface. Game code sends `HudPresentation` built directly from runtime
state and menu text/values; RmlUi handles stay in the implementation. DOM text is
escaped and unchanged text is retained. UI listeners enqueue actions; `DemoGame`
applies them at the start of its update, using existing save/settings/session
operations. Screen changes clear stale UI actions and gameplay held inputs.

The existing SDL event loop dispatches developer input and focused RmlUi menu input;
passive HUD/finale documents have no pointer ownership. Context dimensions, geometry
and scissor use framebuffer pixels; the official SDL translator converts mouse
coordinates by window pixel density. Authored `dp` layout scales from a 1280×720
reference with a 0.75 compatibility minimum. Menus scroll on short windows.

After the scene's tone mapping/display conversion, `VulkanMeshRenderer` appends
RmlUi to the final target's UNORM view using encoded premultiplied alpha, then
developer ImGui. Current HDR setting is an FP16 scene/tone-mapping path with SDR
presentation, not an HDR10 swapchain. UI bypasses exposure, fog and scene grading.
Geometry and atlases are persistent. Two scene slots retain RmlUi draw references
and their descriptor/pipeline owner through scene → UI → presentation-copy timeline
completion. Document changes or context teardown can release CPU ownership while
those snapshots remain alive. Mutable scene data and targets belong to each slot;
reuse waits for its final GPU use. ImGui/readback and intentional reconfiguration
remain synchronous. Renderer drains presentation and both slots before destroying
SDL/Vulkan/VMA. VK14-04 evidence is in the renderer modernization plan.

Use existing `demo-debug`/`graphics-debug` presets; relevant checks are
`graphics.game_hud`, `graphics.player_ui`, `graphics.game_lifecycle`, `graphics.hdr_settings` and
`graphics.vulkan_color_baseline`. Review actual captures before changing graphics
baselines. ImGui's config switch controls developer tools; player UI remains enabled.

## Испытательный полигон

После перезагрузки CMake в CLion выберите **RelayCore — Sandbox** и нажмите Run.
Запустится меню полигона; «Начать испытания» открывает `assets/levels/development_sandbox.toml`.
Редактор из этого меню также получает полигон. Основной `relaycore_demo` запускает прежний эпизод.
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
Текущий вариант без рук. После обновления расположения предмета использовать F7
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

## Освещение материалов и сглаживание

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
сохраняются. MSAA сглаживает покрытие геометрии; temporal AA здесь не реализован.

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
cmake --build --preset demo-debug --target relaycore_demo relaycore_tests relaycore_content_validation relaycore_game_lifecycle
ctest --test-dir build/msvc-debug -R '^(relaycore\.|validation\.|graphics\.game_lifecycle$)' --output-on-failure
```

Для графического теста нужны Vulkan 1.3 и поддерживаемые device contract возможности. Дополнительные пресеты и полный
порядок проверок перечислены в корневом README; их не требуется запускать после
каждого редактирования карты.

### Контракт автоматических проверок

На Windows `tests-debug` собирает unit/audio targets и CLI-проверку карт. `test-debug` запускает также `relaycore.tooling.pre_commit`, `relaycore.tooling.release_scripts`, `relaycore.tooling.release_workflow` и `relaycore.tooling.level_check`. Нужны PowerShell 7 и `clang-format`; CMake добавляет найденный formatter в PATH pre-commit теста. Release-тесты используют изолированные fixtures и имитацию публикации. Дополнительная Java-проверка интерпретатора CLion остаётся ручной: `pwsh -NoProfile -File tests/release_workflow_tests.ps1 -Java <java.exe>`.

В `.github/workflows/ci.yml` обычные GitHub-hosted jobs проверяют сборку, unit/content/tooling и компиляцию graphics targets. Это не подтверждает работу GPU. Полное выполнение `graphics.*` запускается через `workflow_dispatch` с `run_graphics=true` на заранее настроенном runner с метками `self-hosted`, `Windows`, `X64`, `relaycore-graphics`. На нём нужны рабочий интерактивный Windows desktop, драйвер Vulkan, Vulkan SDK с validation layers и build tools. Метки не создают runner: зарегистрируйте подходящую машину перед запуском job.

Graphics job выполняет configure/build с validation=ON, проверяет loader/layers через существующий `relaycore_vulkan_toolchain_check`, затем результат CTest/JUnit: report должен содержать хотя бы один test case, без failure/error/skipped. Количество тестов не фиксировано. Доступность features, создание instance/device, presentation, uploads и validation проверяются самими Vulkan-тестами. Локальный эквивалент: `cmake --build --preset graphics-debug`, затем `ctest --preset graphics-debug --no-tests=error --output-junit graphics-results.xml`; после CTest нужно проверить отсутствие skip в report. Удалённый graphics gate не считается пройденным по локальной сборке.

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

Для интегрированного scripted workload используйте отдельный target
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

Runtime проверяет Vulkan 1.3, dynamic rendering, synchronization2, timeline semaphore,
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
коллекция выполняется после fence waits. `hostQueryReset` и timestamp queries
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

## Статические модели: GLB → native product

### Blender: проектные навыки

Навыки установлены локально в `.agents/skills` и хранятся вместе с проектом.
Начальная точка — [relaycore-blender](../.agents/skills/relaycore-blender/SKILL.md):
она связывает моделирование, материалы, визуальную проверку и экспорт с правилами
RelayCore. Состав, исходные версии и локальные адаптации записаны в
[BLENDER_SOURCES.md](../.agents/skills/BLENDER_SOURCES.md).

Основной путь — установленный Blender и его Python через CLI. MCP, Bun и отдельный
Python-пакет `bpy` не требуются для моделирования, проверки геометрии и рендера.
Задайте `$env:BLENDER_EXECUTABLE` для текущего процесса либо найдите `blender` в PATH;
путь конкретного компьютера не хранится в проекте. Сначала проверьте `--version`.
В этой установке инструменты проверены с Blender 5.2.2 LTS.

```powershell
& $env:BLENDER_EXECUTABLE --background --factory-startup --disable-autoexec `
  --threads 2 --python-exit-code 1 `
  --python .agents/skills/blender-asset-validation/scripts/inspect_asset.py -- `
  --input content/models/power_cabinet.glb --output build/blender-review/metrics.json
```

Проверки и рендеры сохраняйте в `build/blender-review/<asset>/<revision>`;
воспроизводимые исходники — по существующим соглашениям `scripts` и `content/models`.
Сравнивайте форму с референсами, открывайте изображения с нескольких ракурсов и
дорабатывайте конкретные дефекты. Красивый рендер Blender не подтверждает совместимость
с игровым импортёром: ограничения следующего раздела остаются обязательными.

### Поддерживаемый импорт

Общие интерактивные корпуса VIS-17/18 воспроизводятся через
`scripts/export-interactive-hardware-glb.py`: `door_lift`, `door_slide` и четыре
`panel_*` модели. Полотно нормализовано в исходный движущийся AABB, направляющие
находятся вне swept depth. У `gameplay_entity` с `kind = panel/button/terminal/console`
необязательный render-only `panel_style` принимает `door`, `isolator`, `start`,
`instrument`; остальные значения вызывают диагностику. По умолчанию panel/button
использует door, terminal/console — instrument. Позиция кнопки, feedback, focus,
collision и действия задаются прежними игровыми данными.
`panel_style` исключён из level fingerprint, чтобы смена корпуса сохраняла
совместимость прогресса; gameplay properties и геометрия по-прежнему проверяются.

`relaycore_model_import` компилирует статический GLB в `.rcmodel`. Поддерживаются
triangle primitives с POSITION/NORMAL, TEXCOORD_0 для текстур, default scene,
иерархия и transforms узлов, повторное использование meshes, зеркальные экземпляры,
несколько opaque материалов с base-color factor и встроенными PNG/JPEG. Factors
применяются в linear space; base color хранит visual sRGB с top-left UV/image origin.
Normal map требует экспортированных FLOAT VEC4 TANGENT и TEXCOORD_0; без tangent
importer отказывает с инструкцией включить экспорт в Blender. Tangent xyz — единичный
вектор, перпендикулярный normal, w = ±1; handedness совпадает внутри треугольника.
[glTF basis](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes):
`B = cross(N,T) * w`. Mirrored nodes запекают X reflection в mesh и меняют tangent.w
вместе с winding; runtime трансформирует T обычной матрицей, N inverse transpose,
ортогонализует T и учитывает знак determinant.
Normal map хранит исходные linear UNORM RGB, +Y tangent convention, положительный Z;
alpha игнорируется. UV и green channel не переворачиваются. `normalTexture.scale`
сохраняется и применяется к XY перед нормализацией. В обоих цветовых режимах normal
texture всегда linear. Shadow receiver bias/offset использует геометрическую нормаль,
освещение — mapped normal.
Sampler — repeat, linear/trilinear; при поддержке `samplerAnisotropy` устройство
включает анизотропию до `min(16, maxSamplerAnisotropy)`. Без этой optional feature
сохраняется трилинейная фильтрация; GPU startup contract не расширяется. Выбранная
степень записывается в startup log. Неподдерживаемые attributes/extensions, skins,
animation, morphs, sparse accessors и alpha/double-sided
вызывают ошибку с путём source. IMG-01/03 поддерживают embedded metallic-roughness
(G=roughness, B=metallic) и occlusion (R) textures: linear RGBA8, UV0, repeat/linear,
без texture transform. MR maps требуют `--metallic-roughness`, умножаются на factors;
AO strength в [0,1] ослабляет только непрямой свет. Неиспользуемые каналы игнорируются.
IMG-06 adds core glTF emissive textures/factors: embedded PNG/JPEG RGB is sRGB,
alpha is ignored, and the independent linear RGB factor is finite and in [0,1].
The same UV0/sampler restrictions apply. RMDL v5 retains readers for v1–4 and
stores the map and factor separately; runtime emission maps always use sRGB
sampling, including the legacy color pipeline. Emission is added independently
of direct/indirect light and AO, before fog and tone mapping. Core factor-only
materials work; glTF extensions (including emissive-strength) remain unsupported.
The first consumer is the containment crosshead's illuminated plate, authored
by `scripts/author-containment-emission.py` and the owning containment exporter.
Its factor is zero in standby and restored when `grip_ready` powers the drive; it does
not illuminate geometry. A separate conditional light belongs to the existing
control indicator. Bloom was not required by this restrained first consumer.
Verification: `graphics.emissive_maps`, native/GLB cases and containment HDR/SDR.
Metallic/roughness scalars поддерживаются
при явном выборе новой модели освещения. По умолчанию importer сохраняет прежний
base-color/normal путь; `--metallic-roughness` включает glTF factors для всех материалов
модели. В декларации CMake этот выбор задаёт
`set(RELAYCORE_MODEL_METALLIC_ROUGHNESS_power_cabinet ON)`; `lab_bench` также
явно включён в metallic/roughness список, `service_cylinder` использует прежнее
освещение. Актуальный список opt-in моделей задаёт `cmake/RelayCoreModelAssets.cmake`.
Декларация входит в SHA256 receipts, смена выбора пересобирает product.

Новая модель — GGX distribution, separable Smith visibility, Schlick Fresnel и Lambert
diffuse по [glTF Appendix B](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation).
Factors — linear значения в [0,1]; invalid/nonfinite данные отклоняются. Roughness=0
сохраняется в product, но shader регуляризует сингулярный GGX lobe: perceptual roughness
минимум 0.05 (alpha минимум 0.0025). Это явное численное приближение.
Directional/local/spot используют текущие light/shadow contracts, геометрический shadow
bias и mapped normal для BRDF. Hemisphere ambient — приближение diffuse irradiance;
environment specular/IBL пока отсутствует. Металл без прямого света остаётся тёмным.
Для этой модели shading, fog и postfx вычисляются в linear space в обоих color modes,
затем legacy кодирует RGB вручную, hardware использует sRGB attachment. Прежние материалы
сохраняют свою цветовую обработку. SDR saturation сохраняется в default-пути;
opt-in HDR/exposure описаны в разделе Native HDR profile ниже.

```powershell
cmake --build --preset demo-debug
# Для работы только с ресурсами или полигоном:
cmake --build --preset demo-debug --target relaycore_demo_assets
cmake --build --preset demo-debug --target relaycore_sandbox
```

Модели подключены к сборке через явный список в `cmake/RelayCoreModelAssets.cmake`.
Первый slice E-004 включал `service_cylinder.glb`, `lab_bench.glb` и
`power_cabinet.glb` из `content/models`; текущий список также содержит последующие
facility, signage, interactive hardware и weapon модели.
Для новой модели добавьте GLB в `content/models` и имя файла в этот список
(латинские буквы, цифры, `_` и `-`, без подкаталогов);
product получает путь `models/<имя>.rcmodel`. Файл не нужно добавлять в `assets`:
двойное authored/generated владение одним путём вызывает ошибку сборки.

Каждая конфигурация создаёт свои products в `build/<preset>/generated/model-assets`,
затем синхронизирует их в `build/<preset>/src/relaygame/assets` вместе с authored
ресурсами и SPIR-V. Изменение GLB, importer executable или compiler rules вызывает
перекомпиляцию; обычная повторная сборка не переписывает неизменившуюся модель.
Неподдерживаемый/повреждённый GLB останавливает сборку до staging и сохраняет
прежний рабочий product. CTest и Check Map используют полный staged runtime root.

`generated/model-receipts` хранит build-only receipts с SHA256 source/importer/
compiler rules/product. Упаковка сверяет их с текущими файлами и staged product,
поэтому отклоняет устаревшую модель даже при сохранённом timestamp исходника.
Это timestamp-based CMake dependency tracking и проверка целостности при упаковке;
content-addressed cache, streaming и автоматический Blender export сюда не входят.
GLB и receipts не попадают в ZIP. Сам CLI остаётся доступен для отдельного импорта,
например `build/msvc-debug/relaycore_model_import.exe source.glb build/test.rcmodel`.

Для проверки текущей сборки без упаковки используйте существующий checker:

```powershell
pwsh -NoProfile -File scripts/check-model-assets.ps1 -ProjectRoot . `
  -BuildRoot build/msvc-debug -RuntimeAssets build/msvc-debug/src/relaygame/assets
```

Он сверяет source/importer/rules/declaration/product SHA256 и staged product для
каждого пути из `generated/runtime-models.txt`. Stale state исправляется повторной
компиляцией исходника, а не ручной правкой receipt. `.rcmodel` остаётся build-only
product; воспроизводимый export и editable source хранятся в `scripts`/`content`.
Отдельный native collision opt-in и его receipts описаны в [COLLISION_ASSETS.md](COLLISION_ASSETS.md).

Сценарий `scripts/export-service-cylinder-glb.py`
запускается в Blender через `--background --disable-autoexec --python`; он использует
исходный project OBJ/BMP, сохраняет метры/+Y вверх и выделяет корпус/вентиль.

В Sandbox баллон у устройств (`[29,0,2.5]`) использует новый путь. `.rcmodel`
доступен в каталоге моделей редактора; материалы входят в файл, поле `texture`
для него отсутствует. Коллизия остаётся отдельным authored AABB. F7 после компиляции
и сборки подготавливает новый ресурс, неудачный reload сохраняет рабочий; bounds
редактора обновляются при загрузке карты. Runtime не содержит fastgltf/WIC.

Проверочная native-комната N1 — `assets/levels/native_lab.toml`. В меню редактора
выберите «Открыть карту → native_lab.toml», затем «Играть» для preview. Три стола,
два шкафа и три баллона используют общие model products с разными поворотами и
масштабами; коллизия каждого экземпляра — явно заданный local AABB. Верхняя кнопка
основного шкафа включает рабочий свет, нижняя открывает кладовую. Это content fixture
без миссии, а не новый эпизод или завершённая production assets phase C.

`scripts/export-native-lab-glb.py` создаёт стол и шкаф в Blender, сохраняет editable
`.blend` и экспортирует GLB в `content/models`. Запуск: Blender
`--background --disable-autoexec --python scripts/export-native-lab-glb.py`.
Размеры в метрах; origin на полу; Blender +Z вверх / фасад -Y переходит в runtime
+Y вверх / фасад +Z. Окрашенная сталь шкафа использует normal map покрытия;
`power_cabinet_normal.png` — воспроизводимый source, также packed в `.blend` и встроен
в GLB. Для повторного экспорта только шкафа добавьте `-- --cabinet-only`.
После изменения GLB выполните обычную сборку, затем повторно запустите preview
или нажмите F7 в обычной игре. Во время editor preview F7 не выполняет reload:
редактор сохраняет собственное состояние для возврата.
Карта и три `.rcmodel` включаются в обычный playtest ZIP; Blender/GLB для запуска
не требуются. Исходные игровые карты и default startup map остаются прежними.

Compiler пишет только product v3. Loader читает v1–v3; v1 остаётся base-color-only,
v2 — base-color/normal, оба используют прежнее освещение. Старые runtimes не читают v3:
для обновлённого runtime
products пересобираются обычной сборкой, receipts/staging учитывают изменение importer.
Little-endian header — RMDL/version/mesh/material/part counts. В v2 mesh начинается
с flags (бит 0 = tangents), vertex/index counts; vertex — 16 float (position, color,
normal, material_kind, UV, tangent). После base-color width/height/RGBA каждого
материала идут normal width/height, float scale и RGBA; 0/0 означает отсутствие карты.
Part mesh/material и column-major transform не изменены. Нет зависимости от GLM padding.
В v3 после normal payload каждого материала добавлены uint flags (бит 0 = новая модель
освещения), float roughness и float metallic; при flags=0 записываются 1/0. V1/v2 не
приобретают новую модель при чтении или повторной записи. V4 после v3 factors
хранит MR width/height/RGBA, AO width/height/RGBA и float AO strength; 0/0 означает
отсутствующую карту. Loader сохраняет чтение v1–3 без изменения освещения.
Normal/MR/AO maps остаются встроенными, repeat/linear, с GPU mip chain, без TextureAsset/KTX2;
отдельные текстурные продукты и mip policy требуют следующего consumer.
Loader проверяет размеры, indices, finite geometry/transforms,
материалы и точный конец файла до создания runtime resources; лимит product 256 MiB.

Для проверки normal slice:

```powershell
ctest --preset test-debug -R '^relaycore\.(native|GLB|Blender|tooling.model_assets)' --output-on-failure
ctest --preset graphics-debug -R '^graphics\.(vulkan_mesh|vulkan_color_baseline|power_restoration)$' --output-on-failure
# RenderDoc: 120 кадров native_lab в hardware_srgb, шкаф крупным планом.
# Рабочая директория build/msvc-debug/tests; RELAYCORE_ASSET_ROOT — staged assets.
./relaycore_vulkan_mesh_tests.exe --profile-native
# Только analytical BRDF/material checks в обоих color modes:
./relaycore_vulkan_mesh_tests.exe --metallic-roughness
```

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

Environment specular/probe bake/import ещё не добавлены: это следующий шаг N3.

## Где объекты во время игры

Файл карты TOML читается в `BlockoutLevel`: `solids` и `entities` — отдельные
`std::vector`. `EntitySpawnData` содержит класс, позицию, углы и набор текстовых
свойств. Это исходное описание карты, а не обновляемый на каждом кадре объект.

`LevelSessionBuilder` превращает описание в типизированные данные. В
`GameWorldRuntime` живут отдельные `std::vector<RuntimeDoor>`, `RuntimeTrigger`,
`RuntimeUsable`, объёмы опасности, противники и выходы. Реестр объектов хранит
стабильный ID, тип и индекс элемента в соответствующем векторе. Игровые правила
обновляют эти структуры, не перечитывая TOML каждый кадр.

Рендер подготавливает меши стен и экземпляры статических OBJ-моделей отдельно;
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
