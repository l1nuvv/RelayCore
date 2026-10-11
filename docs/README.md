# Документация RelayCore

Документация актуализирована 2026-10-11: продукт 1.4.1, авторская приёмка G-019 и отдельный план renderer modernization. Source/presets проверялись выборочно; новая runtime-верификация не выполнялась. Версия сборки задаётся в корневом `VERSION`; результаты прошлых запусков не подтверждают сегодняшнее рабочее дерево.

## Текущая работа и запуск

| Документ | Назначение |
| --- | --- |
| [README](../README.md) | Требования, быстрый старт, проверки и управление |
| [PLAYABLE_SLICE_ROADMAP](PLAYABLE_SLICE_ROADMAP.md) | Очередь gameplay/content tickets, статусы и evidence; G-019 принята автором |
| [RENDERER_MODERNIZATION_PLAN](RENDERER_MODERNIZATION_PLAN.md) | VK14-00–04 evidence, обязательный Vulkan 1.4 с 2026-10-12; hardware/human gates и дальнейшие этапы |
| [ASSESSMENT_SCOPE](ASSESSMENT_SCOPE_2026-10-11.md) | Границы текущей статической сверки и различие HEAD/рабочего дерева |
| [PROJECT_PLAN](PROJECT_PLAN.md) | Стратегия и решения автора; старые milestones сохраняются как история |
| [DEVELOPMENT](DEVELOPMENT.md) | Окружение, CLion, UI, полигон, проверка и диагностика |
| [PLAYTEST_GUIDE](PLAYTEST_GUIDE.txt) | Инструкция участнику без решения маршрута |
| [Протокол G-019](playtests/G-019-2026-10-09.md) | Наблюдение самостоятельного прохождения главы |
| [UPDATES](UPDATES.md) | Подготовка/публикация Release, канал и updater |
| [Выпуск 1.4.1](playtests/release-1.4.1.md) | Опубликованный релиз, Release checks, ZIP/SHA256 и ограничения |
| [EDITOR_AND_CAMPAIGN](EDITOR_AND_CAMPAIGN.md) | Редактор, карта, переходы и сохранения |

## Архитектура и контент

| Документ | Назначение |
| --- | --- |
| [ARCHITECTURE](ARCHITECTURE.md) | Текущие runtime owners, frame flow, paths и failure policies |
| [ENGINE_ARCHITECTURE_REFERENCE](ENGINE_ARCHITECTURE_REFERENCE.md) | Долгосрочные возможности и ограничения; исходный snapshot отдельно от реализации |
| [DEPENDENCY_STACK](DEPENDENCY_STACK.md) | Ответственность зависимостей и условия интеграции; точные pins в vcpkg manifest/overlays |
| [COLLISION_ASSETS](COLLISION_ASSETS.md) | Отдельные native collision products, pairing и opt-in |
| [ART_DIRECTION](ART_DIRECTION.md) | Авторитетное художественное направление |
| [ASSET_SOURCES](ASSET_SOURCES.md) | Поиск и адаптация внешних ресурсов |
| [DESIGN_REFERENCES](DESIGN_REFERENCES.md) | Принципы игровых и архитектурных references |
| [Модели](../assets/models/README.md), [текстуры](../assets/textures/README.md), [графика](../content/graphics/README.md), [звук игрока](../assets/audio/player/README.md) | Источники и воспроизводимый авторинг ресурсов |

## История и проверочные материалы

[VULKAN_MIGRATION_PLAN](VULKAN_MIGRATION_PLAN.md) хранит gates завершённого перехода, включая прежние OpenGL-команды; текущий запуск описан в DEVELOPMENT. [VISUAL_REVAMP_AUDIT](VISUAL_REVAMP_AUDIT.md) — визуальные observations и scope, а не вторая implementation-очередь. [BUILD_TIME_ANALYSIS](BUILD_TIME_ANALYSIS.md) — измерения конкретной машины и вариантов сборки.

