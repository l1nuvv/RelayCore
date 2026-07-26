# RelayCore

**RelayCore** — приватный учебный C++20-проект: небольшой GoldSrc-inspired indoor FPS vertical slice. Текущий режим — maintenance и исправление подтверждённых пунктов аудита; новые функции не являются основной задачей. Публичная поставка и packaging не поддерживаются.

Проект использует SDL3, OpenGL 4.6 Core, CMake, vcpkg manifest mode, Dear ImGui, Tracy и data-driven blockout pipeline.

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

## Требования

Минимум для рабочего dev-окружения:

- Windows 11
- Visual Studio 2022 / MSVC toolchain
- CMake 4.0.0+
- Git
- vcpkg
- Ninja

## Сборка и проверки

Проект использует `CMakePresets.json`, а зависимости ставятся через `vcpkg.json` в manifest mode.

Перед первой командой CMake откройте `Developer PowerShell for VS 2022` или `Developer Command Prompt for VS 2022`, чтобы `cl.exe`, `rc.exe` и Windows SDK tools уже были доступны в окружении.

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

CMake копирует каталог `assets` рядом с demo executable. По умолчанию runtime использует `<exe-directory>/assets`; `RELAYCORE_ASSET_ROOT` допускается только как абсолютный override. Основной конфиг — `assets/config/engine.toml`.

## Управление

| Ввод | Действие |
| --- | --- |
| W/A/S/D | движение |
| Space | прыжок |
| Shift | ускорение |
| Mouse | обзор |
| ЛКМ | атака |
| E | взаимодействие |
| Escape | выход |
| F1 | переключение захвата мыши |
| F2 | diagnostics |
| F3 | noclip |
| F4 | inspector visualization |
| F5 | pin inspector |
| F6 | advanced diagnostics |
| F7 | live reload |
| F8 | открыть источник в inspector |
| F9 | сохранить runtime snapshot |
| F10 | восстановить runtime snapshot |
| V | переключить vsync |

Mouse look и fire требуют relative mouse mode. Gameplay keyboard/mouse input блокируется, когда ImGui захватывает соответствующее устройство; global debug shortcuts остаются доступны.

## Документы

- [Краткая архитектура](docs/ARCHITECTURE.md)
- [Maintenance checkpoint](STATUS.md)

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
