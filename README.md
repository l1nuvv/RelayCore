# RelayCore

**RelayCore** — собственный C++20-движок и небольшой GoldSource-inspired indoor FPS. Текущий игровой эпизод посвящён исследованию и восстановлению питания технического сектора. Есть ZIP-упаковка и обновление standalone-сборок из меню; [последняя опубликованная сборка](https://github.com/l1nuvv/RelayCore-releases/releases/latest) доступна в отдельном публичном репозитории. Выпуск с публикацией из CLion: конфигурация **RelayCore — Release** → Run; из терминала: `pwsh -NoProfile -File scripts/release.ps1 -Release`. Устройство и публикация описаны в [UPDATES.md](docs/UPDATES.md).

Проект использует SDL3, Vulkan 1.3, CMake, vcpkg manifest mode, Dear ImGui, Tracy и TOML-карты. Vulkan — единственный runtime backend; результаты перехода и дальнейшая проверка архитектуры находятся в [плане миграции](docs/VULKAN_MIGRATION_PLAN.md).

## Текущее направление

Единая очередь задач и результаты проверок находятся в [PLAYABLE_SLICE_ROADMAP](docs/PLAYABLE_SLICE_ROADMAP.md). Цель — законченный эпизод на 5–15 минут самостоятельной игры; длительность и понятность маршрута ещё не подтверждены новым игроком. Таблица статусов в roadmap отделяет выполненные технические проверки от игровой приёмки.

Основной сценарий — **«Восстановление питания» → «Новый эпизод»**, карта `power_restoration.toml`. Он связывает входной зал, сервисный узел с резервной шиной и роботом, щитовую, насосную, фильтрационную с турелью и обратный переход к шлюзу. «Два сектора» остаётся коротким сценарием проверки переходов между картами. Для нового участника используйте [инструкцию без решения маршрута](docs/PLAYTEST_GUIDE.txt); этапы и временные отметки записывает наблюдатель в [протокол G-001](docs/playtests/G-001-2026-09-27.md).

Инженерный трек — VK-013/014 завершены: основной runtime использует Vulkan, GL-код удалён; ревью владения ресурсами, 10 целевых проверок и свежие Tracy/RenderDoc baseline не выявили блокирующих проблем. Небольшая задача по диагностическим строкам записана отдельно в [плане миграции](docs/VULKAN_MIGRATION_PLAN.md); следующий capability-блок — production MeshAsset/ModelAsset. Игровой трек — независимый playtest G-011 нового эпизода, затем H-008/H-009; статусы и зависимости находятся в соответствующих планах.

## Возможности

- Фиксированная симуляция, движение, столкновения, приседание, прыжки и фонарик.
- Взаимодействия, задания, двери, инвентарь, два оружия, турели и охранные роботы.
- TOML-карты, сохранение состояния мира и переходы между картами.
- Встроенный редактор, отдельный испытательный полигон и CLI-проверка карт.
- Подготовленные очереди отрисовки, статические OBJ/BMP-модели, свет, тени и звук.
- Unit-, content- и graphics-проверки, Tracy, упаковка и обновление standalone-сборок.

## Требования

Для сборки: Windows 11, Visual Studio с C++ workload и Windows SDK, CMake 4.0+, Ninja, Git, PowerShell 7 и vcpkg в manifest mode. SDL3 с Vulkan support, Vulkan headers/loader, vk-bootstrap, VMA и host-tool Slang устанавливаются из закреплённого manifest; `slangc` компилирует shaders в SPIR-V во время сборки. Отдельный глобальный Slang не нужен. Windows tooling-регрессии также требуют `clang-format`.

Для запуска игры и graphics-тестов нужны Vulkan 1.3, поддерживаемые выбранным устройством features/extensions и рабочая presentation surface; текущий device contract проверяется `VulkanDevice`. Установите LunarG Vulkan SDK с validation layers и корректным `VULKAN_SDK`; SDK предоставляет validation layers и инструменты диагностики; loader/layers проверяются собственным toolchain probe, а build dependencies разрешаются через vcpkg. RenderDoc и Tracy используются для углублённой диагностики. Подготовка окружения описана в [плане миграции](docs/VULKAN_MIGRATION_PLAN.md#11-developer-machine-setup-windows-11).

## Сборка и проверки

Проект использует `CMakePresets.json`, а зависимости ставятся через `vcpkg.json` в manifest mode.

Перед первой командой CMake откройте Developer PowerShell или Developer Command Prompt установленной Visual Studio, чтобы `cl.exe`, `rc.exe` и Windows SDK tools были доступны в окружении.

Задайте путь к готовой установке vcpkg и проверьте toolchain file:

```powershell
$env:VCPKG_ROOT = 'C:\dev\vcpkg'
Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

`False` означает неверный путь или неготовую установку vcpkg.

### Debug demo

```powershell
cmake --preset msvc-debug
cmake --build --preset demo-debug
```

### Unit-тесты

```powershell
cmake --preset msvc-debug
cmake --build --preset tests-debug
ctest --preset test-debug --output-on-failure
```

### Content validation

```powershell
cmake --preset msvc-debug
cmake --build --preset validation-debug
ctest --preset validation-debug --output-on-failure
```

### Полная Debug-проверка

```powershell
cmake --workflow --preset debug-discipline-workflow
```

### RelWithDebInfo и ASan

```powershell
cmake --workflow --preset relwithdebinfo-discipline-workflow
cmake --workflow --preset asan-discipline-workflow
```

CI не запускает графическое demo. ASan workflow проверяет unit-тесты и content validation без сборки или запуска demo.

### Локальные graphics-проверки

Требуются Vulkan-capable GPU и обычный интерактивный Windows desktop для screen-pixel tests. Проверяются color pipeline, UI, shadows, uploads, presentation/recreation, gameplay lifecycle и владение ресурсами. 158 фиксированных baselines сохраняют регрессии ранее принятого GL/Vulkan паритета.

```powershell
cmake --build --preset graphics-debug
ctest --preset graphics-debug --output-on-failure
```

Недоступный graphics backend даёт `Skipped`, а не подтверждение успешной проверки.

### clang-tidy

```powershell
cmake --fresh --preset msvc-debug
pwsh -File scripts/run-clang-tidy.ps1 -BuildDirectory build/msvc-debug
```

Проверяются production `.cpp` в `src/relaycore` и `src/relaygame`.

## Запуск demo

После `demo-debug` executable находится в target build directory, определяемой текущим CMake target:

```powershell
.\build\msvc-debug\src\relaygame\RelayCore.exe
```

Цветовой pipeline выбирается только при startup и по умолчанию остаётся `legacy_gamma`:

```powershell
.\build\msvc-debug\src\relaygame\RelayCore.exe --config assets\config\rc022_capture.toml --color-pipeline legacy_gamma
.\build\msvc-debug\src\relaygame\RelayCore.exe --config assets\config\rc022_capture.toml --color-pipeline hardware_srgb
```

`hardware_srgb` использует линейные scene data и sRGB image view; `legacy_gamma` — UNORM scene view. ImGui намеренно компонуется в encoded space через UNORM view того же target. Presentation переносит готовый encoded кадр в UNORM swapchain. Vulkan color baseline проверяет численные значения, UI и texture filtering; отсутствующие возможности устройства завершают startup ошибкой без fallback.

CMake копирует каталог `assets` и собранные SPIR-V рядом с demo executable. По умолчанию runtime использует `<exe-directory>/assets`; `RELAYCORE_ASSET_ROOT` допускается только как абсолютный override на полный runtime-каталог с собранными SPIR-V. Основной конфиг — `assets/config/engine.toml`.

## Частный плейтест

В Visual Studio developer environment с настроенным `VCPKG_ROOT`:

```powershell
cmake --preset msvc-relwithdebinfo
cmake --build --preset demo-relwithdebinfo
pwsh -NoProfile -File scripts/package-playtest.ps1
```

Скрипт создаёт каталог и ZIP в `build/playtest`, включая executable, соседние DLL, ресурсы и `PLAYTEST_GUIDE.txt`. Он отвергает устаревшие staged assets по SHA256, сверяет каждый файл пакета и ZIP, записывает `BUILD_MANIFEST.json` с revision/состоянием исходников и контрольными суммами, а рядом с ZIP — `.sha256`. Перед упаковкой по-прежнему нужна свежая сборка: manifest идентифицирует файлы, но не заменяет компиляцию.

Распакованная игра запускается через `RelayCore.exe`. Требуются Vulkan 1.3 с features/extensions из device contract и Microsoft Visual C++ Redistributable x64. Пользовательские настройки и прогресс в архив не входят. Локальная проверка вне проекта и графический Tracy baseline описаны в [протоколе G-006/H-002](docs/playtests/G-006-H-002-2026-09-28.md). 28 сентября автор принял G-003–G-006 и согласовал закрытие H-002/H-003 с имеющимися проверками. Условия другого ПК и полный живой Tracy-профиль отдельно не зафиксированы; актуальные статусы и ограничения находятся в основной очереди.

Актуальный кандидат M3 после ASan-аудиоисправления и интегрированного Tracy baseline — в [протоколе H-002](docs/playtests/H-002-2026-09-28.md). Исходный кандидат G-001 и его SHA256 сохранены в [старом протоколе](docs/playtests/G-001-2026-09-27.md); для сравнимых наблюдений не заменяйте сборку посередине прохождения.

## Управление

Основная карта — `power_restoration.toml`; запуск начинается с меню. Взаимодействие — **E**. Питание включает рабочий свет, охлаждение запускает вентиляцию и снимает блокировку шлюза. После смерти игрок возвращается к точке появления с восстановленным здоровьем; изменения мира и задания сохраняются. Выход во время смерти также сохраняет актуальное состояние: «Продолжить» завершает ожидание возрождения. Прежний боевой `test_room.toml` остаётся доступен через `game.level_path`.

**F9/F10** — инструменты локального повтора встречи: снимок хранит игрока и его движение, противников, двери, миссию, время, фонарик и эффекты окружения в памяти. Команды не записывают `progress.toml`; дальнейшее штатное автосохранение и выход работают как обычно. Вход в редактор и смена сессии сбрасывают снимок, editor preview не использует снимок кампании. Дисковое «Продолжить» загружает обычное сохранение игры.

Поверхности используют собственные растровые текстуры в духе индустриальных FPS конца 1990-х: бетон, окрашенная сталь, плитка, рифлёный настил, решётки и дерево. Исходник и описание упаковки находятся в `assets/textures/facility_source.png` и `assets/textures/README.md`.

Для проверки освещения и получения кадров до/после:

```powershell
cmake --build --preset graphics-baseline-debug --target relaycore_power_restoration_graphics
ctest --test-dir build/msvc-debug -R '^graphics.power_restoration$' --output-on-failure
```

Кадры сохраняются в `build/msvc-debug/tests/power-captures`. Поле `required_target` у `render_light` и `audio_zone` включает их после активации указанной цели панелью или триггером; отсутствие поля сохраняет постоянное действие.

| Ввод | Действие |
| --- | --- |
| W/A/S/D | движение |
| Space | прыжок |
| Shift | ускорение |
| Mouse | обзор |
| ЛКМ | атака |
| E | взаимодействие |
| Escape | пауза / продолжить; выход через меню |
| F | фонарик |
| F11 | редактор / играть |
| F1 | переключение захвата мыши |
| F2 | diagnostics |
| F3 | noclip |
| F4 | inspector visualization |
| F5 | pin inspector |
| F6 | advanced diagnostics |
| F7 | live reload |
| F8 | открыть источник в inspector |
| F9 | сохранить локальный снимок встречи |
| F10 | восстановить локальный снимок встречи |
| V | переключить vsync |

Mouse look и fire требуют relative mouse mode. Gameplay keyboard/mouse input блокируется, когда ImGui захватывает соответствующее устройство; global debug shortcuts остаются доступны.

## Документы

- [Краткая архитектура](docs/ARCHITECTURE.md)
- [Действующий план playable slice](docs/PLAYABLE_SLICE_ROADMAP.md)
- [Разработка, полигон и локальные проверки](docs/DEVELOPMENT.md)
- [Редактор и переходы между картами](docs/EDITOR_AND_CAMPAIGN.md)
- [Архитектурное направление](docs/ENGINE_ARCHITECTURE_REFERENCE.md)
- [Миграция Vulkan](docs/VULKAN_MIGRATION_PLAN.md)
- [Зависимости и условия интеграции](docs/DEPENDENCY_STACK.md)
- [Арт-направление и style board](docs/ART_DIRECTION.md)
- [Выпуск и обновление сборок](docs/UPDATES.md)

`Doxyfile` остаётся необязательным локальным developer tool; Doxygen reference не входит в обязательный workflow.

## Структура репозитория

```text
src/
  relaycore/   # engine runtime
  relaygame/   # game/demo layer
assets/        # shaders, textures, audio, level data
cmake/         # helper modules and dependency glue
tests/         # doctest-based tests
```

## Принципы проекта

- debuggability важнее «умной» архитектуры
- явные границы между engine / game / data
- минимум скрытой магии
- профилирование и диагностика с самого начала
