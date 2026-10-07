# Project plan — RelayCore

Дата исходного анализа: 2026-09-29. Текущее состояние и порядок актуализированы 2026-10-07; исторические проверки ниже сохранены отдельно.

Это краткий план следующего цикла и инструкция для Codex. Основная очередь и статусы задач остаются в [PLAYABLE_SLICE_ROADMAP.md](PLAYABLE_SLICE_ROADMAP.md); номера M0–M6 ниже обозначают этапы этого документа. ID G/H сохранены из roadmap. План не запускает реализацию автоматически.

## План развития после E-004 — 2026-10-02

Этот раздел — актуальная последовательность развития по текущему рабочему дереву, актуализированная 2026-10-07. Предыдущий план M0–M6 и журнал ниже сохранены: они описывают игровой цикл и его проверки. Основная очередь и фактические статусы остаются в roadmap; N1 выполнен как E-005, выбранные N2 и N3 slices завершены как E-006–E-009; по последующему поручению выполнены E-010–E-013/N4.1–N4.4. Следующая инженерная задача — E-014/N4.5; остальные этапы требуют своих зависимостей и поручения автора. Наличие плана само по себе не запускает реализацию.

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
| Геометрия/движение | AABB и opt-in native collision products; прежний box movement, mesh sweeps; rover движется к `last_seen` без поиска пути | Render-модель требует отдельной authored collision; игровой consumer G-012 ещё впереди |
| Освещение | Dynamic lights/shadows, fog/post; E-008 linear FP16 HDR/exposure/UI; E-009 baked diffuse SH9 probes | Native lab имеет offline one-bounce product и invalidation; lightmaps, reflection probes и масштабирование света требуют отдельного consumer |
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

### Ближайшая рекомендуемая итерация

**E-014 / N4.5 — collision authoring/debug и asset inspection.** E-010–E-013 завершены: общий query contract, collision products и opt-in native gameplay/lifecycle path готовы. Следующий шаг — overlay/query hit/normal/surface/authored identity и product inspection в существующих tools. Затем игровой consumer G-012 использует неправильную статическую форму/проём; surface feedback добавляется по потребности. G-011 передаётся новому игроку, когда автор возобновит человеческую проверку; её статус сохраняется. Архивы создавать только по прямой просьбе.

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
3. Назвать ожидаемый результат и минимальный scope; применить подходящие `relaycore-implement/assets/profile/verify` skills.
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
