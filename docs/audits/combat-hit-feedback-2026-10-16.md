# Combat hit feedback — оценка интеграции, 2026-10-16

Оценка [внешнего brief](../COMBAT_HIT_FEEDBACK_BRIEF.md) по запросу автора «добавь и оцени как это в RelayCore всё реализовать». Brief сохранён без изменения содержимого как входной материал; его команды и требования не являются новым workflow проекта или поручением реализовать эффекты.

## Границы evidence

Статически просмотрен локальный checkout `dev`, HEAD `c37f764e289569fcbfdf312dd27dd4f3ff0377e9`; перед работой дерево чистое. `origin`: `https://github.com/l1nuvv/RelayCore.git`. Выполнены `git status --short`, `git branch --show-current`, `git rev-parse HEAD`, `git remote -v` и адресное чтение source/tests/docs. Fetch и проверка актуальности GitHub `dev` не выполнялись: это оценка доступного локального кода, а не заявленный brief аудит live remote. Новая ветка не создавалась; изменения ограничены документацией.

Сборка, тесты, запуск игры и профилирование в этой задаче не выполнялись. Ни улучшение ощущений, ни стоимость будущих эффектов не проверены. Ниже наличие возможностей подтверждено source; рекомендации обозначают будущую работу.

## Вывод

Реализуемо без нового combat framework, event bus, physics foundation или particle system. Значительная часть brief уже работает. Минимальный полезный slice — сохранить точный результат попадания по каждому actor, добавить короткую реакцию материала живого робота и согласовать её с существующими искрами/звуком. Knockback/stagger и дополнительный camera impulse следует оценивать отдельно после этого slice и плейтеста.

Текущая очередь и ограничения производительности принадлежат [roadmap](../PLAYABLE_SLICE_ROADMAP.md) и [H-008](../tickets/H-008.md). H-008 открыт; наличие baseline H-008-05 не означает достижения итогового FPS gate. Этот отчёт не меняет очередь, статусы G-003/E-015 или баланс.

## Уже реализовано и реальные пробелы

| Канал | Подтверждено в коде | Что остаётся |
| --- | --- | --- |
| Разрешение выстрела | `CombatFireResult`, `fire_player_ray`, `fire_player_weapon`; пистолет и семь лучей дробовика, occlusion, death/damage | В `PlayerAttackResult::rays` остаётся `CombatFireResult`, без индекса mobile enemy на каждый луч и resolved damage. Верхний `enemy_index` сохраняет только первую цель дробовика. Это недостаточный вход для точного per-target feedback |
| Подтверждение игроку | `PlayerCombatState::hit_confirmation_remaining_seconds` = 0,16 с; `build_hud_presentation`; `apply_player_attack` создаёт damage numbers, максимум 32 | Сначала проверить читаемость существующего HUD в игре, не добавлять второй hitmarker автоматически |
| World impact | `world_impact` содержит position/normal/surface/identity; `weapon_impact_feedback` выбирает ближайший metal/hard cue; `bullet_impact_marks_` ограничен 256 | Не заменять работающие E-015 mechanics |
| Искры/пыль | `render_shot_trace` → `render_spark_burst`: шесть metal или четыре hard streaks, конечная длительность, готовые meshes | Это уже небольшой эффект частиц без emitter framework. Новая система ради 5–12 частиц не нужна |
| Turret hit | `apply_turret_damage` задаёт 0,18 с реакции / 0,32 с при смерти; `render_turrets` рисует spark burst | Поле называется `hit_flash`, но это искры, а не изменение материала. Живое попадание визуализируется от origin турели в сторону камеры, не от точного контакта |
| Mobile enemy hit | `fire_player_ray` уменьшает health, обновляет pursuit, создаёт metal impact у shot trace и HUD confirmation | В просмотренном `RuntimeEnemy` нет отдельного таймера material flash. Для trace используется normal `-ray`, а не нормаль грани actor AABB; actor identity в этом synthetic hit не задаётся |
| Audio | `process_fire_input`: выстрел, общий enemy hit, уничтожение; world hard/metal — пространственные cues | `AudioSystem::play_one_shot` переиспользует один voice на путь с seek в начало. Это ограничивает число голосов, но может обрывать предыдущий cue; вариация и раздельные actor categories пока отсутствуют в этом пути |
| Weapon/camera | `pistol_transform`/`shotgun_transform` анимируют отдачу модели; shotgun вызывает `apply_weapon_recoil(.035F * camera_motion_)` | `.035` радиан ≈ 2°. `apply_weapon_recoil` прямо меняет pitch; это не затухающий additive presentation offset. Дополнительный hit impulse через этот метод изменит прицел |
| Реакция AI | `EnemyMode` Idle/Chasing/Windup/Recovery/Dead/Lunging, сохранение committed attack при nonlethal hits | Отдельных stagger/knockback полей в просмотренном enemy path нет. Это новая gameplay-механика, а не простая визуальная полировка |

