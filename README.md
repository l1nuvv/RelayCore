# RelayCore

**RelayCore** — небольшой C++20-проект про разработку движка и ретро-FPS с упором не на “технодемо”, а на понятный, расширяемый runtime для маленького indoor FPS slice.

Проект вдохновлён GoldSrc / Half-Life 1 по ощущению пространства и читаемости систем, но реализуется современным и поддерживаемым способом: SDL3, OpenGL 4.6 Core, CMake, vcpkg manifest mode, Dear ImGui, Tracy и простой data-driven blockout pipeline.

## Что уже есть

- SDL3 окно и жизненный цикл приложения
- OpenGL 4.6 Core + glad2
- engine/game split
- логирование через spdlog
- Dear ImGui overlay
- базовая аудиосистема на miniaudio
- загрузка blockout-уровней из TOML
- базовый runtime для interaction/doors/triggers
- doctest для unit-тестов
- Tracy для профилирования

## Ключевая цель

Довести проект до маленького, но играбельного indoor FPS vertical slice:

- spawn
- walking
- collision
- doors / buttons / triggers
- materials / textures
- lighting
- ambient/world audio
- один небольшой полированный уровень

## Технологии

- C++20
- SDL3
- OpenGL 4.6 Core
- glad2
- GLM
- spdlog
- Dear ImGui
- toml++
- miniaudio
- doctest
- Tracy
- CMake
- vcpkg

## Требования

Минимум для рабочего dev-окружения:

- Windows 11
- Visual Studio 2022 / MSVC toolchain
- CMake 4.0.0+
- Git
- vcpkg
- Ninja

## Сборка

Проект использует `CMakePresets.json`, а зависимости ставятся через `vcpkg.json` в manifest mode. Такой режим vcpkg рекомендует как основной для большинства пользователей; он также нужен для versioning и project-local dependency trees.

Перед первой командой CMake откройте `Developer PowerShell for VS 2022` или `Developer Command Prompt for VS 2022`, чтобы `cl.exe`, `rc.exe` и Windows SDK tools уже были доступны в окружении.

Задайте путь к готовой установке vcpkg и проверьте toolchain file:

```powershell
$env:VCPKG_ROOT = 'C:\dev\vcpkg'
Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

`False` означает неверный путь или неготовую установку vcpkg.

### Demo

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

### Полная локальная проверка

```powershell
cmake --workflow --preset debug-discipline-workflow
```

Дополнительные CI-конфигурации можно проверить локально:

```powershell
cmake --workflow --preset relwithdebinfo-discipline-workflow
cmake --workflow --preset asan-discipline-workflow
```

CI собирает demo в RelWithDebInfo, но не запускает графическое приложение. ASan проверяет unit-тесты и content validation без сборки или запуска demo.

### clang-tidy

```powershell
cmake --fresh --preset msvc-debug
pwsh -File scripts/run-clang-tidy.ps1 -BuildDirectory build/msvc-debug
```

Проверяются только production `.cpp` в `src/relaycore` и `src/relaygame`.

Локальные preset'ы в `CMakeUserPresets.json` могут также содержать машинно-специфичные пути к MSVC/Windows SDK. После обновления Visual Studio, Windows SDK или CLion эти пути нужно синхронизировать.

`BUILD_TESTING` обычно приходит из `include(CTest)`, а сам `CTest` рекомендуют подключать именно в верхнем `CMakeLists.txt`.

## clangd / compile_commands.json

В корневом `CMakeLists.txt` включён `CMAKE_EXPORT_COMPILE_COMMANDS`, чтобы clang-based tools могли использовать `compile_commands.json`. CMake документирует, что эта переменная генерирует точные compile commands, но реально работает для Makefile и Ninja generators.

Для проекта также есть `.clangd` с:

- `CompilationDatabase: Ancestors`
- `clang-tidy` checks для bugprone / analyzer / performance
- stricter include cleaner
- отдельное отключение диагностики для `extern/` и build-директорий

## Форматирование

В корневом `CMakeLists.txt` есть два utility-target'а:

- `format` — форматирует tracked C/C++ файлы
- `format-check` — проверяет формат без переписывания

`.clang-format` настроен на более стабильные diff'ы: без агрессивного выравнивания колонок и без one-line `if`/`function` конструкций.

## Документация

`Doxyfile` настроен на генерацию HTML + XML в `docs/doxygen/`, использует `README.md` как main page и фокусируется на публично полезной документации вместо шума от private/local symbols. Doxygen поддерживает конфигурацию через `Doxyfile`, а `CLANG_ASSISTED_PARSING` помогает разбирать современный C++ код.

Пример запуска:

```powershell
doxygen Doxyfile
```

## Структура репозитория

```text
src/
  relaycore/   # engine runtime
  relaygame/   # game/demo layer
assets/        # shaders, textures, audio, level data
cmake/         # helper modules and dependency glue
tests/         # doctest-based tests
```

### `src/relaycore/`

- `app/` — bootstrap, loop, frame timing, engine services
- `audio/` — аудиосистема
- `core/` — logging, config, paths, profiling helpers
- `debug/` — Dear ImGui overlay
- `gameplay/` — interaction descriptors/runtime
- `physics/` — collision и motion
- `platform/` — SDL / window / GL context
- `render/` — camera, shaders, renderer, diagnostics
- `world/` — blockout level loading/data

### `src/relaygame/`

- `demo_game.*` — demo/game glue
- `player_controller.*` — player-side logic
- `world_scene_renderer.*` — render bridge for current game world

## Принципы проекта


- debuggability важнее «умной» архитектуры
- явные границы между engine / game / data
- минимум скрытой магии
- профилирование и диагностика с самого начала

## vcpkg manifest

`vcpkg.json` описывает project dependencies. vcpkg использует manifest file `vcpkg.json` как декларативный источник зависимостей, а `builtin-baseline` нужен для repeatable version resolution в top-level project manifests.
