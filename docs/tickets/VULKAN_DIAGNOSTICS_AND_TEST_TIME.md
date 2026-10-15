# Vulkan: диагностика, ручной проход и время проверок

Дата: 2026-10-15. Этот файл — точка продолжения задачи из чата.
Процедуры запуска и validation принадлежат [DEVELOPMENT](../DEVELOPMENT.md),
runtime ownership — [ARCHITECTURE](../ARCHITECTURE.md). Здесь хранятся только
объём этой задачи, решения и её evidence; статусы renderer milestones не дублируются.

## Что запросил пользователь

- Ускорить регулярно запускаемые при аудитах и исправлениях тесты карт G-009/G-019.
  Пример желаемого результата: 15 → 3 секунды, сохранив результат и полноту проверок.
  Это целевой пример, а не подтверждённое достижение или гарантированный бюджет.
- Убрать известное предупреждение о версии слоя OBS, чтобы оно не проваливало тесты.
- Собрать полный список остальных наблюдаемых предупреждений и ошибок Vulkan,
  объяснить важность; не скрывать потенциально важные сообщения.
- Пользователь готов самостоятельно пройти игру от первой до последней карты.
  Использовать логи этого прохода вместе с автоматическими проверками.
- Проверить качество логов и логгер; улучшать их при выявленных недостатках.
  Не считать логгер причиной задержек без измерений.
- Сохранить план в файле и дать пользователю конкретные действия.
- Уточнение пользователя: постоянно писать один `RelayCore_latest.log`,
  перезаписывая предыдущую сессию; отдельные датированные логи не создавать.

## Уже сделано и ограничения evidence

- Коммит `d07a0f7`: в `src/relaycore/render/vulkan_device.cpp` подавлен только точный
  loader warning о Vulkan 1.3 у `VK_LAYER_OBS_HOOK` при API приложения 1.4.
  OBS не отключён. Другие предупреждения/ошибки и строгий CTest gate сохранены.
- Узкая сборка `cmake --build --preset graphics-debug --target relaycore_vulkan_device_tests -j 2`
  и `ctest --preset graphics-debug -R '^graphics\.vulkan_device$' --output-on-failure --no-tests=error`
  прошли вне sandbox. Итоговый тест: 4,23 с, validation=true, без пропуска.
  Форматирование и `git diff --check` прошли.
- Изменение `assets/shaders/vulkan/mesh.slang` уже было в рабочем дереве;
  оно не принадлежит исправлению OBS. Сохранять его и учитывать при сравнении сборок.
- Пересобран только device test. Остальные ранее собранные executable ещё могут
  печатать OBS warning: это не доказательство неработающего исправления в исходниках.
- В G-019 игровое время уже продвигается программно, без ожидания каждой игровой секунды.
  Простого фиксированного ожидания на 15 секунд в исследованном сценарии не найдено.
- Предыдущие sandbox-запуски G-019: 14,28/13,20 с, но с access denied при чтении модели
  и сохранении pipeline cache. Эти результаты не являются baseline исправного теста.
- Предыдущий G-009 сообщил `VUID-vkCmdDrawIndexed-None-08600`: descriptor set 2
  не привязан/несовместим с pipeline layout. Также провалилась проверка
  `Point light occluder did not darken the floor`. Требуется повтор на согласованной
  сборке; нельзя объявлять это исправленным или переносить вывод на все карты.
- В логах обоих сценариев есть ошибка RmlUi о загрузке `branding/relaycore-menu.bmp`.
  Это сообщение UI, а не Vulkan VUID; учитывать отдельно и проверить видимый результат.
- При первоначальном исследовании найден пробел: Vulkan validation callback писал в stderr через
  `fprintf`, а не в файловые sinks логгера. Исправление направляет validation в
  логгер и его файл; stderr остаётся резервом вне времени жизни логгера.
- `RelayCore — Play` и `RelayCore — G-019 Playtest` используют `msvc-dev`;
  в текущем cache Vulkan validation выключена. Такой проход полезен для runtime
  ошибок и наблюдений игрока, но не доказывает отсутствие validation ошибок.
  Обычный Debug с validation не подходит для оценки скорости playable build.
- Файловый логгер поддерживает latest и датированные сессии, периодический flush — 1 с.
  По новому запросу датированные сессии по умолчанию отключены, latest перезаписывается.
  Оснований переписывать его или обещать ускорение пока нет.
- Режим одного файла собран для `RelayCore — Play` командой
  `cmake --build --preset demo-dev -j 2`. Debug targets `relaycore_tests` и
  `relaycore_vulkan_device_tests` также собраны. Проверки
  `ctest --preset test-debug -R '^relaycore\.logger' --output-on-failure --no-tests=error`
  прошли 2/2 (в том числе перезапись между сессиями и отсутствие новых архивов);
  `ctest --preset graphics-debug -R '^graphics\.vulkan_device$' --output-on-failure --no-tests=error`
  прошла 1/1 без skip, 4,40 с. Результат последующего ручного прохода приведён ниже.
