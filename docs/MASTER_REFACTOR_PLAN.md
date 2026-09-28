# RelayCore — MASTER REFACTOR PLAN

Исторический аудит и набор предложений RF. С 2026-09-27 действующая очередь — [PLAYABLE_SLICE_ROADMAP](PLAYABLE_SLICE_ROADMAP.md). Сведения о состоянии дерева, проверках и «следующих» шагах ниже относятся к датам аудита/записей. Не запускать RF-очередь автоматически: каждую проблему перепроверять по текущему коду и брать только в согласованный scope.

Audit snapshot: commit 413cbf8c120e05f348b1de24307129c80f8440f3. Statements below about a clean working tree and checks not run describe the audit date. RF-001 through RF-003 were subsequently implemented in the working tree on 2026-09-23; they remain uncommitted.

**RelayCore не нуждается в переписывании архитектуры.** Основной выигрыш дадут исправления конкретных нарушений инвариантов: обработка ошибок при замене сессии, числовая валидация, корректность render queues и надёжность проверок.

Аудит выполнен по текущему рабочему каталогу, commit `413cbf8c120e05f348b1de24307129c80f8440f3`. Рабочее дерево осталось чистым. Файлы не изменялись, RF-этапы не выполнялись.

Это статический аудит исходников, зависимостей, CMake, тестов и инструментов. Сборку, runtime, ASan и Tracy captures я не запускал; соответствующие выводы ниже не выдаются за воспроизведённые сбои. Инвентаризация обнаружила 177 собственных C++ файлов и 244 объявления `TEST_CASE`; наличие теста не означает, что он был выполнен.

**1. Executive Summary**

Фактически RelayCore — небольшой специализированный FPS runtime с редактором, кампанией, сохранениями и OpenGL renderer. Его устройство уже пропорционально масштабу проекта:

- две основные static libraries и небольшой executable;
- последовательный main loop;
- фиксированный шаг игровой симуляции;
- преимущественно value ownership;
- RAII для основных SDL/GL ресурсов;
- подготовка новой сессии до её установки;
- отдельные CPU- и GPU-представления мира;
- содержательная проверка shipped content.

Основные слабые места:

1. Замена сессии начинается до завершения потенциально падающих операций. В обработчике ошибки перехода дополнительно остаётся ссылка на уже уничтоженный объект.
2. Валидация конечности отдельных чисел не гарантирует безопасность последующих преобразований, вычислений и spatial indexing.
3. Формирование render queues содержит ошибки в отборе shadow casters, общем порядке прозрачности и компараторе локального света.
4. Live reload не имеет согласованного источника актуальной конфигурации.
5. Graphics/lifecycle-проверки не включены в стандартные CI workflows; часть профилировочных зон измеряет неправильный участок.
6. Есть отдельные дефекты инструментов: несинхронное удаление runtime assets и нарушение partial staging в pre-commit hook.

**Рекомендация:** сначала correctness и проверяемость. Архитектурную перекладку каталогов, массовые rename и новые framework-компоненты не включать.

---

**2. Actual Architecture**

Фактические build boundaries:

```text
relaycore_demo
    └── RelayGame::runtime
            └── RelayCore::engine
                    ├── SDL3 / OpenGL / glad
                    ├── miniaudio
                    ├── ImGui
                    ├── glm / toml++ / spdlog
                    └── Tracy при включённой инструментализации
```

Основные namespaces: `RelayCore`, `RelayCore::core`, `render`, `physics`, `gameplay`, `debug`, `detail`, а также `RelayGame`.

`relaycore_engine` содержит больше, чем платформенный движок: blockout format, interaction/combat primitives, игровой HUD и структуры игровой диагностики. Это **семантически смешанная**, но не циклическая граница. По проверенным связям engine не включает `relaygame`.

Основной control flow:

```text
main
 → run_application: logging и обработка исключений
 → Application: SDL → config → window/context → renderer → audio
 → AppDelegate::on_startup
 → события SDL
 → DemoGame::on_update
     → GameWorldRuntime: fixed simulation, 120 Hz, максимум 8 шагов
     → presentation/audio/environment
 → DemoGame::on_render
     → WorldSceneRenderer формирует RenderItem
     → Renderer выполняет shadow/main passes
 → UI → swap
 → on_shutdown
 → уничтожение engine resources
```

