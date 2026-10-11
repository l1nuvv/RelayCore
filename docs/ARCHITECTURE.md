# Архитектура RelayCore

Текущие runtime owners, update/frame flow, paths и failure policies. Планы и статусы — [roadmap](PLAYABLE_SLICE_ROADMAP.md) и [renderer-план](RENDERER_MODERNIZATION_PLAN.md); запуск и диагностика — [DEVELOPMENT](DEVELOPMENT.md).

## Player UI ownership (RmlUi 6.3)

`Renderer` owns the RmlUi context, SDL platform/IME interface and
`VulkanRmlRenderer`. Game state supplies plain `HudPresentation` and queued menu
actions. `DemoGame::change_screen` derives document visibility from the existing
game screen; UI documents do not own a second game state machine. Actions execute
at the next game update, before session changes. The single SDL loop routes menus
to RmlUi, gameplay to the controller, and developer tools to ImGui. Passive HUD
does not capture input. Renderer appends UI after scene display conversion and
before the optional developer overlay, on its existing Vulkan device and VMA.
Two scene slots retain RmlUi draw snapshots, geometry, atlases, descriptors and
pipelines through their final GPU use. Presentation drains before device/allocator
destruction; ImGui and readback remain synchronous.
See [authoring and verification](DEVELOPMENT.md#игровой-ui-rmlui-63).

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
| `Window` | SDL Vulkan window; surface создаёт и освобождает `VulkanDevice` |
| `Renderer` | `VulkanDevice`, `VulkanSwapchain`, scene backend, RmlUi player UI, ImGui developer overlay и кэш CPU-моделей; backend владеет GPU mesh/texture/shadow resources |
| `AudioSystem` | miniaudio backend |
| `DemoGame` | экран, текущая игровая сессия и её сцена, сохранение игрока, настройки, `LevelEditor`; на время редактора — приостановленная исходная сессия |
| `GameWorldRuntime` | gameplay/world runtime state |
| `WorldSceneRenderer` | подготовленная геометрия и представление объектов карты; общие ресурсы моделей заимствуются из кэша `Renderer` |
| `LevelEditor` | документ, история правок и подготовленная сцена редактора; документ живёт между пробными запусками |

`RelayGame::runtime` собирает игровой код один раз для приложения и проверок. Движок не включает заголовки игры. Задания (`mission_runtime`), снимки сессии (`runtime_model`) и правила предметов (`usable_interaction`) принадлежат игре; геометрические запросы, ввод и базовые механизмы дверей остаются в `relaycore`.

Экран задаёт режим ввода и паузы через `DemoGame::change_screen`. Обычный путь: меню → игра → пауза → игра/меню. При входе в редактор исходная сессия, сцена, время и фонарик временно сохраняются в `EditorReturnState`. Пробная сессия создаётся отдельно; F11 возвращает к документу, закрытие редактора возвращает к исходному экрану и сессии.

`prepare_level_session` загружает и проверяет уровень, применяет сохранение и готовит сцену до замены живой сессии. До установки `prepare_level_session_activation` проверяет применение камеры и подготавливает захват ввода. `replace_level_session` завершает предыдущую сессию, очищает локальный снимок встречи и диагностику, активирует подготовленную. Выход в меню освобождает игровые объекты и останавливает звуки.

`Renderer` уничтожается до `Window`. При shutdown сначала завершается presentation и освобождается swapchain, затем scene backend и его preview descriptors, затем RmlUi context/resources и ImGui. Vulkan device и SDL window остаются живы до освобождения этих ресурсов. CPU mesh/texture data общие и неизменяемые; backend хранит weak references к источникам и удаляет устаревшие GPU cache entries после завершения scene fence.

## Сохранение и локальный повтор встречи

[`SavedGame`](../src/relaygame/save_game.h) содержит `RuntimeLevelSnapshot`, player pose/motion, отпечаток карты, время игры, фонарик, состояния подвижных противников и плоскую историю неактивных карт. Запись использует формат 5; parser принимает 1–5. Формат 5 добавляет состояние сдерживания G-019. Выбранное оружие входит в snapshot; форматы 1–3 выбирают точное оружие, форматы 4–5 требуют допустимый выбор и наличие дробовика в инвентаре при его выборе. Применение дополнительно проверяет fingerprint, состав противников и совместимость runtime snapshot. Поэтому поддержка чтения старой версии не означает возможность продолжить любую старую карту.

`DemoGame` владеет дисковым прогрессом и историей посещённых карт. `save_progress` записывает разрешённую игровую сессию; editor/preview не записываются. Состояние смерти также сохраняется вместе с оставшимся временем до respawn: выход во время смерти не откатывает мир к предыдущему сохранению. После «Продолжить» завершается ожидание и игрок возвращается к spawn с полным здоровьем. Автосохранение срабатывает при смене задания, через 25 секунд, при паузе и выходе. Смерть сохраняет задачи, устройства и состояние врагов; это не откат к дисковому checkpoint. Текущий формат сохранения — 5.

F9 создаёт `EncounterSnapshot`: полный `SavedGame` текущей карты, время сцены и состояния эффектов окружения. F10 подготавливает восстановление на копии `GameWorldRuntime`, проверяет камеру и только затем заменяет runtime. Восстанавливаются движение и взгляд игрока, здоровье/тайминги боя, rover state, двери, предметы, миссия, simulation tick, фонарик и фазы оборудования. Краткие трассеры, hit confirmation и feedback устройств от отброшенной попытки очищаются. Ошибки подготовки публикуются в iteration diagnostics и сохраняют прежнюю активную сессию.

Локальный снимок принадлежит `DemoGame`, живёт только в памяти и сбрасывается при смене сессии или входе в редактор. Обработчики F9/F10 не вызываются в editor preview. Команда снимка обрабатывается до записи прогресса в этом кадре; после успешной команды синхронизируются счётчики autosave. Штатные последующие автосохранения и выход продолжают работать. Формат `progress.toml` ради локального повтора не менялся.

## Frame flow

```text
SDL events
→ update
→ renderer begin
→ game/world queue и viewmodel
→ reuse completed scene slot / Vulkan scene submit
→ RmlUi submit → optional synchronous ImGui developer submit
→ acquire / image blit on scene timeline / present
```

`GameWorldRuntime` ведёт fixed simulation: шаг `1/120` секунды, не более 8 fixed steps за frame. RmlUi menu захватывает игровой ввод; passive HUD/finale не захватывают его. Захваченное ImGui developer device также не передаёт соответствующий gameplay input.

Scene backend owns two slots with separate command pools/buffers, mutable UBOs, material/tone-mapping descriptor sets and color/depth targets. Scene, RmlUi and presentation copy signal the same serialized graphics timeline; slot reuse waits for its final copy. Shared shadows and pipelines remain ordered on that timeline. Mesh/material construction uses GPU upload dependencies, with 64 MiB/32-batch staging backpressure; readback, ImGui, reflection replacement, reload and shutdown retain deliberate waits. `frame_backend` retains the prepared backend through presentation during live reload; backend destruction drains both slots. There is no normal-frame `vkDeviceWaitIdle`.

Material descriptor sets share renderer-owned persistent pools. Each material retains its two slot sets until scene/upload completion permits retirement; released sets can be reused, and collection reclaims empty extra pools while retaining one reusable block. Tone-mapping and UI descriptor ownership remain with their existing pass owners.

`WorldSceneRenderer` получает подготовленные данные и состояния объектов от игры. `GameWorldRuntime::build_hud_presentation` читает combat/mission/inventory/usable state напрямую и передаёт минимальный `HudPresentation`; HUD не читает diagnostics. Обычный HUD показывает здоровье, действие, цель, предметы и актуальное предупреждение; счётчики боя остаются в diagnostics. Сообщения задач не перекрашивают весь кадр; попадание и урон имеют отдельные краткие сигналы. Более тяжёлые runtime/audio/inspector diagnostics зависят от видимости соответствующих инструментов.

## Live reload

F7 перечитывает engine/render authoring config и текущую карту по её runtime path, подготавливает renderer resources, level runtime, world scene и камеру во временном состоянии. Изменение `game.level_path` в конфиге не переключает текущую карту при F7. После подготовки выполняются renderer commit, замена сессии и commit применённого config. Сессия начинает карту заново, локальный снимок и время сбрасываются. При ошибке подготовки прежняя сессия сохраняется, ошибка публикуется в diagnostics. Этот порядок не означает полной транзакционности для всех ошибок после commit; параметры создания окна/device остаются startup-only.

## Runtime paths

- По умолчанию assets: `<exe-directory>/assets`.
- `RELAYCORE_ASSET_ROOT` — только абсолютный override asset root.
- Runtime root содержит скопированные authored assets и собранные `shaders/vulkan/*.spv`; исходный `assets/` сам по себе недостаточен для запуска renderer.
- Основной конфиг: `assets/config/engine.toml`.
- Прогресс и настройки: SDL preference directory `RelayCore/RelayCore` (`%APPDATA%/RelayCore/RelayCore` на Windows), файлы `progress.toml` и `player_settings.toml`.
- Логи: SDL preference directory `l1nuvv/RelayCore`, подкаталог `logs`.
- Нормальный запуск не зависит от source-tree paths.

## Failure policies

- Ошибка инициализации аудио оставляет приложение работающим без звука и записывает warning.
- Ошибка SDL3 ImGui binding отключает overlay; ошибка Vulkan ImGui binding освобождает частично созданные ресурсы и передаёт исключение вызывающему коду.
- Неподдерживаемый Vulkan device/presentation contract останавливает startup с диагностикой; fallback backend отсутствует.
- Неверный TOML type или нарушенный обязательный invariant в `engine.toml` вызывает явную ошибку с dotted key.

## Graphics backend direction

Direct Vulkan — единственный runtime backend после VK-013; с 2026-10-12 обязательный минимум loader/GPU — API 1.4. Instance/effective/VMA API — 1.4, версия видна в renderer-owned capabilities; fallback и forced-1.3 mode удалены. VK-014 завершил ревью ownership/error paths и снял локальный Tracy/RenderDoc baseline. Loader API 1.4 недостаточно: startup проверяет dynamic rendering, synchronization2, timeline semaphore, SDL graphics/present queues, `VK_KHR_swapchain`, `VK_KHR_swapchain_maintenance1` / `swapchainMaintenance1`, `VK_KHR_surface_maintenance1` и `VK_KHR_get_surface_capabilities2`. Tracy-сборка дополнительно требует host query reset и timestamp queries на graphics queue. Эти требования осознанно ограничивают совместимость; её подтверждает реальный запуск, не toolchain probe.

E-003 вводит первый native MeshAsset/ModelAsset product: GLB компилируется отдельным инструментом в `.rcmodel`, engine читает CPU geometry, opaque RGBA материалы и transforms частей. `StaticModelCache` разделяет неизменяемые ресурсы; game передаёт части в существующую очередь, backend владеет GPU uploads. OBJ/BMP остаются действующим путём прежних моделей.

IMG-01–03 extend native v4 materials with linear metallic/roughness and AO maps,
preserving v1–3 lighting. A game-owned prepared scene may supply one static
reflection cube with bounds; the engine owns its validated CPU product, Vulkan
upload and descriptors. Inside its bounds, box-projected GGX IBL replaces SH
specular; material AO attenuates indirect illumination only. Capture/staleness
inputs are documented in [RENDERING](RENDERING.md#static-reflection-probes-img-02). Lightmaps and dynamic reflections
remain separate capabilities. IMG-06 adds native v5 emissive
textures/factors; the reader accepts products v1–5.

Долгосрочные engine/world/render constraints находятся в `ENGINE_ARCHITECTURE_REFERENCE.md`, порядок перехода — в `VULKAN_MIGRATION_PLAN.md`, approved third-party foundations и staged vcpkg policy — в `DEPENDENCY_STACK.md`. Эти документы задают направление и границы, а фактические этапы интеграции фиксирует roadmap.

`physics::WorldQuery` обслуживает combat/LOS/focus и movement. Карты без native collision сохраняют AABB backend. Если `prop_static` явно задаёт collision product, `LevelSessionBuilder::build()` загружает отдельный `.rccollision`, проверяет pairing с моделью, применяет placement transform и создаёт immutable Jolt `PhysicsWorld` вместе с world colliders. Render triangles не становятся коллизией автоматически. `GameWorldRuntime` получает shared ownership при activation; копии prepared session разделяют immutable collision data, queries не выносят Jolt handles за границу physics. `Application` collision сценой не владеет. Контракт авторинга, компиляции и staging — в [COLLISION_ASSETS.md](COLLISION_ASSETS.md).
