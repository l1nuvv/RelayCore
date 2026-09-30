# Архитектура RelayCore

Этот документ описывает актуальный runtime contract snapshot по состоянию на 2026-09-30. Направление работы и evidence находятся в [действующем плане](PLAYABLE_SLICE_ROADMAP.md); здесь описаны владельцы и границы текущего кода.

## Bootstrap

```text
main
→ make_demo_game() возвращает unique_ptr<AppDelegate>
→ run_application передаёт владение в Application
→ Application
→ EngineServices
→ callbacks DemoGame / AppDelegate
```

`Application` владеет `DemoGame` через `unique_ptr<AppDelegate>` и создаёт engine subsystems. Guard внутри `Application::run` вызывает `on_shutdown` до разрушения подсистем, в том числе при исключении из startup или frame callbacks. `EngineServices` — заимствующая facade к живым подсистемам `Application`, а не их владелец; через неё `DemoGame` получает доступ к этим подсистемам.

## Ownership

| Владелец | Ресурсы |
| --- | --- |
| `Application` | `AppDelegate`, SDL lifetime, config, `Window`, `Renderer`, `AudioSystem` |
| `Window` | SDL window и OpenGL context |
| `Renderer` | ImGui overlay, shaders, textures, shadow resources и другие GPU resources |
| `AudioSystem` | miniaudio backend |
| `DemoGame` | экран, текущая игровая сессия и её сцена, сохранение игрока, настройки, `LevelEditor`; на время редактора — приостановленная исходная сессия |
| `GameWorldRuntime` | gameplay/world runtime state |
| `WorldSceneRenderer` | подготовленная геометрия и представление объектов карты; общие ресурсы моделей заимствуются из кэша `Renderer` |
| `LevelEditor` | документ, история правок и подготовленная сцена редактора; документ живёт между пробными запусками |

`RelayGame::runtime` собирает игровой код один раз для приложения и проверок. Движок не включает заголовки игры. Задания (`mission_runtime`), снимки сессии (`runtime_model`) и правила предметов (`usable_interaction`) принадлежат игре; геометрические запросы, ввод и базовые механизмы дверей остаются в `relaycore`.

Экран задаёт режим ввода и паузы через `DemoGame::change_screen`. Обычный путь: меню → игра → пауза → игра/меню. При входе в редактор исходная сессия, сцена, время и фонарик временно сохраняются в `EditorReturnState`. Пробная сессия создаётся отдельно; F11 возвращает к документу, закрытие редактора возвращает к исходному экрану и сессии.

`prepare_level_session` загружает и проверяет уровень, применяет сохранение и готовит сцену до замены живой сессии. До установки `prepare_level_session_activation` проверяет применение камеры и подготавливает захват ввода. `replace_level_session` завершает предыдущую сессию, очищает локальный снимок встречи и диагностику, активирует подготовленную. Выход в меню освобождает игровые объекты и останавливает звуки.

`Renderer` уничтожается до `Window`, поэтому его OpenGL-ресурсы освобождаются, пока GL context ещё существует. ImGui overlay принадлежит `Renderer`; shadow resources также являются его RAII-ресурсами.

## Сохранение и локальный повтор встречи

[`SavedGame`](../src/relaygame/save_game.h) содержит `RuntimeLevelSnapshot`, player pose/motion, отпечаток карты, время игры, фонарик, состояния подвижных противников и плоскую историю неактивных карт. Запись использует формат 4; parser принимает 1/2/3/4. Выбранное оружие входит в snapshot; форматы 1–3 выбирают точное оружие, формат 4 требует допустимый выбор и наличие дробовика в инвентаре при его выборе. Применение дополнительно проверяет fingerprint, состав противников и совместимость runtime snapshot. Поэтому поддержка чтения старой версии не означает возможность продолжить любую старую карту.

`DemoGame` владеет дисковым прогрессом и историей посещённых карт. `save_progress` записывает разрешённую игровую сессию; editor/preview не записываются. Состояние смерти также сохраняется вместе с оставшимся временем до respawn: выход во время смерти не откатывает мир к предыдущему сохранению. После «Продолжить» завершается ожидание и игрок возвращается к spawn с полным здоровьем. Автосохранение срабатывает при смене задания, через 25 секунд, при паузе и выходе. Смерть сохраняет задачи, устройства и состояние врагов; это не откат к дисковому checkpoint. Формат сохранения остаётся 4.