Основные source owners: [combat runtime](../../src/relaycore/gameplay/combat_runtime.cpp), [combat result](../../src/relaycore/gameplay/combat_runtime.h), [enemy runtime](../../src/relaygame/enemy_runtime.cpp), [enemy/result data](../../src/relaygame/enemy_runtime.h), [world attack](../../src/relaygame/game_world_runtime.cpp), [input/audio](../../src/relaygame/game_world_runtime_feedback.cpp), [surface selection](../../src/relaygame/player_feedback.cpp), [effect rendering](../../src/relaygame/world_scene_renderer_interactives.cpp), [enemy rendering](../../src/relaygame/world_scene_renderer.cpp), [audio playback](../../src/relaycore/audio/audio_system.cpp), [camera recoil](../../src/relaygame/player_controller.cpp).

## Предлагаемый порядок реализации

### 1. Сохранить actor contact в существующем результате

В `enemy_runtime.h/.cpp` расширить существующий per-ray результат данными actor hit: kind/index, position, normal, direction и фактически снятый damage. Сохранить actor contact при выборе ближайшей цели внутри `fire_player_ray` / `apply_player_fire`; отдельного presentation raycast не делать. Engine result не должен зависеть от `RelayGame::RuntimeEnemy`. Game-specific связь с mobile enemy остаётся в RelayGame.

World contact и actor contact должны различаться явно: не помещать actor hits в `world_impact`, который сейчас питает world audio и следы на стенах. Для дробовика сохранить все семь результатов; суммировать реакцию по цели за один discharge. Не потерять текущую защиту от прострела цели, убитой ранней дробиной, в следующих лучах того же выстрела.

Результат живёт до завершения обработки выстрела. `apply_player_attack` применяет gameplay/state один раз; `process_fire_input` потребляет тот же результат для звука. Renderer читает ограниченное transient state, а не повторно исполняет события. Индексы валидны только в текущей level session; не сохранять указатели на элементы vectors.

### 2. Короткая визуальная реакция без изменения AI

Добавить transient hit timer/contact к `RuntimeEnemy` и уточнить contact для turret. Таймер обновлять в fixed step, рядом с существующими combat timers. Начальное пробное значение — 0,07 с; повторное попадание обновляет интервал, без бесконечного накопления. При death/restore/restart transient state очищать. Эффекты не включать в disk save; отдельно проверить копирование encounter snapshot.

В `world_scene_renderer.cpp` и `queue_interactive_model` подать реакцию и для legacy rover mesh, и для multipart shooter. Сначала попробовать имеющуюся emission в `RenderItem` / `RenderQueue::push` и `assets/shaders/vulkan/mesh.slang`, сохранив исходную emission модели после реакции. Не копировать textures/material assets на каждое попадание.

**Ограничение backend:** `vulkan_mesh_renderer.cpp` включает `emissive_factor` в ключ материала. Непрерывно меняющийся float не следует использовать для fade без проверки cache/resource behavior. Первый кандидат — два фиксированных, заранее подготовленных состояния normal/hit, переключаемых таймером. Для материалов с emissive map отдельно проверить, покрывает ли реакция всю требуемую поверхность: текущий shader умножает emission на карту. Если нужен независимый равномерный flash, рассмотреть небольшой per-draw параметр только после проверки material/instance data path, без обещания «shader уже всё поддерживает».

