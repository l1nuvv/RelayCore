# Документация RelayCore

Начните с нужной строки; читать весь комплект для узкой задачи не требуется. Source/presets/tests/runtime — первичное evidence, документы объясняют контракты и сохраняют решения. Версия продукта находится в [VERSION](../VERSION).

## Выбор документа по задаче

| Задача | Основной источник | Читать дополнительно при необходимости |
| --- | --- | --- |
| Запустить игру / узнать управление | [README](../README.md), [инструкция игроку](PLAYTEST_GUIDE.txt) | [Editor/campaign](EDITOR_AND_CAMPAIGN.md) для переходов и сохранений |
| Собрать, проверить или диагностировать | [DEVELOPMENT](DEVELOPMENT.md) | [Build timing](BUILD_TIME_ANALYSIS.md), [verify skill](../.agents/skills/relaycore-verify/SKILL.md) |
| Оптимизация всего проекта, FPS и frame pacing | [H-008: полный поэтапный план](tickets/H-008.md#h-008-remaining) | [Измерения и эксперименты](profiling/H-008-2026-10-14.md), [profile skill](../.agents/skills/relaycore-profile/SKILL.md) |
| Gameplay/content ticket | [Roadmap: статусы и ссылки на карточки](PLAYABLE_SLICE_ROADMAP.md) | [Стратегия](PROJECT_PLAN.md), соответствующий playtest/report |
| VK14 milestone | [Renderer plan: очередь и gates](RENDERER_MODERNIZATION_PLAN.md) | [Execution reports](renderer/), нужные runtime owners и graphics checks |
| Текущие runtime owners / порядок кадра / paths | [ARCHITECTURE](ARCHITECTURE.md) | [DEVELOPMENT](DEVELOPMENT.md) для запуска и диагностики |
| Новая engine capability / Source 2 boundary | [ENGINE_ARCHITECTURE_REFERENCE](ENGINE_ARCHITECTURE_REFERENCE.md) | [DEPENDENCY_STACK](DEPENDENCY_STACK.md) при выборе библиотеки |
| Добавить или изменить зависимость | [DEPENDENCY_STACK](DEPENDENCY_STACK.md) | Точные pins: [manifest](../vcpkg.json) и repository overlays |
| GLB/import/native product/staging | [MODEL_ASSETS](MODEL_ASSETS.md) | [Collision](COLLISION_ASSETS.md), asset declarations и receipts |
| Material, lighting, HDR, probes | [RENDERING](RENDERING.md) | [ART_DIRECTION](ART_DIRECTION.md), выбранный renderer milestone |
| RML/RCSS, player UI | [DEVELOPMENT: UI](DEVELOPMENT.md#игровой-ui-rmlui-63) | [Runtime ownership](ARCHITECTURE.md#player-ui-ownership-rmlui-63), [UI ticket](tickets/VIS-43-46.md) |
| Карты, редактор, кампания, save/transition | [EDITOR_AND_CAMPAIGN](EDITOR_AND_CAMPAIGN.md) | [Полигон](DEVELOPMENT.md#испытательный-полигон), [collision contract](COLLISION_ASSETS.md) |
| Художественное решение / внешний ресурс | [ART_DIRECTION](ART_DIRECTION.md), [ASSET_SOURCES](ASSET_SOURCES.md) | [Design references](DESIGN_REFERENCES.md), source records конкретного asset |
| Подготовить/опубликовать выпуск или updater | [UPDATES](UPDATES.md) | Протокол выбранного выпуска в [playtests](playtests/) |

## Владельцы сведений

- **Статус задачи:** gameplay/content — roadmap; VK14 — renderer-план. В обзоре достаточно ссылки, без второй таблицы статусов.
- **Текущий контракт:** owning runtime/asset/dependency/art документ. Индексы и handoff не переопределяют контракт.
- **Проверка:** карточка или датированный отчёт с build/source/assets identity, командами, результатами и ограничениями. Старый зелёный запуск не проверяет новое дерево; последующий успешный запуск не стирает первоначальный failure.
- **Workflow агента:** [AGENTS](../AGENTS.MD). Skills добавляют способы выполнения, а не вторую очередь или копию общих правил.

## Детали и история

| Каталог / документ | Что хранится |
| --- | --- |
| [tickets](tickets/) | Требования, implementation details и датированные результаты отдельного G/E/H/UI ticket |
| [renderer](renderer/) | Команды, captures, сравнения, profiling и ограничения выполненного VK14 milestone |
| [history](history/) | Прежние стратегии D/N/M, исходный slice scope, baseline и журнал |
| [art](art/), [art-references](art-references/) | Before/after, технические отчёты и авторские visual references |
| [playtests](playtests/), [profiling](profiling/), [audits](audits/) | Человеческие протоколы, измерения и датированные обзоры |
| [VULKAN_MIGRATION_PLAN](VULKAN_MIGRATION_PLAN.md) | Завершённые gates OpenGL → Vulkan; прежние GL-команды исторические |
| [VISUAL_REVAMP_AUDIT](VISUAL_REVAMP_AUDIT.md) | Визуальные observations и исходный scope; текущие статусы берутся из roadmap |
| [Assessment 2026-10-11](ASSESSMENT_SCOPE_2026-10-11.md), [handoff 2026-10-11](NEXT_STEPS_2026-10-11.md) | Границы исходного статического обзора и ссылки на продолжение |
| [Graphics baselines](../tests/graphics-baselines/README.md), [RC-022 baseline](color_pipeline_baseline.md) | Authority отдельных samples и численные цветовые проверки |

Пути в командах и code blocks считаются от корня репозитория, если явно не указана другая working directory.

Source/export records рядом с ресурсами: [модели](../assets/models/README.md), [текстуры](../assets/textures/README.md), [графика](../content/graphics/README.md), [звук](../assets/audio/player/README.md), [third-party sources](../content/third_party/). Их provenance и adaptation evidence сохраняются независимо от task status.

`Doxyfile` генерирует API-справочник из README/`src`; `docs/doxygen` — generated output, а не ещё один источник контрактов. Repo-local skills находятся в `.agents/skills`; [Blender sources](../.agents/skills/BLENDER_SOURCES.md) сохраняют их upstream pins.