RC-022: [численный baseline](color_pipeline_baseline.md), [visual reference](color_pipeline_visual_reference.md), [authored values](rc022f_authored_values_audit.md), [overlay contract](rc022g_pipeline_authoring_overlay.md). Их исходные GL-сценарии исторические; свежий Vulkan-аудит помечен отдельно в baseline.

`art/` хранит before/after и технические отчёты, `art-references/` — авторские references, `profiling/` — датированные измерения, `playtests/` — протоколы человеческой и технической приёмки, `audits/` — датированные обзоры. Эти файлы сохраняют доказательства и ограничения. Завершённый RELEASE_AUTOMATION_PLAN удалён: действующий workflow находится в UPDATES, результаты U-001–U-005 — в [протоколе 1.3.2](playtests/release-1.3.2.md).

При чтении старого отчёта `current`/«текущий», SHA256, число моделей, версия product и результат теста относятся к его датированному срезу. Для нового запуска используйте текущие presets и staged assets из DEVELOPMENT, фиксируйте новую идентичность файлов. Итоговый ticket status берётся из roadmap; последующие исправления не превращают прежний упавший запуск в успешный.

| Проверочные материалы | Что искать |
| --- | --- |
| [IMG-01–03](art/img01-03-materials-reflections-ao.md), [IMG-04–06](art/img04-06-lighting-stability-emission.md) | MR/AO, bounded reflections, emissive, свет и ограниченный temporal proxy |
| [VIS-34](art/vis34-service-pistol.md), [VIS-35](art/vis35-shotgun.md), [VIS-36–38](art/vis36-38-pickups-security-scientist.md), [VIS-39](art/vis39-player-body.md) | Оружие, руки, pickups, противники/учёный и тело игрока |
| [VIS-48–49](art/vis48-49-cost-and-review.md), [graphics baselines](../tests/graphics-baselines/README.md) | Измеренная стоимость, причины расхождений и authority отдельных samples |
| [G-012 profile](profiling/G-012-2026-10-07.md), [G-014 profile](profiling/G-014-2026-10-07.md), [E-018](profiling/E-018-2026-10-07.md), [H-008](profiling/H-008-2026-10-08.md) | Локальные before/after workloads; непрерывный sound-on проход этим не закрывается |
| [G-011](playtests/G-011-2026-09-29.md), [G-012](playtests/G-012-2026-10-07.md), [G-017](playtests/G-017-2026-10-08.md) | Протоколы отдельных эпизодов и последующие отзывы |
| [Авторские references](art-references/2026-10-07/README.md), [agent audit](audits/agent-infrastructure-2026-10-07.md) | Происхождение ориентиров и датированная оценка инфраструктуры |

Происхождение внешних ресурсов: [Poly Haven](../content/third_party/polyhaven/README.md), [pressure transfer](../content/third_party/polyhaven/pressure_transfer/README.md), [containment](../content/third_party/polyhaven/containment/README.md), [Guardian](../content/third_party/game_ready_mutant/source.md), [MakeHuman](../content/third_party/makehuman/README.md). Эти записи сохраняют авторов, исходные файлы и выполненные адаптации.

## Инструкции агентам и API-справочник

[AGENTS](../AGENTS.MD) задаёт правила проекта. Repo-local skills и их references находятся в `.agents/skills`; [происхождение Blender skills](../.agents/skills/BLENDER_SOURCES.md) сохраняет upstream pins. [RmlUi skill](../.agents/skills/relaycore-rmlui/SKILL.md) направляет к реализованному player UI, [verify](../.agents/skills/relaycore-verify/SKILL.md) — к выбранным проверкам. Skills дополняют owning docs, а не дублируют очередь tickets.

`Doxyfile` описывает генерацию API-справочника по README и `src`; вывод `docs/doxygen` генерируется локально и не является отдельным источником runtime contracts.