Искры переиспользовать, но поставить их в сохранённую точку контакта с корректным направлением. Не дублировать trace burst и target burst для одного попадания. Количество эффекта ограничить на discharge/target; проверить переживание следующего выстрела, поскольку текущие traces перезаписываются.

### 3. Довести звук, затем оценить gameplay response

На первом этапе оставить существующие cached cues и лимит двух world cues на discharge. Actor hit sound связывать с actor result, пространственный impact отличать от общего HUD confirmation. Для разнообразия достаточно небольшого конечного набора заранее загруженных вариантов; необходимость нескольких одновременных voices подтвердить прослушиванием дробовика/частой стрельбы. Не расширять AudioSystem заранее ради произвольной полифонии.

Stagger вводить отдельным изменением с условиями eligible weapon/role, cooldown и пределом продления. В `update_enemies` использовать явное краткое состояние/таймер реакции, не подменять общий attack timer при каждом попадании. Сначала определить, можно ли прерывать Windup/Lunging: текущие regression tests защищают committed attack и отсутствие restart/recovery bypass. Турели и guardian не должны автоматически получать правила rover.

Если knockback нужен по результатам плейтеста, добавить ограниченную затухающую horizontal velocity к существующему `move_and_collide`; не сдвигать position напрямую и не вводить ragdoll. У роботов нет подтверждённого authored mass-контракта в этом пути: не выдумывать физическую массу из health. Сохранить collision, navigation и reset invariants; любые новые gameplay fields проверить на save/restore.

### 4. Camera feel — только при подтверждённой необходимости

Начать с имеющейся анимации оружия. Если нужен hit-only impulse, использовать отдельный ограниченный presentation offset с decay, масштабируемый `camera_motion_`; не менять базовый mouse-look pitch и направление следующего выстрела. Значения brief 0,15–0,35° — эксперимент, не утверждённый default. Global hitstop исключить; animation-only pause отложить, пока нет конкретной анимационной потребности.

## Что поправить при превращении brief в исполнительную задачу

- Удалить требование совместимости с Vulkan 1.3: [текущий контракт](../RENDERER_MODERNIZATION_PLAN.md) требует 1.4, fallback 1.3 отменён.
- Не считать flash/VFX/audio/recoil отсутствующими: G-003 и E-015 уже закрыли значительную часть этих каналов. Точные пробелы перечислены выше.
- Разделить visual confirmation и balance-changing stagger. Не выдавать длительность реакции из brief за разрешение менять committed AI attacks.
- Fetch/worktree правила brief относятся к предложенному будущему workflow; этот импорт не делает их обязательными инструкциями. Источник и revision каждой будущей реализации фиксировать по действующему поручению.
- Сохранить приоритет H-008 и использовать его сопоставимые captures при замере нового эффекта; не утверждать, что эффект дешёвый только из-за малого числа частиц.

## Проверка будущей реализации

Расширить существующие [combat tests](../../tests/combat_runtime_tests.cpp) и [enemy tests](../../tests/enemy_runtime_tests.cpp): nearest actor/world occlusion, реальные normal/identity/damage, несколько целей дробовика, один reaction trigger на target/discharge, miss/cooldown, смерть во время эффекта. Сохранить tests `nonlethal hits do not restart rover windup or bypass its recovery` и `lunge rover cannot cross a newly closed door or retarget when hit`; менять ожидания только при явном выборе нового gameplay-контракта.

На уровне runtime проверить очистку timer/contact и offsets при death, retry, restart и disk restore. Для renderer — normal/hit/expired изображения legacy rover, multipart shooter и turret, SDR/HDR, исходная emission и отсутствие material-cache роста при повторных попаданиях. Выбрать существующие targets/presets по текущему CMake и verify skill в момент реализации; здесь команды сборки не выдаются за выполненные.

Ручной sound-on прогон G-019 и имеющихся bench encounters: пистолет/дробовик, попадания и промахи, металл/бетон/робот, overlapping hits, дверь в движении, убийство, повтор встречи, низкая частота кадров. Оценить читаемость, шум, слышимость и комфорт камеры. После этого — сопоставимый CPU/GPU/frame-pacing замер с включёнными/выключенными новыми эффектами на одной revision/settings/route. Дополнительные ZIP/standalone packages для этой работы не нужны.