- Повтор вне sandbox завершён; команда:
  `ctest --preset graphics-debug -R '^graphics\.(containment_loop|containment_loop_hdr|power_restoration|vulkan_device)$' --output-on-failure --no-tests=error --output-log build/vulkan-warning-inventory-2026-10-15.log`.
  Device прошёл (4,43 с). G-019 SDR/HDR выполнили сценарии, но CTest отклонил
  старое OBS warning (42,26/42,19 с). G-009 снова сообщил descriptor VUID выше
  и `VUID-VkGraphicsPipelineCreateInfo-layout-07988`: shader resources отсутствуют
  в объявленном pipeline layout: set 2/binding 0 `sceneInstances`, set 0/bindings
  12–16 `metallicRoughnessMap`, `occlusionMap`, `reflectionMap`, `reflectionData`,
  `emissiveMap`. Также провалена проверка тени. Эти ошибки важны и не подавляются.
  Всего 1 passed, 3 failed, без skips, 91,17 с.
  Access denied и ошибка сохранения cache в этом повторе не наблюдались;
  ошибка загрузки изображения RmlUi осталась. Эти времена относятся к текущим
  существующим Debug binaries, не к оптимизированной игровой сборке.

## Ручной проход и оценка лога — 2026-10-15

Пользователь завершил проход; агент прочитал
`%APPDATA%\l1nuvv\RelayCore\logs\RelayCore_latest.log`.
Сессия `2026-10-15_11-53-44`: от 11:53:44.042 до 11:56:06.659,
около 2 мин 23 с, 190 строк, 20 961 байт, штатный shutdown.
В файле нет warning/error/critical, но указано `validation=false`:
это не подтверждение отсутствия Vulkan validation ошибок и не опровержение
ошибок прежних автоматических проверок.

Загрузки карт зафиксированы в порядке `power_restoration`, `foundation_intake`,
`foundation_pump`, `bench_relay`, `pressure_transfer`, `cooling_loop`,
`occult_lab_b17`, `containment_loop`. Отдельного события завершения кампании нет;
по одним загрузкам карт и штатному выходу достижение финала не доказывается.

133 из 190 строк — периодические FPS-сообщения. Исходники
`src/relaycore/debug/imgui_overlay.cpp` показывают, что FPS и время кадра берутся
из `FrameInfo::delta_seconds`, ограниченного 100 мс в
`src/relaycore/app/frame_timer.cpp`. Поэтому строка `10 FPS / 100 ms`
может скрывать более долгую остановку. Это отдельные кадры примерно раз в секунду
и скользящее среднее, а не распределение всех кадров за интервал.
Размер этого файла не даёт оснований считать файловое логирование причиной
замедления игры. Переписывать spdlog или обещать выигрыш производительности не требуется.

## Изменения диагностики к выполнению

