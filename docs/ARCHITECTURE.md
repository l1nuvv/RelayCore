# Архитектура RelayCore

Этот документ описывает текущий runtime contract, а не API reference.

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
| `LevelEditor` | документ, история правок и подготовленная сцена редактора; документ живёт между пробными запусками |

`RelayGame::runtime` собирает игровой код один раз для приложения и проверок. Движок не включает заголовки игры. Задания (`mission_runtime`), снимки сессии (`runtime_model`) и правила предметов (`usable_interaction`) принадлежат игре; геометрические запросы, ввод и базовые механизмы дверей остаются в `relaycore`.

Экран задаёт режим ввода и паузы через `DemoGame::change_screen`. Обычный путь: меню → игра → пауза → игра/меню. При входе в редактор исходная сессия, сцена, время и фонарик временно сохраняются в `EditorReturnState`. Пробная сессия создаётся отдельно; F11 возвращает к документу, закрытие редактора возвращает к исходному экрану и сессии.

`prepare_level_session` загружает и проверяет уровень, применяет сохранение и готовит сцену до замены живой сессии. `replace_level_session` завершает предыдущую сессию, очищает диагностические снимки и активирует подготовленную. Выход в меню освобождает игровые объекты и останавливает звуки. Сохранение версии 1 содержит путь карты и отпечаток её содержимого; несовместимость обнаруживается до замены сессии.

`Renderer` уничтожается до `Window`, поэтому его OpenGL-ресурсы освобождаются, пока GL context ещё существует. ImGui overlay принадлежит `Renderer`; shadow resources также являются его RAII-ресурсами.

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

## Live reload

Reload сначала подготавливает config, renderer resources, level runtime и world scene во временном состоянии. `Renderer` commit происходит только после успешной подготовки renderer resources; затем старые runtime/world завершаются и заменяются подготовленными. Неудачная подготовка shadow resources не должна разрушать прежнее live renderer state. Этот порядок не означает полной транзакционности для всех ошибок после commit.

## Runtime paths

- По умолчанию assets: `<exe-directory>/assets`.
- `RELAYCORE_ASSET_ROOT` — только абсолютный override asset root.
- Основной конфиг: `assets/config/engine.toml`.
- Логи: SDL preference directory, подкаталог `logs`.
- Нормальный запуск не зависит от source-tree paths.

## Failure policies

- Ошибка инициализации аудио оставляет приложение работающим без звука и записывает warning.
- Ошибка инициализации ImGui backend отключает overlay с rollback его частичной инициализации.
- Неверный TOML type или нарушенный обязательный invariant в `engine.toml` вызывает явную ошибку с dotted key.