Объём: actor-result + visual confirmation — средний локальный slice; audio variation — небольшой отдельный slice; stagger/knockback — средний gameplay slice с более высоким риском регрессии. Это оценка связности работ, не календарный срок. Реализация и runtime-приёмка остаются будущей работой.

## Реализация actor contact по отдельному поручению автора

2026-10-16 реализован только шаг сохранения actor contact. `CombatFireResult::actor_contact` — optional `CombatActorContact` с kind (`Turret`/`Enemy`), индексом в соответствующем runtime span текущей level session, position, normal, нормализованным direction и фактически снятым damage. Индексы не являются persistent ID и не предназначены для переноса между сессиями. Engine data не содержит game-specific types.

`apply_player_fire` и `fire_player_ray` сохраняют нормаль из существующего AABB intersection вместо `-direction`, без дополнительного raycast; та же нормаль передаётся shot trace. Для mobile enemy также заполняется `hit_distance`. Каждый элемент `PlayerAttackResult::rays` сохраняет свой contact, включая разные цели одного выстрела дробовика; верхние aggregate fields оставлены для существующих consumers. Для всех попаданий дробовика authoritative список — `rays[0..ray_count)`, а не aggregate `combat`.

Resolved actor hits не помещаются в `world_impact`. Существующий special collider path guardian и прочих query actors не изменён: он ещё может возвращать `world_impact` и не означает resolved damage по turret/mobile enemy. Новые эффекты, AI reactions, форматы save и tuning не добавлены.

Проверка: Windows / VS 2026 x64 Developer PowerShell, существующее дерево `msvc-dev`, валидный `VCPKG_ROOT`. `cmake --build --preset tests-dev --target relaycore_tests -j 2` сначала остановился внутри sandbox с C1902 (PDB manager mismatch) до компиляции изменённых файлов; тот же build вне sandbox прошёл, включая итоговую пересборку после форматирования. Были предупреждения в незатронутых `demo_game_source_debug_commands.cpp`, `world_scene_renderer_props.cpp` и `pipeline_authoring_overlay_tests.cpp`.

После discovery через `ctest --preset test-dev -N -R '^relaycore\.(.*actor contact.*|shotgun .*|combat runtime .*|player fire .*|player bullets .*|a missed shot .*|nonlethal hits .*|lunge rover cannot .*|weapon impacts .*)$'` выполнен тот же фильтр с `--output-on-failure --no-tests=error` вместо `-N`: **16/16 Passed, без Skipped**, 3,99 с. Три новых cases проверяют turret/mobile contact, face normal при косом попадании, lethal damage cap и разные mobile targets дробовика; существующие cases дополнены assertions actor contact для occlusion/range/early-kill и нескольких turret targets. Проверки formatting изменённых runtime-участков и `git diff --check` прошли. Ручной плейтест, graphics review и измерение производительности не выполнялись.

Pre-commit потребовал исправить сортировку includes в `combat_runtime_tests.cpp`; после `clang-format -i tests/combat_runtime_tests.cpp`, повторной узкой сборки и того же CTest-фильтра результат снова **16/16 Passed**, 4,01 с.

## Короткая визуальная реакция — реализация 2026-10-16

По отдельному поручению автора реализован шаг 2 поверх уже сохранённых actor contacts. Живые rover, multipart shooter и turret получают transient `CombatHitFeedback`: первый контакт цели в discharge и интервал 0,07 с. Повторное попадание заменяет интервал; дробины не складывают длительность и не создают отдельный burst на каждую дробину. Эффект остаётся у цели после следующего выстрела/промаха и не зависит от перезаписываемых traces. World impacts и их существующие следы/звук сохранены.

Материал переключается между исходным состоянием и фиксированной тёплой emission `(0.65, 0.35, 0.12)`. Для flash emissive map временно отключена, чтобы её тёмные texels не скрывали реакцию; после истечения интервала возвращаются исходные map/factor. Оба варианта заранее включены в `resource_items()`, textures/material assets при попадании не копируются. Плавный float fade и shader/backend изменения не добавлены.

