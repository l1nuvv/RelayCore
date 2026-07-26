# Архитектура RelayCore

Этот документ описывает текущий runtime contract, а не API reference.

## Bootstrap

```text
main
→ создаёт DemoGame
→ run_application(DemoGame)
→ Application
→ EngineServices
→ callbacks DemoGame / AppDelegate
```

`main` владеет локальным `DemoGame` до завершения `run_application` и передаёт его в engine как реализацию `AppDelegate`. `run_application` создаёт `Application`, а тот создаёт engine subsystems. `EngineServices` — заимствующая facade к живым подсистемам `Application`, а не их владелец; через неё `DemoGame` получает доступ к этим подсистемам.

## Ownership

| Владелец | Ресурсы |
| --- | --- |
| `Application` | SDL lifetime, config, `Window`, `Renderer`, `AudioSystem` |
| `Window` | SDL window и OpenGL context |
| `Renderer` | ImGui overlay, shaders, textures, shadow resources и другие GPU resources |
| `AudioSystem` | miniaudio backend |
| `DemoGame` | `GameWorldRuntime`, `WorldSceneRenderer`, сохранённый runtime snapshot |
| `GameWorldRuntime` | gameplay/world runtime state |

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
