# Разработка RelayCore 1.0

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

```powershell
cmake --build --preset demo-debug --target relaycore_demo relaycore_tests relaycore_content_validation relaycore_game_lifecycle
ctest --test-dir build/msvc-debug -R '^(relaycore\.|validation\.|graphics\.game_lifecycle$)' --output-on-failure
```

Для графического теста нужен рабочий OpenGL. Дополнительные пресеты и полный
порядок проверок перечислены в корневом README; их не требуется запускать после
каждого редактирования карты.

### Профилирование

Профиль `msvc-relwithdebinfo` включает Tracy. Для сравнимого захвата:

```powershell
cmake --preset msvc-relwithdebinfo
cmake --build --preset demo-relwithdebinfo
```

Запустите собранный `build/msvc-relwithdebinfo/src/relaygame/RelayCore.exe` и
подключите [Tracy 0.13.1](https://github.com/wolfpld/tracy/releases/tag/v0.13.1).
На сценарии «Два сектора» запишите обычный кадр в мастерской, переход в насосную
и редактирование комнаты через F11. Сравнивайте CPU/GPU зоны на одном разрешении,
одном маршруте и одинаковых настройках. Выделения памяти сейчас отдельно не
размечены; этот профиль не позволяет оценить их количество.

Для 1.0 снят воспроизводимый захват на графическом сценарии
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