Существующие искры живой цели перенесены в actor contact с нормалью пересечённой грани. Соответствующий trace burst подавлен, поэтому нет двойного эффекта. Для уничтоженного mobile actor сохранён один trace burst на цель; существующий death burst турели сохранён. Новый flash очищается при уничтожении, истечении времени, смерти игрока и успешном snapshot/disk restore. Encounter snapshot использует `SavedGame`, а не копию `RuntimeEnemy`; новый transient не сериализуется. Перезапуск получает свежий level session.

AI modes, committed windup/lunge/recovery, damage, cooldown, navigation, camera и audio tuning не менялись. Guardian остаётся за пределами этого шага. Человеческая оценка читаемости/комфорта, sound-on маршрут и сопоставимый замер стоимости эффекта остаются открытыми; ускорение или отсутствие влияния на FPS не заявляются. Текущие статусы G-003/E-015/H-008 этим изменением не переопределяются.

**Проверка:** VS 2026 x64 Developer PowerShell, существующий `msvc-dev`. Собраны `relaycore_tests`, `relaycore_power_restoration_graphics`, `relaycore_vulkan_mesh_tests` через `cmake --build --preset tests-dev --target relaycore_tests relaycore_power_restoration_graphics relaycore_vulkan_mesh_tests -j 2`; после последующих уточнений успешно пересобраны затронутые targets. Игровой executable также собран командой `cmake --build --preset tests-dev --target relaycore_tests relaycore_demo -j 2`. Сборки выполнялись вне sandbox: первоначальная попытка упёрлась в сетевое ограничение vcpkg; один промежуточный copy DLL конфликтовал с запущенным graphics-тестом. Обязательный route shooter добавлен в новую fixture после её первого запуска. Предупреждения остались в прежних незатронутых `demo_game_source_debug_commands.cpp` и `world_scene_renderer_props.cpp`.

Итоговая выборка после CTest discovery:

```powershell
$hitFilter = '^(relaycore\.(.*actor contact.*|actor hit feedback .*|turret hit feedback .*|shotgun (actor|hit|has|respects|can|handles).*|combat runtime .*|player fire .*|player bullets .*|a missed shot .*|nonlethal hits .*|lunge rover cannot .*|weapon impacts .*|enemy HUD .*)|graphics\.(actor_hit_feedback.*|resource_preparation))$'
ctest --preset test-dev -R $hitFilter --output-on-failure --no-tests=error --output-log build/msvc-dev/actor-hit-feedback-ctest.log
```

**22/22 Passed, 0 Skipped, 10,13 с**: 19 combat/lifecycle cases и 3 graphics cases на RTX 4060 Ti. Проверены refresh/expiry, miss/cooldown, lethal hit, разные живые цели дробовика, отсутствие дублированных trace bursts, disk/encounter restore и прежние committed attack regressions. Старое ожидание metal trace для живого попадания обновлено на actor feedback; для уничтоженной цели trace по-прежнему проверяется.

`graphics.resource_preparation` теперь также проверяет 12 переключений normal/hit у rover/shooter/turret: **90 материалов и 489 GPU allocations**, без роста после подготовки. Первоначальный провал прежней части теста оказался освобождением одного 16-byte upload staging buffer (867 → 866 allocations, 254366432 → 254366416 bytes), а не новой allocation при отрисовке. В тесте перед baseline добавлены завершение preparation uploads и collection staging; строгое последующее сравнение сохранено. Runtime/backend не менялся. Три повторные подготовки pressure_transfer после этого сохраняют 866 allocations / 254366416 bytes.

`graphics.actor_hit_feedback` и `graphics.actor_hit_feedback_hdr` сохранили normal/hit/expired кадры всех трёх представлений в `build/msvc-dev/tests/power-captures/hit-{sdr,hdr}/`; визуально просмотрены HDR shooter normal/hit/expired, SDR rover/turret hit. Исходный вид возвращается, flash покрывает multipart model. Это hardware-sRGB SDR и HDR scene pipeline с readback, не приёмка физического HDR-дисплея. В `msvc-dev` Vulkan validation выключена (`validation=false`); core/synchronization validation этой проверкой не подтверждены. Проверки `clang-format` изменённых C++ файлов и `git diff --check` прошли. ZIP/standalone package не создавались.
