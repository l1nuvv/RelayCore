# Project plan — RelayCore

Дата исходного анализа: 2026-09-29. Текущее состояние и порядок актуализированы 2026-10-08; исторические проверки ниже сохранены отдельно.

Этот документ хранит стратегию, зависимости, milestones и решения о границах разработки. Актуальный план — раздел [Development strategy](#development-strategy--2026-10-07); нижележащие N0–N8 и M0–M6 сохраняют историю прежнего планирования. Единственная implementation-очередь, фактические статусы tickets и evidence остаются в [PLAYABLE_SLICE_ROADMAP.md](PLAYABLE_SLICE_ROADMAP.md). Запись плана не запускает реализацию автоматически.

## Development strategy — 2026-10-07

### Основание и главный вывод

Стратегический анализ выполнен по чистому рабочему дереву `dev`, commit `c1118a1`. Проверены текущие source/CMake/tests, карты, asset declarations, существующие JUnit reports и runtime captures. Новые build/test/profile/playtest при анализе и записи этого плана не выполнялись. Код, assets, зависимости, версия и release artifacts не менялись.

Рекомендация: **развивать законченные игровые итерации, добавляя engine capabilities только под обнаруженные препятствия**. G-015/E-018 технически выполнены. По переданному 2026-10-07 отзыву ближайший порядок: **G-016 / HUD и турели → бой pressure_transfer → атмосфера → «Два сектора» → повторный слепой тест**, затем следующий компактный эпизод. Полученный отзыв не содержит версии и замеров; внешняя приёмка остаётся открытой. Другой ПК по-прежнему отложен.

Обозначения evidence: **Confirmed** — видно в реализации или указанном отчёте; **Likely** — обоснованный вывод без прямого измерения; **Hypothesis** — требуется runtime, profiling или наблюдение игрока. Оценки стоимости и распределения времени ниже — рекомендации, не измерения.

### Фактическая точка проекта

| Area | Current state / evidence | Limitation / urgency |
| --- | --- | --- |
| Lifecycle и ownership | [Application](../src/relaycore/app/application.cpp) владеет SDL/window/renderer/audio/delegate; EngineServices заимствует доступ | Сохранять порядок shutdown и session replacement; нового service framework не нужно |
| Simulation/input | [FixedSimulationLoop](../src/relaygame/fixed_simulation_loop.h), 120 Hz; discrete actions ждут tick; отдельный ImGui input ownership | Retry/activation проверять на границах состояний |
| Engine/game | Игра готовит typed runtime collections и render submissions; engine не зависит от game headers | Combat primitives/GameHud специализированы, но feature blocker для массового переноса не установлен |
| Vulkan | [VulkanDevice](../src/relaycore/render/vulkan_device.cpp), swapchain, VMA, dynamic rendering, synchronization2, timeline uploads, Slang SPIR-V | Migration завершена; API 1.3 недостаточно без требуемого swapchainMaintenance1; другой GPU ещё не принят |
| Rendering | [WorldSceneRenderer](../src/relaygame/world_scene_renderer.cpp): opaque/overlay/transparent, shadow casters/viewmodel, chunk/frustum culling, transparency sorting | Отдельные draw submissions; measured steady-state scaling bottleneck не установлен |
| Materials/light | [.rcmodel v3](../src/relaycore/render/model_asset.h): normal/tangents, scalar roughness/metalness; shadows, HDR/tone mapping, SH9 probes | Нет ORM/emissive maps/environment specular; probes используются не всеми игровыми картами |
| GPU preparation | [VulkanMeshRenderer](../src/relaycore/render/vulkan_mesh_renderer.cpp) приобретает mesh/material и ждёт uploads при первом render | Подтверждённый consumer E-018; NOW |
| Native pipeline | [20 GLB declarations](../cmake/RelayCoreModelAssets.cmake), importer, versioned products, SHA256 receipts, manifest/staging/cache/reload | Static opaque subset; embedded textures; нет universal catalog/content cache, и пока они не обязательны |
| Physics | [WorldQuery](../src/relaycore/physics/world_query.h), [PhysicsWorld](../src/relaycore/physics/physics_world.h): AABB плюс opt-in Jolt static meshes/sweeps, independent collision product | Dynamics/CharacterVirtual отсутствуют; useful static Jolt slice уже выполнен |
| Gameplay | Два оружия, rover/turret/hazards, interaction/mission, death/continue/finale/transitions; [pressure tests](../tests/pressure_transfer_tests.cpp) | Поздний respawn у исходного входа; самостоятельное качество нового эпизода ещё не принято |
| AI | [Enemy runtime](../src/relaygame/enemy_runtime.cpp): LOS/range, last_seen, Idle/Chasing/Windup/Recovery/Dead | Нет обхода препятствий; текущие две тактики работают, E-016 условный |
| Authoring/tools | [Editor](../src/relaygame/level_editor_document.h): transforms/properties/placement/browser/links/undo/preview; collision overlay/inspection, CLI checker | Save flatten-ит groups/templates; repository source, staged assets и user copy различаются |
| Engineering | [Presets](../CMakePresets.json), [tests](../tests/CMakeLists.txt), [CI](../.github/workflows/ci.yml), ASan/clang-tidy/Tracy, release/update tools | GPU CI opt-in; существующие reports не равны новому полному зелёному baseline |

Устойчивая foundation для текущего масштаба: explicit lifecycle, fixed-step/input, prepared session replacement, interaction/combat/mission/save flows, Vulkan cutover и declared native model build/staging. Transitional: сосуществование AABB/OBJ/BMP/native paths, синхронный scene submission, lazy GPU acquisition, string-based authored properties, save fingerprint и editor flattening. Это ограничения, а не автоматическая очередь rewrite.

G-011–G-014 технически реализованы, но человеческая приёмка открыта. Только направления/условная работа: navigation, KTX2/shared textures, animation, dynamics, common catalog/compiler, Source 2 importer. Не предлагать заново Vulkan migration, VMA, Slang, native models, normal maps, HDR, decals, steam/sparks или frustum culling: они уже существуют в ограниченном полезном виде.

### Bottlenecks и баланс engine/game

1. **Player experience подтверждён слабее технической достижимости.** [G-011](playtests/G-011-2026-09-29.md) и [G-012/G-013](playtests/G-012-2026-10-07.md) сохраняют незаполненные человеческие критерии. Scripted routes не доказывают pacing, самостоятельный выбор оружия, читаемость проёма или удовольствие. Эти утверждения остаются Hypothesis.
2. **Retry и session entry имеют конкретные издержки.** [Respawn](../src/relaygame/game_world_runtime_feedback.cpp) возвращает к исходному spawn при сохранённом progress; [G-014 profile](profiling/G-014-2026-10-07.md) фиксирует first-frame uploads/waits. Это G-015/E-018, а не новый save/renderer framework.
3. **Повторяемость content production не измерена.** Assets/tools есть, но скорость изготовления следующей комнаты/эпизода и самая дорогая ручная операция неизвестны. Likely следующий прирост — reusable kit и локальное сокращение workflow, не universal editor/database.

Проект сейчас сильнее ограничен game/content iteration, чем отсутствием engine capabilities. Рабочая стратегическая оценка — около двух третей продуктовых/content ограничений и одной трети конкретных engine/workflow ограничений; это не статистика. Native models/static collision перестали быть исходными capability blockers. AI становится blocker только на необходимом layout с воспроизводимым обходом.

### Архитектурные решения

Сохранять ownership Application → subsystems, заимствующий EngineServices, authored → typed session split, renderer-owned GPU resources и Jolt Pimpl. Порядок fixed step существенен: movement/focus/fire → use/triggers → doors → enemy movement/LOS → combat threats → mission/presentation. Первые consumers видят предыдущее завершённое состояние дверей, enemies — обновлённое.

Scene submission завершается до UI, frame backend удерживается через UI reload/present. Это понятный lifetime contract с ограниченным CPU/GPU overlap; E-018 не требует его автоматической замены. Размер GameWorldRuntime сам по себе не доказывает structural blocker.

TOML сохраняется: groups/templates, targets, volumes, entries/exits и native placement достаточны небольшим indoor episodes. Ограничения — flattening, string properties, sequential mission steps и fingerprint-based save compatibility, а не простота синтаксиса. Branching quest graph/GUID migration нужны только под реальное требование.

### Assets, rendering и physics: порядок расширения

```text
GLB → importer/ModelAssetData → .rcmodel + receipt → manifest/staging
    → StaticModelCache/immutable CPU resources → renderer-owned GPU uploads
collision TOML → compiler → independent .rccollision + pairing/receipt
    → staging → session preparation → PhysicsWorld/WorldQuery
map/models/materials/grid → fingerprint/baker → .rcprobe → runtime staleness check
```

Несколько primitives/materials/transforms уже поддержаны. .rcmodel v3 читает также v1/v2. Unsupported skins/animation/morphs/alpha/double-sided/maps диагностируются. Collision не выводится неявно из render triangles. Relative product path — текущая authored asset identity; canonical path/encoding/file metadata — runtime cache key, а не content-addressed compiler cache. Reload следует за build/staging и сохраняет рабочий ресурс при failed preparation.

**Минимальный следующий прирост pipeline:** reusable набор existing props/materials/collision conventions → новая комната через TOML/editor → запись ручных шагов/задержек → одно локальное исправление. E-017 уже переиспользует актуальные irradiance products; старый 202-секундный rebake не оправдывает новый cache framework.

| Direction | Priority / consumer |
| --- | --- |
| GPU preparation | NOW: session entry/retry pressure_transfer, E-018 |
| Lighting/shadows | Сначала authored настройка и наблюдение читаемости угроз/устройств; не новый lighting framework |
| Decals/FX/transparency | Использовать существующие marks/steam/sparks/sorting; impact visual или native alpha — под конкретный недостаток/asset |
| ORM/emissive/reflections | Later: один используемый asset не выражается factors/текущим освещением |
| KTX2/shared textures | Later: compressed/offline mip/HDR/cubemap или measured memory/loading consumer |
| Skinning/animation | Later: реальный animated enemy/character |
| Batching/instancing/occlusion/streaming | Только measured scene submission/GPU/memory pressure; shared CPU assets уже есть, instanced GPU draws ещё нет |
| Shader tooling | Build-time Slang/SPIR-V и rollback; reflection/checks при конкретной interface problem |
| Render graph/bindless/GPU-driven/async transfer | Сейчас consumer не установлен |
| Physics | Useful Jolt slice уже выполнен; следующий independent collision product под traversable geometry; dynamics только под игровое взаимодействие |

### Стратегические варианты

| Option | Benefit | Cost/risk | Learning/game value / delayed work |
| --- | --- | --- | --- |
| Game-first | Быстрый новый playable content | M–L; обходные решения и session hitch могут сохраниться | Высокая gameplay польза; GPU preparation/workflow позже |
| Engine-foundation | Rich materials/navigation/texture/renderer capabilities | XL; высокий риск отсутствия consumer | Высокое low-level обучение; приёмка/эпизоды откладываются |
| Content-pipeline-first | Ускорение export/build/stage/editor | L; риск автоматизировать редкую операцию | Asset/tooling обучение; combat/pacing позже |
| **Hybrid вокруг эпизода** | Два подтверждённых исправления → content iteration → условная capability | M–L; нужны scope и stop conditions | Связывает C++/Vulkan обучение с игровым результатом |

Выбран Hybrid: одновременно один content consumer и не больше одного обслуживающего capability ticket; новая система не добавляется ради количества. Техническая готовность и человеческая приёмка учитываются отдельно.

### NOW / NEXT / LATER / NOT NOW

**NOW:** D1/G-015 и D2/E-018 выполнены; технические итерации D3/G-016 завершены. Автор 2026-10-08 явно отложил приёмку G-016 и поручил D4: новый «Контур охлаждения» технически реализован как [G-017](PLAYABLE_SLICE_ROADMAP.md#g-017--d4--контур-охлаждения-и-повторяемое-производство). **NEXT:** локальный проход нового эпизода, открытые прослушивание/слепой тест/приёмка G-016; D5 и полный профиль после возобновления внешних проверок. E-016, новый collision product и material slice — условные ветви по consumer.

**LATER:** Recast/Detour, KTX2/shared textures, animation, reflection probes и Source 2 offline import после собственных native-content gates. **NOT NOW:** ECS, service/DI/plugin frameworks, render graph, bindless/GPU-driven/ray tracing, scripting/networking, engine-wide jobs, universal asset database, procedural generation, general dynamics, custom allocator, large rewrite.

### Milestones и impact

D1–D5 — обозначения milestones этого стратегического цикла, не новые ticket IDs и не дублирующие статусы. Impact 0–3 дан в порядке game / engine / iteration / learning. Scope S/M/L относительный, без календарного обещания.

| Milestone | Impact | Cost | Risk | Dependency | Consumer |
| --- | --- | --- | --- | --- | --- |
| D1. Локальный retry | 3/1/3/2 | S–M | Medium | Технический G-014 | Смешанная встреча pressure_transfer |
| D2. GPU preparation | 2/3/2/3 | M | Medium | D1, свежий baseline | Session/reload/transition |
| D3. Доводка эпизода | 3/1/2/3 | M | Medium | D1; D2 желательно | Игрок pressure_transfer |
| D4. Повторяемое производство | 3/2/3/3 | L | Medium | Локально принятый D3 | Следующая карта |
| D5. Приёмка/hardening | 3/2/1/3 | M | Medium | Проходимый эпизод, внешняя доступность | Игрок/целевое оборудование |

### D1 — безопасный локальный retry

**Goal / why now:** после достижения mixed hall смерть возвращает к защищённому подходу; конкретный недостаток исходного respawn уже подтверждён. **Scope/deliverable:** G-015, минимальная authored привязка retry pose к существующим progress/entry data. **Non-goals:** полный rollback, оживление врагов, checkpoint framework, новый save format без необходимости. **Player/developer effect:** меньше повторного маршрута и быстрее проверка боя. **Dependency:** технический G-014.

| Problem → Change → Expected effect → Verification |
| --- |
| Respawn только у входа → явный выбор безопасной pose по progress → повтор нужного участка → смерть до/после mixed hall, pose/native collision/door clearance |
| Save/snapshot/session replacement могут расходиться с retry → согласовать consumers → живой continue сохраняет позицию, мёртвый получает корректный retry → affected mission/respawn/save/lifecycle и обе pressure routes |

**Exit:** состояния согласованы, kills/inventory/devices/mission не откатываются, прежние карты сохраняют поведение. Удобство/длительность retry человеком остаются отдельным наблюдением. **Stop:** не добавлять произвольный checkpoint graph/UI.

Реализация по поручению 2026-10-07: `mission_step.retry_entry` связывает завершённый шаг с существующим `level_entry`; подготовка сессии разрешает и проверяет pose. В `pressure_transfer` шаг входа в mixed hall выбирает защищённый подход `(28;0,01;29)`. Фактические проверки и отложенная человеческая приёмка — в карточке G-015 roadmap.

### D2 — GPU resources до первого игрового кадра

**Goal / why now:** uploads из measured first-frame path выполняются до playable frame; loading cost остаётся видимым. **Scope/deliverable:** E-018 поверх prepared scene и renderer-owned acquisition. **Non-goals:** async streaming, global residency cache, frames-in-flight rewrite, render graph/jobs. **Player/developer effect:** ровнее вход в сессию и явная preparation boundary. **Dependencies:** D1, актуальный baseline.

| Problem → Change → Expected effect → Verification |
| --- |
| G-014 capture предшествует placement fixes → повторить тот же workload на текущих inputs → корректная исходная точка → build/map/config identity, loading/first/warm intervals |
| mesh/material upload происходит при render → подготовить required resources до gameplay → убрать uploads из первого игрового кадра → optimized Tracy same-workload before/after и first-frame completeness |
| Prepare/replacement может завершиться ошибкой → сохранить lifetime/rollback → нет stale handles/накопления → repeated menu/editor/reload/session, failed-prepare, affected graphics/validation без skips |

**Exit:** required uploads отсутствуют в первом playable frame, visual/lifetime/rollback сохранены; loading/memory cost явно записаны. **Stop:** не менять steady-state scheduling без нового measured bottleneck; перенос затрат не называть сокращением total load time.

Реализация по поручению 2026-10-07: сцена перечисляет mesh/material resources независимо от visibility и игрового состояния; renderer-owned acquisition выполняется до установки игровой/editor-сессии и commit нового backend при reload/HDR. Existing weak CPU identities и очистка expired GPU resources сохранены. Before/after и границы проверки — в [отчёте E-018](profiling/E-018-2026-10-07.md); фактический статус принадлежит roadmap.

### D3 — содержательная доводка pressure_transfer

**Goal / why now:** вход, три роли встреч, перепуск, выход и retry образуют понятный короткий опыт; новая native-встреча уже интегрирована. **Scope/deliverable:** одна серия наблюдений и небольшие layout/feedback/presentation changes. **Non-goals:** новая карта/оружие/enemy и увеличение времени пустыми переходами. **Player/developer effect:** читаемые решения и evidence для следующей capability. **Dependencies:** D1; D2 желательно.

| Problem → Change → Expected effect → Verification |
| --- |
| Две scripted траектории не доказывают человеческий выбор → авторский проход с записью выбора оружия/позиции → найти реальную проблему → время, смерти, пропущенные cues, interventions |
| Выявлено главное затруднение → изменить одну причину: sightline/cue/placement/timing → улучшить конкретное решение → повтор того же сценария и affected content/graphics |
| Сила реакции мира неизвестна → проверить существующие свет/вентилятор/ambient/exit cues, при нужде согласовать → заметный перепуск → observation before/after, save/continue, реальное прослушивание audio |

**Exit:** автор явно принимает локальную итерацию, существенная обнаруженная проблема исправлена; человеческие критерии G-011/G-014 проверяются отдельно. **Stop:** не расширять systems после принятия выбранного опыта. HUD сейчас объясняет тактику текстом; его избыточность — Hypothesis, не повод автоматически удалять подсказку.

#### План по отзыву слепого теста — 2026-10-07

Автор передал отзыв стороннего игрока: мелкий текст, предпочтение задач слева, непонятные и короткие «Два сектора», отсутствие причины использовать дробовик и шкафы, лёгкий/пустой «Перепуск давления», пожелание более тёмной загадочной атмосферы и висящие турели. Это человеческий отчёт об опыте; сборка, дата самого прохода, время, смерти и подсказки неизвестны. Причины ещё требуют воспроизведения. Нельзя автоматически относить замечания к последнему visual pass. Исходные наблюдения сохранены в [протоколе G-011](playtests/G-011-2026-09-29.md#отзыв-стороннего-игрока-передан-2026-10-07).

**Цель цикла:** игрок понимает ближайшую цель, осознанно выбирает оружие/позицию и видит результат действий. Владелец scope/status/evidence — [G-016](PLAYABLE_SLICE_ROADMAP.md#g-016--доводка-существующих-сценариев-по-слепому-тесту). По последующему поручению автора итерации 1–4 технически выполнены 2026-10-07/08; итерация 5 подготовлена. Остались прослушивание, повторный слепой проход обоих сценариев и приёмка человеком; подробности в [протоколе](playtests/G-011-2026-09-29.md#g-016--техническая-доводка-2026-10-08).

| Порядок / объём | Изменение и границы | Проверяемый результат |
| --- | --- | --- |
| 1. HUD и турели / S | Задачи перенести влево вверх; стартовая проба увеличения текста цели и взаимодействий на 20–25%, с корректным переносом строк. Проверить крепления, высоту и ориентацию турелей в затронутых сценариях; исправить подтверждённые зазоры/пересечения, переиспользуя kit. | На 1280×720 и 1920×1080 длинная русская цель, подсказка, здоровье/оружие читаются без перекрытий. Турель имеет видимую физическую опору; лучи, muzzle и направление атаки согласованы с моделью. |
| 2. Бой и пространство pressure_transfer / M | Сначала воспроизвести прохождение одним пистолетом. Затем по одной менять дистанцию, подход, sightlines и timing: ближний rover даёт короткое читаемое окно для дробовика; галерея поддерживает точный огонь; смешанный зал требует выбора позиции. Второй зал получает отдельное решение с пультом/перекрёстной угрозой. Убрать лишнюю площадь и шкафы либо связать их с реальной защищённой позицией. | У каждой встречи есть своё действие игрока. Дробовик заметно сокращает опасное ближнее столкновение, пистолет полезен издали. Открытая позиция действительно опаснее укрытия, смена позиции позволяет добраться до цели. Нельзя безопасно решить все встречи одной неподвижной позицией. |
| 3. Атмосфера pressure_transfer / S–M | После стабилизации layout настроить существующий свет, ambient и оборудование: локальные рабочие источники, тёмный фон, один связный загадочный мотив. Переиспользовать текущий manifold/cover/lining и art brief. Сопоставить перепуск до/после по свету, звуку и доступу к выходу. | Цель, проход, враг и укрытие различимы из игровых позиций. Игрок замечает изменение после перепуска и понимает, куда идти. Пустоту не заполнять случайным декором; новый renderer не требуется. |
| 4. «Два сектора» / M | После локальной проверки pressure_transfer довести foundation_intake/foundation_pump как короткий связный сценарий. Предлагаемая цепочка на existing mission/door/interaction: получить доступ в первом секторе → восстановить оборудование во втором → заметно открыть выход. Добавить одно пространственное или боевое решение, отличить функции/ориентиры секторов, сохранить переход и перенос инвентаря. Точные targets/layout определить в blockout. | Игрок может объяснить, зачем перешёл во второй сектор; действие во втором меняет мир и открывает понятный путь. Полный маршрут имеет хотя бы одно решение сверх последовательного нажатия кнопок. Переходы, save/continue и возврат, если он сохранён, работают. |
| 5. Повторный слепой тест / S | Использовать одну зафиксированную сборку и актуальные staged assets. Для оценки первого прохода нужен человек, не знающий новую карту; прежний тестер полезен для сравнения. Записать время участков, смерти, подсказки, оружие/позиции и причины выбора. | Без подсказки пройдены оба сценария; игрок читает цель, использует преимущества оружия/укрытия и замечает реакцию мира. Все восемь пунктов исходного отзыва получили наблюдаемый результат либо явно остались открытыми. |

**Решение по дробовику:** обязательную неуязвимость к пистолету пока не вводить. Сначала проверяется выгодность ближнего оружия в encounter. Если выбор всё ещё отсутствует, следующая итерация должна опираться на записанную причину; новый тип брони/врага не входит в этот цикл автоматически.

**Проверки по итерациям:** ближайшие HUD/graphics проверки для шага 1; Check Map, affected content/encounter tests, обе тактики смешанного зала и death/retry/save/continue для шага 2; одинаковые игровые камеры до/после и реальное прослушивание для шага 3; transition/inventory/mission/save tests и полный маршрут двух карт для шага 4. Точные существующие presets/targets выбрать перед реализацией через relaycore-verify. После каждой правки — просмотр scoped diff и `git diff --check`. Сборка/скрипты подтверждают техническую работоспособность; шаг 5 подтверждает опыт игрока.

**Длительность и stop conditions:** ориентир G-011 5–15 минут проверять замером; «Два сектора» могут остаться компактнее при явной авторской приёмке. Увеличивать время только осмысленными решениями. Заканчивать каждую итерацию на проверяемом результате, не менять одновременно глобальный баланс оружия и все встречи. Новые системы, противники, пакеты/ZIP и новый эпизод не входят в G-016. После повторного теста исправить оставшуюся главную проблему и решить о переходе к D4.

### D4 — повторяемое производство следующего эпизода

По явному поручению автора 2026-10-08 D4 начат с отложенной приёмкой G-016. Реализован «Контур охлаждения»; [G-017](PLAYABLE_SLICE_ROADMAP.md#g-017--d4--контур-охлаждения-и-повторяемое-производство) владеет статусом, [production evidence](playtests/G-017-2026-10-08.md) — командами, замерами и ограничениями. Человеческая приёмка D3 этим не подменена.

**Goal / why now:** следующий компактный episode создаётся existing kit/tools без копирования runtime logic; это проверка зрелости content workflow. **Consumer:** небольшой технический сектор с обзором цели, боковым доступом/коротким обходом, разными дистанциями боя и заметным включением оборудования перед выходом; main mission chain может остаться последовательной. **Scope/deliverables:** blockout, reused kit, 2–3 решения, полный route и production measurements. **Non-goals:** большой editor/schema rewrite и обязательные новые AI/material/physics features. **Player/developer effect:** новый опыт и доказанное reuse. **Dependency:** локально принятый D3.

| Problem → Change → Expected effect → Verification |
| --- |
| Reuse не проверен полным новым cycle → собрать blockout из existing props/doors/triggers/enemies → gameplay без нового foundation → Check Map, route, continue/exit |
| Authoring cost неизвестен → записать export/build/stage/placement/reload и ручные исправления → найти настоящий bottleneck → сопоставимая операция до/после |
| Операция часто повторяется → локально исправить существующий tool → меньше ручных шагов → повтор task без workaround |

Если необходимый layout требует rover обхода, сначала fixture E-016; затем минимальная navigation capability. Не открывать navmesh заранее и не ограничивать будущий дизайн прямыми коридорами бесконечно.

**Exit:** проходимый новый эпизод, reused gameplay rules, нет ручной правки products, costs записаны. Ускорение заявляется только по сопоставимым измерениям. **Stop:** tooling заканчивается после устранения выбранной операции.

### D5 — приёмка эпизода и measured hardening

Автор 2026-10-08 подтвердил, что непрерывный проход, прослушивание и слабый ПК пока недоступны, и поручил продолжить. Эти критерии отложены; доступная независимая работа — устранение воспроизводимых технических failures. Выбрано историческое падение `graphics.vulkan_mesh` на `mesh-reload-0`.

**Техническая итерация 2026-10-08:** failure воспроизведён с validation=true: `mask_diff=0`, `color_diff=0.111389`, `mean_rgb_delta=3.32784`. Причина установлена по inputs: frozen VK-012 samples из `d57f8d3` используют ambient floor/zenith/min 0,38/0,56/0,24, текущая игровая конфигурация — 0,14/0,22/0,08. Для synthetic mesh/reload/pass checks добавлен точный snapshot прежнего `render_authoring.toml` и отдельный `graphics.mesh_pipeline` (`--pipeline-only`). Все три reload и одиннадцать pass comparisons проходят с `color_diff=0`; shader failure rollback, VMA lifetime и resize assertions сохранены. Runtime, игровые configs, RCG samples и допуски не менялись.

Проверки: `cmake --build --preset graphics-debug --target relaycore_vulkan_mesh_tests -j 6` и `ctest --preset graphics-debug -R '^graphics\.mesh_pipeline$' --no-tests=error --output-on-failure --output-junit d5-mesh-pipeline.xml` успешны, 1/1 executed, без skipped/validation warnings. Повтор полного `graphics.vulkan_mesh` теперь проходит reload/pass checks и останавливается на отдельном live-world `power_restoration-0`: `color_diff=0.943333`, `mean_rgb_delta=19.2998` (`build/msvc-debug/d5-mesh-after.xml`). Его причина ещё не установлена; full graphics suite не объявлен зелёным. Это следующий независимый технический разбор, а не основание ослаблять или автоматически перезаписывать world baseline. Изменённые пути: `tests/vulkan_mesh_tests.cpp`, `tests/CMakeLists.txt`, `tests/graphics-baselines/{render_authoring.toml,README.md}`. Scoped clang-format и `git diff --check` прошли.

Локальная часть H-008 начата 2026-10-08 после коммита `fc88baa`: [optimized baseline обновлённого pressure_transfer](profiling/H-008-2026-10-08.md) содержит восемь повторяемых состояний и раздельные CPU/GPU/query/wait измерения. Непрерывный проход, звук, FPS budget и слабый ПК остаются открытыми; отложенная внешняя приёмка не объявляется выполненной.

**Следующая техническая итерация 2026-10-08 — power_restoration:** причина `power_restoration-0` подтверждена изолированным causal run: staged assets скопированы в `build/d5-reference-inputs`, только 11 authoring/map/atlas/security-model inputs восстановлены из `d57f8d3`. Текущий renderer прошёл четыре старых power samples (`color_diff` 0/0/0,000556/0,000556; mean RGB 0,000666/0,000709/0,150022/0,149554). Контрольный лог — `build/msvc-debug/d5-reference-world.log`; весь reference run не прошёл из-за отдельного pressure sample. Текущие power map/atlas/ambient отличаются после уже выполненного facility art pass, поэтому после просмотра четырёх актуальных кадров явно обновлены только `tests/graphics-baselines/power_restoration-{0,1}{,-shotgun}.rcgb`. Tests не генерируют samples автоматически; одноразовое точное BMP→RCG1 преобразование — `build/d5_promote_power.py`.

Для экспорта временно пропускались только четыре power comparisons; диагностическая правка удалена, test executable пересобран до контрольного запуска. `cmake --build --preset graphics-debug --target relaycore_vulkan_mesh_tests -j 6` успешен. `ctest --preset graphics-debug -R '^graphics\.vulkan_mesh$' --no-tests=error --output-on-failure --output-junit d5-world-after.xml` с validation=true прошёл mesh/pass и все четыре power comparisons с `mask_diff=color_diff=mean_rgb_delta=0`, затем остановился на `pressure_transfer-0` (`mask_diff=0.0197222`, `color_diff=0.973611`, mean RGB 22,8684). Full graphics suite остаётся красным; это следующий отдельный разбор. Runtime/config/assets, остальные RCG samples и допуски в этой итерации не менялись. Power map SHA256 `0637EE037268284A327241A0D7320F8DADEC5AA28CF292DFD8A6A5E325AC92CC`; atlas `237E95D08BA0F0C6953F614BBD782996A9B919A14FCABCEA70F3FFE59CAAA238`; render authoring `47F541C9B467F1226CA1E541DD8163EAC5D1CCDF2C4AD540F7D696E85373B4DE`. Scoped clang-format и diff-check прошли; непрерывный проход/звук/слабый ПК остаются отложенными.

**Goal / why now:** accepted playable result с актуальными техническими границами; scripted route/captures не устанавливают самостоятельную понятность. **Scope/deliverables:** human observations, continuous optimized capture, targeted fixes и согласованный другой GPU/ПК. **Non-goals:** обязательный ZIP/публикация/performance rewrite. **Player/developer effect:** проверенный опыт и основание следующего цикла. **Dependencies:** проходимый эпизод, возобновление внешних проверок автором.

| Problem → Change → Expected effect → Verification |
| --- |
| Нет blind first-pass evidence → existing protocol → принять/исправить readability/pacing → observations с подсказками/смертями |
| Профиль локальных состояний не покрывает маршрут → H-008 continuous capture → реальные CPU/GPU/load/query costs → fixed settings, percentiles/spikes |
| Compatibility только основного GPU → согласованный runtime/validation run → device/presentation и budget evidence → executed checks без skips, hardware/driver |

**Exit:** критерии приняты или конкретные ограничения явно согласованы. H-009 delivery отдельно по запросу candidate/release. **Stop:** исправлять observed/measured проблемы; не превращать hardening в общую portability программу.

### Dependency graph

```text
Existing Vulkan/native models/static collision
  → G-015/local retry → E-018/GPU preparation → pressure_transfer iteration
  → next compact episode → measured production loop → one local tooling fix
                         ↘ required blocked layout → E-016/navigation fixture
                         ↘ required shape → next collision product
                         ↘ required asset semantic → one material/texture slice
Playable episode → human acceptance when resumed → continuous profile/compatibility
```

Navigation не требуется retry/текущим двум тактикам; общий catalog не является dependency E-018.

### Existing roadmap reconciliation

| Existing item | Decision | Reason |
| --- | --- | --- |
| G-015 / E-018 | KEEP | Подтверждённые consumers; E-018 требует свежего baseline |
| G-011–G-014 | KEEP | Открытая человеческая приёмка сохраняется |
| H-008 | MODIFY | G-014 — partial local evidence; full route/другой ПК открыты |
| H-009 | REORDER | После приёмки/профиля и прямого delivery request |
| E-016 / G-010 | KEEP deferred | Нужен blocker/недостаток существующих угроз |
| E-003–E-014 / E-015 / E-017 / VK milestones | KEEP completed | Не повторять implementation; technical gates не закрывают human acceptance |
| Architecture C/D/E | MODIFY | Многие slices выполнены; broad production capability не объявлять целиком законченной |
| N5 rich features | MODIFY | По одному consumer, не обязательный комплект систем |
| N6 catalog/compiler; N7/N8 Source 2/full map | REORDER | После measured pipeline pressure/native-content gates; отдельный research consumer |
| fastgltf/Jolt/VMA/Slang | KEEP | Реальные foundations |
| meshoptimizer/KTX2/BLAKE3/zstd | KEEP conditional | Approved direction не означает немедленной интеграции |
| Обязательная modern renderer rewrite | DROP | Текущий workload этого не требует |

Architecture/capability reference и dependency policy остаются направлениями, не implementation queue. В roadmap отделять Active (один ticket), Near-term (2–3 milestones), Long-term capabilities (ссылки), Deferred (return condition), Completed (history/evidence). Не дублировать там все rationale и статусы в этом документе.

### Meaningful technical debt

| Location / symptom | When it matters | Minimal correction |
| --- | --- | --- |
| VulkanMeshRenderer lazy mesh/material uploads | Уже session entry | E-018 |
| GameWorldRuntime respawn у initial spawn | Уже late encounter retry | G-015 |
| game_world_runtime_update/player_feedback: audio occlusion/footstep surface смотрят solids/prop AABB, не общий native world | Native floors/walls дают другие surface/occlusion semantics | Перевести конкретный consumer на WorldQuery при появлении |
| LevelEditorDocument::save flatten-ит groups/templates | Повторное редактирование modular kit | Сначала authoritative source workflow; сохранять grouping при доказанной цене потери |
| runtime_model/save_game IDs/source/fingerprint | Требование saves через content updates | Explicit compatibility policy; stable authored IDs в owning ticket |
| Несколько исторических точек продолжения в plans | Агент повторяет закрытую работу | Одна queue; явно отделённая история |

Массовый перенос GameHud/combat, typed entity schema и file splitting не являются отдельной cleanup campaign без feature blocker.

### Performance и engineering evidence

[G-014](profiling/G-014-2026-10-07.md): first frame около 80 мс, следующие first-session frames 13–19 мс; scene GPU p99 ≤0,293 мс; mean queries/frame ≤0,00184 мс, views 77–85 draws. Это 1280×720/VSync off/RTX 4060 Ti local workload, не full-route budget и не measured regression от одной модели. Map hash текущего checkout отличается: placement changes произошли после capture. E-018 нужен актуальный baseline.

Likely future pressure: отдельные draws/descriptors, RGBA residency, scene preparation и более крупные bakes. Theoretical сейчас: custom allocator, async transfer, bindless/meshlets/jobs. Оптимизацию выбирать по attributable measurements.

При анализе прочитаны существующие Debug/optimized mounting reports по 9/9, decals по 2/2, G-014 по 8/8 без skips. Combined E-015/E-017 report содержит один failure ожидания диагностики; final tooling report 1/1 соответствует записанному исправлению. Это scoped historical evidence, не новая полная проверка. GPU CI opt-in и отдельно отклоняет skips; обычный CI не доказывает GPU execution.

Наибольший ROI checks: retry/save/mission, failed preparation/rollback, resource lifetime menu/editor/reload, importer negative/backward cases, collision pairing/transforms, rendered states и реальный audio. Presets/targets брать из текущего CMake; не создавать infrastructure ради coverage. ZIP/release только по прямому запросу.

### Iteration speed и learning

Текущие логические стадии (не stopwatch measurements): gameplay edit/build/tests/runtime — 4; shader edit/compile-stage/reload/view — 4; native texture edit/export-product/stage/reload/view — 4–5; model export/compile/stage/place/collision-view — 5–6; map edit/validate/stage/preview-route — 4–5; collision source/compile-pair/stage/overlay/gameplay — 5. Renderer требует affected GPU/lifetime checks, importer — negative tests и representative product/runtime; full release cycle не нужен.

Обучение: G-015 — state/persistence/ownership; E-018 — Vulkan upload/fence/timeline/lifetime/rollback/profiling; D3 — gameplay observation/controlled iteration; D4 — asset/tooling boundaries/reuse; E-016 при необходимости — nav geometry/path queries; texture slice — color/mips/compression/memory; D5 — CPU/GPU attribution/compatibility/evidence.

Распределение ближайшего цикла: **55% gameplay/content/observations, 25% конкретные engine capabilities, 20% tools/verification/evidence**. Не обязательная дневная пропорция. После NOW ожидаются local retry, prepared GPU resources и более обоснованная встреча; после medium horizon — следующий episode на existing kit и измеренный production loop. Новые AI/KTX2/animation заранее не обещаются.

### Stop conditions и отложенные направления

Asset pipeline останавливается, когда assets consumer воспроизводимо собираются/валидируются/загружаются без ручной правки products. Navigation — после необходимого обхода, door/unreachable behavior и regressions. Materials — на semantics используемого asset. Editor — после выбранной repeated operation. Performance — при выполненном budget workload; перенос времени между фазами не speedup total load.

| Not now | Return condition |
| --- | --- |
| ECS/service/DI/plugins, large rewrite | Повторяющийся structural feature blocker, не предпочтение стиля |
| Render graph | Реальные pass/resource dependencies стали неуправляемыми |
| Bindless/GPU-driven/instancing overhaul | Measured submission/descriptor scaling pressure |
| Ray tracing | Конкретный visual consumer и hardware budget |
| Scripting | Частые content changes требуют повторной C++ runtime logic/rebuild |
| Networking | Отдельное multiplayer product decision |
| Engine-wide jobs/custom allocator | Attributed parallel/allocation bottleneck |
| Universal asset database | Воспроизводимая проблема dependency/reference scale |
| Full PBR/animation/complex physics | Один используемый material/character/dynamic interaction consumer |
| Procedural generation | Доказанная replayability/production потребность |

### Три следующих действия

1. G-015: regression case смерти после mixed-hall entry с сохранением kills/mission/inventory, затем безопасная authored retry pose.
2. E-018: повторить существующий optimized `--pressure-transfer --profile` на текущей карте; отдельно loading/first/warm и identity inputs.
3. После retry: авторский проход pressure_transfer, запись потери маршрута/выбора оружия/повторного подхода; выбрать одну confirmed gameplay problem.

## Текущий цикл — игровая отдача native-базы, 2026-10-07

Этот раздел сохраняет основания и последовательность начала цикла. E-015/E-017 и технические G-013/G-014 уже выполнены; актуальный приоритет определяет Development strategy выше и единственная очередь roadmap. Формулировки о первоначальных проблемах ниже относятся к началу цикла.

Автор поручил продолжить развитие игры и движка на текущем ПК, временно отложив нового игрока и другой компьютер. Эти проверки сохраняют прежние открытые критерии, но не блокируют реализацию следующего цикла. Упаковка и публикация в поручение не входят. Статусы и evidence ведутся только в [roadmap](PLAYABLE_SLICE_ROADMAP.md#e-015--surface-aware-impact-feedback-по-потребности-n5).

Ближайшая цель — превратить уже работающие native geometry, collision, два оружия и две угрозы в более выразительные боевые решения. Сначала улучшить обратную связь, затем проверить два способа прохождения одной встречи, после этого перенести результат в существующий эпизод. Не наращивать число кнопок, площадь карты или список engine features ради объёма.

```text
E-015: слышимое попадание в поверхность и укрытие
  + E-017: пропуск повторного bake при совпадающих inputs
  → G-013: два различающихся решения встречи в bench_relay
  → E-016: обход препятствия rover, только если G-013 выявит необходимость
  → G-014: использовать результат в одном участке pressure_transfer
  → G-015: локальный retry этой встречи после смерти
  → E-018: подготовка GPU-ресурсов до первого кадра по Tracy G-014
  → локальный профиль, только если новый workload выявит проблему
```

**Основание на старте цикла:** `combat_runtime` обрезал трассу по ближайшему world hit, но терял material/normal/identity; `process_fire_input` воспроизводил hit cue только при уроне actor. В `bench_relay` уже есть metal/concrete surfaces, native стол с нижним проёмом и бой; это конкретный consumer E-015 без новой карты. Rover в `enemy_runtime.cpp` идёт прямо к `last_seen`, затем collision обрезает перемещение; обход препятствий отсутствует. Это ограничение возможностей, а застревание в будущей встрече ещё надо воспроизвести до E-016.

При проверке E-015 проявилось ограничение скорости итераций: перелинковка общего baker после audio/combat правки назначает полный bake трёх карт, хотя транспорт света и inputs прежние. E-017 добавлен как локальное исправление существующего pipeline с той же runtime-проверкой fingerprint; новый compiler/cache framework не требуется.

**Дизайн:** задать опыт и проверяемое решение игрока, связать layout, угрозы, оружие и feedback, затем менять одну причину за итерацию — по [DESIGN_REFERENCES.md](DESIGN_REFERENCES.md). Производственные примеры для дальнейшего изучения: [id Software, Embracing Push Forward Combat](https://www.gdcvault.com/play/1024940/Embracing-Push-Forward-Combat-in) описывает согласование FPS-боя с целевым опытом агрессии и скорости; [Epic, Designing GEARS OF WAR: Iteration Wins](https://gdcvault.com/play/529/Designing-GEARS-OF-WAR-Iteration) — отдельный доклад об итерации. Это источники для сравнения; для RelayCore сохраняются исследование, пространственное ориентирование, заметное изменение мира и локальные угрозы. Приведённые страницы не заменяют просмотр докладов или наблюдение собственной игры.

Каждая итерация заканчивается работающим изменением, ближайшими deterministic/content checks и runtime-проверкой затронутого пути. Автоматический маршрут подтверждает доступность решений и сохранение состояния; предпочтение тактики, читаемость и удовольствие остаются гипотезами до наблюдений автора/игроков. Отложенная внешняя приёмка не останавливает следующий технический ticket. Rendering, catalog, Source 2 import, новый противник и новое оружие остаются условными направлениями из N5–N8, а не обязательной следующей работой.

Продолжение после интеграции встречи — улучшить её повтор. Текущий respawn сохраняет прогресс, но возвращает к началу карты; для позднего боя это повтор уже пройденного маршрута. G-015 ограничен локальным retry выбранного участка и сохранением существующих save/world semantics. Его конкретные критерии и фактический статус ведутся только в roadmap.

Tracy G-014 выявил first-use uploads/waits в первом кадре новой сессии при малой steady-state GPU/query стоимости. После G-015 E-018 ограничен переносом необходимых GPU uploads в подготовку с проверкой lifetime и before/after на том же workload; новый renderer/cache framework из этих данных не следует. Подробные замеры принадлежат profiling report, критерии — roadmap.

## План развития после E-004 — 2026-10-02

Этот раздел сохраняет инженерную последовательность N0–N8, актуализированную 2026-10-07; ближайшую работу определяет текущий цикл выше. Предыдущий план M0–M6 и журнал ниже описывают игровой цикл и его проверки. Основная очередь и фактические статусы остаются в roadmap; N1 выполнен как E-005, выбранные N2 и N3 slices завершены как E-006–E-009; по последующему поручению выполнены E-010–E-014/N4.1–N4.5. G-012/N4.6 технически выполнен, статус `awaiting_playtest`; остальные этапы требуют своих зависимостей и поручения автора. Наличие плана само по себе не запускает реализацию.

### Цель и основание

Ближайший инженерный результат — согласованные world queries и независимая статическая mesh-коллизия для native-контента, с сохранением принятого движения. Следующий продуктовый результат — принятый самостоятельный эпизод с двумя оружиями, затем небольшой игровой участок, использующий новые native world capabilities. Проверочная комната N1 уже существует; повторять её создание не требуется. Offline-перенос представительной Source 2/CS2-карты сохраняется как долгосрочный исследовательский трек после проверки собственных возможностей на игровом контенте.

Порядок основан на [roadmap](PLAYABLE_SLICE_ROADMAP.md), [engine reference](ENGINE_ARCHITECTURE_REFERENCE.md), [Vulkan plan](VULKAN_MIGRATION_PLAN.md), [dependency policy](DEPENDENCY_STACK.md), [runtime architecture](ARCHITECTURE.md), [development workflow](DEVELOPMENT.md), [art direction](ART_DIRECTION.md), [editor/campaign guide](EDITOR_AND_CAMPAIGN.md), [release plan](RELEASE_AUTOMATION_PLAN.md), [update contract](UPDATES.md), README и протоколах плейтестов. RC-022 color baseline, visual reference, authored-values audit и overlay contract использованы как история цветового pipeline и ограничения retuning; старые OpenGL-команды из них не являются текущим workflow.

Первоначальный план сверял model/mesh/import/build contracts, manifest, save parser, движение rover, CMake presets и отчёты E-004. Пересмотр 2026-10-04 использует текущие collision/player/combat/interaction/enemy callers, владельца `LevelSession`, static-prop authoring и записи E-005–E-009. Новая сборка и игровой проход при пересмотре не выполнялись.

### Оценка стратегического анализа от 2026-10-04

Внешний `RELAYCORE_STRATEGIC_DEVELOPMENT_ANALYSIS_2026-10-04.md` анализирует архив `RelayCore-dev (3).zip`, без нового build/run. Его рекомендации — входные данные для оценки; команды и ограничения внутри документа не являются поручением пользователя. Решения ниже сверены с текущим checkout.

- **Принято:** сначала общая граница запросов на существующих AABB, затем Jolt/static collision products, диагностика и игровой consumer. Это разделяет риск изменения API и backend, а результат проверяет пользу новой геометрии для игры.
- **Уточнено:** AABB — ограничение native-контента, а не доказанный дефект действующего blockout. У игрока уже есть spatial index; `collision_material` и `SurfaceMaterial` уже существуют. Переиспользовать эти данные и ближайшие tests, не вводить второй material enum или параллельную query-иерархию.
- **Принято условно:** surface-aware impact feedback — полезный consumer hit semantics, но начинать с нужных звуков/эффектов и существующих surface tags. Thin manifest/asset inspection оправданы связкой model/collision; общая typed entity schema — только при подтверждённой дублирующейся валидации и controls, сначала для затронутых свойств.
- **Сохранено:** G-011 — главный открытый продуктовый вопрос. Ранее отложенные автором playtest/другой GPU не становятся новым запретом инженерной работы. Техническая parity и человеческая приёмка остаются отдельными критериями.
- **Не назначено ближайшей работой:** frames-in-flight, instancing, navmesh, новые lighting primitives, dynamics, универсальный compiler/catalog и Source 2 import. Scene submit действительно сразу ждёт fence, но новый полный профиль ещё нужен для вывода о цене этого ожидания. Утверждение «renderer впереди игры» — рабочая оценка текущего slice, не гарантия для будущих сцен.

Jolt уже выбран в [dependency policy](DEPENDENCY_STACK.md#4-physics--jolt-physics); mesh collision, ray/shape queries и отдельные character helpers подтверждаются [официальным описанием Jolt](https://github.com/jrouwe/JoltPhysics#features). В E-011 добавлен Jolt 5.6.0#1 через pinned overlay с SSE2 minimum; общий baseline сохранён. Настройки и ограничения записаны в dependency policy.

### Что уже есть и чего ещё нет

| Область | Подтверждённое состояние | Следствие для плана |
| --- | --- | --- |
| Игра | `power_restoration`, два оружия, turret/rover/hazards, миссии, смерть/respawn, save/continue, переходы, Sandbox | Использовать существующие механики; G-009 завершена, G-010 отложена |
| Новый эпизод | `pressure_transfer` технически проходим; G-011 `awaiting_playtest` | Нужны наблюдения самостоятельного игрока, а не повторная реализация карты |
| Vulkan | VK-013, VK-014 и VK-F01 завершены; единственный runtime backend | Развивать native capabilities поверх действующего backend |
| Модели | GLB → `.rcmodel` v3; loader читает v1–v3; opaque base-color, normal и opt-in roughness/metalness factors | N2 native material slice завершён; IBL, material data maps, alpha, skin/animation ещё не поддерживаются |
| Сборка контента | E-004: явный список GLB, per-preset products, staging, SHA256 receipts и packaging checks | Следующая модель должна проходить обычную сборку; общего asset catalog/cache ещё нет |
| Геометрия/движение | AABB и opt-in native collision products; прежний box movement, mesh sweeps; rover движется к `last_seen` без поиска пути | Render-модель требует отдельной authored collision; consumer G-012 технически выполнен, human acceptance ожидается |
| Освещение | Dynamic lights/shadows, fog/post; E-008 linear FP16 HDR/exposure/UI; E-009 baked diffuse SH9 probes | Native lab и G-012 имеют offline one-bounce products и invalidation; lightmaps, reflection probes и масштабирование света требуют отдельного consumer |
| Инструменты/доставка | Редактор и CLI проверки карты; Release/ZIP/updater уже существуют | Расширять действующий workflow под новый контент |
| Приёмка | H-008/H-009 открыты; blind playtest и другой GPU ранее отложены автором | Инженерный трек может продолжаться; полную игровую и аппаратную приёмку не объявлять |

### Порядок и размер итераций

```text
Завершённая native-база: N1/E-005 → N2/E-006–007 → N3/E-008–009
  → N4.1/E-010: общие world queries поверх AABB, без Jolt
  → N4.2/E-011: Jolt static world на изолированном fixture
  → N4.3/E-012: native collision product и build/staging
  → N4.4/E-013: movement/query parity и runtime lifecycle
  → N4.5/E-014: collision authoring/debug tools
  → N4.6/G-012: небольшой игровой участок с native collision
  → профиль этого участка; E-015 impact feedback при нужном consumer

Затем по потребности: N5 world features / N6 compiler/catalog
Исследовательский трек позже: N7 Source 2 prop/участок → N8 полная карта

Игровой трек: G-011 → H-008 → H-009
```

Размеры S/M/L ниже относительные, без обещаний календарных сроков. Этап L делится на несколько законченных slices; одновременно вести одну основную инженерную capability. Compiler развивается вместе с products: N6 завершает общую основу, а не откладывает CLI/детерминизм до конца. Независимые измерения можно выполнять раньше при конкретной проблеме.

| Этап | Относительный объём | Основная зависимость |
| --- | --- | --- |
| N0 | S | Текущее дерево и релевантные существующие reports |
| N1 | S–M | E-003/E-004 |
| N2 | M на один material slice | Native fixture N1 и конкретный нужный материал |
| N3 | M–L | Явные color/data semantics и material path N2 |
| N4 | L, шесть отдельных slices | Завершённая native-база; E-010 перед сменой backend |
| N5 | M–L на выбранную feature | Контент-потребитель; для навигации/акустики — стабильная world geometry |
| N6 | M–L | Несколько типов products и явные реальные dependencies |
| N7 | L | Проверенные native capabilities для выбранного prop/участка |
| N8 | L | Успешный участок N7 и список требований конкретной карты |

### N0 — проверяемая точка продолжения

Зафиксировать текущую версию, source/content/build inputs и релевантные reports. При проверке кандидата использовать существующий подходящий artifact и его manifest; новый ZIP готовить только по прямому запросу на упаковку/выпуск. Коммит не является условием плана. Повторять проверки только при изменениях или сомнениях в соответствии бинарников исходникам.

Проверить расхождения документации с текущими владельцами при следующей содержательной правке: старый editor guide описывает save format 2 и только OBJ/BMP, update guide содержит старое требование save format 3; текущая запись использует format 4 и читает 1–4. Roadmap называет G-011 ближайшим действием, но более поздние записи явно разрешают E-003/E-004 при отложенной человеческой проверке. Сохранять эту развилку явно. Pipeline overlay из старого аудита не назначать задачей до проверки текущих callers.

**Готово:** известны исходный candidate и результаты проверок, исторические evidence отделены от новых, ни одна неподтверждённая приёмка не закрыта.

### N1 — закончить первый native content workflow

**Статус:** `done` — E-005, 2026-10-02; evidence и команды в
[roadmap](PLAYABLE_SLICE_ROADMAP.md#e-005--native-комната-n1).

Создать небольшую проверочную комнату существующим TOML/editor-процессом: несколько статических GLB-моделей, повторные экземпляры общей модели, разные transforms и несколько поддерживаемых материалов. Один из props должен иметь игровую функцию или помогать ориентированию; остальные проверяют повторное использование и масштаб. AABB коллизия на этом этапе остаётся явно authored.

Проверить цепочку Blender export → список `RelayCoreModelAssets.cmake` → build product → staging → editor → preview/F7. Standalone ZIP проверять при отдельном поручении на упаковку/выпуск; исторический результат N1 ниже сохранён. Изменение исходной модели должно доходить до runtime обычной сборкой; повреждённый source не должен заменять рабочий staged product. Изменения importer/editor делать только по препятствию, найденному в этой комнате.

Визуальная цель: функциональное исследовательское помещение конца 1980-х/1990-х, читаемые крупные формы, существующие индустриальные материалы и практическое освещение. Один связанный с функцией occult-акцент допустим. B-17 остаётся референсом; его полный состав не является обязательным scope N1.

**Готово:** несколько моделей и instances работают в редакторе и игре, reload/rollback и доставка проверены, исходные уровни сохраняют поведение. Результат закрывает полезный native workflow; production assets phase C целиком ещё не считается выполненной.

**Результат:** `native_lab.toml`, три стола, два электрических шкафа и три баллона;
два новых Blender/GLB sources, shared products, yaw/nonuniform scale и отдельные
authored AABB. Шкаф управляет рабочим светом и кладовой. Проверены CMake/staging,
source update и failed import rollback, editor open/save/preview, F7/material update,
bad-product rollback/recovery и standalone ZIP. F7 применяется в обычной игре после
возврата из preview; preview сохраняет существующий контракт редактора без reload.
Комната без задания/выхода намеренно показывает соответствующее предупреждение
редактора. G-011/H-008/H-009 остаются открытыми.

### N2 — материалы и текстурные продукты под конкретный контент

**Статус:** `done` для выбранного native material consumer: E-006 tangent/normal map
и E-007 roughness/metalness factors (2026-10-03). `power_cabinet` использует окрашенную
сталь и открытый металл с GGX/Smith/Schlick, product v3 читает старые v1/v2 с прежним
освещением. Evidence в [roadmap](PLAYABLE_SLICE_ROADMAP.md#e-007--roughnessmetalness-n2).
Emissive/AO/alpha и metallic-roughness textures этому шкафу не нужны и остаются
неподдерживаемыми с явным отказом importer. Отдельного TextureAsset пока нет;
normal image встроен в model product. HDR/environment specular относятся к N3.

Выбрать один материал из native-комнаты, которому недостаточно base color. Минимальная последовательность: tangent contract и normal map → roughness/metalness в выбранной модели освещения → emissive/AO или alpha только для реально нужного объекта. Каждый шаг включает importer, versioned product, loader, renderer и fixture; изменение product format требует явной версии и определённой политики старых products.

При появлении отдельного `TextureAsset` добавить KTX2/libktx для нужных mip/format semantics; GPU upload остаётся у renderer. Color/sRGB, normal и linear data должны иметь разные явные семантики. Вынесение текстур и resource references должно устранять конкретное дублирование или обслуживать нужные форматы. Alpha mask/blend, two-sided, UV transforms и UV1 добавлять отдельными шагами при наличии consumer.

Meshoptimizer вводить вместе с конкретной offline-операцией production mesh compiler и проверкой её результата. Не назначать LOD/meshlets/compression обязательными ради самого стека; claims о скорости требуют измерения. Существующий base-color slice и OBJ/BMP сохраняются до переноса зависимого контента.

**Готово:** выбранный материал воспроизводимо импортируется и отображается на native fixture; data textures не декодируются как sRGB, normal/tangent и mirrored transforms проверены, неподдерживаемые glTF semantics дают понятный отказ.

### N3 — production rendering и освещение

**Статус:** `done` для выбранного native slice, 2026-10-03: E-008/N3.1 HDR и
E-009/N3.2 baked diffuse irradiance probes основной комнаты `native_lab` выполнены.
Есть reproducible CPU bake, versioned `.rcprobe`, invalidation/F7 rollback и Vulkan
SH9 sampling. Debug build, unit/content/graphics, before/after и RenderDoc прошли;
ограничения one-bounce/single-volume записаны в E-009. Другие lighting primitives
добавляются по требованиям нового контента.

Ввести linear HDR scene target, затем явные exposure/tone mapping и composition UI. Проверить sRGB decode/encoding, alpha и отсутствие двойной конверсии. Политика `legacy_gamma`/`hardware_srgb` и authored retuning решается явно; принятые сцены не должны менять вид незаметно. Исторические RC-022 reference values не переносить автоматически на все уровни.

После этого выбрать один необходимый комнате способ освещения: baked lightmap, irradiance/light probe или reflection/environment probe. Сделать отдельный native slice с reproducible bake/import и invalidation. Остальные способы добавлять по требованиям контента или выбранного Source 2 участка. Число local lights, culling, instancing и GPU submission менять только по profiler evidence; текущие fence waits не являются автоматической задачей на переделку.

**Готово:** native-сцена демонстрирует выбранные material/light/environment primitives; graphics checks и RenderDoc подтверждают formats/state/transitions; есть до/после кадры и scoped retuning. Текущие карты, тени, UI, reload и presentation проходят релевантные regressions.

### N4 — произвольная статическая коллизия через Jolt

**Порядок:** E-010 → E-011 → E-012 → E-013 → E-014 → G-012. Критерии и единственные фактические статусы — в [roadmap](PLAYABLE_SLICE_ROADMAP.md#e-010--общие-world-queries-поверх-aabb-n41). E-015 — условное продолжение по surface feedback.

1. **N4.1 / E-010 (M):** один конкретный RelayCore-owned query contract поверх действующих AABB. Перенести world blockers combat/LOS/interaction и collision inputs игрока/rover, сохранить math и выбор actor targets. Raycast, sweep/overlap вводить под текущие callers; typed hit хранит нужные fraction/distance, normal, surface и identity. Маски отражают существующие различия запросов. Контракт не требует virtual interface или backend registry.
2. **N4.2 / E-011 (M):** Jolt за этой границей, сначала static boxes/triangle fixture и ray/shape queries. `PhysicsWorld` принадлежит lifetime активного уровня: staged preparation в `LevelSessionBuilder`, передача владельцу игрового мира при activation. `Application` не становится владельцем collision сцены; `JPH::*` не выходят в gameplay/formats. До принятой parity backend остаётся ограничен fixture.
3. **N4.3 / E-012 (M):** независимый versioned `CollisionAsset`, authored или явно выбранная упрощённая geometry. Render triangles не превращать в collision автоматически. Units/axes, transform/winding, surface mapping и bounds фиксируются в product contract. Детерминированный compile, receipts, invalidation, staging и failed-update rollback развивать в действующей цепочке; связку model/collision объявить явно. Общий catalog не является зависимостью.
4. **N4.4 / E-013 (M):** подключить mesh-world к gameplay и сохранить acceleration/friction/air control/jump/step/ground probe/snap при fixed 120 Hz. До переключения зафиксировать ожидаемое поведение на текущих fixtures. Проверить thin walls/ceilings, скольжение, двери и актуализацию blocker transforms до запросов, LOS/выстрелы, interaction, trigger entry, reload/teardown, respawn, snapshot/disk continue и preview. `CharacterVirtual` выбирать только при доказанной необходимости и movement parity. Новые crouch, slopes policy и динамические rigid bodies — отдельные задачи по consumer.
5. **N4.5 / E-014 (S–M):** в существующем editor/inspector показать collision отдельно от render geometry, query hit/normal, surface/layer и authored identity; inspect source/product/receipt и missing/stale ошибки. Базовая диагностика нужна уже на E-011 fixture, этот шаг завершает authoring workflow.
6. **N4.6 / G-012 (M):** один короткий участок с проёмом или неправильной формой, где единый AABB мешает проходу/линии огня, и существующими боем, interaction, objective/save. Сначала технический fixture, затем игровой маршрут и отдельный blind playtest. Результаты G-011 использовать, когда получены; текущая отсрочка не требует новой кампании или повторного создания `pressure_transfer`.

**Готово:** игровой участок использует native mesh-коллизию для реального пространственного решения, основные world consumers согласованы, принятое движение и lifecycle сохранены, collision редактируется/диагностируется существующими tools. Техническая готовность G-012 получает `awaiting_playtest`, если человеческих наблюдений ещё нет; N4 не объявляется полностью принятым только по fixture.

### N5 — world features по наблюдаемой потребности

Первый кандидат — E-015: surface-aware impact sound/эффект на существующих `SurfaceMaterial` из query hit. Выбрать минимум поверхностей/feedback, полезный бою G-012; не назначать decals, particles и новую material систему обязательным комплектом. При отсутствии наблюдаемой потребности задача остаётся deferred.

Выбрать по одной feature с конкретным consumer в игре или acceptance map. Для rover сначала воспроизвести застревание/необходимый обход на требуемой арене: нынешнее движение к `last_seen` само по себе не означает дефект существующих встреч. Если нужна навигация, добавить Recast build и Detour queries за native boundary, затем простое path following с проверкой дверей и восстановления. Оценить актуальность vcpkg port в момент начала; overlay нужен только при подтверждённом недостающем fix.

Decals, particles, ladders/water, animation и richer map logic добавлять отдельными slices. Ozz-animation — кандидат при первом skeletal consumer. Steam Audio — только при нужной геометрической акустике и стабильной collision/world geometry; miniaudio сохраняет playback/mixing. Visibility/streaming/chunking расширять после измерения целевой сцены.

**Готово:** каждая выбранная возможность работает из native data без Source 2 tools, имеет явного владельца и проверки gameplay/save/editor lifecycle там, где она влияет на состояние.

### N6 — общий asset compiler и catalog

Начать с thin declaration/inspection связки model/collision, если E-012 выявит повторяемую ручную работу или неоднозначные references. Затем обобщить работающие CLI/CMake/staging receipts ровно под реально появившиеся dependencies. Устойчивые logical asset IDs и catalog вводить вместе с нужным rename/reference workflow, не как самостоятельную инфраструктуру. Сохранять prepare/validate до замены живой сцены; весь N6 не блокирует G-012.

BLAKE3 оценивать при конкретной потребности в content fingerprints/cache keys; существующие SHA256 receipts достаточны для первых collision products. Ключ включает source, settings и importer/compiler semantics; logical ID не равен content hash. SHA256 receipts E-004 не называют content-addressed cache. Zstd вводить только для выбранных products/bundles после замеров size/I/O; неизменившаяся сборка, rollback и удаления обязаны оставаться проверяемыми.

**Готово:** чистые native sources воспроизводимо дают все нужные runtime products, повторная сборка и dependency invalidation проверены, package не зависит от compiler/DCC/source paths. Background processor, universal IR и сложный pack/streaming framework не нужны без конкретной цены текущего workflow.

### N7 — первый Source 2 offline importer

Условие начала — native collision/gameplay участок принят, необходимые собственные semantics устойчивы и автор отдельно выбирает import/research работу. N7/N8 не являются автоматическим продолжением каждого engine slice.

Выбрать локальный prop и небольшой участок карты, определить список обязательных geometry/material/collision/light semantics. `de_dust2` — возможный кандидат, выбор карты остаётся отдельным решением. Начать с prop: model + materials/textures + transform + collision; затем участок с world geometry, repeated instances и светом. Переиспользовать native compiler/runtime products.

Importer остаётся tool-only. Source 2/CS2 classes и parser types не появляются в публичных runtime contracts. Каждую семантику отметить в import report как `exact`, `approximated` или `unsupported`; неизвестные entities сохранить как metadata/report entries. Не воспроизводить чужие shader/particle/animation runtimes. Нужную native capability сначала проверить на собственном fixture.

**Готово:** prop и участок воспроизводимо конвертируются из локальных источников, units/orientation/material intent/collision проверены, repeated models остаются shared instances; повторный импорт не требует ручной правки сцены.

### N8 — полная карта и hardening

Расширить импорт до выбранной представительной карты только после успеха участка. Проверить обязательные baked/probe/reflection/sky/fog data, traversal, relevant triggers/nav/audio/FX и явно описать оставшиеся approximations. Профилировать load time, RAM/VRAM и CPU/GPU/frame-time до оптимизации. По результатам выбирать batching/culling/streaming, а не заранее.

**Готово:** один документированный workflow из чистой локальной установки CS2 даёт нормальный RelayCore level, player проходит карту через native world queries, повторный импорт воспроизводим, согласованные budgets и compatibility checks выполнены. Удаление Source 2 importer не лишает движок native capabilities. Приёмка импорта карты отдельно от её пригодности как игрового эпизода RelayCore.

### Игровой трек и готовность выпуска

1. **G-011:** первый проход нового игрока без подсказок по [протоколу](playtests/G-011-2026-09-29.md); длительности, смерти, выбор оружия, потеря маршрута, три главных затруднения. Исправлять только наблюдаемые проблемы. Если эпизод короче 5–15 минут, явно принять компактный результат или добавить решения/контент; пустые коридоры не решают задачу.
2. **H-008:** после принятого маршрута снять непрерывный optimized Tracy capture и повторяемые тяжёлые состояния, cold/first-use и повторный проход. Зафиксировать точную сборку/inputs или existing candidate, GPU/driver, resolution, VSync, pipeline, settings; разделить CPU work, GPU и present/waits, first-use uploads, draw counts и world-query cost. Записать p50/p95/p99 и spikes, согласовать целевой FPS и слабый ПК. Например, 60 FPS соответствует 16,67 мс бюджета, но не является уже выбранной целью проекта. Entrance baseline не закрывает полный маршрут; профиль G-012 нужен отдельно при изменении геометрии/workload.
3. **H-009:** по прямому поручению на candidate/выпуск подготовить точный Release candidate действующими scripts или использовать подходящий существующий, проверить hashes и внешний запуск, полный проход, save/continue и обновление предыдущей standalone версии. На другом согласованном GPU/ПК проверить реальный device/presentation contract, HUD/input/audio; Vulkan loader probe и собственный RTX не заменяют этот тест. Версия/notes относятся к конкретному candidate; публикация — по отдельной команде автора.

G-001 остаётся отдельным незавершённым измерением прежнего эпизода и может выполняться независимо. G-010 возвращается только при наблюдаемом недостатке существующих угроз/геометрии. Пока blind playtest и другой GPU отложены, инженерные итерации допускаются; G-011/H-008/H-009 и полную приёмку не закрывать техническими ZIP или авторским подтверждением startup.

### Историческая точка продолжения после N0/N4

Запись ниже предшествует продолжению E-015/E-017/G-013/G-014. Указания «сейчас нет» и «E-015 deferred» описывают тот момент и не задают текущую очередь; следующий implementation ticket указан в roadmap.

**N0 / интеграционная проверка, 2026-10-07:** прежние import blockers сняты без новых source/content правок. Переиспользованы актуальные optimized products; staging и целевые targets собраны, SHA-256 20 моделей и model/collision pairing проверены. Content/native/collision **58/58**, asset-tooling **2/2**, SDL/Vulkan lifecycle и G-012 **2/2** без пропусков; Check Map valid=true, warnings=0, новые кадры просмотрены. Graphics — `validation=false`, нового профиля нет; Debug bake не завершался. Команды, reports и границы evidence — в [карточке G-012](PLAYABLE_SLICE_ROADMAP.md#g-012--короткий-игровой-участок-с-native-collision-n46). Безусловных implementation tickets с выполненными зависимостями сейчас нет; следующий критерий G-012 — новый игрок, E-015 остаётся условным.

**G-012 / N4.6 — `awaiting_playtest`.** E-010–E-014 завершены; технический consumer «Реле мастерской» выполнен 2026-10-07. Rover проходит под native-столом, общий AABB блокирует тот же путь; два оружия, turret, реле/выход, objective/save и editor/lifecycle проверены. Debug: content 36/36, editor/irradiance/physics 47/47, graphics 2/2; optimized G-012 3/3. [First-use/warm профиль](profiling/G-012-2026-10-07.md) фиксирует CPU/GPU/query/draw/upload/present/wait costs трёх состояний на RTX 4060 Ti; непрерывный rendered маршрут человека не измерен. Следующая приёмка — [новый игрок без подсказок](playtests/G-012-2026-10-07.md), время, смерти, выбор оружия и читаемость проёма. E-015 остаётся deferred до конкретной feedback-потребности; N4 не объявлен принятым по автотесту. G-011 передаётся новому игроку, когда автор возобновит человеческую проверку; G-011/H-008/H-009 сохраняют статусы. Архивы создавать только по прямой просьбе.

E-014 выполнен 2026-10-07: мастерская назначает native product/AABB с pairing validation и undo/save/reload; overlay и ray/box queries показывают geometry, hit/normal, surface/layer и authored identity. Inspector поддерживает native mesh и pin restoration; product inspection проверяет существующие receipts и staged collision/model. Debug: 106/106 targeted checks, включая SDL/Vulkan lifecycle и выбор collision щелчком; opening/support/selected captures просмотрены. Lighting inputs и rcprobe bytes сохранены, повторный bake не запускался. Evidence и ограничения — в [карточке E-014](PLAYABLE_SLICE_ROADMAP.md#e-014--collision-authoringdebug-и-asset-inspection-n45).

E-012/E-013 выполнены 2026-10-07: `.rccollision` v1 с compiler/receipts/staging и явным `prop_static.collision_asset`; native geometry подключена к movement, shots, LOS и focus. Box trajectories сохранены точно на 120 Hz; mesh использует sweeps. Debug: 120 unit, 31 physics и 33 content checks прошли; финальный native/lifecycle прогон 4/4, ASan physics 31/31. Snapshot/continue, editor preview, transition, reload rollback/recovery, death/respawn и teardown проверены на shared fixture. Обычные карты остаются AABB без явного opt-in. Evidence и ограничения — в [карточке E-013](PLAYABLE_SLICE_ROADMAP.md#e-013--mesh-world-movementquery-parity-и-lifecycle-n44), contract — в [COLLISION_ASSETS.md](COLLISION_ASSETS.md). Человеческий проход и performance этим не закрываются.

### Проверка каждой итерации

Выбирать существующие presets/targets из текущих CMake files. Для model/format/build — importer/native/tooling tests; для комнаты — `relaycore_level_check`, content и editor/lifecycle/graphics; для renderer — native fixture, core/synchronization validation и RenderDoc; для physics/gameplay — ближайшие math/collision/combat/save tests и реальный проход. GPU-assisted validation, ASan и полный Release цикл расширять по затронутым границам и готовности кандидата. Performance claims подтверждать одинаковым workload в optimized build.

После implementation просмотреть scoped diff, выполнить `git diff --check`, обновить реальный ticket и evidence в основном roadmap. В proposal не копировать конкурирующую таблицу фактических статусов. Код, assets, dependency baseline, VERSION и release notes этим планированием не менялись.

По поручению автора от 2026-10-07 после каждого завершённого и проверенного checkpoint делать отдельный коммит в `dev`: конкретный subject с ID задачи и описанием изменения, body с существенными решениями, проверками и ограничениями. Stage explicit paths; push не поручен.

### Проверка пересмотра 2026-10-04

Сверены `physics/collision.h`, `physics/player_motion.h`, player collision inputs/spatial index, combat/interaction/enemy world blockers и ближайшие tests; `LevelSession`, static-prop authoring/material parsing, manifest и immediate scene submit/wait. Сопоставлены записи N1–N3 и architecture/dependency policy. Это инспекция исходников и прежнего evidence, а не новый build/run или performance measurement. Изменены только этот план и roadmap: 42 локальные Markdown-ссылки/якоря и наличие единственных строк/карточек E-009–E-015/G-012 проверены; `git diff --check` прошёл. Внешний анализ не копируется в репозиторий как ещё один конкурирующий план.

### Исторические проверки исходного плана 2026-10-02

Существующие JUnit E-004 прочитаны: Debug unit/tooling/audio 261/261, content 31/31, graphics Debug/optimized по 4/4, focused ASan 11/11; failures/skip отсутствуют. Это evidence предыдущего выполнения, а не заново запущенные полные suites.

В этой задаче на существующей Debug-сборке выполнены inventory и узкий повтор:

```powershell
ctest --preset test-debug -N -R '^relaycore\.(native|GLB|Blender|tooling.model_assets)'
ctest --preset test-debug -R '^relaycore\.(native|GLB|Blender|tooling.model_assets)' --output-on-failure --no-tests=error
```

Выбранные проверки прошли 8/8: native product/validation/cache/editor, GLB transforms/rejection/recompile и build/staging receipts. Новая сборка, полный graphics/ASan/Release прогон, runtime playthrough, Tracy capture и другой GPU в этой задаче не выполнялись. Изменены этот документ и ссылка на него в roadmap. Итоговый `git diff --check` прошёл; существующие предупреждения LF/CRLF не являются whitespace errors.

## Предыдущий план игрового цикла M0–M6

## Анализ текущего состояния

1. **Рабочая основа уже существует.** Есть принятый автором эпизод `power_restoration`, движение и взаимодействия, турели, melee rover, hazards, миссии, смерть/возрождение, save/continue и переходы карт. Полигон G-007 и сценарии G-008 приняты. Следующий цикл должен использовать эту основу в новом маршруте.
2. **G-009 реализована и принята автором.** В `player_weapon.h` есть точное оружие и дробовик. Тесты покрывают дистанцию, блокирование лучей, события уничтожения, подбор, переключение и восстановление. В renderer подключены корпус и помпа OBJ, отдача и отдельные следы картечи. Авторская приёмка записана 29 сентября; добавлять дробовик заново не требуется.
3. **Следующий полезный результат — самостоятельный эпизод с выбором оружия.** Пока не подтверждено, что оба оружия нужны в реальном маршруте и игрок понимает их роль без объяснения чисел. Полигон позволяет проверить это до дорогого авторинга.
4. **Новый противник — условная задача.** Турель ограничивает позиции, rover создаёт ближнее давление. Мобильный стрелок полезен только при наблюдаемом недостатке этих угроз. Дополнительное поведение увеличит объём save/restore, validation, editor, audio и graphics-проверок.
5. **Геометрия встречи должна учитывать текущее движение AI.** В `enemy_runtime.cpp` rover движется прямо к `last_seen` через `move_and_collide`; поиска пути вокруг препятствий нет. Это ограничение для проектирования маршрута, а не доказанный дефект всех существующих встреч. Сначала воспроизвести проблему на нужной арене, затем выбрать изменение геометрии или минимальную доработку поведения.
6. **Ограничитель сейчас — проверка игрового результата.** Длительность нового эпизода, ориентирование нового игрока, баланс двух оружий и производительность полного нового прохода ещё не измерены. Инфраструктура сборки, проверка карт, полигон и выпуск уже существуют.
7. **Документацию нужно читать с учётом времени записи.** В roadmap сохранён анализ одного оружия от 28 сентября и промежуточная запись об отсутствии модели. Последующая запись и текущий renderer подтверждают подключение модели. Такие исторические записи не превращать в новые implementation tickets.

Приоритет: завершить G-009 → решить судьбу G-010 → выполнить G-011 → H-008 → H-009. Открытое измерение прежнего эпизода G-001 можно провести независимо.

**Точка остановки автономного выполнения 2026-09-29:** M1 принята автором; M2 завершена решением `deferred` для G-010; M3 `done`. M4 `awaiting_playtest`: нужен первый проход нового игрока без подсказок с длительностями, смертями, выбором оружия и тремя главными затруднениями. Новая карта и Debug-сборка готовы, технические проверки прошли. Исторический blocker M1 снят; M5/M6 не начаты, поскольку поручение требует остановки на следующем человеческом blocker.

## M0 — Проверяемая исходная база

Статус: `done` для ограниченной исходной проверки; результаты приведены в конце документа.

**Что делать мне:** сверить текущие предпосылки с кодом, сборкой и ближайшими тестами. Сохранить пользовательские изменения. При найденном падении отделить проблему среды от дефекта runtime и зафиксировать минимальное воспроизведение; исправление выполнять в рамках отдельно порученной implementation-задачи.

- [x] analysis: сверить roadmap, два оружия, viewmodel, save и движение rover;
- [x] build: собрать текущие unit/content targets и инструмент проверки карты;
- [x] tests: проверить оружие, сохранения и сценарии полигона;
- [x] review: записать точные результаты и ограничения проверки.

Done when:

- известен результат выбранных проверок текущего дерева;
- исторические результаты отделены от выполненных сегодня;
- нет неподтверждённого заявления о полной приёмке проекта.

## M1 — G-009: принять два оружия и дробовик

Статус: `done`. Зависимость: M0; полигон G-007/G-008 уже доступен. Авторская приёмка 2026-09-29: модель/feedback, звук, отдача, помпа и переключение приняты; оба оружия имеют различные роли, блокирующих замечаний нет.

**Что делать мне:** подготовить воспроизводимый запуск полигона и короткий протокол сравнения. Проверить актуальную модель, следы выстрела, помпу, звук и переключение. По конкретным замечаниям автора изменить только мешающие параметры или поведение; после правки повторить ближайшую проверку и обновить evidence G-009.

- [x] validation: сравнить оба оружия на дистанциях 3/7/12 м, против rover и в смешанной встрече;
- [x] playtest: получить оценку автора вида, звука, отдачи и причины выбора каждого оружия;
- [x] implementation: по результатам авторской приёмки доработка не требуется;
- [x] build/tests: проверить combat/enemy, weapon input, save/snapshot, sandbox и затронутую graphics-проверку;
- [x] review: проверить diff и обновить статус G-009; техническая и авторская приёмка выполнены.

Основные пути: `enemy_runtime.*`, `game_world_runtime*`, `player_controller*`, `world_scene_renderer*`, `save_game.*`, `assets/levels/development_sandbox.toml`; ближайшие `enemy_runtime_tests`, `save_game_tests`, `development_sandbox_tests`, `power_restoration_graphics`.

Done when:

- автор принял отображение и ощущение дробовика;
- есть понятная ситуация с преимуществом каждого оружия, одно не доминирует во всех сценариях;
- переключение не обходит cooldown; стены и двери блокируют выстрел; события уничтожения не дублируются;
- F9/F10, смерть, меню и disk continue сохраняют согласованное состояние;
- нужные тесты проходят, замечания и фактическая приёмка записаны.

Для человеческой проверки использовать новый запуск или F7, затем подбор E и выбор 2. Обновлённая карта меняет fingerprint; существующие сохранения не удалять.

## M2 — G-010: решить, нужен ли новый противник

Статус: `done` — G-010 `deferred`. Зависимость: принят M1.

Решение 2026-09-29: автор подтвердил полезность дробовика вблизи rover и разные роли обоих оружий в смешанном бою. Недостающее решение игрока, требующее мобильного стрелка, не обнаружено. M3 использует существующие угрозы и геометрию. Условие возврата: наблюдение нового эпизода показывает, что фиксированный огонь турели, сближение rover и укрытия не заставляют менять позицию/оружие; сначала проверить геометрию, затем одну новую атаку на полигоне.

**Что делать мне:** разобрать наблюдения M1 и сформулировать конкретное недостающее решение игрока. Если его дают существующие угрозы и геометрия, отметить G-010 как `deferred` с условием возврата и перейти к M3. Если недостаток подтверждён, реализовать один вид противника и проверить его сначала на полигоне.

- [x] analysis: по наблюдениям M1 нехватка конкретного поведения не подтверждена;
- [x] decision: G-010 `deferred`, причина и условие возврата записаны;
- [x] implementation, если нужна: не требуется при `deferred`;
- [x] integration, если нужна: не требуется при `deferred`;
- [x] build/tests/playtest, если нужна: новый runtime не добавлен, проверки M1 сохранены;
- [x] review: новый враг отложен, M3 разрешена с существующими угрозами.

Предварительный кандидат — мобильный стрелок, заставляющий менять укрытие. Его конкретная атака выбирается под доказанную роль. Параметрические варианты описывать typed data; отдельное поведение отделять при реальной разнице логики. Общий интерфейс вводить только при потребности текущих callers.

Done when:

- решение по G-010 принято и записано;
- при реализации враг меняет действия игрока относительно турели и rover;
- предупреждение читается, укрытия/двери работают, pause/restore/death не создают лишних атак;
- при отложении M3 может быть выполнен с существующими угрозами.

## M3 — G-011, часть 1: проходимый блок-аут нового эпизода

Статус: `done` для технического блок-аута `pressure_transfer.toml`. Зависимость: M1 принят, решение M2 записано. Независимая игровая приёмка принадлежит M4 и остаётся открытой.

**Что делать мне:** зафиксировать краткое задание эпизода, имя карты, старт, изменение состояния мира и выход. Собрать одну карту существующим editor/TOML-процессом. Сделать маршрут: вход → безопасное знакомство с дробовиком → отдельная угроза → сочетание угроз → устройство, меняющее путь → выход. Сначала проверить пространство и решения игрока.

- [x] design: equipment bay → rover hall → длинная turret gallery → смешанный relay room → перепуск → выход; схема и назначения записаны в протоколе;
- [x] implementation: одна новая карта, семь шагов миссии, получение дробовика, перепуск/разблокировка выхода, финал и выбор сценария в меню;
- [x] validation: CLI без warnings, content tests, collision-проход и focus/occlusion проверки обязательных взаимодействий;
- [x] runtime: три полных scripted прохода через collision/combat/interaction/mission owners; rover у открытой двери и угла, реальные DemoGame continue/SDL E/menu save/finale;
- [x] review: scoped diff и кадры просмотрены; прямые rover lanes свободны, подтверждённые ошибки тестовой постановки исправлены. Независимая оценка понятности маршрута остаётся в M4.

Основные пути: новая карта в `assets/levels`, релевантные content tests и существующие mission/interaction/session APIs. `power_restoration.toml` сохранить как контрольный эпизод. Новую runtime capability добавлять только для конкретного неработающего сценария.

Done when:

- маршрут проходится от старта до финала;
- три встречи требуют различимых решений: дистанция/момент атаки, укрытие/перемещение, сочетание угроз;
- получение дробовика и изменение состояния мира встроены в маршрут;
- обязательные взаимодействия достижимы, AI не делает нужную встречу нерабочей;
- карта проходит проверку, независимая игровая приёмка ещё явно открыта.

## M4 — G-011, часть 2: самостоятельный игровой эпизод

Статус: `awaiting_playtest`. Зависимость: проходимый M3. [Сборка, сценарий наблюдения и таблица результатов](playtests/G-011-2026-09-29.md) готовы; результатов нового игрока нет.

**Что делать мне:** подготовить первый проход нового игрока, собрать наблюдения и исправить главные затруднения. После принятия маршрута довести функциональные ориентиры, свет, звук и материалы. Проверить сохранения в критических состояниях и обновить протокол G-011.

- [ ] playtest: хотя бы один новый игрок проходит без подсказок; записаны время участков, смерти, выбор оружия и три главных затруднения;
- [ ] implementation: исправить подтверждённые проблемы маршрута, встречи и feedback;
- [ ] presentation: согласовать свет/звук/материалы с ретро-индустриальным направлением;
- [x] tests: смерть/respawn, disk continue и encounter restore проверены до/после подбора, устройства и команды открытия выхода;
- [x] build/graphics: Debug unit/audio 250/250, content 31/31, graphics/lifecycle 2/2; сцены и ключевые состояния настоящего runtime просмотрены;
- [ ] review: игровая приёмка ещё не выполнена; технический diff просмотрен, ограничения и blocker записаны.

Done when:

- новый игрок доходит до финала без объяснения обязательных действий;
- оба оружия применяются по понятным причинам;
- фактическая длительность измерена; ориентир 5–15 минут проверен наблюдением;
- более короткий результат осознанно принят либо доработан содержанием и решениями игрока;
- save/continue и retry не ломают прогрессию; нужные проверки проходят.

Человеческий первый проход выполняет человек. Я готовлю сборку, сценарий наблюдения, протокол и исправления; собственное знание карты и автотест не заменяют этот критерий.

## M5 — H-008: производительность полного нового маршрута

Статус: `pending`. Зависимость: проходимый G-011. Раннее исследование возможно при воспроизводимой задержке.

**Что делать мне:** определить целевой FPS и условия измерения, снять непрерывный Tracy capture полного прохода и повторяемые тяжёлые состояния. Разделить simulation/render CPU, GPU при доступном измерении и present. Исправлять только подтверждённый вклад в проблему кадра; сравнивать до/после на одинаковом workload.

- [ ] profiling: основной ПК, сборка RelWithDebInfo, hardware/разрешение/VSync/settings записаны;
- [ ] analysis: frame-time median/p95/p99, пики, тяжёлая встреча и освещение;
- [ ] validation: тот же маршрут на согласованном слабом ПК;
- [ ] implementation, если нужна: минимальная оптимизация измеренного узкого места;
- [ ] tests/reprofile: проверить поведение и сравнить одинаковые условия;
- [ ] review: сохранить baseline и вывод по бюджету кадра.

Done when:

- измерения воспроизводимы и относятся к новому эпизоду;
- задан бюджет кадра относительно целевого FPS и виден запас либо конкретная проблема;
- тяжёлые состояния проверены на основном и слабом ПК;
- заявленная оптимизация подтверждается измерениями; при отсутствии проблемы она не требуется.

## M6 — H-009: проверенный кандидат следующего эпизода

Статус: `pending`. Зависимость: G-011 принят, H-008 выполнен, серьёзные регрессии устранены.

**Что делать мне:** собрать кандидата существующим процессом, проверить точный архив вне проекта, подготовить release notes и актуальный статус. Проверить save/continue и обновление с предыдущей standalone-версии. Завершить подготовку кандидата до отдельной команды на публикацию.

- [ ] build: нужные Release/RelWithDebInfo targets и ASan для изменённого runtime;
- [ ] tests: unit/content, затронутые graphics/lifecycle, release/update checks;
- [ ] packaging: существующие scripts, manifest/хеши, полный состав assets/DLL;
- [ ] validation: распакованный архив вне checkout, полный проход и continue;
- [ ] playtest: слабый ПК — окно/HUD, управление, звук и новый бой;
- [ ] documentation/review: README, roadmap, версия/release notes по существующему процессу, приёмка конкретного кандидата.

Done when:

- конкретный архив воспроизводимо запускается и проходит нужные проверки;
- состояние сохранений и обновление проверены для заявленной совместимости;
- автор принял кандидата, известные ограничения записаны;
- документы соответствуют фактической сборке; публикация выполняется только по отдельной команде.

## После M6 — отдельные решения по следующему циклу

- **OBJ-контент:** записанная ранее замена геометрии остаётся отдельным будущим этапом. Начать с одного показательного участка: назначение, масштаб/оси/UV, материалы, импорт и проверка в игре. Затем оценить стоимость расширения. Для стен и пола учитывать авторинг, collision, свет/тени и читаемость. Массовую замену не смешивать с G-011.
- **Ammo/reload:** вернуться, если наблюдения показывают отсутствие значимых решений по ресурсу или темпу боя. Сначала испытать один ограниченный вариант на полигоне; пока это гипотеза.
- **Маршрутизация AI:** вернуться при воспроизводимом блокере принятой встречи, который нельзя разумно устранить геометрией. Требуемое поведение и ближайшие tests определить до выбора алгоритма.
- **Editor/runtime:** дорабатывать под записанное неудобство авторинга или подтверждённый дефект. ECS, общий weapon framework, navmesh и новая система рендеринга не имеют самостоятельного приоритета в этом цикле.

## Порядок работы для Codex

1. Взять порученный этап; прочитать `AGENTS.MD`, актуальную карточку roadmap и ближайший протокол. Не повторять закрытую реализацию.
2. Проверить рабочее дерево, named paths, ближайшие tests и прямые call sites. Существующие пользовательские правки сохранить.
3. Назвать ожидаемый результат и минимальный scope; выбрать только релевантные repo-local skills по их descriptions. Обычные C++ правки и code review не требуют отдельного skill.
4. Выполнить небольшую законченную итерацию, собрать затронутые targets и проверить поведение. Условные пункты закрывать записанным решением, а не фиктивной реализацией.
5. После правок выполнить `git diff --check`, просмотреть diff, обновить статусы и evidence в основном roadmap. Не коммитить и не публиковать без команды.
6. Если остаётся человеческая проверка, записать `awaiting_playtest` и конкретный сценарий. Не объявлять весь этап `done` по одним автотестам.

Команды выбирать по текущим `CMakePresets.json` и CMake targets. MSVC-сборка требует VS developer environment и корректный `VCPKG_ROOT`. Начинать с ближайших тестов; полный Debug/ASan/Release цикл нужен для кандидата либо затронутых общих contracts.

## Проверки при составлении этого плана

Выполнено 2026-09-29 в VS developer environment с `VCPKG_ROOT`, соответствующим существующему cache:

```powershell
cmake --build --preset tests-debug --target relaycore_tests relaycore_content_validation relaycore_level_check --parallel 1
ctest --preset test-debug -R '^relaycore\.(shotgun|selected weapon|saved game|invalid save|malformed|save compatibility|campaign save)' --output-on-failure
ctest --preset validation-debug -R '^validation\.sandbox' --output-on-failure
./build/msvc-debug/src/relaygame/relaycore_level_check.exe assets/levels/development_sandbox.toml
git diff --check
```

Результат: сборка успешна; выбранные unit tests 11/11 и sandbox tests 7/7 прошли; CLI вернул `valid: true`, warnings: 0. `git diff --check` прошёл. Сборка в sandbox не завершалась; эти попытки остановлены, успешная сборка выполнена вне sandbox. Это не установленный дефект исходников.

Создан только этот документ; runtime, assets и существующие пользовательские документы не редактировались. Исходники и ближайшие tests просмотрены. Полный набор unit/content/graphics, ASan/Release, игровой проход, приёмка дробовика человеком, другой ПК и Tracy capture в этой задаче не выполнялись. Результаты Debug/ASan 250/250 и content 26/26 из протокола G-009 остаются историческими, здесь заново не подтверждались.

## Автономное выполнение — 2026-09-29

Полностью прочитаны `AGENTS.MD` и этот план. Сверены актуальная карточка G-009 и протокол, текущие weapon/combat/input/render/save owners, ближайшие tests, CMake targets и конфигурация запуска Sandbox. Существующие пользовательские изменения сохранены. Повторной реализации дробовика и неподтверждённой настройки баланса не требуется: технические проверки не выявили дефекта, но это не вывод о субъективном игровом результате.

MSVC environment импортирован существующей функцией `Import-ReleaseBuildEnvironment` из `scripts/release-functions.ps1`; `VCPKG_ROOT=C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg` соответствует текущему Debug cache. Первый build внутри sandbox остановлен после зависания без вывода; идентичный build вне sandbox завершился успешно. Проблема среды устранена для этой проверки, исходники и build cache не менялись вручную.

Выполнены:

```powershell
cmake --build --preset demo-debug --target relaycore_tests relaycore_content_validation relaycore_power_restoration_graphics relaycore_game_lifecycle relaycore_audio_lifetime relaycore_sandbox relaycore_level_check --parallel 1
ctest --preset test-debug -R '^relaycore\.(shotgun|rover|nonlethal|selected weapon|saved game|invalid save|malformed|save compatibility|campaign save|version|gameplay input|player controller|player fire|turret|combat|audio_lifetime)' --output-on-failure
ctest --preset validation-debug -R '^validation\.sandbox' --output-on-failure
ctest --preset graphics-debug -R '^graphics\.(power_restoration|game_lifecycle)$' --output-on-failure
./build/msvc-debug/src/relaygame/relaycore_level_check.exe assets/levels/development_sandbox.toml
git diff --check
```

Результат: build успешен; выбранные unit/audio 30/30, sandbox 7/7, graphics/lifecycle 2/2 прошли без skipped checks; CLI exit 0, `valid: true`, warnings: 0. Просмотрены свежие `shotgun-ready.bmp`, `shotgun-fired.bmp`, `shotgun-pump.bmp` в `build/msvc-debug/tests/power-captures`: модель и HUD видны, следы различимы, цевьё меняет положение. Настоящий SDL/backend проверяет проигрывание WAV; субъективная слуховая оценка не выполнена. Тест дистанции сравнивает 3 и 12 м; полный человеческий сценарий 3/7/12 м, rover и смешанная встреча остаётся открытым. Полные Debug suites, ASan/Release и performance capture в этом запуске не выполнялись.

**Неразрешимый из доступных данных blocker:** для завершения M1 необходим отзыв автора после игры с текущим дробовиком: внешний вид, звук, отдача и причина выбора каждого оружия на 3/7/12 м, против rover и в смешанной встрече. Проверены план, roadmap и ближайший протокол: последняя запись оставляет эту оценку открытой. Есть готовый Debug Sandbox и [протокол запуска/сравнения](playtests/G-009-2026-09-28.md); автоматические проверки состояния и визуальные captures выполнены. Нет человеческого отзыва, который позволил бы принять M1 или определить подтверждённую доработку. Самостоятельно создать такой отзыв невозможно.

Продолжение: запустить **RelayCore — Sandbox**, новый запуск или F7, подбор E → 2; выполнить таблицу испытаний протокола и записать отзыв автора. Затем устранить подтверждённые замечания, принять M1 и сразу перейти к M2. M2 требует наблюдений M1; M3 требует принятого M1 и решения M2; последующие этапы зависят от нового эпизода. Эти зависимости не пропущены и не объявлены выполненными. Текущий запуск остановлен по условию настоящего blocker из поручения, а не после завершения одного этапа.

## Продолжение после авторской приёмки M1 — 2026-09-29

Автор явно принял внешний вид/feedback, звук, отдачу, помпу, переключение и роль дробовика. Наблюдения: 3 м — два выстрела на убийство; 7 м — пять, роль отличается от точного оружия; 12 м — точное оружие явно предпочтительнее, записано пять выстрелов. Тип мишени/прицеливание и оружие последнего счётчика не уточнялись, поэтому damage/health не менялись. Против rover дробовик полезен вблизи; в смешанном бою различимы роли обоих оружий. Блокирующих замечаний нет. M1 `done`, историческая остановка выше снята.

M2 `done`: G-010 `deferred`, причина и условие возврата записаны в карточке. M3 `done`: создана одна карта `assets/levels/pressure_transfer.toml` (43 solids, 32 entities), семь mission steps, две турели и два rover. Подбор обязателен для служебной двери; перепуск запускает fan/audio/light и разблокирует пульт выхода. Существующий menu selector получил третий сценарий; заголовок и captions нового финала исправлены после просмотра. Старые сценарии, формат save и `power_restoration.toml` сохранены. Общая система оружия, новый AI, authored schema и новые зависимости не добавлены.

Новые пять content tests проверяют три полных collision/combat прохода с четырьмя уничтожениями, обоими оружиями и без смерти; focus/LOS всех обязательных взаимодействий; gate prerequisites; движение rover у двери/угла; укрытие от турели при сближении rover; смерть/respawn, disk continue и encounter restore в пяти состояниях; editor save/load с неизменным fingerprint. Scripted walker знает координаты и намеренно выбирает оружие по расстоянию: это техническая проходимость, а не наблюдение самостоятельного выбора человека. Graphics test настоящего DemoGame проверяет продолжение новой карты, SDL E на перепуске, menu save, выход и шестисекундный финал. Просмотрены свежие equipment/rover/gallery/mixed/control-before/control-after/finale/complete кадры.

В процессе исправлены вызов doctest `CAPTURE`, слишком малая вертикальная скорость тестового walker относительно collision epsilon, позиция doorway test за пределом detection range и недостаточное время ожидания финала. Это ошибки новых проверок, а не основание менять physics/AI. Параллельный link/discovery столкнулся с Windows sharing violation `0xc0000043`; повторная serial-сборка успешна. Начальная регенерация CMake восстановила существующие зависимости из cache; manifest/baseline/версии не менялись в этом запуске.

Финальные команды в прежнем VS developer environment:

```powershell
cmake --build --preset demo-debug --target relaycore_content_validation relaycore_power_restoration_graphics relaycore_demo relaycore_tests relaycore_audio_lifetime relaycore_game_lifecycle relaycore_level_check --parallel 1
ctest --preset test-debug --output-on-failure
ctest --preset validation-debug --output-on-failure
ctest --preset graphics-debug -R '^graphics\.(power_restoration|game_lifecycle)$' --output-on-failure
./build/msvc-debug/src/relaygame/relaycore_level_check.exe assets/levels/pressure_transfer.toml
clang-format --dry-run --Werror tests/pressure_transfer_tests.cpp tests/power_restoration_graphics.cpp src/relaygame/demo_game_menu.cpp src/relaygame/demo_game_progress.cpp
git diff --check
```

Результат: Debug build успешен; unit/audio 250/250, content 31/31, graphics/lifecycle 2/2 прошли без пропусков; CLI exit 0, `valid: true`, warnings: 0; formatting и diff-check успешны. Release/ASan, слабый ПК, Tracy и packaging в этом продолжении не выполнялись. Нужные проверки кандидата остаются в M5/M6.

**Следующий настоящий blocker — M4:** нужен хотя бы один новый игрок, проходящий `Перепуск давления` без объяснения обязательных действий. Нет section timings, смертей, причин выбора оружия и трёх главных затруднений; отсутствует измерение длительности относительно 5–15 минут и решение автора по более короткому результату. Готовы Debug demo, выбор сценария и [протокол наблюдения](playtests/G-011-2026-09-29.md). Техническое состояние проверено; собственное знание карты и автотест не заменяют прямо установленный критерий M4. Исправления по наблюдениям и окончательное согласование presentation зависят от этого результата. Автономный запуск остановлен на этом blocker, M4 не объявлена `done`; M5/M6 не пропущены и не объявлены выполненными.