Оба приоритета реализованы: [evidence захода 1](#заход-1--реализация-и-проверки-2026-10-15)
и [evidence захода 2](#заход-2--реализация-и-проверки-2026-10-15).
Сохранены один перезаписываемый файл и текущая основа spdlog.

| Приоритет | Изменение | Критерий проверки |
| --- | --- | --- |
| 1 | Отделить диагностическое время кадра от ограниченного игрового шага; измерять фактическое wall-clock время. | Детерминированная проверка интервала свыше 100 мс: диагностическое значение сохраняет длительность, ограничение simulation delta не меняется. |
| 1 | Заменить поток одиночных FPS-значений периодической сводкой: число кадров, среднее, p95/p99, максимум и текущая карта. Разделять игру, меню, паузу и загрузку. | В сводку входят все кадры заявленного интервала; явно указаны границы измерения и режим, нет ложной трактовки одиночных выборок как распределения. Проверить расчёт на известной последовательности. |
| 1 | Фиксировать начало, успех и отказ перехода: исходная/новая карта, причина перехода, длительность. | По логу различимы попытка и успешно завершённый переход; при отказе видны ошибка и сохранённое активное состояние. |
| 1 | Добавить отдельное событие завершения кампании. | Реальное достижение финала записывается отдельно от shutdown; обычное закрытие игры такого события не создаёт. |
| 1 | Дополнить заголовок сессии версией/идентичностью сборки и применёнными настройками: executable, asset root, разрешение, VSync, графический пресет, HDR, масштаб рендера, состояние validation. Записывать изменения этих настроек в течение сессии. | По файлу можно воспроизвести условия; используются эффективные значения после применения player settings, а не только исходный config. Не вводить тяжёлую систему build metadata ради этого пункта. |
| 2 | Различать причины отказа pipeline cache: отсутствует, несовместим, повреждён; добавить время его загрузки/подготовки и результат использования там, где это измеримо. | Причина подтверждается соответствующей веткой проверки cache; попадание в cache не объявляется ускорением без сравнимого замера. |
| 2 | Убрать дублирующиеся startup-сообщения и отделить подробности для debug от полезной info-сводки. | Разобрана двойная запись `Loaded engine config`; сокращение вывода не скрывает реальную повторную загрузку или ошибку. |
| 2 | Добавить понятный источник сообщений: renderer, assets, gameplay и другие фактически нужные области вместо одной категории `[engine]`. | Источник сообщения можно установить по строке; не создавать новую подсистему логирования или отдельные файлы для категорий. |
| 2 | Различать первичную инициализацию графических компонентов и их пересоздание; указывать причину пересоздания. | Повторные строки об atlas/materials/geometry/depth/culling позволяют определить назначение операции; повтор не объявляется лишней работой без исследования. |
| 2 | Направить оставшиеся ошибки завершения Vulkan в файловый лог при живом логгере; оставить безопасный stderr fallback. | Проверить device/material descriptors/mesh/scene/presentation/upload shutdown paths. Сохранить noexcept, сообщения вне lifetime логгера и строгий CTest gate; исключить двойную печать. |

CPU work, present wait и GPU time не выводить из FPS. При добавлении таких
показателей явно указывать границы и источник; для подробного CPU/GPU анализа
использовать существующий Tracy workflow. Не превращать обычный лог в profiler capture.

### Effort и выполнение в два захода

Пользователь планирует выполнить весь список минимум в два захода. Ниже effort
означает рекомендуемый reasoning effort агента; сложность — отдельная качественная
оценка объёма и риска, не обещание времени выполнения. Уровни рекомендованы по
текущему коду RelayCore. Доступность уровней зависит от выбранной модели/клиента,
см. [официальный справочник OpenAI](https://learn.chatgpt.com/docs/config-file/config-reference).
Настройки модели этим документом автоматически не меняются.

**Заход 1 — достоверные измерения и события прохода. Рекомендуемый effort: `high`.**

| Изменение из списка выше | Effort отдельно | Сложность |
| --- | --- | --- |
| Реальное время кадра без ограничения 100 мс | `high` | Средняя: отделить измерение от игрового шага, не изменить simulation. |
| Сводка кадров с p95/p99/max, картой и режимом | `high` | Высокая: корректные выборки, границы интервалов и учёт смены режима. |
| Начало, успех и отказ перехода между картами | `high` | Средняя: различить подготовку, commit состояния и отказ перехода. |
| Событие завершения кампании | `medium` | Низкая: найти авторитетный момент финала, исключить повтор и ложное событие при выходе. |
| Заголовок сессии и изменения эффективных настроек | `medium` | Средняя: собрать фактические настройки после player settings и связать их изменения. |

Порядок: сначала фактическое время и события смены карты/режима, затем агрегаты
и контекст настроек. Заход завершён после проверок времени/статистики, успешного
и неуспешного перехода, финала и обычного закрытия; короткий runtime-проход должен
показать заголовок, понятные сводки и события. Сохранить один файл и ограничение
simulation delta. Выполнить узкие build/tests, diff check и отдельный коммит.

**Заход 2 — понятный вывод и сохранность диагностики. Рекомендуемый effort: `high`.**

| Изменение из списка выше | Effort отдельно | Сложность |
| --- | --- | --- |
| Причины отказа pipeline cache и измеримые длительности | `medium` | Средняя: сохранить правила совместимости/инвалидации и точно обозначить измеряемую работу. |
| Устранение дублей startup и разделение info/debug | `low` | Низкая: проверить реальные call sites, не скрыть повторную операцию. |
| Источники renderer/assets/gameplay | `medium` | Средняя: единый понятный формат без новой архитектуры логирования. |
| Первичная инициализация и пересоздание компонентов | `medium` | Средняя: установить назначение операций и доступные причины пересоздания. |
| Ошибки Vulkan shutdown в файле и stderr fallback | `high` | Высокая: lifetime логгера, noexcept teardown, сохранение ошибок и отсутствие двойного вывода. |

Порядок: обозначить источники/назначение операций и упорядочить startup/cache
сообщения; затем проверить все перечисленные shutdown owners. Заход завершён
после узких проверок cache и диагностики завершения, проверки отсутствия дублей
и повторного подтверждения единственного файла. Сопоставить результат с заходом 1,
сохранить строгий Vulkan warning/error gate и сделать отдельный коммит.

Оба захода целиком разумно запускать на `high`: в первом сложны границы измерений,
во втором — корректное завершение ресурсов. Для изолированных небольших пунктов
достаточны уровни из таблиц. `xhigh` оставлять для конкретного неразрешённого
вопроса ownership/synchronization или неоднозначного воспроизведения; оснований
назначать его либо `max`/`ultra` всему списку сейчас нет. Если такой вопрос появится,
вынести его в дополнительный заход с evidence, сохранив уже завершённые части.
Это разделение относится к улучшениям логирования; исправление прежних Vulkan
ошибок и ускорение автоматических тестов остаются отдельными этапами общего плана.

После реализации сравнить новый файл с этим проходом по полноте событий и
понятности вывода, проверить узкими тестами расчёты/переходы/ошибки записи и выполнить
короткий runtime-проход. Полный повтор пользователем запрашивать только при
конкретном пробеле evidence. Существующий latest может быть перезаписан следующим
запуском: приведённые выше результаты — уже прочитанная сводка, не обещание
неизменности файла. Копии и архивы автоматически не создавать.

## Заход 1 — реализация и проверки 2026-10-15

Выполнены все пять пунктов приоритета 1. Исходная ревизия — `c17fee0`, рабочее
дерево перед задачей было чистым. Изменения относятся только к диагностике;
shader/assets, правила simulation, сохранений и активации карт не изменялись.
Этот заход не включал приоритет 2, исправление прежних Vulkan ошибок и ускорение
G-009/G-019; ускорение этим коммитом не заявляется.

- `FrameTimer` сохраняет необрезанный steady-clock интервал в `wall_delta_seconds`;
  прежний `delta_seconds` и его предел 100 мс сохранены, включая первый кадр.
  Overlay и Tracy frame-time plot используют необрезанное значение.
- Сводки собирает цикл приложения независимо от видимости ImGui. Они содержат
  все завершённые циклы группы, mean/p95/p99/max и явный wall-clock интервал.
  Игровые, меню, пауза, редактор, финал, suspended и пограничные/loading циклы
  разделены. Подготовка карты и графическая реконфигурация не попадают в обычный
  игровой bucket. Формат и границы измерений принадлежат
  [DEVELOPMENT](../DEVELOPMENT.md#vulkan-capture-и-validation).
- Startup, новая игра, Continue, переходы через выходы, live reload,
  editor preview/return получили begin/success/failed. Записываются причина,
  source/destination/entry, длительность и фактически активная карта. Success
  отмечает commit runtime до записи прогресса; отказ сохранения не превращает
  успешную смену карты в отказ перехода. Сохранены прежние сообщения ошибок.
- Авторитетный финал штатной `levels/containment_loop.toml` создаёт однократное
  `Campaign completed`; восстановление завершённого save обозначено отдельно
  `Campaign completion restored`. Закрытие игры и посещение карты не создают финал.
- Заголовок содержит version, build configuration и compile timestamp,
  executable и asset root. Снимки фактического renderer/window после player
  settings показывают pixel extent, VSync request и текущий present mode,
  определённый по эффективным эффектам preset, HDR сцены/SDR выхода, scale,
  dynamic resolution, color pipeline, validation и эффекты. Изменения пишутся
  при изменении применённых значений, включая debug-реконфигурацию и resize.

Проверки выполнены вне sandbox, в Visual Studio Developer PowerShell x64:

```powershell
cmake --build --preset tests-debug --target relaycore_tests relaycore_game_lifecycle relaycore_window_lifetime_tests -j 2
ctest --preset test-debug -R '^relaycore\.(frame |logger |fixed simulation)' --output-on-failure --no-tests=error
ctest --preset graphics-debug -R '^graphics\.(game_lifecycle|campaign_route|hdr_settings|diagnostics_smoke|window_lifetime)$' --output-on-failure --no-tests=error --output-log build/diagnostics-pass1-graphics-2026-10-15.log
cmake --build --preset demo-dev -j 2
git diff --check
```

Unit: **8/8 passed**, включая известную последовательность 1…100 мс
(mean 50,5 / p95 95 / p99 99 / max 100), интервал 350 мс при simulation delta
100 мс, границы/хвосты/переключения сводок, suspended, отключение вывода,
перезапись latest и отказ файлового sink с явным console fallback.

Graphics: **5/5 passed, 0 skips**, validation=true (RTX 4060 Ti); строгий Vulkan warning/error gate сохранён.
`game_lifecycle` проверяет commit, отказ с сохранённой активной картой, отказ
записи прогресса после commit и редакторские переходы. `campaign_route` проверяет
восемь глав, реальное завершение последней zone, отсутствие дубля/ложного финала
при shutdown и Continue завершённого save. `hdr_settings` проверяет заголовок,
изменения применённых настроек и отсутствие ложной записи при отказе применения.
`window_lifetime` проверяет нормальное и аварийное завершение приложения.

`diagnostics_smoke` — короткий проход настоящего `Application::run` со staged
Debug assets: menu → game → несуществующая карта → pause → успешный переход
foundation_intake → foundation_pump → resize → menu → штатный shutdown.
Финальный smoke отдельно повторён после уточнения fixture-конфига: `cmake --build --preset graphics-debug --target relaycore_game_lifecycle -j 2`, затем `ctest --preset graphics-debug -R '^graphics\.diagnostics_smoke$' --output-on-failure --no-tests=error --output-log build/diagnostics-pass1-smoke-2026-10-15.log`: **1/1 passed, 0 skips**.
Временный профиль player settings переопределяет config VSync=false на true;
лог проверяет эффективный запрос, FIFO, 800×600 после resize, все нужные сводки,
события переходов и единственный `RelayCore_latest.log`. Профиль/лог теста
изолированы; пользовательский progress не меняется. В сравнении с ручным логом
теперь видны распределения всех кадров и commit/отказ переходов, воспроизводимый
контекст сборки/настроек и отдельное доказательство финала. Полный новый ручной
проход пользователя для этого захода не требуется.

`demo-dev` собран для обычного **RelayCore — Play**. Форматирование task-owned
C++ (`clang-format --dry-run --Werror`) и `git diff --check` прошли.
При сборке оставались предупреждения в не менявшихся
`demo_game_source_debug_commands.cpp` и `pipeline_authoring_overlay_tests.cpp`;
новых предупреждений в добавленной диагностике не наблюдалось.
Сборочные asset targets переиспользовали существующие probe/reflection products.
ZIP, release, push и отдельные датированные копии логов не создавались.
Времена этой верификации не являются новым performance baseline.

## Заход 2 — реализация и проверки 2026-10-15

Исходная ревизия — `0fad9be`, рабочее дерево было чистым. Выполнены пять пунктов
приоритета 2; сохранены spdlog, общий latest-файл и строгие CTest regex для Vulkan
validation и shutdown errors. Формат/cache schema, правила совместимости,
порядок освобождения GPU ресурсов и gameplay не менялись.

- `Vulkan pipeline cache load` сообщает конкретную ветку отказа: missing,
  incompatible (schema/vendor/device/driver/UUID/shaders), corrupt
  (размер/checksum/Vulkan header), отдельно I/O, подготовку hash и Vulkan
  create/merge failure. Успех означает merge в живой cache, а не доказанные
  pipeline hits. Записываются принятый размер и steady-clock длительность
  полной попытки. Backend/pipeline preparation имеет собственные границы времени.
- Подтверждён один вызов загрузчика config в startup и две записи о нём:
  загрузчик и `Application`. Удалена только запись `Application`; реальный
  live reload по-прежнему виден. Extensions и atlas/material/geometry/depth/culling
  details доступны на debug; info сохраняет контекст устройства и сводки операций.
- Основные startup/runtime сообщения получили источники renderer/assets/gameplay/
  config/app/audio в тексте сообщения без новых loggers, sinks и файлов.
- Backend preparation различает initialize/recreate и startup/graphics_settings/
  live_content_reload; prepared отделён от commit. Presentation отмечает первый
  запуск, pixel extent, VSync, acquire/present и surface loss. Shader pipeline
  preparation отдельно отличает backend construction от shader reload. Повторы
  не объявляются лишней работой.
- Шесть прежних stderr-only веток device/material descriptors/mesh/scene/
  presentation/upload используют `log::error_noexcept`. Живой logger получает
  ошибку единожды; до init, после shutdown или при исключении используется
  stderr. Числовой VkResult форматируется в стековый buffer. Снимок shared logger
  берётся под его mutex; fallback не обращается к уничтоженному logger.
  Также закрыт бросающий путь диагностики cache save из `Renderer::~Renderer`.

Проверки выполнены в Visual Studio Developer PowerShell x64, вне sandbox:

```powershell
cmake --build --preset tests-debug --target relaycore_tests relaycore_vulkan_device_tests relaycore_vulkan_mesh_tests relaycore_vulkan_presentation_tests relaycore_vulkan_upload_tests relaycore_game_lifecycle relaycore_window_lifetime_tests -j 2
ctest --preset test-debug -R '^relaycore\.(logger |frame |fixed simulation)' --output-on-failure --no-tests=error
ctest --preset graphics-debug -R '^graphics\.(vulkan_device|mesh_pipeline|vulkan_presentation|vulkan_recreation_failure|surface_loss|vulkan_upload|material_descriptors|scene_submission|diagnostics_smoke|hdr_settings|window_lifetime|game_lifecycle|campaign_route)$' --output-on-failure --no-tests=error --output-log build/diagnostics-pass2-graphics-2026-10-15.log
cmake --build --preset demo-dev -j 2
git diff --check
```

Unit: **9/9 passed** на окончательной сборке. Новый тест проверяет по одной записи
шести типов teardown diagnostics в файл, отсутствие дублирования в stderr,
VkResult, fallback до init/после shutdown и при исключении sink/error handler.
Существующие проверки latest-only/truncate и frame diagnostics первого захода прошли.

Graphics: **13/13 passed, 0 skips**, validation=true, RTX 4060 Ti,
**193,22 с** суммарно. Сводка CTest — `build/diagnostics-pass2-graphics-2026-10-15.log`,
подробный вывод — `build/msvc-debug/Testing/Temporary/LastTest.log` (он заменяется
следующим CTest run). В выводе нет `Vulkan validation error/warning` и
`Vulkan ... shutdown failed`. `mesh_pipeline` проверил успешный warm merge,
отдельные причины schema/vendor/device/driver/UUID/shader incompatibility,
checksum/size corruption, I/O и missing, сохранив рабочий pipeline после отказов.
Device, upload, scene submission, descriptors и presentation выполнили штатное
освобождение ресурсов; recreation failure и surface loss — восстановление после
внедрённых отказов. Настоящая потеря GPU и ошибки Vulkan create/merge/shutdown
не провоцировались: аварийный reporter проверен синтетически, call sites просмотрены.

`diagnostics_smoke` прошёл настоящий `Application::run` на уровне info:
menu → game → отсутствующая карта → pause → переходы → resize → menu → shutdown.
Подтверждены единственная запись загрузки config, отсутствие debug extensions/
material details в info, метки источников, initialize/startup и recreate/pixel_extent,
сводки/события первого захода и один latest-файл. `hdr_settings` проверил применение
и отказ настроек, `game_lifecycle` — commit/rollback и live reload, `campaign_route`
— восемь глав и авторитетный финал. В полном выводе видны recreate с причинами
live_content_reload и graphics_settings. Повторный полный ручной проход не нужен.

Обычный **RelayCore — Play** (`demo-dev`) пересобран. Все task-owned C++ прошли
`clang-format --dry-run --Werror`; `git diff --check` прошёл. Первый sandbox build
остановился на MSVC C1902 (PDB manager); повтор вне sandbox прошёл. В сборке
осталось прежнее C4100 в не менявшемся `demo_game_source_debug_commands.cpp`;
новых предупреждений task-owned C++ не наблюдалось. Штатные asset dependencies
перекомпилировали модели и переиспользовали существующие probe/reflection products.
Время этой проверки не является baseline производительности; прежние VUID и
ускорение G-009/G-019 остаются отдельными этапами общего плана.

## Продолжение — свежий inventory и время G-009/G-019, 2026-10-15

Исходная ревизия — `461d8a7`, рабочее дерево было чистым. Работа относится к
оставшимся пунктам этого ticket: актуальному inventory, восстановлению проверок
и измеренному времени тестов. Полный runtime-аудит H-008 этим не выполняется.

Свежая сборка с validation и согласованными staged assets:

```powershell
cmake --build --preset graphics-debug --target relaycore_power_restoration_graphics relaycore_containment_loop_graphics relaycore_vulkan_device_tests -j 2
ctest --preset graphics-debug -R '^graphics\.(power_restoration|containment_loop|containment_loop_hdr|vulkan_device)$' --output-on-failure --no-tests=error --output-log build/vulkan-diagnostics-fresh-2026-10-15.log
```

Реестр подтверждённого результата, RTX 4060 Ti, API loader 1.4.357 / GPU 1.4.341:

| Сообщение / проверка | Свежий результат и статус |
| --- | --- |
| `VUID-vkCmdDrawIndexed-None-08600` | В свежих проверенных draw paths не воспроизведён; не подавляется. |
| `VUID-VkGraphicsPipelineCreateInfo-layout-07988` (set 2/binding 0 и set 0/bindings 12–16) | В свежей согласованной сборке не воспроизведён; отдельный shader/layout patch по старому логу не нужен. |
| `Point light occluder did not darken the floor` | Свежий G-009 прошёл эту проверку и дошёл до следующих assertions. |
| RmlUi: загрузка `branding/relaycore-menu.bmp` | На свежих assets прежнее сообщение не воспроизведено; текстура не отключалась и fallback не добавлялся. |
| Точное OBS loader warning | Свежий CTest его не отклоняет; прежний узкий фильтр сохранён. |
| `Unpowered sector must use emergency lights only` | Воспроизведён устаревший literal 8: текущий бюджет 32 и текущая карта содержат больше источников. Заменён проверкой количества и состава относительно авторских power gates; powered brightness assertion сохранён. |
| `Small viewport HUD is missing` и UI text log | Проверка искала HUD в ImGui после миграции в RmlUi. Проверяются видимость и bounds status/objective/reticle; evidence содержит видимый RmlUi text, исключая hidden elements. Кадр `small-gameplay.bmp` просмотрен: HUD находится внутри 960×540. |
| `Final scene failed to reach its authored camera` | Завершённая campaign map теперь имеет completion exit. Retained cinematic проверяется на временной standalone-копии без links, с прежними ending/camera assertions. Кампания проверяется отдельно штатным `campaign_route`. Исходные карты не менялись. |
| `Menu weapon input leaked into gameplay` | Старое ожидаемое имя «Точное оружие» заменено текущим «Пистолет». Проверки pause/text/death/editor input и cooldown сохранены. Viewmodel count сверяется с подготовленными текущими частями оружия, а не отсутствием pistol viewmodel. |

Первый fresh run: device **passed 4,57 с**, G-019 SDR **passed 40,86 с**,
HDR **passed 40,36 с**; G-009 остановился на устаревшем light-count assertion,
**7,06 с**. Это время отказавшего G-009 не является baseline полного теста.
После включения stage measurements G-019 SDR/HDR прошли за **40,25/40,22 с**.
До изменений loader сняты hashes **86 BMP** (43 SDR + 43 HDR); после одной
инструментации все 86 совпали. Отдельные dated runtime logs/архивы не создавались.

### Границы измерений и локальные изменения

G-019 печатает steady-clock wall time последовательных фаз startup/renderer,
level session, world CPU preparation, scene assets, GPU resource preparation,
environment views, gameplay views/checks. Startup включает SDL/log/config/device/UI;
GPU preparation включает host work и waits, а не измерение GPU time.
Вложенные counters `render_or_readback_ms` и `bmp_ms` относятся к capture helper:
первый может включать lazy scene rendering, второй — SDL surface/BMP write.
Их нельзя добавлять к уже включающим их view phases. G-009 отдельно печатает
capture render/readback/RGB conversion, brightness reduction и BMP wall times.
Simulation по-прежнему продвигается программно; реальные ожидания не добавлялись.

Два исходных Debug G-019 дали world CPU **10,59/10,53 с**, scene assets
**24,66/24,70 с**, GPU resource preparation **0,56/0,58 с**; render/readback всех
43 captures **1,64/1,67 с**, BMP **0,31/0,32 с**. Следовательно, причина основной
задержки находится в CPU preparation, а не в сохранении captures или pipeline cache.

Первый локальный эксперимент заменил per-texel GLM normal length на эквивалентную
byte-domain проверку; Debug G-019 SDR/HDR **passed 19,07/18,85 с**. Все **86 BMP**
совпали с baseline по SHA-256. Native/model/meshlet/probe unit checks **15/15 passed**.
Второй шаг убирает вызовы доступа к vector и success-only validation function
из byte loops, а также per-pixel GLM temporaries при усреднении reflectance.
Порядок float additions по каждому каналу сохраняется: bake signatures не должны
измениться. Проверяются все texels; schema, данные, stale checks и лимиты сохранены.

Оптимизированный baseline до loader patch снят отдельно в `msvc-dev`, validation=false,
1280×720, VSync request=false, hardware_srgb, SDR: три последовательных процесса,
`build/containment-before-dev{,-2,-3}.log`. Первый использовал несовместимый shader
cache после Debug, следующие два — warm cache. OS/driver caches не очищались.
Это отдельное сравнение внутри optimized configuration, не способ выдать выключение
validation за ускорение Debug тестов.

### Финальный результат

Оставшаяся работа этого ticket выполнена. Старые Vulkan VUID и RmlUi asset error
не воспроизведены в свежих выбранных сценариях; shader/layout fixes без воспроизведения
не вносились. Исправлены устаревшие fixtures/assertions G-009, а не runtime input,
HUD или campaign semantics. Для text input fixture явно выставляет оба ImGui-флага
владения keyboard/text, как требует текущий input contract.

Дополнительно к byte loops:

- Bake подготавливает неизменные рёбра треугольников один раз, отбрасывает misses
  до оставшихся barycentric/distance вычислений. Исходный порядок и строгое `<`
  сохраняют первый hit при равной дистанции; это защищено отдельным unit-тестом.
- G-009 использует штатный `SDL_ConvertPixels` для RGBA32 → RGB24 вместо
  per-pixel `std::copy_n`. Rendering, readback, brightness assertions и BMP сохранены.
- Native-lab fixture по-прежнему реально rebake-ит 48 probes × **64 rays** после
  изменения материала. Authored продукт остаётся 48 × **1024 rays**; ни samples,
  ни геометрия, ни validation ради ускорения не сокращались.

Debug / validation=true, RTX 4060 Ti, прежние staged assets/settings, 1280×720,
hardware_srgb; viewport subtests сохраняют прежние размеры. Процессы запускались
последовательно, без параллельной сборки/GPU workload. Первый fresh запуск и повторы
не считаются cold-cache: OS/driver caches не очищались, pipeline cache не менялся.

| Исправный сценарий | До, с | После, с |
| --- | --- | --- |
| G-019 SDR, Debug | 40,86; 40,25 | 14,65; 14,84 |
| G-019 HDR, Debug | 40,36; 40,22 | 14,61; 15,12 |
| G-009 SDR, Debug | 105,62 (один полный baseline) | 62,14; 63,19 |
| G-009 HDR, Debug | Полного исправного baseline не было | 62,38 |

G-009 baseline 105,62 с уже содержит исправленные fixtures и loader byte loops:
это сравнение до/после SDL conversion и visibility preparation, а не с неработавшим
исходным executable. Прежние 7,06 с на раннем assertion не участвуют в сравнении.

В двух итоговых SDR G-019 world CPU занимает **3,05/3,02 с**, scene assets
**6,32/6,56 с** против исходных **10,59/10,53** и **24,66/24,70 с**.
G-009 native probe rebake (включая подготовку bake scene внутри измеряемого вызова)
занимает **26,65/26,70 с**; прежний log gap этого участка был около 57 с,
но отдельного baseline timer там не было. Сумма распознанных capture
render/readback/RGB intervals: **12,51 → 4,16 с** в первом сравнении.
stdout может перемежать строки capture timing и logger: baseline/первый итог содержат
113 распознанных записей, повтор — 114 (113 уникальных имён, `native-lab.bmp` дважды).
Поэтому эта сумма — диагностическая оценка, а не точный total всех capture calls.
Ни один вызов capture не удалён.

Отдельное optimized сравнение G-019 в `msvc-dev`, validation=false, те же SDR настройки:

| Сумма последовательных фаз, мс | До | После |
| --- | --- | --- |
| Первый процесс, incompatible pipeline cache после Debug | 4482,71 | 3812,93 |
| Warm повтор 2 | 4285,87 | 3689,28 |
| Warm повтор 3 | 4445,85 | 3668,60 |

Уменьшение warm phase sum — около **14–17%**. World CPU стал **519–521 мс**
вместо **680–684 мс**, scene assets **1419–1422 мс** вместо **1925–2001 мс**.
Полное время трёх новых процессов, включая завершение: **4,37/4,10/4,10 с**;
эквивалентного process stopwatch baseline нет, поэтому эти числа не смешиваются
с phase sums. В каждом процессе 43 captures. Это время автоматического теста,
не FPS, frame-time distribution или оценка ручного прохождения. Пример бюджета
3 с из запроса не достигнут и не является доказанным требованием этого workload.
Логи: `build/containment-{before,after}-dev{,-2,-3}.log`.

### Сохранность результата и проверки

- Все **86/86** baseline BMP G-019 (43 SDR + 43 HDR) совпали по SHA-256 после
  финальной сборки и повторного Debug run. Все **112/112** имеющихся baseline BMP
  G-009 совпали после SDR повтора; это объём сохранённого manifest, не заявление
  о byte comparison дополнительных кадров, отсутствующих в baseline manifest.
  `small-gameplay.bmp` повторно просмотрен: RmlUi HUD находится внутри 960×540.
- Полный authored `native_lab` заново baked оптимизированным инструментом на тех же
  Debug staged inputs: **48 probes, 13796 triangles, 3 static lights, 1024 rays**,
  signature **14886341886457994988**. Входной output path ранее отсутствовал, это
  именно `baked`, не `reused`. SHA-256 всего `.rcprobe` совпал с прежним продуктом:
  `632562EFA4794C7B4E901896D59E7B0D578A7D0148B22345E63D6407F5F8BC6D`.
  Команда: `$env:RELAYCORE_ASSET_ROOT='E:/Code/C++/RelayCore/build/msvc-debug/src/relaygame/assets'; & ./build/msvc-dev/src/relaygame/relaycore_irradiance_bake.exe assets/levels/native_lab.toml assets/config/render_authoring.toml build/native-lab-visibility-dev-after-2026-10-15.rcprobe`.
  Она завершилась за 15,62 с по session timestamps; дублирующий Debug bake остановлен
  без результата и не учитывается как successful check или timing baseline.
- Debug build: `cmake --build --preset graphics-debug --target relaycore_power_restoration_graphics relaycore_containment_loop_graphics relaycore_tests relaycore_vulkan_mesh_tests relaycore_game_lifecycle relaycore_irradiance_bake -j2`.
  Dev build: `cmake --build --preset demo-dev --target relaycore_demo relaycore_containment_loop_graphics relaycore_power_restoration_graphics -j2`.
  MSVC developer environment импортирован через `scripts/release-functions.ps1`.
- `ctest --preset test-debug -R '^relaycore\.(native|meshlet|irradiance)' --output-on-failure --no-tests=error --output-log build/vulkan-diagnostics-units-final-2026-10-15.log`:
  **20/20 passed**, 3,12 с. Включает normal-map boundary/last-texel, opacity,
  meshlets, deterministic/stale/malformed probes, blockers и equal-distance hits.
- `ctest --preset graphics-debug -R '^graphics\.(power_restoration|hdr_episode|containment_loop|containment_loop_hdr|resource_preparation|irradiance_probes|material_maps_reflections|game_lifecycle|campaign_route|vulkan_device)$' --output-on-failure --no-tests=error --output-log build/vulkan-diagnostics-final-2026-10-15.log`:
  **10/10 passed**, без skip, 227,44 с. Device 4,47 с; resource preparation 4,97 с;
  probes 2,45 с; maps/reflections 1,45 с; lifecycle 18,93 с; campaign route 41,34 с.
- `ctest --preset graphics-debug -R '^graphics\.(power_restoration|containment_loop|containment_loop_hdr)$' --output-on-failure --no-tests=error --output-log build/vulkan-diagnostics-repeat-2026-10-15.log`:
  **3/3 passed**, без skip, 93,19 с. Parsed phase evidence:
  `build/vulkan-diagnostics-{final,repeat}-details-2026-10-15.json`.
- В обеих финальных graphics сериях нет Vulkan validation warning/error;
  строгий CTest gate не менялся. `[error]` для намеренно испорченной config/model
  и stale probes, а также предупреждения negative lifecycle fixtures ожидаемы:
  сценарии проверяют отказ, сохранение рабочей сцены и recovery. Это не новые
  Vulkan/runtime failures. Вывод ограничен выполненными сценариями.
- `clang-format --dry-run --Werror` для семи изменённых C++ файлов и
  `git diff --check` прошли. Исходные maps/assets, schemas, cache policy,
  CMake/CTest registration и logger configuration не менялись. ZIP и push не нужны.

## План выполнения

1. **Согласовать и сохранить evidence.** Зафиксировать revision, рабочий diff,
   executable, staged assets, настройки и состояние validation для каждого запуска.
   Узкая выборка завершена; её полный вывод сохранён в
   `build/vulkan-warning-inventory-2026-10-15.log`. Она использует существующие
   executable, поэтому учитывать ограничение старых binaries выше.
2. **Подготовить полную диагностику.** Для ручного прохода сохранять файловый лог
   и stdout/stderr. Проверить, как надёжно включить validation в отдельном
   диагностическом запуске. При изменении логгера сохранить сообщения до его
   инициализации/после shutdown, noexcept callback и отсутствие двойной печати.
   Проверить необходимость записи VUID, map/session и контекста перехода;
   добавлять только недостающие данные. Не терять ошибки ради сокращения логов.
   Конкретный объём улучшений по уже прочитанному логу задан в разделе выше.
3. **Ручной проход пользователя — выполнен, лог прочитан.** Повторять первый
   проход сейчас не требуется. При необходимости отдельно собрать validation
   evidence; обычный `RelayCore — Play` его не заменяет. После изменения диагностики
   сначала провести узкую проверку, затем решать, требуется ли полный повтор.
4. **Составить реестр сообщений.** Для каждого уникального VUID/сообщения указать
   точный текст, источник/время, карту/действие, частоту, серьёзность, воспроизведение
   и статус. Разделить Vulkan, UI/assets, ограничения sandbox и старые сборки.
   Полнота относится к выполненным сценариям, не ко всем возможным состояниям игры.
   Сначала исправлять подтверждённые ошибки; OBS уже обработан отдельно.
5. **Измерить длительность исправных тестов.** На одинаковых настройках отдельно
   измерить startup/device, shader/pipeline creation, загрузку ресурсов, simulation,
   rendering/readback и запись captures. Разделить первый запуск и повторы;
   записать несколько повторов и состояние cache. Не запускать GPU workload параллельно.
   Для деталей CPU/GPU использовать текущий Tracy workflow из DEVELOPMENT.
6. **Ускорить подтверждённые лишние операции.** По измерениям проверить повторную
   подготовку ресурсов/pipelines, лишние captures и дисковую работу. Сохранять
   сценарии, assertions, validation, настройки качества и проверяемые кадры.
   Сокращение coverage или отключение validation не считать эквивалентным ускорением.
   Проверять инвалидацию cache при изменении shaders/assets/driver, если меняется cache.
7. **Проверить и передать результат.** Сравнить ошибки, assertions/captures и время
   до/после при одинаковых условиях. Выполнить узкие build/tests и diff check;
   коммитить только task-owned изменения. ZIP, release и push не нужны.

## Что делать пользователю сейчас

Первый ручной проход и оба захода улучшений диагностики завершены;
[проверки захода 2](#заход-2--реализация-и-проверки-2026-10-15) выполнены агентом.
Дополнительных запусков пользователя сейчас не требуется. Свежая проверка прежних
сообщений, восстановление G-009 и измерение/ускорение G-009/G-019 завершены;
результаты и границы проверки приведены в разделе продолжения выше.
Для будущего прохода файл остаётся тем же: путь печатается строкой `Latest log`,
стандартный каталог — `%APPDATA%\l1nuvv\RelayCore\logs`.
После нового прохода нужно закрыть игру и сообщить агенту до следующего запуска,
который перезапишет файл. Дополнительные validation/Tracy запуски подготавливает
агент для конкретных вопросов; они не подменяются обычным игровым логом.
