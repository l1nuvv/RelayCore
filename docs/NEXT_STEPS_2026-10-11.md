# RelayCore — следующий цикл и передача контекста

Дата: 2026-10-11. Этот файл сохраняет рекомендацию после анализа документации и указание пользователя перейти в следующий чат. Это handoff, а не новая очередь статусов; gameplay/content evidence остаётся в PLAYABLE_SLICE_ROADMAP.md, VK14 milestones — в RENDERER_MODERNIZATION_PLAN.md.

## Главный вывод

Текущий контракт с 2026-10-12: **обязательный Vulkan 1.4 без 1.3 fallback и forced-1.3 режима**. VK14-00–04 уже имеют execution evidence в [owning renderer plan](RENDERER_MODERNIZATION_PLAN.md); следующий milestone выбирается из его pending задач по отдельному поручению. Не начинать весь renderer backlog одним проходом. Исходная рекомендация и результаты двух API-профилей ниже сохранены как история; 1.3 compatibility больше не является gate.

Проект уже имеет принятую автором главу G-019, связанную кампанию, native asset pipeline, материалы/probes, RmlUi, редактор, сохранения и release/update workflow. Наполнение G-019 технически завершено; повторная реализация этих возможностей не нужна. Версия продукта — 1.4.1; при исходной рекомендации runtime API — Vulkan 1.3. Результат следующего цикла записан в разделе выполнения ниже. Версии продукта и API независимы.

Последние рассмотренные performance measurements не описывают нынешнюю G-019 целиком в непрерывном движении/бою со звуком. VIS-48 относится к более раннему состоянию, 1280×720 и audio off. Scene-completion waits наблюдались, но их роль в актуальной главе требует нового измерения.

## Исходный рекомендуемый порядок — историческая запись

1. **VK14-00:** зафиксировать source state, executable/assets, GPU/driver, build и настройки. Проверить актуальную G-019, loading, бой, возвращение, Continue/retry, UI и resize; снять Tracy CPU/GPU и контрольные виды. Получить baseline, воспроизводимые дефекты и реальные затраты. Выбрать целевое разрешение/FPS и бюджет памяти; численные бюджеты пока не согласованы. При отсутствии ответа использовать явно записанный рабочий профиль, не выдавая его за утверждённый hardware target.
2. **VK14-01:** negotiation 1.4/1.3, диагностический отчёт и принудительный effective-1.3 режим. Проверить launch/render/UI/resize на доступных профилях. Реальный 1.3-only GPU не заменять принудительным режимом; отсутствующую hardware coverage отмечать отдельно.
3. **Цвет и изображение:** воспроизвести замечания к SDR-пересвету, экспозиции, бликам и UI на совпадающих игровых видах. Исправлять подтверждённые дефекты; внутренний FP16 HDR не считать HDR monitor output.
4. **Lifetime/scheduling:** определить GPU last-use и retirement ресурсов. Затем измерить пользу двух scene frames-in-flight и GPU timeline waits вместо обычных host upload waits. Удалять ожидания только при безопасном lifetime и конкретном измеренном результате.
5. **Первый новый эффект:** один потребитель и одна задача. Кандидаты — bloom для emissive equipment либо AO для контакта геометрии. Выбор по текущим игровым видам; отдельный cost budget, off parity и учёт существующего material AO.

## Человеческая приёмка

Проверить итоговое наполнение G-019, HUD/materials в движении и звук. Независимый первый проход нужен для длительности и понятности. G-019 остаётся done по авторской приёмке 2026-10-10; отдельные VIS/IMG/G-010, duration/sound-on и other-GPU gates не закрываются этим автоматически. При недоступности участника техническую модернизацию можно продолжать с открытыми внешними gates.

RT, mesh shaders, bindless, VRS, SSGI и frame generation остаются последующими этапами. Общий navmesh, skeletal animation и расширение compiler начинать с конкретного gameplay consumer, который упирается в текущие ограничения. Новая глава и повторный art-pass всей кампании не являются первым шагом этой рекомендации.

## Контекст для следующего чата

- Локальный HEAD при анализе: c9b4dcb619def034f81eca0cca5604bb5e63709c. Рабочее дерево содержит значимые незакоммиченные изменения; SHA не идентифицирует его целиком. Перед продолжением проверить текущее состояние, сохранить чужие правки, не commit/push без поручения.
- В предыдущем чате обновлены README и обзорные документы; внешний Vulkan-план перенесён в docs с дроблением крупных milestones и temporal dependencies. Палитра включена в ART_DIRECTION.md. Внешние attachments сохранены.
- Документационные проверки: 486 локальных file links, 82 Markdown anchors — без ошибок; git diff --check прошёл. Это результаты предыдущей сверки до добавления этого handoff.
- Свежие builds, CTest, runtime playthrough, validation, RenderDoc и Tracy в анализе и документационном обновлении не выполнялись. Старые результаты — датированное evidence, не новый зелёный baseline.
- Следующий конкретный рекомендуемый deliverable — VK14-00 baseline/report в owning renderer plan. VK14-01 выполняется отдельным шагом после baseline; сохранённая рекомендация не означает, что implementation уже началась.
- Использовать существующие presets/targets и narrow verification, relevant skills и AGENTS.MD. Routine development не создаёт ZIP, release package или новый standalone candidate.