F9 создаёт `EncounterSnapshot`: полный `SavedGame` текущей карты, время сцены и состояния эффектов окружения. F10 подготавливает восстановление на копии `GameWorldRuntime`, проверяет камеру и только затем заменяет runtime. Восстанавливаются движение и взгляд игрока, здоровье/тайминги боя, rover state, двери, предметы, миссия, simulation tick, фонарик и фазы оборудования. Краткие трассеры, hit confirmation и feedback устройств от отброшенной попытки очищаются. Ошибки подготовки публикуются в iteration diagnostics и сохраняют прежнюю активную сессию.

Локальный снимок принадлежит `DemoGame`, живёт только в памяти и сбрасывается при смене сессии или входе в редактор. Обработчики F9/F10 не вызываются в editor preview. Команда снимка обрабатывается до записи прогресса в этом кадре; после успешной команды синхронизируются счётчики autosave. Штатные последующие автосохранения и выход продолжают работать. Формат `progress.toml` ради локального повтора не менялся.

## Frame flow

```text
SDL events
→ update
→ renderer begin
→ game/world render
→ ImGui/UI
→ buffer swap
```

`GameWorldRuntime` ведёт fixed simulation: шаг `1/120` секунды, не более 8 fixed steps за frame. Gameplay и ImGui имеют явную input ownership policy: захваченное ImGui устройство не передаёт соответствующий gameplay input.

`WorldSceneRenderer` получает подготовленные данные и состояния объектов от игры. Player-facing HUD пока читает `PlayerDiagnostics` и `InteractionDiagnostics`: они публикуются даже при скрытой F2-панели. Обычный HUD показывает здоровье, действие, цель, предметы и актуальное предупреждение; счётчики боя остаются в diagnostics. Сообщения задач не перекрашивают весь кадр; попадание и урон имеют отдельные краткие сигналы. Более тяжёлые runtime/audio/inspector diagnostics зависят от видимости соответствующих инструментов. Это текущая связь UI с diagnostics, а не отдельная gameplay-система внутри renderer.

## Live reload

F7 перечитывает engine/render authoring config и текущую карту по её runtime path, подготавливает renderer resources, level runtime, world scene и камеру во временном состоянии. Изменение `game.level_path` в конфиге не переключает текущую карту при F7. После подготовки выполняются renderer commit, замена сессии и commit применённого config. Сессия начинает карту заново, локальный снимок и время сбрасываются. При ошибке подготовки прежняя сессия сохраняется, ошибка публикуется в diagnostics. Этот порядок не означает полной транзакционности для всех ошибок после commit; параметры создания окна/context остаются startup-only.

## Runtime paths

- По умолчанию assets: `<exe-directory>/assets`.
- `RELAYCORE_ASSET_ROOT` — только абсолютный override asset root.
- Основной конфиг: `assets/config/engine.toml`.
- Прогресс и настройки: SDL preference directory `RelayCore/RelayCore` (`%APPDATA%/RelayCore/RelayCore` на Windows), файлы `progress.toml` и `player_settings.toml`.
- Логи: SDL preference directory `l1nuvv/RelayCore`, подкаталог `logs`.
- Нормальный запуск не зависит от source-tree paths.

## Failure policies

- Ошибка инициализации аудио оставляет приложение работающим без звука и записывает warning.
- Ошибка инициализации ImGui backend отключает overlay с rollback его частичной инициализации.
- Неверный TOML type или нарушенный обязательный invariant в `engine.toml` вызывает явную ошибку с dotted key.

## Graphics backend direction

Текущий repository snapshot всё ещё работает на OpenGL 4.6; существующие GL ownership/lifetime rules выше остаются фактом, пока соответствующие этапы миграции реально не применены.

Direct Vulkan — утверждённый целевой graphics backend. Миграция parity-first: сначала переносится и проверяется текущее поведение renderer-а, затем отдельной программой развиваются production materials, HDR, lightmaps, probes, scalable lighting и другие Source 2-class capabilities. Не считать запланированные Vulkan-компоненты уже существующими по одному наличию этого раздела.

Долгосрочные engine/world/render constraints находятся в `ENGINE_ARCHITECTURE_REFERENCE.md`, порядок перехода — в `VULKAN_MIGRATION_PLAN.md`, approved third-party foundations и staged vcpkg policy — в `DEPENDENCY_STACK.md`. Эти документы задают направление и границы, но не означают, что Jolt, Recast, fastgltf, KTX, Steam Audio или другие будущие зависимости уже интегрированы.
