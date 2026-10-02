# Разработка RelayCore

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
конфигурации CLion используют `RELAYCORE_ASSET_ROOT=$PROJECT_DIR$/assets`.
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
$env:RELAYCORE_ASSET_ROOT = (Resolve-Path assets).Path
build/msvc-debug/src/relaygame/relaycore_level_check.exe assets/levels/development_sandbox.toml
build/msvc-debug/src/relaygame/relaycore_level_check.exe assets/levels/development_sandbox.toml --entry arena
pwsh -NoProfile -File tests/level_check_tests.ps1 -Executable build/msvc-debug/src/relaygame/relaycore_level_check.exe
```

Код 0 означает успешную подготовку сессии; предупреждения остаются в отчёте.
Код 1 — ошибка карты/ресурса/назначения перехода; 2 — неверные аргументы.
Проверка не заменяет игровой проход и не доказывает достижимость всего контента.

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

`Test-Path` должен вернуть `True`. Для работы редактора и игры запускайте
`build/msvc-debug/src/relaygame/RelayCore.exe` из корня проекта или из CLion.
Корень ресурсов определяется проектом; переменная `RELAYCORE_ASSET_ROOT` полезна
только для прямого запуска тестовых бинарников.

### CLion

Откройте корень репозитория как проект CMake. Выберите профиль **MSVC Debug**
(`msvc-debug`) из `CMakePresets.json` и toolchain Visual Studio. Укажите
`VCPKG_ROOT` в окружении CMake и убедитесь, что `cl.exe` и `rc.exe` доступны CLion.
Создайте **CMake Application** с target `relaycore_demo` и рабочим каталогом корня
репозитория. Исполняемый файл называется `RelayCore.exe`. Не выбирайте
`relaycore_demo_assets`: этот target только копирует ресурсы и не запускает игру.

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

Профиль `msvc-relwithdebinfo` включает Tracy.

Для обычной игры в CLion выбирайте **RelayCore — Play**: эта конфигурация собирает `relaycore_demo` с `msvc-relwithdebinfo` и использует runtime assets рядом с executable. Debug/ASan оставлены для отладки и Vulkan core/synchronization validation; их FPS не характеризует игровую сборку. Локальное сравнение после VK-013, RTX 4060 Ti, 1280×720, VSync off, один scripted entrance: Debug p50 42,79 мс / p95 44,16 мс, RelWithDebInfo p50 1,63 мс / p95 2,57 мс. Captures: `build/vk-013-regression-{debug-before,before}.tracy`; полный workload — `build/vk-013-regression-episode-{debug-before,before}.log`. Это сравнение конфигураций, не ускорение renderer патчем.

Для сравнимого захвата:

```powershell
cmake --preset msvc-relwithdebinfo
cmake --build --preset demo-relwithdebinfo
```

Запустите собранный `build/msvc-relwithdebinfo/src/relaygame/RelayCore.exe` и
подключите [Tracy 0.14.1](https://github.com/wolfpld/tracy/releases/tag/v0.14.1), соответствующий текущему manifest client.
Для текущего slice запишите маршрут «Восстановление питания»: вход, три встречи,
включение питания и охлаждения, возврат и финал. Меню/редактор выделяйте отдельно.
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

Основной `RelayCore.exe` использует Vulkan. Отдельный Vulkan workload
отрисовывает `power_restoration` и `pressure_transfer`, тени, viewmodel и ImGui,
480 кадров в 640×360 с hardware sRGB. Он проверяет profiling/presentation,
а не gameplay lifecycle или скорость: задержка 16 мс намеренно задаёт темп захвата.

```powershell
cmake --build --preset demo-relwithdebinfo --target relaycore_vulkan_mesh_tests
$env:RELAYCORE_ASSET_ROOT = (Resolve-Path assets).Path
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
- порядок world/overlay/transparent → viewmodel с очисткой depth → ImGui →
  transfer/present, attachment reuse barriers и переходы layout/access/stage;
- D24 shadow writes → fragment reads, directional/spot/point layers и отсутствие
  validation messages. Сверяйте scene/UI и swapchain pixels, а не только успешный present.

`graphics.vulkan_color_baseline` не создаёт GL context и не использует GL
framebuffer assertions. Он проверяет transfer function, texture decode/filtering,
clear, UI и presentation/teardown. GL comparison tests пока остаются migration reference.

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

Graphics lifecycle и episode profile используют runtime assets с собранными SPIR-V; при ручном запуске задайте `RELAYCORE_ASSET_ROOT` на `build/<preset>/src/relaygame/assets`. CTest делает это автоматически. Изолированный mesh workload использует свой build-time SPIR-V path и допускает исходный `assets` для content fixtures.
