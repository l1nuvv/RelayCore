# RelayCore

![RelayCore](content/graphics/branding/relaycore-wordmark.svg)

Собственный C++20-движок и GoldSource-inspired indoor FPS для Windows 11. SDL3/Vulkan, статические native assets, встроенный редактор, сохранения и переходы между картами. Игровой HUD/меню — RmlUi; диагностика и editor UI — ImGui.

[Скачать сборку](https://github.com/l1nuvv/RelayCore-releases/releases/latest) · [Документация](docs/README.md) · [Выпуск и обновления](docs/UPDATES.md)

## Состояние проекта

Версия задаётся в [VERSION](VERSION). Gameplay/content и открытая приёмка находятся в [roadmap](docs/PLAYABLE_SLICE_ROADMAP.md), VK14 milestones — в [renderer-плане](docs/RENDERER_MODERNIZATION_PLAN.md). [Протоколы выпусков](docs/playtests/) описывают проверенные payloads своих дат и не подтверждают последующие изменения исходников.

Главный игровой сценарий — «Контур сдерживания»; меню также предлагает связанную кампанию и короткие проверочные карты. [Маршрут, переходы и сохранения](docs/EDITOR_AND_CAMPAIGN.md#сквозное-прохождение-комплекса), [инструкция игроку без решения маршрута](docs/PLAYTEST_GUIDE.txt).

## Быстрый старт разработчика

Нужны Visual Studio с C++ workload и Windows SDK, CMake 4.0+, Ninja, Git, PowerShell 7 и vcpkg. Откройте Visual Studio Developer PowerShell для x64:

```powershell
$env:VCPKG_ROOT = 'C:\dev\vcpkg'
cmake --preset msvc-dev
cmake --build --preset demo-dev
.\build\msvc-dev\src\relaygame\RelayCore.exe
```

Loader и GPU должны поддерживать Vulkan 1.4 и [требуемые device/presentation features](docs/ARCHITECTURE.md#graphics-backend-direction). Для Debug validation нужны LunarG loader/layers; для Release Vulkan SDK не требуется.

[Настройка окружения и CLion](docs/DEVELOPMENT.md#сборка-с-нуля) · [Выбор build/tests](docs/DEVELOPMENT.md#узкие-проверки) · [Capture/validation](docs/DEVELOPMENT.md#vulkan-capture-и-validation) · [Профилирование](docs/DEVELOPMENT.md#профилирование).

В обычной разработке выполняются релевантные build/tests. Подготовка архива и публикация — отдельный [release workflow](docs/UPDATES.md), запускаемый по запросу.

## Игра и управление

| Ввод | Действие |
| --- | --- |
| W/A/S/D | движение |
| Space | прыжок |
| Shift | ускорение |
| Mouse | обзор |
| ЛКМ | атака |
| 1/2 | выбор оружия |
| E | взаимодействие |
| Escape | пауза / продолжить; выход через меню |
| F | фонарик |
| F11 | редактор / играть |
| F1 | захват мыши |
| F2 | diagnostics |
| F3 | noclip |
| F4 | inspector visualization |
| F5 | pin inspector |
| F6 | advanced diagnostics |
| F7 | live reload |
| F8 | открыть источник в inspector |
| F9 | сохранить локальный снимок встречи |
| F10 | восстановить локальный снимок встречи |
| V | vsync |

Mouse look и стрельба требуют relative mouse mode. F9/F10 работают с локальным снимком в памяти; смена сессии или вход в редактор сбрасывает его. Пользовательское сохранение и совместимость — [отдельный runtime contract](docs/ARCHITECTURE.md#сохранение-и-локальный-повтор-встречи).
