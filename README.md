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
- CMake 3.28+
- Git
- vcpkg
- Ninja или другой generator из ваших preset'ов

## Сборка

Проект использует `CMakePresets.json`, а зависимости ставятся через `vcpkg.json` в manifest mode. Такой режим vcpkg рекомендует как основной для большинства пользователей; он также нужен для versioning и project-local dependency trees.

Пример типичного запуска:

```powershell
cmake --preset msvc-debug
cmake --build --preset demo-debug
```

Если есть тестовый preset:

```powershell
ctest --preset tests-debug --output-on-failure
```

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

- маленькие вертикальные срезы вместо больших абстрактных переписок
- debuggability важнее «умной» архитектуры
- явные границы между engine / game / data
- минимум скрытой магии
- профилирование и диагностика с самого начала

## vcpkg manifest

`vcpkg.json` описывает project dependencies. vcpkg использует manifest file `vcpkg.json` как декларативный источник зависимостей, а `builtin-baseline` нужен для repeatable version resolution в top-level project manifests.

## Следующий разумный шаг

Сделать один законченный игровой цикл в комнате:

1. spawn
2. move + collision
3. use interaction
4. одна дверь / одна кнопка / один триггер
5. один полированный тестовый уровень