Источники: [Application](../src/relaycore/app/application.cpp#L91), [fixed loop](../src/relaygame/fixed_simulation_loop.h#L27), [runtime update](../src/relaygame/game_world_runtime_update.cpp#L14).

Поток данных уровня:

```text
TOML
 → BlockoutLevel: geometry, entity properties, provenance
 → LevelSessionBuilder
     → typed interactions, combat, mission, exits, environment
 → GameWorldRuntime: изменяемое состояние
 → PreparedWorldSceneData
 → WorldSceneRenderer: meshes и model instances
 → RenderQueue: временные non-owning ссылки
 → Renderer: OpenGL
```

Игровая симуляция выполняет movement, fire/use, triggers, doors, enemies, combat и mission в явном последовательном порядке. Environment/audio presentation обновляется с frame delta. Поэтому фиксированный timestep здесь **не означает полной детерминированности всего приложения**.

Редактор хранит отдельный document и GPU scene. При входе в редактор текущая игровая сессия перемещается в `EditorReturnState`; preview создаёт другую сессию. Это разумная модель изоляции.

Сохранение состоит из runtime snapshot, состояния игрока, fingerprint уровня, состояния enemies и истории посещённых карт. Дисковая запись предварительно проверяет сериализуемые данные и использует временный файл с последующим rename.

---

**3. KEEP**

| Решение | Почему сохранить |
|---|---|
| `Application` + `AppDelegate` | Оправданная граница engine/game lifecycle. Имеются реальные lifecycle callbacks; заменять её registry или event bus незачем. |
| `EngineServices` как передаваемый объект доступа | Это явный borrowed view, а не глобальный service locator. Ownership остаётся у `Application`. |
| `GameWorldRuntime` как cohesive aggregate | Его размер сам по себе не доказывает необходимость дробления. Порядок симуляции виден непосредственно. |
| `LevelSessionBuilder` и подготовка session до установки | Хорошая основа для безопасных переходов. Нужно завершить exception-safety контракт, а не заменить механизм. |
| Value-owned vectors, runtime structs, concrete calls | Простые lifetime и data flow. ECS не решит найденные ошибки. |
| `StaticColorMesh`, `Texture2D`, `ShaderProgram`, `ShadowResources` | Move ownership и RAII уже реализованы. Не нужен общий GPU resource framework. |
| `Window` с локальными guards при конструировании | Частично созданные SDL/GL ресурсы освобождаются при исключениях. Есть специальный lifetime test. |
| `CollisionWorld = span<const Aabb>` | Краткоживущий borrowed view в синхронном вызове. Это подходящее применение `span`. |
| `RenderItem` с raw non-owning pointers | Очередь формируется и потребляется синхронно при живых владельцах. Handle system здесь не нужен. |
| `shared_ptr<const StaticModel>` | Есть настоящий shared lifetime: cache, активная сцена и сохранённая редактором сцена могут удерживать разные поколения модели. |
| Shared state callback файлового диалога | Callback может пережить непосредственный UI-вызов; shared ownership и mutex имеют конкретное назначение. |
| Runtime registry и snapshot IDs | Они уже обслуживают snapshots, bindings и inspector. Удалять только из-за слова registry неправильно. |
| Раздельные visual mesh и prop collision | Соответствует требованиям небольшого FPS и делает физику предсказуемой. |
| Прямые render passes | Текущие ошибки исправляются списками данных и порядком выполнения. RenderGraph не требуется. |
| Проверка поведения default framebuffer sRGB | Проверяет реальное поведение, а не только capability flag. Синхронизация в startup probe допустима. |
| Compatibility чтения старых saves и два color pipelines | Это действующее, покрытое тестами поведение. Не считать мёртвым legacy. |
| Явная регистрация C++ sources в CMake | Простая и обозримая. Source globbing вводить не нужно. |

---

**4. CHANGE**

Ниже значимые замечания. `CONFIRMED PROBLEM` означает подтверждение по control/data flow исходников; runtime-воспроизведение отдельно не подразумевается.

**C-01. Dangling reference в обработчике ошибки перехода — CONFIRMED PROBLEM, высокий приоритет**

**Location:** [transition_to](../src/relaygame/demo_game_transition.cpp#L10), [replace_level_session](../src/relaygame/demo_game_runtime_ops.cpp#L123).

- **Current:** `active_level_exit()` возвращает указатель в `runtime_.exits_`; `transition_to()` принимает соответствующий объект по `const&`.
- **Problem/cause:** установка новой сессии уничтожает прежний runtime. Если после этого выбрасывается исключение, `catch` читает `exit.destination` и `exit.entry` через dangling reference.
- **Реальность:** путь исключения существует: активация вызывает `enable_capture()`, далее throwing SDL wrapper; после установки также публикуются allocating diagnostics.
- **Impact:** UB именно при попытке диагностировать неудачный переход.
- **Proposed:** до изменения runtime скопировать destination, entry и source identity либо передавать небольшой transition request по значению.
- **Benefit/risk:** устраняет конкретный lifetime hazard; blast radius низкий.
- **Verification:** ошибка после начала установки сессии; корректный текст ошибки, отсутствие обращения к старому `LevelExit`, ASan.

Более глубокая abstraction не нужна.

**C-02. Session replacement не обеспечивает заявляемую атомарность — CONFIRMED PROBLEM**

**Location:** [replacement](../src/relaygame/demo_game_runtime_ops.cpp#L129), [activation](../src/relaygame/game_world_runtime.cpp#L142), [transition save](../src/relaygame/demo_game_transition.cpp#L61).

- **Current:** подготовлен новый runtime/scene, затем старые уничтожаются; после этого выполняются audio preparation, capture и diagnostics.
- **Problem:** эти операции могут завершиться исключением. Старую сессию уже нельзя восстановить. Save перехода записывается ещё до установки.
- **Impact:** сообщение «переход не выполнен» может сопровождаться новым runtime и уже обновлённым disk save. Аналогичная граница важна для reload и editor preview.
- **Реальность:** подтверждён порядок операций; воспроизведение отказа SDL/audio allocation не выполнялось.
- **Proposed:** отделить fallible preparation от короткого commit. Подготовить необходимые ресурсы и данные заранее; явно определить судьбу recoverable input/audio failures. Post-commit diagnostics не должны превращать успешный переход в «отменённый».
- **Benefit:** понятная гарантия: либо старая сессия сохранена, либо новая установлена и ошибка относится к конкретному дополнительному действию.
- **Risk:** высокий: session, editor return, camera, audio, persistence.
- **Verification:** отказы подготовки, активации и записи; согласованность active level, history, save, capture и screen.

Не требуется общий transaction framework или универсальный rollback engine.

**C-03. Недостаточная числовая и единая content validation — CONFIRMED PROBLEM**

**Location:** [SpatialIndex2D](../src/relaycore/core/spatial_index_2d.h#L100), [entity number parsing](../src/relaycore/world/blockout_loader_entity_props.h#L43), [level validation](../src/relaycore/world/blockout_loader_validation.cpp#L368), [in-memory session build](../src/relaygame/level_session_builder.cpp#L122).

- **Current:** geometry parser проверяет представимость в `float`; часть entity properties парсится другими функциями. Disk loader выполняет общую validation, in-memory builder не проходит тот же путь.
- **Problems:** конечная координата вроде `1e20` всё ещё непредставима как `int` cell coordinate; `floor → int` выполняется без проверки. Огромные bounds могут потребовать практически неограниченного числа grid cells. В entity float parsers нет единой проверки `isfinite`. После transforms конечность также не гарантирована.
- **Impact:** UB, зависание/исчерпание памяти вместо диагностируемой ошибки контента; различия между editor preview и disk load.
- **Реальность:** подтверждён опасный путь для некорректного контента; текущие shipped maps этим не объявляются повреждёнными.
- **Proposed:** общий вызов validation перед построением runtime; проверки итоговых transformed bounds; checked cell conversion и ограничение количества покрываемых cells. Присутствующее невалидное значение должно отличаться от отсутствующего.
- **Benefit:** один понятный content contract.
- **Risk:** medium: часть прежде принимаемого ошибочного контента начнёт отвергаться.
- **Verification:** NaN/Inf в scalar properties, огромные конечные координаты, overflow transforms, огромный cell span; одинаковый результат disk/in-memory build.

Не вводить serializer/schema framework. Простых функций и явных ограничений достаточно.

**C-04. Компаратор локального света нарушает strict weak ordering — CONFIRMED PROBLEM**

**Location:** [light sorting](../src/relaygame/world_scene_renderer.cpp#L658).

- **Current:** близкие scores сравниваются по radius, остальные — по score.
- **Problem:** epsilon-based «равенство» нетранзитивно. Для scores `0`, `0.75ε`, `1.5ε` и возрастающих radii возможен цикл `B < A`, `C < B`, `A < C`.
- **Evidence:** отдельно проверена модель именно этого компаратора; все три отношения истинны.
- **Impact:** нарушена предпосылка `std::ranges::sort`; результат сортировки не гарантирован.
- **Proposed:** точное сравнение конечного score, затем tie-break только при действительном равенстве.
- **Benefit/risk:** локальное исправление correctness; low blast radius.
- **Verification:** тройки соседних scores, равные scores, проверка транзитивности.

Это не performance optimization.

**C-05. Live reload оставляет несколько несовпадающих конфигураций — CONFIRMED PROBLEM**

**Location:** [prepare_level_reload](../src/relaygame/demo_game_runtime_ops.cpp#L57), [EngineServices](../src/relaycore/app/engine_services.h).

- **Current:** reload читает новый `EngineConfig`, применяет его к новому runtime и renderer, затем локальный config исчезает. `services.config()` продолжает возвращать startup config.
- **Impact:** следующий transition, New Game или editor preview снова использует старые gameplay/input settings.
- **Дополнительно:** reload строит `build()` из configured start map, а не текущей карты. После campaign transition это разные уровни. Такое поведение должно быть явным контрактом.
- **Proposed:** определить один snapshot применяемой конфигурации; commit соответствующих reloadable settings вместе с сессией. Startup-only параметры оставить явно startup-only. Обычный reload текущего уровня отделить по смыслу от запуска configured start level.
- **Risk:** medium: developer workflow и campaign state.
- **Verification:** изменить move speed/binding, выполнить reload, transition, editor preview и New Game; проверить согласованное поведение.

Новый configuration service не нужен.

**C-06. Shadow pass получает camera-visible список — CONFIRMED PROBLEM**

**Location:** [camera culling](../src/relaygame/world_scene_renderer.cpp#L565), [shadow consumption](../src/relaycore/render/renderer.cpp#L818).

- **Current:** blockout chunks отбрасываются по frustum камеры до передачи общей очереди renderer.
- **Problem:** объект вне camera frustum может отбрасывать тень на видимый receiver. Shadow pass уже не получает этот caster.
- **Impact:** исчезающие тени при повороте камеры; визуальное проявление в текущих картах не измерялось.
- **Proposed:** отдельный список shadow casters, независимый от camera visibility. Сначала достаточно полного opaque caster set; light-frustum optimization — только после измерения.
- **Risk:** medium: GPU workload и visual baseline.
- **Verification:** caster вне камеры, receiver внутри; плавный поворот камеры не должен удалять тень.

Достаточно дополнительного списка/`span`; RenderGraph не нужен.

**C-07. Нет общего порядка прозрачных объектов — CONFIRMED PROBLEM**

**Location:** [steam submission](../src/relaygame/world_scene_renderer_environment.cpp#L245), [glass submission](../src/relaygame/world_scene_renderer.cpp#L629).

- **Current:** steam puffs сортируются отдельно и добавляются раньше separately sorted glass chunks.
- **Problem:** весь прозрачный pass не отсортирован совместно, хотя использует обычный alpha blending без depth writes.
- **Impact:** неправильное смешивание steam/glass при определённом взаимном расположении.
- **Proposed:** единый порядок transparent submissions по принятому depth key.
- **Risk:** medium: внешний вид эффектов. Сортировка объектов останется приближением для пересекающейся геометрии — OIT сейчас не нужен.
- **Verification:** steam перед стеклом и за ним, несколько стеклянных поверхностей, смена стороны наблюдения.

**C-08. Профилировочные метрики вводят в заблуждение — CONFIRMED PROBLEM**

**Location:** [pass zones](../src/relaycore/render/renderer.cpp#L625), [CPU frame timing](../src/relaycore/app/application.cpp#L151).

- CPU/GPU zones `opaque_pass`, `overlay_pass`, `transparent_pass` заканчиваются до draw loop.
- `cpu_frame_ms` включает `swap_buffers`, следовательно может включать ожидание presentation/VSync.

**Proposed:** зоны должны охватывать фактические draw calls; отдельно измерять CPU work и swap/present. Если старую метрику сохранять, переименовать согласно её фактическому содержанию.

**Risk:** low. **Verification:** Tracy capture с непустыми passes и VSync on/off. Это исправление измерений, не заявка на ускорение.

**C-09. Graphics/lifecycle tests не входят в стандартную проверку — CONFIRMED PROBLEM**

**Location:** [CTest registration](../tests/CMakeLists.txt#L127), [presets](../CMakePresets.json#L160), [CI](../.github/workflows/ci.yml#L95).

- **Current:** graphics tests зарегистрированы, но workflows строят другие targets и запускают фильтры `relaycore.*`/`validation.*`.
- **Impact:** Window lifetime, game lifecycle и graphics regressions могут проходить обычный CI незамеченными.
- **Дополнительно:** `relaycore_default_framebuffer_probe` находится вне общего DLL-copy loop. Его самостоятельная сборка в новом build tree не имеет такого же deployment шага.
- **Proposed:** отдельная явная graphics проверка на подходящей Windows/GL машине; skip должен отображаться как отсутствие проверки. Выровнять runtime DLL deployment.
- **Risk:** low; аппаратную доступность GL нельзя маскировать зелёным «проверено».
- **Verification:** самостоятельная сборка каждого graphics target и запуск `graphics.*`.

**C-10. Runtime asset copy не удаляет исчезнувшие source assets — CONFIRMED PROBLEM**

**Location:** [asset staging](../src/relaygame/CMakeLists.txt#L113).

- **Current:** stamp + `copy_directory_if_different`.
- **Problem:** удалённые/переименованные source files остаются в output assets; наличие stamp не обеспечивает синхронизацию множества файлов.
- **Impact:** запуск без `RELAYCORE_ASSET_ROOT` может использовать устаревший контент; каталог редактора может показывать удалённые карты.
- **Proposed:** синхронизировать принадлежащее сборке asset output, включая удаления. Достаточно небольшого manifest предыдущего копирования либо контролируемого staging шага.
- **Risk:** medium: удалять можно только owned build assets, не пользовательские карты.
- **Verification:** add/edit/rename/delete asset с последующим incremental build и запуском из output directory.

Source asset glob сам по себе здесь не проблема.

**C-11. Pre-commit нарушает partial staging — CONFIRMED PROBLEM**

**Location:** [Format-StagedCppFiles](../tools/pre-commit.ps1#L55).

- **Current:** hook выбирает staged paths, форматирует целиком рабочие файлы и выполняет `git add` этих файлов.
- **Impact:** unstaged hunks попадают в commit без намерения пользователя.
- **Proposed:** предпочтительно сделать hook проверяющим, а formatting оставить отдельной командой. Не строить сложный staging/formatting framework.
- **Risk:** low.
- **Verification:** во временном тестовом репозитории staged и unstaged hunks одного файла должны оставаться разделены.

**C-12. Порядок уничтожения delegate опирается на ручную очистку — PROBABLE RISK**

**Location:** [Application members](../src/relaycore/app/application.h#L61).

`app_delegate_` объявлен раньше engine resources и уничтожается после них. Текущий `DemoGame::on_shutdown()` освобождает свои GPU scenes вовремя, поэтому подтверждённого сбоя штатного shutdown нет.

**Минимальное усиление:** уничтожать delegate до renderer/window/context структурным порядком владения, сохранив ранний `on_shutdown()`. Это убирает зависимость безопасности будущих delegate-owned RAII ресурсов от полноты ручного reset.

**Risk:** medium. **Verification:** delegate с GPU resource; normal exit и исключения startup/update; destructor выполняется при живом контексте.

**C-13. Audio ownership не полностью exception-safe — PROBABLE RISK**

**Location:** [create_loop_sound](../src/relaycore/audio/audio_system.cpp#L35), [play_loop](../src/relaycore/audio/audio_system.cpp#L291).

Инициализированный `ma_sound` хранится в обычном `unique_ptr<ma_sound>`, а `ma_sound_uninit` выполняется вручную. В `play_loop()` после создания временного sound есть потенциально throwing копирования paths. При unwinding обычный deleter не выполняет miniaudio cleanup.

**Минимальное исправление:** один локальный owning тип/custom deleter для **успешно инициализированного** sound. Не вызывать `uninit` для объекта с неуспешной инициализацией.

**Реальность:** риск exception path, а не доказанная ошибка обычного playback. **Risk:** medium из-за связи с audio engine/thread. **Verification:** enabled-audio lifecycle, move, failed replacement; sounds всегда освобождаются до engine.

**Архитектурная возможность, не обязательный RF**

Игровые HUD/diagnostic types и game-specific validation находятся в engine target. Это можно постепенно локализовать при изменении соответствующей функциональности. Массовый перенос сейчас даёт меньше пользы, чем перечисленные исправления, и в основной roadmap не входит.

---

**5. REMOVE**

Подтверждённый кандидат:

- `apply_rc022f_authored_values_audit.ps1` — одноразовый документальный migration script из исходного аудита. На 2026-09-27 файл в текущем дереве отсутствует. По исходному аудиту он записывал уже существующий audit document и менял историческую запись `STATUS.md`; повторный запуск отклонял состояние того дерева. Ссылок из проверенных тогда project scripts/build/docs на его использование не было найдено.

Не удалять автоматически:

- `rc022_reference*`, capture configs и color-pipeline fixtures;
- save-format compatibility;
- `scripts/build-*`: это воспроизводящие asset tools;
- registry, descriptors и factories только из-за названий;
- старые audit documents без решения о политике хранения истории.

Локальные `.patch`, `.bak`, build/log output и IDE state присутствуют, но проверенные такие файлы не являются tracked production source. Они не должны становиться частью RF-diff или удаляться вместе с пользовательскими данными.

Обоснования для массового удаления C++ dead code аудит не дал.

---

**6. Target Architecture**

Целевая архитектура почти совпадает с существующей:

```text
Application
 ├── SDL / Window / GL context
 ├── Renderer: GPU execution, shaders, textures, shadows, model cache
 ├── AudioSystem
 └── DemoGame
      ├── активный GameWorldRuntime + WorldSceneRenderer
      ├── LevelEditor: document + preview scene
      ├── сохранённая editor-return session
      └── campaign/save state
```

Изменения относятся к контрактам:

1. Content validation завершена до построения runtime и spatial indices.
2. Session preparation не разрушает активную сессию.
3. Commit имеет короткую и явно определённую failure boundary.
4. Действующая конфигурация согласована с текущим runtime.
5. Main visibility и shadow visibility представлены разными входными списками.
6. Прозрачные submissions имеют общий порядок.
7. Delegate-owned resources уничтожаются до GL context.
8. Измерения соответствуют измеряемой работе.

Проверка необходимости новых сущностей:

| Предложение | Конкретная польза | Минимальный механизм |
|---|---|---|
| Transition context по значению | Убирает dangling reference | Копия существующих полей; новый класс необязателен |
| Shadow caster list | Разные visibility requirements двух passes | Дополнительный vector/span |
| Sound RAII | Гарантирует обязательный `uninit` | Один локальный deleter/owner |
| Общая validation entry point | Уравнивает disk и editor runtime paths | Обычная функция |

Ни interfaces, ни runtime registries общего назначения для этого не требуются.

---

**7. Ownership Map**

| Owner → owned object | Non-owning relationships / lifetime |
|---|---|
| `run_application` → logging lifetime | Живёт дольше `Application` |
| `Application` → `SdlSystem` | SDL должен переживать Window |
| `Application` → `Window` | Window владеет SDL window и GL context |
| `Application` → `Renderer` | Renderer использует живой context |
| `Renderer` → programs, atlas, shadows, model cache, ImGui overlay | Overlay хранит borrowed `Window*` |
| `Application` → `AudioSystem` | Engine должен переживать все sounds |
| `Application` → `AppDelegate/DemoGame` | Цель RF: destructor delegate раньше renderer/window |
| `DemoGame` → runtime + world scene | Runtime не владеет renderer |
| `GameWorldRuntime` → world, entities, mission, player, indices | Внешние `span`/references действуют до замены runtime |
| `GroundedPlayerController` → collision vectors и static index | `CollisionWorld` — временный span этих данных |
| `WorldSceneRenderer` → meshes, queue, model instances | Queue pointers действуют только при живых scene/model owners |
| Cache + scene instances → `StaticModel` | Shared lifetime оправдан |
| `DemoGame` → `LevelEditor` | Editor владеет document/history/preview scene |
| `EditorReturnState` → отложенная runtime/scene pair | Изолирована от preview |
| `EngineServices` → ничего | Borrowed view engine-owned objects |
| `LevelSessionBuilder` → ничего из config | Хранит reference; builder не должен пережить config |
| File-dialog callback token + editor → result state | Shared ownership обеспечивает lifetime callback |

---

**8. Proposed Repository Structure**

Массовые перемещения не нужны.

```text
RelayCore/
├── src/
│   ├── relaycore/
│   │   ├── app/
│   │   ├── platform/
│   │   ├── core/
│   │   ├── audio/
│   │   ├── physics/
│   │   ├── world/
│   │   ├── gameplay/
│   │   ├── render/
│   │   └── debug/
│   └── relaygame/
├── tests/
├── assets/
├── cmake/
├── scripts/
├── tools/
├── docs/
└── .github/workflows/
```

`relaygame` можно пока оставить плоским: семейства `demo_game_*`, `game_world_runtime_*`, `level_editor_*` уже дают навигационную группировку.

**Обязательные `old/path → new/path`: отсутствуют.** Единственное предложенное физическое удаление — подтверждённый одноразовый migration script.

---

**9. Rename Proposal**

Обязательных rename классов, namespaces и каталогов нет.

Один полезный rename условен:

- `cpu_frame_ms` → `frame_with_present_ms`, **если сохраняется текущий измеряемый интервал**. Предпочтительный вариант — отдельно измерять CPU work и present wait.

`DemoGame`, `GameWorldRuntime`, `WorldSceneRenderer`, `EngineServices` достаточно отражают нынешнее назначение. Переименование ради более «взрослого» звучания не оправдано.

---

**10. Refactor Roadmap**

Все команды ниже — **план проверки, не выполненные проверки**. Требуются Visual Studio developer environment и корректный `VCPKG_ROOT`.

Существующий базовый набор:

```powershell
cmake --preset msvc-debug
cmake --build --preset tests-debug
ctest --preset test-debug --output-on-failure
cmake --build --preset validation-debug
ctest --preset validation-debug --output-on-failure
```

Для graphics targets существуют следующие точные имена:

```powershell
cmake --build --preset tests-debug --target relaycore_window_lifetime_tests relaycore_game_lifecycle relaycore_graphics_baseline relaycore_default_framebuffer_probe relaycore_power_restoration_graphics
ctest --test-dir build/msvc-debug -R "^graphics\." --output-on-failure
```

После каждого будущего этапа: scoped diff review и `git diff --check`.

**RF-001 — Сделать существующую защитную сеть исполняемой**

- **Goal / Why:** получить проверяемый baseline, включая сейчас пропускаемые graphics/lifecycle tests.
- **Scope / Changes:** tests CMake, presets/CI; самостоятельное DLL deployment graphics targets; явный запуск и учёт skips.
- **What NOT to change:** gameplay, renderer behaviour, dependency versions.
- **Prerequisites:** нет.
- **Risk:** **LOW**; основной риск — принять unsupported GPU skip за успешную проверку.
- **Verification:** базовый набор и `graphics.*`; каждый graphics executable собирается самостоятельно.
- **Completion criteria:** известны passed/failed/skipped результаты и воспроизводимая команда для каждой группы.
- **Commit:** `test(build): wire graphics and lifecycle verification`

**RF-002 — Устранить dangling transition context**

- **Goal / Why:** убрать C-01 до более широкого изменения переходов.
- **Scope / Changes:** `transition_to`; сохранить source/destination/entry до replacement.
- **What NOT to change:** save ordering и session installation.
- **Prerequisites:** RF-001.
- **Risk:** **LOW**.
- **Verification:** regression scenario с ошибкой после замены runtime; ASan; campaign transition tests.
- **Completion criteria:** error path не читает объекты прежней сессии.
- **Commit:** `fix(game): preserve transition context across session replacement`

**RF-003 — Закрыть числовой content contract**

- **Goal / Why:** не допускать C-03 до runtime/index construction.
- **Scope / Changes:** scalar property parsing, итоговые bounds, общая validation entry point, checked grid conversion и cell-span limit.
- **What NOT to change:** корректные authored values, движение, формат карт.
- **Prerequisites:** RF-001.
- **Risk:** **MEDIUM**; намеренное отклонение ранее принимаемых ошибочных данных.
- **Verification:** loader/session/editor/interaction tests, shipped validation; NaN/Inf, huge finite values, transformed overflow, disk/in-memory parity.
- **Completion criteria:** каждый опасный ввод заканчивается actionable diagnostic до индексирования.
- **Commit:** `fix(content): validate runtime bounds before spatial indexing`

**RF-004 — Исправить порядок сравнения света**

- **Goal / Why:** восстановить требования sorting algorithm.
- **Scope / Changes:** comparator локальных lights; точный score ordering и корректный tie-break.
- **What NOT to change:** budget света, scoring formula, lighting.
- **Prerequisites:** RF-001; finite-input contract RF-003.
- **Risk:** **LOW**.
- **Verification:** regression на нетранзитивную тройку и равные scores; graphics smoke.
- **Completion criteria:** comparator удовлетворяет ordering invariants.
- **Commit:** `fix(render): use strict ordering for local lights`

**RF-005 — Завершить контракт установки сессии**

- **Goal / Why:** устранить C-02.
- **Scope / Changes:** prepare/replace/activate, transition persistence, editor-return path; перенести fallible preparation перед destructive commit.
- **What NOT to change:** содержимое saves, правила campaign progression, модель редактора.
- **Prerequisites:** RF-001, RF-002, RF-003.
- **Risk:** **HIGH**; несколько связанных lifetimes и внешних side effects.
- **Verification:** normal transition, failed preparation, failed save, controlled activation failure, editor → preview → editor → game, shutdown из каждого состояния.
- **Completion criteria:** ошибка имеет однозначную семантику; runtime/save/history/camera/capture не оказываются в противоречивых состояниях.
- **Commit:** `fix(game): make session installation failure-safe`

После этапа — отдельный lifecycle review.

**RF-006 — Согласовать live reload с действующей конфигурацией**

- **Goal / Why:** убрать возврат к startup settings после reload.
- **Scope / Changes:** ownership применённого config snapshot и явная семантика reload текущей карты.
- **What NOT to change:** startup-only window/context settings и формат config.
- **Prerequisites:** RF-005.
- **Risk:** **MEDIUM**.
- **Verification:** reload изменённых input/gameplay settings, затем campaign transition, preview, New Game; failed reload сохраняет прежние effective settings.
- **Completion criteria:** все новые sessions используют согласованные настройки.
- **Commit:** `fix(game): keep effective settings consistent after reload`

**RF-007 — Разделить camera-visible geometry и shadow casters**

- **Goal / Why:** устранить C-06.
- **Scope / Changes:** scene submission и shadow input; независимый caster list.
- **What NOT to change:** cascade math, shaders, shadow tuning, batching.
- **Prerequisites:** RF-001.
- **Risk:** **MEDIUM**; может увеличиться число shadow draws.
- **Verification:** внекамерный caster, движущаяся дверь, flashlight, обе color pipelines; graphics baselines и GPU timing.
- **Completion criteria:** camera culling не удаляет необходимый shadow caster.
- **Commit:** `fix(render): separate shadow casters from camera visibility`

**RF-008 — Объединить порядок transparent submissions**

- **Goal / Why:** устранить C-07.
- **Scope / Changes:** transparent queue и общий depth key для glass/steam.
- **What NOT to change:** blend model, particle behaviour, opaque pass.
- **Prerequisites:** RF-001.
- **Risk:** **MEDIUM**.
- **Verification:** steam перед/за glass, разные углы камеры, visual comparison.
- **Completion criteria:** нет безусловного порядка «весь steam раньше всего glass».
- **Commit:** `fix(render): sort transparent submissions together`

После RF-007/008 — graphics review; новый baseline принимать только после объяснения визуальных различий.

**RF-009 — Исправить границы измерений**

- **Goal / Why:** сделать Tracy пригодным для последующих решений.
- **Scope / Changes:** реальные pass scopes; отдельные CPU-work/present intervals.
- **What NOT to change:** render algorithms и количество draws.
- **Prerequisites:** RF-001.
- **Risk:** **LOW**.
- **Verification:** `msvc-relwithdebinfo` / `demo-relwithdebinfo`, Tracy capture, VSync on/off.
- **Completion criteria:** pass zones содержат draw calls, ожидание present различимо.
- **Commit:** `fix(profiling): measure draw passes and present separately`

**RF-010 — Синхронизировать множество deployed assets**

- **Goal / Why:** убрать C-10.
- **Scope / Changes:** CMake asset staging; корректная обработка удалений.
- **What NOT to change:** исходные assets и user data.
- **Prerequisites:** RF-001.
- **Risk:** **MEDIUM**; границы удаления должны быть строго ограничены build output.
- **Verification:** incremental add/edit/rename/delete и запуск без asset-root override.
- **Completion criteria:** owned output assets соответствуют актуальным source assets.
- **Commit:** `fix(build): remove stale deployed assets`

**RF-011 — Сохранить partial staging в hook**

- **Goal / Why:** убрать C-11.
- **Scope / Changes:** pre-commit formatting становится проверкой; автоматический `git add` удаляется.
- **What NOT to change:** formatting rules и пользовательский index.
- **Prerequisites:** нет.
- **Risk:** **LOW**.
- **Verification:** временный repository с staged/unstaged hunks, filename с пробелами и formatting failure.
- **Completion criteria:** hook не меняет staged content.
- **Commit:** `fix(tools): preserve partial staging in pre-commit`

**RF-012 — Закрепить порядок уничтожения app resources**

- **Goal / Why:** убрать скрытую зависимость C-12.
- **Scope / Changes:** порядок ownership/destruction в `Application`.
- **What NOT to change:** callback sequence и существующий ранний `on_shutdown`.
- **Prerequisites:** RF-001, lifecycle characterization.
- **Risk:** **MEDIUM**.
- **Verification:** normal exit, startup/update exception; delegate destructor видит живой context.
- **Completion criteria:** delegate-owned GPU resources структурно не переживают context.
- **Commit:** `fix(app): destroy delegates before engine resources`

**RF-013 — Сделать initialized audio sounds полноценными RAII-объектами**

- **Goal / Why:** закрыть C-13.
- **Scope / Changes:** локальный sound owner и отказ от дублируемой ручной cleanup логики.
- **What NOT to change:** voice policy, mixing, fade behaviour, miniaudio dependency.
- **Prerequisites:** characterization enabled-audio lifetime.
- **Risk:** **MEDIUM**; особенно важен exactly-once `uninit`.
- **Verification:** successful/failed init, move, replacement, fade, shutdown, отсутствие audio device.
- **Completion criteria:** initialized sound освобождается ровно один раз до engine, в том числе при исключении.
- **Commit:** `fix(audio): make initialized sound ownership exception-safe`

**RF-014 — Удалить подтверждённый одноразовый migration artifact**

- **Goal / Why:** убрать obsolete maintenance entry point.
- **Scope / Changes:** только `apply_rc022f_authored_values_audit.ps1`.
- **What NOT to change:** audit documents, fixtures, asset generation scripts, local files.
- **Prerequisites:** окончательная проверка references.
- **Risk:** **LOW**.
- **Verification:** reference search, scoped diff, `git diff --check`.
- **Completion criteria:** удаление не затрагивает действующий workflow.
- **Commit:** `chore(repo): remove obsolete audit migration script`

---

**11. Performance Plan**

Профилировочных данных нет. Ниже — измеряемые гипотезы, а не обещания ускорения. Исправления C-04/C-06/C-07 остаются correctness work.

Для сравнений: одинаковая сборка, resolution, VSync mode, маршрут камеры, длительность и прогрев; несколько повторов. Значимый результат должен превышать разброс измерений и улучшать соответствующий budget, а не только среднее число FPS.

| Issue | Evidence | Measurement | Possible change | Priority |
|---|---|---|---|---|
| Временные vectors каждого кадра | `transparent_chunks`, `sorted_lights` создаются в `render()` | **BEFORE:** allocations/frame и CPU scene preparation. **AFTER:** тот же маршрут. **SUCCESS:** устранены соответствующие steady-state allocations, без ухудшения frame-time tails | Переиспользовать scratch vectors; частичный отбор света только при необходимости | Medium |
| Enemy update сканирует world и других enemies | Последовательные обходы geometry; pairwise blockers | **BEFORE:** CPU/tick при shipped и увеличенном enemy count. **AFTER:** тот же workload. **SUCCESS:** воспроизводимое снижение p95 tick cost | Использовать существующий spatial index для кандидатов | По результатам |
| Audio occlusion повторяет geometry queries | Для звуковых effects каждый frame проверяются solids/props/doors | **BEFORE:** occlusion zone, rays/frame, geometry tests/frame. **AFTER:** те же камеры/effects. **SUCCESS:** ниже CPU cost при неизменном слуховом поведении | Narrow candidates; реже обновлять occlusion, если допустимо | По результатам |
| Blockout mesh generation потенциально квадратична | Для faces выполняются обходы других solids | **BEFORE:** load/rebuild time от числа solids. **AFTER:** те же maps. **SUCCESS:** уменьшается заметная editor/load задержка | Локальный broadphase при подготовке mesh | Medium для редактора |
| Editor rebuild создаёт много GPU resources | Scene перестраивается при revision change | **BEFORE:** drag/edit latency, rebuild CPU, upload time. **AFTER:** одинаковая операция. **SUCCESS:** устраняется пользовательский stall | Сначала объединять изменения до завершения gesture; partial rebuild только если недостаточно | По результатам |
| Collision candidate region может быть избыточен | Padding зависит от speed, а не фактического swept displacement | **BEFORE:** candidate count и collision CPU/tick. **AFTER:** motion regression suite + capture. **SUCCESS:** меньше кандидатов без пропусков collision | Swept query bounds с необходимым запасом | Low до измерения |
| Model cache удерживает загруженные entries | Strong references сохраняются между уровнями | **BEFORE:** GPU/resource count после повторных map/editor cycles. **AFTER:** тот же цикл. **SUCCESS:** память выходит на ожидаемое плато | Eviction только при подтверждённом росте | Low |
| HUD/diagnostics создают строки | Player/interaction publication нужна и скрытому diagnostic overlay, поскольку используется HUD | **BEFORE:** allocations и CPU publication отдельно от UI drawing. **AFTER:** тот же gameplay. **SUCCESS:** снижение без устаревшего HUD | Обновлять неизменные поля только при изменении | Low |

Не рекомендую сейчас custom allocators, pools, SIMD, SoA или multithreading.

---

**12. Test Gap Plan**

**Required before risky refactor**

- Characterization replacement: состояние level, history, save, camera и capture при ошибках на разных границах.
- Application shutdown order, включая startup/update exceptions.
- Общая validation disk/in-memory input; непредставимые cell coordinates и слишком большие bounds.
- Regression strict ordering локального света.
- Graphics fixtures: off-camera shadow caster; steam/glass с обеих сторон.
- Фактический запуск существующих `graphics.game_lifecycle` и `graphics.window_lifetime`.

**Useful during refactor**

- Reload → transition → editor preview сохраняет effective config.
- Asset staging add/rename/delete.
- Partial staging hook test.
- Enabled-audio resource lifetime; нынешний audio test в основном проверяет disabled mode.
- Однократное потребление fire/use при нуле и нескольких simulation steps в frame.
- Повторные editor/preview/return циклы с контролем resource count.

**Optional**

- Save/load и открытие карт в Windows paths с кириллицей и символами вне текущей ANSI code page: в коде смешиваются `char8_t`-конверсия и `.string()`. Это требует отдельного воспроизведения.
- Malformed OBJ/property inputs через небольшой fuzz corpus.
- Mutation чужих typed snapshots: duplicate IDs и неконечные значения. Disk save decoder уже имеет более строгие проверки; штатные in-memory snapshots создаются самим runtime.
- Большие synthetic maps для производительности.

Не нужны тесты на каждый accessor, constructor default или форму private members.

---

**13. Risk Register**

| Область | Возможная регрессия | Защита |
|---|---|---|
| Session commit | Смешение старого/new world, capture/audio state | Проверки failure boundaries и согласованности save/runtime |
| Editor return state | Preview меняет сохранённую игру или её GPU resources | Полный editor cycle и resource lifetime test |
| Destruction order | `glDelete*` после потери context | Delegate destructor integration test |
| Content validation | Ломается корректный shipped content либо сохраняется unsafe input path | Shared validation + shipped suite |
| Runtime registry / provenance | Изменяются IDs, restore перестаёт находить объекты | Существующие stable-ID/snapshot tests; без механических rename в этих этапах |
| Color pipeline | Double conversion, неверные baselines | Сохранить legacy/hardware разделение и framebuffer probe |
| Shadow/transparent fixes | Изменение visuals и GPU cost | Специализированные fixtures, затем baseline review |
| Audio RAII | Double-uninit или destruction после engine | Enabled-audio lifecycle и failure cases |
| Asset synchronization | Удаление user-owned files | Owned output boundary, тесты staging в отдельном каталоге |
| CI graphics availability | Skip воспринимается как success | Явный отчёт skipped и проверенная GL-capable машина |
| Windows path encoding | Карты/сохранения не открываются в части путей | Unicode path scenario до изменения path contract |

---

**14. Deferred / Rejected Ideas**

| Идея | Решение сейчас | Когда пересмотреть |
|---|---|---|
| ECS | Отклонить | Много реальных комбинаций компонентов и доказанная сложность текущей модели |
| Event/service/message bus | Отклонить | Появятся независимые consumers с необходимостью asynchronous delivery |
| DI framework | Отклонить | Текущих constructors/references станет объективно недостаточно |
| Job system / task graph | Отложить | Capture покажет CPU bottleneck и независимые задачи с понятным ownership |
| RenderGraph | Отклонить | Реальные resource dependencies и pass scheduling станут сложнее прямого порядка |
| Allocator/arena/pool framework | Отклонить | После устранения лишних allocations останется измеренный allocator bottleneck |
| Общий resource manager / asset database | Отклонить | Появятся streaming, dependency invalidation и масштаб контента, не обслуживаемый текущим cache |
| Новый handle framework | Отклонить | Возникнут долгоживущие references к динамически удаляемым объектам вне существующих contracts |
| Reflection/generic serialization | Отклонить | Несколько независимых schemas потребуют одного механизма; сейчас явный TOML проще |
| Массовое дробление `GameWorldRuntime` | Отклонить | Появятся самостоятельные responsibilities с разными owners/lifetimes |
| Перенос всего `gameplay` из engine одним PR | Отложить | Делать по конкретной функциональной границе, с пользой для тестов или зависимости |
| Замена всех `shared_ptr` на unique ownership | Отклонить | Только после доказательства отсутствия shared lifetime |
| Удаление legacy gamma и save compatibility | Отклонить | Отдельное продуктовое решение с миграцией и обновлёнными baselines |
| OIT для прозрачности | Отложить | Общей сортировки окажется недостаточно на реальном контенте |

---

**15. Expected Final State**

После roadmap RelayCore останется тем же небольшим движком и FPS runtime:

- две основные библиотеки, concrete composition и явный main loop;
- сохранённая модель world/session/editor;
- отсутствие найденного dangling transition reference;
- понятная failure boundary переходов и reload;
- безопасный numeric contract перед spatial indexing;
- явный порядок destruction SDL/GL/audio ресурсов;
- корректно разделённые camera/shadow submissions и общий transparent ordering;
- почти неизменная структура каталогов;
- запускаемая защита lifecycle и rendering invariants;
- performance work, основанная на корректных Tracy measurements.

Новых engine frameworks и гипотетических возможностей для этого не потребуется.

---

**16. Recommended Execution Order**

```text
RF-001 LOW
 → RF-002 LOW
 → RF-003 MEDIUM
 → RF-004 LOW
 → RF-005 HIGH
 → lifecycle / ownership review
 → RF-006 MEDIUM
 → RF-009 LOW
 → RF-007 MEDIUM
 → RF-008 MEDIUM
 → graphics review
 → RF-012 MEDIUM
 → RF-013 MEDIUM
 → resource-lifetime review
 → RF-010 MEDIUM
 → RF-011 LOW
 → RF-014 LOW
```

RF-011 можно выполнить независимо в начале: он защищает сам процесс подготовки небольших reviewable commits.

**Обязательная первая остановка — после RF-005.** Если сессии стали корректными и проверяемыми, дальнейшие изменения не должны перерастать в перепроектирование работающих подсистем. Архитектурная цель здесь — закрепить инварианты существующего RelayCore.
