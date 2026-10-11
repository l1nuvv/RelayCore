# RelayCore

RelayCore — собственный C++20-движок и GoldSource-inspired indoor FPS для Windows 11. Runtime требует SDL3 и Vulkan 1.4 для loader/GPU; игровой HUD и меню — RmlUi 6.3, редактор и диагностика — Dear ImGui. Зависимости закреплены через vcpkg manifest mode.

[Скачать последнюю сборку](https://github.com/l1nuvv/RelayCore-releases/releases/latest) · [Документация](docs/README.md) · [Выпуск и обновления](docs/UPDATES.md)

## Текущее состояние

Версия проекта — **1.4.1** (`VERSION`, vcpkg manifest). [Протокол выпуска](docs/playtests/release-1.4.1.md) хранит проверки опубликованного кандидата; последующие изменения рабочего дерева не входят в него автоматически. Версия продукта 1.4.1 не означает поддержку Vulkan API 1.4.

Главный игровой трек — G-019, **«Контур сдерживания»** (`containment_loop.toml`): связная глава с подготовкой оборудования, центральной угрозой, обходами и возвращением. Глава принята автором 10 октября (`done`). После замечания о пустом окружении выполнен технический проход наполнения помещений; его итоговая художественная приёмка остаётся открытой. Целевые 20–30 минут не измерены независимым проходом; отдельные AI/UI/material gates сохраняют свои статусы в [roadmap](docs/PLAYABLE_SLICE_ROADMAP.md).

По умолчанию меню предлагает **«Кампания — весь комплекс»**: восемь связанных карт от восстановления питания до сдерживания Хранителя. Переходы сохраняют инвентарь, здоровье и состояния посещённых карт; «Продолжить» восстанавливает текущую карту. [Маршрут и переходы](docs/EDITOR_AND_CAMPAIGN.md#сквозное-прохождение-комплекса).

В меню также доступны прежние короткие сценарии: «Восстановление питания», «Перепуск давления», «Два сектора» и другие испытательные карты. Они сохраняются для игры и регрессионных проверок. Карта по умолчанию в `assets/config/engine.toml` — `power_restoration.toml`; нужный сценарий выбирается в меню.

Движок поддерживает fixed-step simulation, движение и столкновения, взаимодействия и задания, два оружия, турели, мобильных противников, сохранения и переходы между TOML-картами. Статические GLB компилируются в native `.rcmodel`; материалы поддерживают normal, metallic/roughness, AO и emissive maps, статические reflection probes. Native collision — отдельный `.rccollision`; карты без явного opt-in используют AABB. OBJ/BMP остаются для прежних ресурсов. Есть встроенный редактор, полигон, CLI-проверка карт, звук, unit/content/graphics tests и Tracy.

Модернизация renderer описана в [отдельном плане](docs/RENDERER_MODERNIZATION_PLAN.md): снят технический baseline G-019, реализованы lifetime/scheduling VK14-03/04, runtime переведён на обязательный Vulkan 1.4. Там же записаны проверки и открытые hardware/human gates. Новые эффекты остаются следующими этапами.

## Сборка

Нужны Visual Studio с C++ workload и Windows SDK, CMake 4.0+, Ninja, Git, PowerShell 7 и vcpkg. Откройте Visual Studio Developer PowerShell для x64 и задайте путь к установленному vcpkg:

```powershell
$env:VCPKG_ROOT = 'C:\dev\vcpkg'
Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --preset msvc-dev
cmake --build --preset demo-dev
.\build\msvc-dev\src\relaygame\RelayCore.exe
```

`Test-Path` должен вернуть `True`. `msvc-dev` — оптимизированная инкрементальная сборка с символами, PCH и Tracy. `msvc-debug` служит для отладки/validation, `msvc-relwithdebinfo` — для профилирования с Unity/IPO, `msvc-release` — для раздачи без Tracy. Подробности окружения, CLion и ресурсов — в [DEVELOPMENT.md](docs/DEVELOPMENT.md#сборка-с-нуля).

Для запуска нужен драйвер, удовлетворяющий Vulkan device/presentation contract; одной поддержки API 1.3 недостаточно. [Точные требования](docs/ARCHITECTURE.md#graphics-backend-direction). Release не требует Vulkan SDK; Debug validation требует установленных loader/layers LunarG Vulkan SDK. SPIR-V компилируется закреплённым host-tool Slang из vcpkg.

## Проверки

```powershell
cmake --preset msvc-debug
cmake --build --preset tests-debug
ctest --preset test-debug --output-on-failure
cmake --build --preset validation-debug
ctest --preset validation-debug --output-on-failure
```

Для выбранных graphics-проверок сначала соберите нужный target либо `graphics-debug`, затем запустите соответствующий фильтр `ctest --preset graphics-debug`. Они требуют работающего Vulkan GPU и интерактивного Windows desktop. `Skipped` не подтверждает успешную проверку. [Выбор узких проверок и диагностика](docs/DEVELOPMENT.md#узкие-проверки).

## Релиз

```powershell
pwsh -NoProfile -File scripts/release.ps1
```

Команда сама загружает окружение Visual Studio, повышает patch, собирает Release, проверяет контент/updater, создаёт ZIP и проверяет установку/запуск игры. Готовый кандидат фиксируется в `build/release/candidate.json`. Перед подготовкой обновите `release/changes.json` реальными изменениями. Публикация проверенного кандидата:

```powershell
pwsh -NoProfile -File scripts/release.ps1 -Publish
```

**RelayCore — Release** в CLion или `scripts/release.ps1 -Release` объединяет подготовку и публикацию. [Повтор после сбоя, версия, SHA256 и канал](docs/UPDATES.md). Для обычной разработки архив не требуется.

## Игра и управление

[Инструкция участнику без решения маршрута](docs/PLAYTEST_GUIDE.txt). Прогресс и настройки находятся в `%APPDATA%/RelayCore/RelayCore`, отдельно от установки; логи — в `%APPDATA%/l1nuvv/RelayCore/logs`. Обновление карты может сделать сохранение несовместимым по fingerprint. Текущий формат записи — 5, читаются 1–5.

1/2 выбирают оружие. F9/F10 сохраняют и восстанавливают локальный снимок встречи в памяти; смена сессии или вход в редактор сбрасывает его. Редакторский preview не заменяет пользовательское сохранение.

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

Mouse look и стрельба требуют relative mouse mode. Игровые меню получают ввод через RmlUi; ImGui обслуживает editor/diagnostics. Владение вводом описано в [DEVELOPMENT.md](docs/DEVELOPMENT.md#игровой-ui-rmlui-63).
