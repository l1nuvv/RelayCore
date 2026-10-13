# RelayCore — gameplay/content roadmap

Этот файл — единственный реестр текущих статусов G/E/H/IMG/VIS. Требования, implementation details и датированные проверки находятся по ссылке каждой строки. Карточка сохраняет историю исполнения; прежний статус в её записи не переопределяет таблицу ниже. VK14 milestones принадлежат [renderer-плану](RENDERER_MODERNIZATION_PLAN.md), стратегия — [PROJECT_PLAN](PROJECT_PLAN.md).

## Точка продолжения

2026-10-14 автор сообщил о завершении VK14 без RT и успешном плейтесте, но
отдельно указал на неудовлетворительную производительность. Следующая выбранная
работа — [H-008: полный аудит производительности](tickets/H-008.md); новый контент
отложен до результатов аудита и устранения главных подтверждённых проблем.
Сообщение о плейтесте записано как авторская приёмка без выдуманного протокола,
длительности, hardware/settings или независимого первого прохождения. Оно не
закрывает автоматически все отдельные G/IMG/VIS gates; их таблицы сохраняют
границы уже записанного evidence. VK14 scope и аппаратные ограничения принадлежат
[renderer-плану](RENDERER_MODERNIZATION_PLAN.md).

G-019 принята автором 2026-10-10. Последующее наполнение окружения технически завершено; его художественная приёмка остаётся открытой. Независимые 20–30 минут первого прохождения, непрерывный sound-on маршрут, отдельные AI/UI/material gates и другой GPU не подтверждены общей авторской приёмкой. Детали — в [G-019](tickets/G-019.md) и [протоколе](playtests/G-019-2026-10-09.md).

Следующую задачу выбирает поручение пользователя с учётом зависимостей карточки. Таблица не является приказом выполнить весь backlog. Новое наблюдение требуется для повторного исправления уже закрытой технической проблемы.

## Статусы и приоритеты

| Статус | Значение |
| --- | --- |
| `ready` | Зависимости выполнены; можно брать по поручению |
| `pending` | Запланировано; ещё нужны предпосылки |
| `in_progress` | Работа начата, критерии завершения ещё не выполнены |
| `awaiting_playtest` | Техническая часть готова; требуется человеческая приёмка |
| `blocked` | Указан конкретный блокер |
| `done` | Выполнены критерии задачи в записанных границах приёмки |
| `deferred` | Отложено с условием возврата |

P0 — crash/UB/broken build/corrupt state; P1 — приёмка slice; P2 — gameplay/iteration; P3 — полезное engine improvement; P4 — research. S/M/L — относительный объём, а не срок.

## Gameplay, engine и engineering health

Порядок строк сохраняет происхождение tickets; порядок реализации определяется их зависимостями. Все статусы перенесены из прежней таблицы без изменения.