## Основные документы

- [Renderer modernization plan](RENDERER_MODERNIZATION_PLAN.md)
- [Gameplay/content roadmap](PLAYABLE_SLICE_ROADMAP.md)
- [Development и проверки](DEVELOPMENT.md)
- [Границы оценки](ASSESSMENT_SCOPE_2026-10-11.md)
- [VIS-48/49 измерения](art/vis48-49-cost-and-review.md)
- [G-019 playtest protocol](playtests/G-019-2026-10-09.md)
- [Art direction и палитра](ART_DIRECTION.md)

Ожидаемый ближайший результат: актуальный baseline G-019, выявленные проблемы и затем проверенная поддержка preferred Vulkan 1.4 / required Vulkan 1.3. Дальнейшие приоритеты выбирать по этой evidence.

## Выполнение в следующем чате — 2026-10-11

По поручению автора выполнен технический VK14-00 и реализована negotiation VK14-01;
актуальные результаты находятся в owning [renderer plan](renderer/VK14-00.md#vk14-00-execution--2026-10-11).
G-019 baseline: 10 585 warm CPU frames, звук on, рабочие 1080p/60 FPS;
на доступном GPU steady p99 4,6–6,0 мс, отдельные retry spikes до 52 мс.
Preferred/forced regression: 7/8 и 5/6 соответственно, без skips, при процессной
изоляции устаревшего OBS hook; 134 G-019 captures совпадают с baseline.
Полный VK14-01 не закрыт: screen-pixel IMMEDIATE clear check падает до изменения
API и в обоих новых профилях, даже без OBS. Причина не установлена;
выключение hook устраняет его API-1.3/1.4 loader warning, но не пиксельный сбой.
Дальше — узко разрешить presentation discrepancy; затем отдельный цветовой этап.
Native 1.3-only GPU и непрерывный sound-on маршрут сохраняют отдельные evidence gaps.
Обычный `RelayCore — Play` пересобран; optimized/Tracy device checks 3/3,
интегрированный G-019 workload на preferred 1.4 завершён с кодом 0.

### Продолжение: локальная проверка VK14-01 — 2026-10-11

По следующему поручению автора presentation/recreation повторены по пять раз
на preferred 1.4 и forced 1.3: 20 успешных checks. Старый белый пиксель не
воспроизвёлся; причина не установлена, исправление не заявляется. Полная локальная
выборка вне файлового sandbox: 9/9 preferred и 7/7 forced, без failures/skips и
validation diagnostics. Все 134 G-019 captures в каждом профиле совпали с baseline
по SHA256. Первую неудачную sandbox-выборку с access denied на canonical/rename
сохранили отдельно. Исходники и допуски в этом цикле не менялись.

VK14-01 технически проверен в доступных профилях; native 1.3-only GPU и прежнее
необъяснённое presentation расхождение сохраняются как ограничения. При повторе
pixel failure возобновить узкую диагностику. Следующий bounded шаг — **VK14-02**:
воспроизвести замечание к SDR/экспозиции на одинаковых видах, затем исправлять
подтверждённую причину. Команды, JUnit и границы — в
[owning renderer plan](renderer/VK14-01.md#vk14-01-presentation-follow-up--2026-10-11).

### VK14-02 — SDR/HDR correctness — 2026-10-11

По отдельному поручению автора проверен текущий цветовой путь после исправлений
2026-10-08. На шести совпадающих видах G-019 прежний пересвет/затемнение не
воспроизвёлся: среднее различие SDR/HDR EV 0 — 0,0194–0,1418 уровня RGB из 255,
p99 — 1, clipping отсутствует. Renderer/shaders/материалы не менялись.
Добавлен `graphics.containment_color`: три фиксированных вида, SDR/HDR EV 0,
экспозиция −1/0/+1 через prepared reload и прежний gameplay/save/exit маршрут.

Узкая Debug-сборка проходит; финальная выборка — **10/10 preferred 1.4 и 10/10
forced 1.3**, без failures/skips и validation diagnostics. В каждом профиле
134 исходных кадра совпадают с baseline; девять новых exposure кадров совпадают
между профилями. Локальная техническая проверка VK14-02 завершена; субъективная
яркость, physical HDR output и native 1.3-only GPU остаются отдельными gates.
Команды, JUnit, captures, контракт вывода и ограничения — в
[owning renderer plan](renderer/VK14-02.md#vk14-02-execution--2026-10-11).
Следующий bounded milestone — **VK14-03**; в этом поручении он не запускался.