| ID | Задача | Приоритет | Статус | Карточка / evidence |
| --- | --- | --- | --- | --- |
| H-001 | Актуальный test baseline | P1 | `done` | [Детали](tickets/H-001.md) |
| G-001 | Наблюдаемый полный проход | P1 | `awaiting_playtest` | [Детали](tickets/G-001.md) |
| G-002 | Начало и ориентирование | P1 | `done` | [Детали](tickets/G-002.md) |
| E-001 | Полный локальный повтор встречи | P2 | `done` | [Детали](tickets/E-001.md) |
| G-003 | Обратная связь оружия и угроз | P1 | `done` | [Детали](tickets/G-003.md) |
| G-004 | Три разные встречи | P1 | `done` | [Детали](tickets/G-004.md) |
| G-005 | Смерть, повтор и продолжение | P1 | `done` | [Детали](tickets/G-005.md) |
| G-006 | Согласованная презентация | P2 | `done` | [Детали](tickets/G-006.md) |
| H-002 | Standalone-кандидат и performance baseline | P1 | `done` | [Детали](tickets/H-002.md) |
| H-003 | Документация принятого состояния | P2 | `done` | [Детали](tickets/H-003.md) |
| H-004 | Сетевые обновления standalone | P1 | `done` | [Детали](tickets/H-004.md) |
| H-005 | Автоматизация выпуска и Release-сборка | P1 | `done` | [Детали](tickets/H-005.md) |
| H-006 | Проверки staged-содержимого и полный отчёт форматирования | P2 | `done` | [Детали](history/playable-slice-2026-09--10.md#13-журнал-выполнения-и-точка-продолжения) |
| H-007 | Проверка конфигурации выпуска после сохранения CLion | P2 | `done` | [Детали](history/playable-slice-2026-09--10.md#13-журнал-выполнения-и-точка-продолжения) |
| E-002 | Проверка карты без запуска игры | P1 | `done` | [Детали](tickets/E-002.md) |
| G-007 | Большой полигон для развития механик | P1 | `done` | [Детали](tickets/E-002.md) |
| G-008 | Три повторяемых боевых сценария на полигоне | P2 | `done` | [Детали](tickets/G-008.md) |
| G-009 | Второе оружие с отдельной боевой ролью | P2 | `done` | [Детали](tickets/G-009.md) |
| G-010 | Один новый противник по результатам испытаний | P2 | `awaiting_playtest` | [Детали](tickets/G-010.md) |
| G-011 | Следующий короткий игровой эпизод | P2 | `awaiting_playtest` | [Детали](tickets/G-011.md) |
| H-008 | Полный аудит производительности и очередь оптимизаций | P1 | `in_progress` | [Детали](tickets/H-008.md) |
| H-009 | Проверяемый кандидат следующего эпизода | P2 | `pending` | [Детали](tickets/H-009.md) |
| VK-014 | Ревью архитектуры после Vulkan cutover | P2 | `done` | [Детали](VULKAN_MIGRATION_PLAN.md) |
| VK-F01 | Диагностические строки Vulkan mesh backend | P3 | `done` | [Детали](VULKAN_MIGRATION_PLAN.md) |
| E-003 | Статический GLB → native MeshAsset/ModelAsset | P2 | `done` | [Детали](tickets/E-003.md) |
| E-004 | Сборка и доставка native моделей из GLB | P2 | `done` | [Детали](tickets/E-004.md) |
| E-005 / N1 | Проверочная native-комната и полный content workflow | P2 | `done` | [Детали](tickets/E-005.md) |
| E-006 / N2.1 | Tangent/normal map для окрашенной стали native-шкафа | P2 | `done` | [Детали](tickets/E-006.md) |
| E-007 / N2.2 | Roughness/metalness факторов native-шкафа | P2 | `done` | [Детали](tickets/E-007.md) |
| E-008 / N3.1 | FP16 HDR, exposure, tone mapping и UI composition | P2 | `done` | [Детали](tickets/E-008.md) |
| E-009 / N3.2 | Baked diffuse SH9 irradiance probes | P2 | `done` | [Детали](tickets/E-009.md) |
| E-010 / N4.1 | Общие world queries поверх AABB | P2 | `done` | [Детали](tickets/E-010.md) |
| E-011 / N4.2 | Jolt static world на изолированном fixture | P2 | `done` | [Детали](tickets/E-011.md) |
| E-012 / N4.3 | Native collision product и build/staging | P2 | `done` | [Детали](tickets/E-012.md) |
| E-013 / N4.4 | Mesh-world movement/query parity и lifecycle | P2 | `done` | [Детали](tickets/E-013.md) |
| E-014 / N4.5 | Collision authoring/debug и asset inspection | P2 | `done` | [Детали](tickets/E-014.md) |
| G-012 / N4.6 | Короткий игровой участок с native collision | P2 | `awaiting_playtest` | [Детали](tickets/G-012.md) |
| E-015 / N5 | Surface-aware impact feedback по потребности | P2 | `done` | [Детали](tickets/E-015.md) |
| G-013 | Два тактических решения native-встречи | P2 | `awaiting_playtest` | [Детали](tickets/G-013.md) |
| E-016 | Обход препятствий rover под встречи главы | P2 | `done` | [Детали](tickets/E-016.md) |
| G-014 | Native-встреча в существующем эпизоде | P2 | `awaiting_playtest` | [Детали](tickets/G-014.md) |
| E-017 | Переиспользование актуальных irradiance products | P2 | `done` | [Детали](tickets/E-017.md) |
| G-015 | Локальный retry смешанной встречи | P2 | `awaiting_playtest` | [Детали](tickets/G-015.md) |
| E-018 | Подготовка GPU-ресурсов до первого игрового кадра | P2 | `done` | [Детали](tickets/E-018.md) |
| G-016 | Доводка существующих сценариев по слепому тесту | P1 | `awaiting_playtest` | [Детали](tickets/G-016.md) |
| G-017 / D4 | Контур охлаждения: следующий эпизод и production loop | P2 | `awaiting_playtest` | [Детали](tickets/G-017.md) |
| G-018 | Сквозное прохождение семи готовых игровых карт | P2 | `done` | [Детали](tickets/G-018.md) |
| G-019 | Полноценная глава: связный маршрут, угроза и задача | P1 | `done` | [Детали](tickets/G-019.md) |

## Изображение, окружение и UI

Статусы отдельных slices не изменены общей приёмкой G-019. Результаты и ограничения каждой строки — в [visual results](art/visual-slice-results.md); для VIS-43–46 есть [общая карточка UI](tickets/VIS-43-46.md).

| ID | Задача | Статус | Evidence |
| --- | --- | --- | --- |
| IMG-01 | Metallic/roughness maps | `awaiting_playtest` | [Отчёт](art/img01-03-materials-reflections-ao.md) |
| IMG-02 | Static reflection probes | `awaiting_playtest` | [Отчёт](art/img01-03-materials-reflections-ao.md) |
| IMG-03 | Material AO | `awaiting_playtest` | [Отчёт](art/img01-03-materials-reflections-ao.md) |
| IMG-04 | Свет containment hall | `awaiting_playtest` | [Отчёт](art/img04-06-lighting-stability-emission.md) |
| IMG-05 | Стабильность mapped-normal highlights | `awaiting_playtest` | [Отчёт](art/img04-06-lighting-stability-emission.md) |
| IMG-06 | Native emissive maps | `awaiting_playtest` | [Отчёт](art/img04-06-lighting-stability-emission.md) |
| VIS-03 | Выход pressure_transfer | `done` | [Отчёт](art/pressure-transfer-visual-slice.md) |
| VIS-04 | Стыки и обратные виды маршрута | `awaiting_playtest` | [Отчёт](art/pressure-transfer-visual-slice.md) |
| VIS-05 | Вход power_restoration | `done` | [Отчёт](art/power-restoration-visual-slice.md) |
| VIS-06 | Служебные крылья power | `done` | [Отчёт](art/power-restoration-visual-slice.md) |
| VIS-07 | Насосная power | `done` | [Отчёт](art/power-restoration-visual-slice.md) |
| VIS-08 | Bench relay | `done` | [Отчёт](art/bench-cooling-visual-slice.md) |
| VIS-09 | Cooling loop | `done` | [Отчёт](art/bench-cooling-visual-slice.md) |
| VIS-10 | Cooling signage | `done` | [Отчёт](art/bench-cooling-visual-slice.md#vis-10--cooling-signage-2026-10-08) |
| VIS-11 | Foundation intake | `awaiting_playtest` | [Отчёт](art/foundation-intake-visual-slice.md) |
| VIS-12 | Foundation pump | `done` | [Отчёт](art/foundation-pump-visual-slice.md) |
| VIS-13 | Occult lab B-17 | `done` | [Отчёт](art/b17-visual-slice.md) |
| VIS-14 | Отделка B-17 и входного коридора | `done` | [Отчёт](art/vis14-16-facility-finish.md) |
| VIS-15 | Соединения труб и вентиляции | `done` | [Отчёт](art/vis14-16-facility-finish.md) |
| VIS-16 | Детали шкафов и насосных корпусов | `done` | [Отчёт](art/vis14-16-facility-finish.md) |
| VIS-17 | Двери | `awaiting_playtest` | [Отчёт](art/vis17-19-interactive-hardware.md) |
| VIS-18 | Интерактивное оборудование | `awaiting_playtest` | [Отчёт](art/vis17-19-interactive-hardware.md) |
| VIS-19 | Service key | `done` | [Отчёт](art/vis17-19-interactive-hardware.md) |
| VIS-20 | Материалы архитектуры | `done` | [Отчёт](art/vis20-22-material-pass.md) |
| VIS-21 | Масштаб UV и повтор текстур | `done` | [Отчёт](art/vis20-22-material-pass.md) |
| VIS-22 | Normal maps и material factors | `done` | [Отчёт](art/vis20-22-material-pass.md) |
| VIS-23 | Причинный износ | `awaiting_playtest` | [Отчёт](art/vis23-causal-wear.md) |
| VIS-24 | Особые материалы | `awaiting_playtest` | [Отчёт](art/vis24-special-materials.md) |
| VIS-25 | Sector signage | `done` | [Отчёт](art/vis25-sector-signage.md) |
| VIS-26 | Route signage | `done` | [Отчёт](art/vis26-route-signage.md) |
| VIS-27 | Экраны и схемы | `done` | [Отчёт](art/vis27-28-instruments-language.md) |
| VIS-28 | Язык обозначений | `done` | [Отчёт](art/vis27-28-instruments-language.md) |
| VIS-29 | Свет помещений | `awaiting_playtest` | [Отчёт](art/vis29-room-lighting.md) |
| VIS-30 | Корпуса и состояния светильников | `awaiting_playtest` | [Отчёт](art/vis30-light-fixtures.md) |
| VIS-31 | Контакт и тени | `awaiting_playtest` | [Отчёт](art/vis31-33-contact-atmosphere-focus.md) |
| VIS-32 | Атмосфера маршрута | `awaiting_playtest` | [Отчёт](art/vis31-33-contact-atmosphere-focus.md) |
| VIS-33 | Визуальный фокус | `awaiting_playtest` | [Отчёт](art/vis31-33-contact-atmosphere-focus.md) |
| VIS-34 | Служебный пистолет | `awaiting_playtest` | [Отчёт](art/vis34-service-pistol.md) |
| VIS-35 | Дробовик | `awaiting_playtest` | [Отчёт](art/vis35-shotgun.md) |
| VIS-36 | Pickups | `awaiting_playtest` | [Отчёт](art/vis36-38-pickups-security-scientist.md) |
| VIS-37 | Противники | `awaiting_playtest` | [Отчёт](art/vis36-38-pickups-security-scientist.md) |
| VIS-38 | Учёный | `awaiting_playtest` | [Отчёт](art/vis36-38-pickups-security-scientist.md) |
| VIS-39 | Тело игрока | `awaiting_playtest` | [Отчёт](art/vis39-player-body.md) |
| VIS-40 | Боевые эффекты | `awaiting_playtest` | [Отчёт](art/vis40-41-combat-environment-effects.md) |
| VIS-41 | Эффекты окружения | `awaiting_playtest` | [Отчёт](art/vis40-41-combat-environment-effects.md) |
| VIS-42 | Состояния оборудования | `awaiting_playtest` | [Отчёт](art/vis42-equipment-states.md) |
| VIS-43 | Player HUD / RmlUi integration | `awaiting_playtest` | [Отчёт](tickets/VIS-43-46.md) |
| VIS-44 | Меню и настройки | `awaiting_playtest` | [Отчёт](tickets/VIS-43-46.md) |
| VIS-45 | Игровые экраны | `awaiting_playtest` | [Отчёт](tickets/VIS-43-46.md) |
| VIS-46 | Разделение игры и developer UI | `awaiting_playtest` | [Отчёт](tickets/VIS-43-46.md) |
| VIS-47 | Asset chain | `done` | [Отчёт](VISUAL_REVAMP_AUDIT.md#vis-47--vis-50--проверка-pipeline-и-документации-2026-10-09) |
| VIS-48 | Стоимость kit | `done` | [Отчёт](art/vis48-49-cost-and-review.md#vis-48-one-current-measurement) |
| VIS-49 | Graphics regression / art review | `awaiting_playtest` | [Отчёт](art/vis48-49-cost-and-review.md) |
| VIS-50 | Pipeline documentation | `done` | [Отчёт](VISUAL_REVAMP_AUDIT.md#vis-47--vis-50--проверка-pipeline-и-документации-2026-10-09) |

## Проверки и обновление evidence

Workflow и автоматические логические коммиты задаёт [AGENTS](../AGENTS.MD#git-and-handoff); выбор build/tests — [DEVELOPMENT](DEVELOPMENT.md#узкие-проверки) и [verify skill](../.agents/skills/relaycore-verify/SKILL.md). После работы обновите статус здесь, а в карточке запишите дату, изменённые пути, точные команды, результат и оставшиеся gates. Не копируйте полный лог в обзорные документы.

Человеческий протокол фиксирует идентичность сборки, знакомство игрока с картой, время/смерти/затруднения, обычное движение и sprint, звук, Continue/retry и решение каждого открытого критерия. Scripted routes и технические кадры не доказывают pacing или понятность. Performance требует сопоставимого measured before/after.

## История

[Исходный scope, baseline и журнал](history/playable-slice-2026-09--10.md), [предыдущие стратегии D/N/M](history/project-planning-2026-09-29--10-10.md), [завершённая Vulkan migration](VULKAN_MIGRATION_PLAN.md). Исторические упаковки и полные проверки не являются обязательным workflow для новой узкой задачи.

<details>
<summary>Ссылки на прежние разделы</summary>

<a id="vis-43--переход-на-игровой-hud-и-ui"></a>[VIS-43 — переход на игровой HUD и UI](tickets/VIS-43-46.md#vis-43--переход-на-игровой-hud-и-ui)

<a id="rmlui-migration--inventory-2026-10-09"></a>[RmlUi migration — inventory, 2026-10-09](tickets/VIS-43-46.md#rmlui-migration--inventory-2026-10-09)

<a id="implementation-and-evidence--2026-10-09"></a>[Implementation and evidence — 2026-10-09](tickets/VIS-43-46.md#implementation-and-evidence--2026-10-09)

<a id="focus-cancellation-follow-up--2026-10-09"></a>[Focus cancellation follow-up — 2026-10-09](tickets/VIS-43-46.md#focus-cancellation-follow-up--2026-10-09)

<a id="ui-art-direction-pass--2026-10-09"></a>[UI art-direction pass — 2026-10-09](tickets/VIS-43-46.md#ui-art-direction-pass--2026-10-09)

<a id="2-исходное-состояние-и-доказательства"></a>[2. Исходное состояние и доказательства](history/playable-slice-2026-09--10.md#2-исходное-состояние-и-доказательства)

<a id="подтверждённая-основа"></a>[Подтверждённая основа](history/playable-slice-2026-09--10.md#подтверждённая-основа)

<a id="выполненная-проверка-2026-09-27"></a>[Выполненная проверка 2026-09-27](history/playable-slice-2026-09--10.md#выполненная-проверка-2026-09-27)

<a id="4-игровое-направление-и-границы-v1"></a>[4. Игровое направление и границы v1](history/playable-slice-2026-09--10.md#4-игровое-направление-и-границы-v1)

<a id="relaycore-vertical-slice-v1"></a>[RelayCore Vertical Slice v1](history/playable-slice-2026-09--10.md#relaycore-vertical-slice-v1)

<a id="5-milestones-и-зависимости"></a>[5. Milestones и зависимости](history/playable-slice-2026-09--10.md#5-milestones-и-зависимости)

<a id="h-001--восстановить-проверки-текущего-эпизода"></a>[H-001 — восстановить проверки текущего эпизода](tickets/H-001.md#h-001--восстановить-проверки-текущего-эпизода)

<a id="g-001--измерить-самостоятельное-прохождение"></a>[G-001 — измерить самостоятельное прохождение](tickets/G-001.md#g-001--измерить-самостоятельное-прохождение)

<a id="g-002--сделать-начало-и-маршрут-понятными"></a>[G-002 — сделать начало и маршрут понятными](tickets/G-002.md#g-002--сделать-начало-и-маршрут-понятными)

<a id="e-001--полный-локальный-повтор-встречи"></a>[E-001 — полный локальный повтор встречи](tickets/E-001.md#e-001--полный-локальный-повтор-встречи)

<a id="g-003--закончить-feedback-оружия-и-угроз"></a>[G-003 — закончить feedback оружия и угроз](tickets/G-003.md#g-003--закончить-feedback-оружия-и-угроз)

<a id="g-004--три-встречи-с-разными-решениями"></a>[G-004 — три встречи с разными решениями](tickets/G-004.md#g-004--три-встречи-с-разными-решениями)

<a id="g-005--принять-правило-смерти-и-продолжения"></a>[G-005 — принять правило смерти и продолжения](tickets/G-005.md#g-005--принять-правило-смерти-и-продолжения)

<a id="g-006--согласованный-визуальный-и-звуковой-эпизод"></a>[G-006 — согласованный визуальный и звуковой эпизод](tickets/G-006.md#g-006--согласованный-визуальный-и-звуковой-эпизод)

<a id="h-002--проверить-standalone-кандидат"></a>[H-002 — проверить standalone-кандидат](tickets/H-002.md#h-002--проверить-standalone-кандидат)

<a id="h-004--обновления-standalone-сборок-по-сети"></a>[H-004 — обновления standalone-сборок по сети](tickets/H-004.md#h-004--обновления-standalone-сборок-по-сети)

<a id="h-005--автоматизация-выпуска-и-release-сборка"></a>[H-005 — автоматизация выпуска и Release-сборка](tickets/H-005.md#h-005--автоматизация-выпуска-и-release-сборка)

<a id="h-003--закрепить-фактический-статус"></a>[H-003 — закрепить фактический статус](tickets/H-003.md#h-003--закрепить-фактический-статус)

<a id="e-002--g-007--испытательный-полигон"></a>[E-002 / G-007 — испытательный полигон](tickets/E-002.md#e-002--g-007--испытательный-полигон)

<a id="g-008--три-повторяемых-боевых-сценария"></a>[G-008 — три повторяемых боевых сценария](tickets/G-008.md#g-008--три-повторяемых-боевых-сценария)

<a id="g-009--второе-оружие-с-отдельной-ролью"></a>[G-009 — второе оружие с отдельной ролью](tickets/G-009.md#g-009--второе-оружие-с-отдельной-ролью)

<a id="g-010--один-новый-противник-если-текущих-угроз-недостаточно"></a>[G-010 — один новый противник, если текущих угроз недостаточно](tickets/G-010.md#g-010--один-новый-противник-если-текущих-угроз-недостаточно)

<a id="g-011--следующий-короткий-эпизод-из-проверенных-механик"></a>[G-011 — следующий короткий эпизод из проверенных механик](tickets/G-011.md#g-011--следующий-короткий-эпизод-из-проверенных-механик)

<a id="e-003--первый-static-meshassetmodelasset-slice"></a>[E-003 — первый static MeshAsset/ModelAsset slice](tickets/E-003.md#e-003--первый-static-meshassetmodelasset-slice)

<a id="e-004--сборка-и-доставка-native-моделей-из-glb"></a>[E-004 — сборка и доставка native моделей из GLB](tickets/E-004.md#e-004--сборка-и-доставка-native-моделей-из-glb)

<a id="e-005--native-комната-n1"></a>[E-005 — native-комната N1](tickets/E-005.md#e-005--native-комната-n1)

<a id="e-006--tangentnormal-map-первый-шаг-n2"></a>[E-006 — tangent/normal map, первый шаг N2](tickets/E-006.md#e-006--tangentnormal-map-первый-шаг-n2)

<a id="e-007--roughnessmetalness-n2"></a>[E-007 — roughness/metalness, N2](tickets/E-007.md#e-007--roughnessmetalness-n2)

<a id="e-008--linear-hdr-exposure-и-ui-первый-шаг-n3"></a>[E-008 — linear HDR, exposure и UI, первый шаг N3](tickets/E-008.md#e-008--linear-hdr-exposure-и-ui-первый-шаг-n3)

<a id="e-009--baked-diffuse-irradiance-probes-n32"></a>[E-009 — baked diffuse irradiance probes, N3.2](tickets/E-009.md#e-009--baked-diffuse-irradiance-probes-n32)

<a id="e-010--общие-world-queries-поверх-aabb-n41"></a>[E-010 — общие world queries поверх AABB, N4.1](tickets/E-010.md#e-010--общие-world-queries-поверх-aabb-n41)

<a id="e-011--jolt-static-world-на-изолированном-fixture-n42"></a>[E-011 — Jolt static world на изолированном fixture, N4.2](tickets/E-011.md#e-011--jolt-static-world-на-изолированном-fixture-n42)

<a id="e-012--native-collision-product-и-buildstaging-n43"></a>[E-012 — native collision product и build/staging, N4.3](tickets/E-012.md#e-012--native-collision-product-и-buildstaging-n43)

<a id="e-013--mesh-world-movementquery-parity-и-lifecycle-n44"></a>[E-013 — mesh-world movement/query parity и lifecycle, N4.4](tickets/E-013.md#e-013--mesh-world-movementquery-parity-и-lifecycle-n44)

<a id="e-014--collision-authoringdebug-и-asset-inspection-n45"></a>[E-014 — collision authoring/debug и asset inspection, N4.5](tickets/E-014.md#e-014--collision-authoringdebug-и-asset-inspection-n45)

<a id="g-012--короткий-игровой-участок-с-native-collision-n46"></a>[G-012 — короткий игровой участок с native collision, N4.6](tickets/G-012.md#g-012--короткий-игровой-участок-с-native-collision-n46)

<a id="e-015--surface-aware-impact-feedback-по-потребности-n5"></a>[E-015 — surface-aware impact feedback по потребности, N5](tickets/E-015.md#e-015--surface-aware-impact-feedback-по-потребности-n5)

<a id="e-017--переиспользование-актуальных-irradiance-products"></a>[E-017 — переиспользование актуальных irradiance products](tickets/E-017.md#e-017--переиспользование-актуальных-irradiance-products)

<a id="g-013--два-тактических-решения-native-встречи"></a>[G-013 — два тактических решения native-встречи](tickets/G-013.md#g-013--два-тактических-решения-native-встречи)

<a id="e-016--обход-препятствий-rover-под-встречи-главы"></a>[E-016 — обход препятствий rover под встречи главы](tickets/E-016.md#e-016--обход-препятствий-rover-под-встречи-главы)

<a id="g-014--native-встреча-в-существующем-эпизоде"></a>[G-014 — native-встреча в существующем эпизоде](tickets/G-014.md#g-014--native-встреча-в-существующем-эпизоде)

<a id="g-015--локальный-retry-смешанной-встречи"></a>[G-015 — локальный retry смешанной встречи](tickets/G-015.md#g-015--локальный-retry-смешанной-встречи)

<a id="e-018--подготовка-gpu-ресурсов-до-первого-игрового-кадра"></a>[E-018 — подготовка GPU-ресурсов до первого игрового кадра](tickets/E-018.md#e-018--подготовка-gpu-ресурсов-до-первого-игрового-кадра)

<a id="h-008--актуальный-профиль-нового-маршрута"></a>[H-008 — актуальный профиль нового маршрута](tickets/H-008.md#h-008--актуальный-профиль-нового-маршрута)

<a id="h-009--кандидат-следующего-эпизода"></a>[H-009 — кандидат следующего эпизода](tickets/H-009.md#h-009--кандидат-следующего-эпизода)

<a id="g-016--доводка-существующих-сценариев-по-слепому-тесту"></a>[G-016 — доводка существующих сценариев по слепому тесту](tickets/G-016.md#g-016--доводка-существующих-сценариев-по-слепому-тесту)

<a id="g-017--d4--контур-охлаждения-и-повторяемое-производство"></a>[G-017 / D4 — контур охлаждения и повторяемое производство](tickets/G-017.md#g-017--d4--контур-охлаждения-и-повторяемое-производство)

<a id="g-019--полноценная-глава-связный-маршрут-угроза-и-задача"></a>[G-019 — полноценная глава: связный маршрут, угроза и задача](tickets/G-019.md#g-019--полноценная-глава-связный-маршрут-угроза-и-задача)

<a id="13-журнал-выполнения-и-точка-продолжения"></a>[13. Журнал выполнения и точка продолжения](history/playable-slice-2026-09--10.md#13-журнал-выполнения-и-точка-продолжения)

<a id="e-003--команды-и-артефакты"></a>[E-003 — команды и артефакты](history/playable-slice-2026-09--10.md#e-003--команды-и-артефакты)

<a id="e-004--команды-и-артефакты"></a>[E-004 — команды и артефакты](history/playable-slice-2026-09--10.md#e-004--команды-и-артефакты)

<a id="g-018--связать-готовые-главы-комплекса-2026-10-08"></a>[G-018 — связать готовые главы комплекса (2026-10-08)](tickets/G-018.md#g-018--связать-готовые-главы-комплекса-2026-10-08)

</details>
