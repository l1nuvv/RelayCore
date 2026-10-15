# История playable slice — 2026-09–2026-10

Датированные исходные требования, оценки и журнал. Статусы внутри записей относятся к тому запуску, а действующие статусы находятся в [roadmap](../PLAYABLE_SLICE_ROADMAP.md). Карточки задач вынесены отдельно; workflow задаёт [AGENTS](../../AGENTS.MD).

## 2. Исходное состояние и доказательства

RelayCore — играбельный прототип небольшого FPS-эпизода со специализированным runtime и встроенным редактором. Игровой цикл уже существует. Следующая цель — качество самостоятельного прохождения, а не создание базовых engine-систем заново.

### Подтверждённая основа

Таблица фиксирует исходный baseline 2026-09-27. Текущий runtime использует SDL3 и прямой Vulkan 1.4 (обязательный минимум с 2026-10-12); актуальное владение описано в `ARCHITECTURE.md`.

| Слой | Что уже есть |
| --- | --- |
| Gameplay | Grounded player, ходьба/бег/прыжок, камера, фонарь, interaction, предметы, двери, задания, hitscan-оружие, турели, rover, hazards, здоровье, смерть/respawn, финал, save/continue, переходы карт |
| Runtime | C++20, SDL3, OpenGL 4.6 Core, glad, GLM, явный lifecycle, fixed simulation 120 Hz, AABB collision, TOML loading/validation, prepared level session, model cache, miniaudio |
| Presentation | Материалы/атлас, OBJ/BMP props, local lights, directional/spot/point shadows, fog, прозрачность, эффекты оборудования, HUD/меню через RmlUi, developer ImGui, шаги и spatial ambience |
| Tooling | Редактор, undo/redo, каталог моделей, размещение по поверхности, связи объектов, validation, preview, inspector, live reload, F9/F10 snapshots |
| Infrastructure | CMake presets, Ninja/MSVC, vcpkg manifest, doctest, Debug/RelWithDebInfo/ASan workflows, CI, clang-tidy, Tracy, graphics tests, asset sync, private playtest ZIP script |

Основной контент — `assets/levels/power_restoration.toml`. `foundation_intake.toml` и `foundation_pump.toml` проверяют прохождение нескольких карт. `test_room.toml`, stress- и color-reference-карты не считать дополнительными законченными эпизодами.

### Выполненная проверка 2026-09-27

В Visual Studio developer environment с соответствующим существующему CMake cache `VCPKG_ROOT` выполнено:

```powershell
cmake --build --preset tests-debug --target relaycore_tests relaycore_content_validation relaycore_game_lifecycle relaycore_power_restoration_graphics relaycore_demo
ctest --test-dir build/msvc-debug -R '^(relaycore\.|validation\.|graphics\.(game_lifecycle|power_restoration)$)' --output-on-failure
git diff --check
```

Сборка успешна. Из 259 проверок прошли 257. Все выбранные content tests и обе graphics-проверки прошли. Два падения воспроизведены отдельно:

| Проверка | Фактическое расхождение |
| --- | --- |
| `editor flattened episode preserves the full mission and authored values` | `tests/level_editor_tests.cpp` ожидает 6 mission steps, текущая карта содержит 9; последующая последовательность взаимодействий также старая |
| `render authoring config loads shipped material and light contract` | `tests/render_authoring_config_tests.cpp` ожидает `local_light_gain = 1.28`, текущий `render_authoring.toml` содержит `2.0` |

Эти результаты — исторический baseline данного плана. Не считать его зелёным и не утверждать, что эти failures сохраняются после последующих правок, без проверки.

Просмотрены свежие кадры `graphics.power_restoration`: предохранитель выделен на столе, изменение света после питания видно; HUD сохраняет диагностическую подачу. Это ограниченная визуальная проверка, не полноценный playtest.

Не выполнены в исходном анализе: полный человеческий проход, измерение meaningful gameplay duration, запуск на другом ПК, clean build, полные RelWithDebInfo/ASan workflows, remote CI и актуальный длительный Tracy capture. Старые captures в `build/profiling` не характеризуют нынешний эпизод с новыми point shadows.

## 4. Игровое направление и границы v1

Игрок исследует аварийный технический сектор, восстанавливает его системы и получает доступ к выходу. Короткие боевые встречи поддерживают исследование.

Три опоры: пространственное ориентирование; заметное изменение мира; локальные угрозы с понятной обратной связью.

Текущая цепочка из девяти этапов:

1. Найти запасной предохранитель.
2. Изолировать повреждённую линию и открыть резервную комнату.
3. Подключить резервную шину.
4. Восстановить питание.
5. Промыть фильтрационную линию.
6. Запустить охлаждение.
7. Пройти служебным переходом.
8. Вернуться к шлюзу и открыть его.
9. Пройти в следующий сектор и завершить эпизод.

В текущей основной карте есть семь usable-объектов, три двери, один rover, две турели и опасный участок. Эти числа описывают исходный снимок, а не обязательный навсегда content contract.

### RelayCore Vertical Slice v1

- Один законченный эпизод на основе `power_restoration`.
- 5–15 минут первого самостоятельного meaningful прохождения, измеренные на игроках.
- Понятное начало, цель, действия, три различающиеся встречи, обратный путь и финал.
- Одно оружие и два имеющихся типа противников.
- Ясные сигналы использования, блокировки, подготовки атаки, попадания, урона и уничтожения.
- Понятные смерть/retry, продолжение после перезапуска и начало нового эпизода.
- Согласованные материалы, ключевые звуки и игровой HUD.
- Standalone-архив запускается вне проекта на другом подходящем Windows-ПК.

Приёмка: три новых игрока пробуют один зафиксированный кандидат; минимум двое завершают без помощи разработчика; никто не встречает progression blocker. Отдельно проверяются смерть, продолжение и новый эпизод.

Если прохождение короче пяти минут, выяснить, каких решений и событий не хватает. Не увеличивать коридоры, не уменьшать скорость и не добавлять одинаковые кнопки только ради таймера. Если дольше пятнадцати — отделить содержательную игру от потерянности и повторного пробега.

## 5. Milestones и зависимости

| Этап | Цель / результат для игрока | Задачи | Зависимости | Критерий завершения | Scope / главный риск |
| --- | --- | --- | --- | --- | --- |
| M0 — достоверная база | Текущий эпизод проверен; известны реальные проблемы | H-001, G-001 | Нет | Зелёные unit/content tests и протокол самостоятельного прохода | S–M; принять автотест за игровую приёмку |
| M1 — понятные действия | Ясное начало, устройства, выстрел и угрозы | G-002, E-001, G-003 | M0 | Игрок начинает маршрут без объяснений и понимает причины урона | M; полировать неверный flow |
| M2 — законченный маршрут | Три разные встречи и приемлемый retry | G-004, G-005 | M1 | Полный проход без тупиков; death/continue сохраняют согласованное состояние | M–L; расширять карту вместо решений |
| M3 — кандидат slice | Согласованная презентация и самостоятельный запуск | G-006, H-002, H-003 | M2 | Выполнена приёмка v1, другой ПК, известен performance baseline | M; бесконечный polish |
| M4 — ограниченная консолидация | Устранены проявившиеся проблемы интерфейсов и рабочего процесса | Выбрать по результатам M1–M3 | Принятый автором результат | Короткие конкретные исправления либо явное решение ничего не менять | S; открыть новый фундаментальный roadmap |
| M5 — рабочий полигон | Встречу удобно запускать и повторять; известны ограничения текущего боя | E-002, G-007, G-008 | Текущий принятый эпизод | Три повторяемых сценария и записанные наблюдения автора | S–M; строить инструменты без реального неудобства |
| M6 — выбор в бою | Два оружия дают разные решения; угрозы требуют движения и укрытия | G-009; G-010 при доказанной потребности | G-008 | Игрок использует разные решения; новые состояния корректно повторяются и сохраняются | M; второй вариант оказывается просто сильнее первого |
| M7 — следующий эпизод | Проверенные механики работают в самостоятельном маршруте | G-011 | G-009; решение по G-010 | Проход без подсказок, понятные цели, три разные встречи | M–L; увеличивать длительность пустыми переходами |
| M8 — проверенный кандидат | Новый контент работает на целевом ПК и доставляется существующим процессом | H-008, H-009 | G-011 | Актуальный профиль, проверки кандидата, ручная приёмка | M; принять scripted workload за полный проход |

```text
H-001 → G-001 → G-002
              ├→ E-001 → G-003 → G-004 → G-005
              └→ наблюдения о presentation ─────┐
G-002 + G-003 + G-004 + G-005 → G-006 → H-002 → H-003
                                              ↓
                                      приёмка v1 → M4
```

E-001 ускоряет настройку, но его задержка не запрещает независимую правку cues или карты. Не превращать удобство инструмента в искусственный блокер gameplay.

Следующий цикл: `G-007 + G-008 → G-009 → решение по G-010 → G-011 → H-008 → H-009`. G-001 остаётся открытым измерением прежнего эпизода; его можно выполнить независимо. G-010 допустимо отложить и перейти к G-011 с текущими угрозами.

**Отложенное направление по запросу автора 29 сентября:** заменить вручную собранную видимую геометрию стен, полов, врагов, оружия и предметов на OBJ-модели. Выполнять позднее отдельным content-этапом; объём, размещение и коллизии уточнить при его открытии. Сейчас подключён только дробовик.

## 13. Журнал выполнения и точка продолжения

**Точка продолжения 2026-10-04:** продуктовый трек — независимый playtest G-011 / PROJECT_PLAN M4, ранее отложенный автором; инженерный трек — E-012/N4.3 (native collision product) после выполненных E-010/E-011, затем collision/parity/tooling slices и G-012. N1–N3 выполнены как E-005–E-009. `pressure_transfer.toml` технически проходим и доступен в меню; человеческая приёмка, полный профиль H-008 и candidate H-009 остаются открытыми. Нужен новый игрок без подсказок, с длительностями, смертями, выбором оружия и тремя главными затруднениями. G-009 принята, G-010 отложена; измерение G-001 отдельно. Наличие плана само по себе не запускает код или упаковку.

| Дата | Задача | Статус / результат | Evidence / ограничения |
| --- | --- | --- | --- |
| 2026-09-27 | Создание плана | План записан; implementation tickets не выполнялись | Исходный audit/build/test baseline приведён в разделе 2 |
| 2026-09-27 | H-001 | done — исправлены `tests/level_editor_tests.cpp` и `tests/render_authoring_config_tests.cpp`. Roundtrip сравнивает все поля mission definition с исходной картой, сохраняет проверки геометрии/entity properties, проверяет начальные блокировки, актуальную цепочку устройств и открытие дверей. Shipped lighting contract принимает authored `local_light_gain = 2.0`. | Оба исходных падения воспроизведены. В VS developer environment с `VCPKG_ROOT` из CMake cache: `cmake --build --preset tests-debug --target relaycore_tests`, затем `ctest --preset test-debug -R 'editor flattened episode\|render authoring config loads shipped' --output-on-failure` — 2/2; `cmake --build --preset tests-debug` и `cmake --build --preset validation-debug` успешны; `ctest --preset test-debug --output-on-failure` — 240/240; `ctest --preset validation-debug --output-on-failure` — 17/17; `git diff --check` — без ошибок. MSVC-сборка выполнена вне песочницы после зависания внутри неё. Graphics/manual playtest не выполнялись и для test-only H-001 не требуются. |
| 2026-09-27 | G-001 | awaiting_playtest — зафиксирован кандидат `RelayCore-playtest-20260927-225948-864.zip`; из `docs/PLAYTEST_GUIDE.txt` убрана последовательность решения; подготовлен `docs/playtests/G-001-2026-09-27.md` для наблюдателя. | `cmake --build --preset demo-relwithdebinfo` и `scripts/package-playtest.ps1` успешны; SHA256 всех 85 файлов совпадают с исходными ресурсами/сборкой и ZIP; `git diff --check` без ошибок. Source state и контрольные суммы сохранены рядом с архивом. Нужен новый игрок: первый проход без подсказок, quit/continue в середине, девять временных отметок и три проблемы; быстрый повтор измерить отдельно. Новый runtime/manual проход не выполнялся. |
| 2026-09-27 | E-001 | done — `demo_game*`, `game_world_runtime.*`, debug iteration panel и `tests/game_lifecycle_tests.cpp`: полный локальный снимок встречи; подготовка restore до применения; изоляция от preview и записи прогресса. | В VS developer environment: `cmake --build --preset tests-debug`, `cmake --build --preset validation-debug`, `cmake --build --preset graphics-debug --target relaycore_game_lifecycle relaycore_power_restoration_graphics`, `cmake --build --preset demo-debug` успешны. `ctest --preset test-debug --output-on-failure` — 240/240; `ctest --preset validation-debug --output-on-failure` — 17/17; `ctest --preset graphics-debug -R '^graphics\.(game_lifecycle\|power_restoration)$' --output-on-failure` — 2/2; `git diff --check` без ошибок. Lifecycle проверяет возврат player/combat/rover/door/mission, повторное развитие Windup, presentation state, отказ несовместимой карты/камеры без частичного применения, F9/F10 и побайтовую неизменность `progress.toml`, preview. Ручной плейтест и ASan/RelWithDebInfo для этой правки не выполнялись. |
| 2026-09-27 | H-003 (ранняя часть) | in_progress — актуализированы `README.md`, `STATUS.md`, `docs/ARCHITECTURE.md`, исторические FOUNDATION/MASTER_REFACTOR/ROADMAP/EPISODE и `docs/PLAYTEST_GUIDE.txt`. Единая очередь, save format 3, F9/F10, private ZIP и правило возрождения описаны по текущему коду. | Сверены source contracts, CMake presets и скрипт упаковки; проверены локальные файловые ссылки; снята ссылка на отсутствующий исторический migration script; `git diff --check` без ошибок. Изменения только документационные, сборка и тесты повторно не запускались. Итоговое согласование H-003 ожидает G-001–G-006/H-002; архив G-001 не изменён. |
| 2026-09-27 | G-001 — отчёт игрока | awaiting_playtest — сестра автора достигла финала с подсказками за примерно 90–120 секунд. Девять длительностей этапов суммарно 68 секунд; три приоритетных затруднения внесены в протокол. | Источник — сообщение автора и ответы на уточнения. Первоначально quit/continue не проверяли; после просьбы выполнить проверку автор сообщил «все ок». Сборка, настройки, технические паузы, урон/смерти, восприятие смены состояния мира и быстрый повтор не уточнены. Самостоятельный проход и цель 5–15 минут не подтверждены. Турель в фильтрационной обойдена без уничтожения — само по себе не дефект. |
| 2026-09-27 | G-001 — второй отзыв | awaiting_playtest — другой участник прошёл за 3:26; записаны непонятные формулировки HUD, нераспознанный зелёный ориентир, потерянность после насосной и круг после выходной панели. Приоритеты G-002 уточнены. | Источник — сообщение автора. Время сохранено отдельно от первого прохода. Сборка и наличие помощи не уточнены. Текущая карта говорит о «зелёной трубе»; восприятие как «провода» не заменяет исходный текст. Сбой mission progression не подтверждён. |
| 2026-09-28 | G-002 | awaiting_playtest — упрощены цели и сообщения устройств, добавлены корректные locked_message, усилена стрелка перехода и заменён номер над выходной панелью на стрелку. Зона возврата принимает короткий и прежний путь. | Новый регрессионный тест сначала воспроизвёл зависание на этапе 7, затем прошёл. validation-debug — 18/18; graphics.power_restoration и graphics.game_lifecycle — по 1/1, просмотрены кадры у охлаждения и выхода; editor flattened episode — 1/1; сборки validation-debug, relaycore_power_restoration_graphics и demo-relwithdebinfo успешны. Графический тест учитывает тип выбранного объекта: расширенная mission zone выбирается вместо solid. git diff --check без ошибок. Новый ZIP 20260928-001031-661: все 85 файлов совпали по SHA256 с исходной сборкой и архивом. Для завершения нужен новый игрок без подсказок; критерии в протоколе. |
| 2026-09-28 | G-002 — приёмка | done — автор сообщил «все ок все лучше» после передачи исправленной сборки. | Качественное улучшение принято автором; технические проверки приведены выше. Новый игрок, отсутствие подсказок и время повторного прохода отдельно не подтверждены. Новые тесты и сборки для записи отзыва не запускались. Следующая задача — G-003. |
| 2026-09-28 | G-003 | awaiting_playtest — семь боевых WAV, сигналы попадания/замаха/удара, индикатор турели, звук уничтожения робота, остановка старых звуков при restore/death. | Unit/audio 243/243, включая декодирование всех семи WAV; graphics 2/2, кадры просмотрены. Нужна оценка слышимости и читаемости человеком. |
| 2026-09-28 | G-004 | awaiting_playtest — воспроизведён и устранён сброс windup/recovery попаданиями; проверены альтернативы в трёх встречах. | Content 19/19, unit по cadence/дистанции/промаху/уничтожению/restore. Урон, темп и карта сохранены. Игровой интерес и сложность человеком не оценены. |
| 2026-09-28 | G-005 | awaiting_playtest — мягкое возрождение объяснено; исправлена потеря последнего состояния при выходе во время смерти. | Повторные смерти до/после питания/охлаждения, disk continue, сохранение устройств и повреждений врагов; отдельный lifecycle воспроизвёл прежний дефект и прошёл после исправления. Формат save 3 сохранён. Новый ZIP 20260928-003337-894, 92 файла сверены; команды, SHA256 и ограничения в протоколе блока. Требуется приёмка правила игроком. |
| 2026-09-28 | G-003 — отзыв о выстреле и турели | awaiting_playtest — переработан звук игрока без тонального гула; предупреждение закреплено на датчике, выстрел показан мгновенным следом на 80 мс. | Устранено визуальное сходство предупреждения со снарядом. Целевые combat/audio/content 6/6, graphics.power_restoration 1/1, кадры on/off/shot просмотрены, WAV без clipping. Новый кандидат 20260928-004331-918; 92 файла проверены по SHA256. Повторная субъективная оценка звука остаётся открытой; подробности в протоколе блока. |
| 2026-09-28 | G-003 — повторный отзыв | Автор сообщил «уже лучше» о выстреле/подготовке турели; остальные критерии приёмки не уточнены. | Новых проверок только ради записи отзыва не запускали. |
| 2026-09-28 | G-006 / H-002 | awaiting_playtest — компактный HUD, сообщения без перекрашивания кадра, equipment audio/pause, проверяемый ZIP и локальный графический Tracy baseline. | RelWithDebInfo unit/audio 243/243, content 19/19; Debug graphics 2/2; внешние graphics/lifecycle/audio/content на распакованных ресурсах и standalone exit 0. 93 файла ZIP сверены. Нужны другой ПК, независимые игроки, оценка микса и полный игровой профиль. Детали — в [протоколе](../playtests/G-006-H-002-2026-09-28.md). |
| 2026-09-28 | H-003 | in_progress — README, STATUS, ARCHITECTURE и DEVELOPMENT согласованы с локальным кандидатом M3 и его ограничениями. | Указаны manifest/хеши, новый HUD и текущий профиль; исторический профиль явно отделён. Следующий шаг — приёмка, v1 не объявлен завершённым. |
| 2026-09-28 | H-002 — продолжение | awaiting_playtest — ASan выявил и подтвердил fix отсутствующего WAV; собран интегрированный Tracy baseline с sim/render/present по подготовленным состояниям. | ASan unit 243/243, content 19/19, graphics 2/2; release audio 1/1; scripted game workload 11/11 состояний, трасса записана. ZIP 012216-433: 93 файла сверены; запуск вне проекта exit 0. Другой ПК, независимые игроки и непрерывный проход отсутствуют; [протокол](../playtests/H-002-2026-09-28.md). |
| 2026-09-28 | H-002 — ноутбук | awaiting_playtest — автор подтвердил запуск на Vivobook Go 15 / Radeon 610M, но интерфейс обрезан. Добавлена адаптация oversized window к рабочей области дисплея. | Debug window/lifecycle/graphics 3/3; меню и HUD при 960×540; RelWithDebInfo demo собрана. ZIP 103650-973: 93 файла сверены; внешний запуск, client area внутри usable desktop, WM_CLOSE exit 0. Нужен повторный запуск исправленной сборки на ноутбуке; [протокол](../playtests/H-002-2026-09-28.md). |
| 2026-09-28 | G-003–G-006 — человеческая приёмка | done — автор сообщил: «человек прошел все ок» и подтвердил приёмку всего блока. | Ранее выполненные технические проверки приведены выше. Архив, ПК, длительность, число независимых первых прохождений и условия профилирования не уточнены; приёмка блока не закрывает отдельные критерии H-002. Новые сборки и тесты для записи отзыва не запускались. |
| 2026-09-28 | H-002/H-003 — итоговое согласование | done — автор сообщил «все нормально, все согласую» после разъяснения оставшихся критериев. | Кандидат и документация приняты автором с ранее зафиксированными проверками и ограничениями. Новые измерения и непрерывный Tracy capture не предоставлены; дополнительные проверки не заявлены выполненными. Обновлены текущие статусы и README/STATUS; исторические записи сохранены. |
| 2026-09-28 | E-002 / G-007 | E-002 done; G-007 awaiting_playtest — отдельный большой полигон и запуск CLion, изолированный progress, headless JSON-проверка карты. | Debug build, content 22/22, builder 5/5, graphics/lifecycle 2/2, CLI fixtures, clang-format и diff-check; пять видов просмотрены. Нужна ручная оценка масштаба и управления автором. Версия, публикация и Git-индекс не менялись. |
| 2026-09-28 | План следующего цикла | Добавлены M5–M8 и G-008–G-011/H-008/H-009; G-008 ready, остальные pending. | Сверены текущий combat/enemy runtime, ближайшие tests, snapshots/save loader, Sandbox и CMake. Изменён только roadmap; новая сборка, runtime-проверки и реализация tickets не выполнялись. Второе оружие — рекомендуемая гипотеза; новый противник условный. |
| 2026-09-28 | G-007 — приёмка | done — автор сообщил «полигон я проверил все ок». | Принят существующий полигон; точные длительности и число повторов не предоставлены. Новую встречу G-008 этим отзывом не считать принятой. |
| 2026-09-28 | G-008 | awaiting_playtest — три сценария, смешанная встреча с дополнительной турелью, стоячее укрытие и стартовые полосы. | Debug build, content 25/25, graphics.power_restoration 1/1, CLI fixtures; три новых GL-вида просмотрены. Существующий encounter snapshot проверен тремя автоматическими повторами. Ручные наблюдения и роль второго оружия ещё нужны; [протокол](../playtests/G-008-2026-09-28.md). Runtime, версия и публикация не менялись. |
| 2026-09-28 | G-008 — приёмка | done — автор сообщил «все ок» и попросил следующий большой блок. | Точные длительности, число ручных повторов и отдельные наблюдения не предоставлены. |
| 2026-09-28 | G-009 | awaiting_playtest — второе оружие, подбор, выбор 1/2, звук/прицел/отдача и сохранение. | Debug/ASan unit/audio 249/249, content 26/26, graphics/lifecycle 2/2, CLI fixtures; два HUD-кадра просмотрены. Формат записи 4, чтение 1/2/3 сохранено. [Протокол](../playtests/G-009-2026-09-28.md). Роль и баланс требуют игровой оценки; новый противник, новый эпизод и выпуск не выполнялись. |
| 2026-09-29 | PROJECT_PLAN M1 / G-009 | awaiting_playtest — техническая перепроверка завершена; автономный запуск остановлен на обязательной авторской приёмке. | Debug build успешен вне sandbox после зависания sandbox build; выбранные unit/audio 30/30, sandbox 7/7, graphics/lifecycle 2/2; CLI valid без warnings; свежие idle/fire/pump кадры просмотрены. Runtime/assets не менялись. Нужна оценка автором модели, звука, отдачи и выбора оружия на 3/7/12 м, против rover и в смешанном бою. M2–M6 не начаты из-за зависимостей; [точный blocker и команды](../PROJECT_PLAN.md), [протокол](../playtests/G-009-2026-09-28.md). |

| 2026-09-29 | G-009 / PROJECT_PLAN M1 | done — автор принял внешний вид/feedback, звук, отдачу, помпу, переключение и боевые роли. | 3/7/12 м, rover и смешанный бой пройдены; 3 м — два выстрела, 7/12 м — пять. Детали цели/прицеливания не уточнены; параметры не менялись. M1 human acceptance явно passed; [отзыв](../playtests/G-009-2026-09-28.md). |
| 2026-09-29 | G-010 / PROJECT_PLAN M2 | deferred — существующие угрозы уже дают разные роли двух оружий; недостающее поведение не подтверждено. | Нового AI нет. Вернуться, если новый эпизод выявит недостающее решение по позиции/оружию, которое нельзя дать существующими угрозами и геометрией. M2 done записанным решением. |
| 2026-09-29 | G-011 / PROJECT_PLAN M3–M4 | awaiting_playtest — M3 done, M4 ждёт первого прохождения новым игроком. | Новая карта: семь задач, две турели/два rover, pickup, перепуск, выход/финал; меню и captions. Debug unit/audio 250/250, content 31/31, graphics/lifecycle 2/2, CLI valid/0 warnings, formatting/diff-check. Три scripted прохода, editor fingerprint, saves/retry и SDL E; кадры просмотрены. Длительность и игровая приёмка открыты; [протокол](../playtests/G-011-2026-09-29.md). M5/M6 не начаты. |

| 2026-10-01 | Repository cleanup / H-003 | done — удалены семь исторических планов/аудитов и дублирующий status, два устаревших локальных patch-файла; README сокращён, навигация документов и redundant ignore entries согласованы. | 57 локальных Markdown-ссылок без ошибок; все собственные C/C++ translation units названы в CMake; Python AST, JSON и 12 PowerShell-скриптов разобраны; `cmake --list-presets` и `git diff --check` успешны. Runtime, build/CI и генераторы ассетов не менялись; сборка и runtime-тесты не запускались. `new.patch` оставлен: полная применённость не подтверждена. |

| 2026-10-01 | CI / updater readiness review | Исправлены подтверждённые CI/lifecycle проблемы; PowerShell-регрессии включены в CTest, README и testing contract актуализированы. | Debug unit/audio/tooling 254/254, content 31/31, graphics 16/16 без skip; loader/layers probe, YAML/shell/JUnit gate checks, formatting/links/diff-check прошли. Remote graphics job и runner ещё не проверены; VK-009 не начинался. Решения по всем предложениям и точные команды — в `VULKAN_MIGRATION_PLAN.md`, Infrastructure review. |
| 2026-10-01 | VK-009 | done — Vulkan ambient/bounce, directional и текущие nearest-eight local lights, с прежней подготовкой world data. | Debug graphics 16/16, selected checks 36/36, content 31/31; 74 GL/Vulkan пары без различий coverage и без RGB delta >8. Optimized mesh прошёл; ASan результат и границы — в `VULKAN_MIGRATION_PLAN.md`, VK-009. Основная игра остаётся на GL; автор поручил сразу продолжить VK-010. |
| 2026-10-01 | VK-010 | done — directional cascades, spot и point shadows с текущими culling/bias/PCF и явной синхронизацией Vulkan. | Debug graphics 16/16, unit/audio/tooling 254/254, content 31/31; optimized mesh/presentation/lifecycle 3/3, ASan mesh 1/1. 122 GL/Vulkan пары, включая 48 shadow cases; validation чистая. RenderDoc подтвердил D24 storage, depth writes/reads, transitions и шесть активных point faces. Точные допуски/evidence/границы — в `VULKAN_MIGRATION_PLAN.md`, VK-010. Следующий gate — VK-011; основной runtime пока GL. |
| 2026-10-02 | VK-011 | done — color pipeline, fog/post, material/emissive parity; native Vulkan color baseline. | 158 GL/Vulkan пары, обе карты/цветовые modes; texture decode/filtering, clear и UI без повторного encoding. RenderDoc подтвердил SRGB scene / UNORM UI views и raw UNORM blit. Исправлен порядок декодирования/интенсивности индикатора панели; legacy сохранён. Evidence — `VULKAN_MIGRATION_PLAN.md`, VK-011. |
| 2026-10-02 | VK-012 | done — после восстановления desktop две presentation/recreation проверки прошли 2/2, Tracy presentation 1/1. | Debug graphics 15/17 (две остановились на `GetPixel == CLR_INVALID`), unit/audio/tooling 254/254, content 31/31; optimized 3/3, ASan 2/2, Debug+Tracy validation 3/3. GPU-AV не нашёл shader-access ошибок; Tracy capture: 480 рабочих кадров, 22 709 GPU events. Workflows и RenderDoc checklist добавлены в `DEVELOPMENT.md`; границы и итоговые проверки — в `VULKAN_MIGRATION_PLAN.md`, VK-012. Исходный GL runtime заменяется в VK-013. |

| 2026-10-02 | VK-013 | done — основной runtime, ImGui, reload, readback и presentation переведены на Vulkan; GL/glad/GLSL и OpenGL binding удалены. | Свежие configure/build, 297/297 основных проверок и desktop presentation/recreation 2/2; optimized 3/3, ASan 4/4, финальный reload/presentation lifecycle в обеих конфигурациях 1/1. 158 замороженных графических эталонов, чистая validation, package/SPIR-V hash checks и отказ при повреждённом shader. Точные команды, evidence и ограничения — `VULKAN_MIGRATION_PLAN.md`, VK-013. |

| 2026-10-02 | VK-013 — runtime feedback | Разобраны ~25 FPS и постоянное шипение: добавлен оптимизированный CLion запуск `RelayCore — Play`, loop пара сделан тише и менее высокочастотным. | Одинаковый scripted entrance 1280×720/VSync off: Debug p50 42,79 мс, RelWithDebInfo 1,63 мс; оба полных episode workloads прошли, Tracy GPU scene median около 0,28 мс. Это сравнение сборок, не ускорение renderer. Автор подтвердил источник шипения `steam.wav`; PCM16/mono/44100 Hz/6,4 с и бесшовность сохранены, RMS −9,38 дБ, другие loops идентичны. Native audio 1/1, обе demo-сборки и runtime asset hashes, Python/XML и diff-check прошли. Оценка нового звука на слух автором ещё не получена. |

| 2026-10-02 | VK-013 — авторская приёмка runtime | done — автор сообщил «все ок работает» и поручил продолжить план после исправления запуска и звука пара. | Принят текущий runtime; новых измерений или независимого прохода этим отзывом не заявлено. G-011/H-008 остаются в прежнем состоянии. |
| 2026-10-02 | VK-014 | done — read-only ревью ownership/error paths, границ API, кэшей и frame waits; обязательных архитектурных исправлений не подтверждено. | Целевые Debug graphics/lifecycle/failure checks 10/10 без skip/validation; optimized workload 11/11, свежая Tracy baseline без concurrent GPU tests, RenderDoc обеих карт и thumbnails. Entrance p50/p95 1,5409/1,9504 мс; GPU Scene median 0,276480 мс. Запланирована отдельная P3 VK-F01 по диагностическим строкам; runtime не менялся. Команды, артефакты и ограничения — `VULKAN_MIGRATION_PLAN.md`, раздел 28. |
| 2026-10-02 | VK-F01 / documentation | done — borrowed diagnostic strings, актуальный Vulkan contract, G-011 playtest guide и оптимизированный запуск; Sandbox использует runtime assets с SPIR-V. | Debug build и graphics 6/6, RelWithDebInfo build и graphics 5/5; без skip. ZIP 215444-058: 93 файла/хеши сверены, внешний standalone и Sandbox startup/WM_CLOSE exit 0; lifecycle и оба эпизода на внешних ресурсах прошли. Formatting/XML/local links/diff-check прошли. G-011 human measurements и другой Vulkan GPU остаются открытыми; H-008/H-009 pending. Команды и ограничения — `VULKAN_MIGRATION_PLAN.md`, раздел 29. |
| 2026-10-02 | E-003 | done — первый static GLB → native product slice по явной команде автора; blind playtest/другой GPU отложены. | Blender 5.2.2 export, два meshes/materials, deterministic Debug/optimized compile; units 260/260, content 31/31, graphics по 4/4, ASan 10/10, final editor checks 19/19. GPU cache/reload/allocation checks, native/Sandbox кадры и ZIP 94 файла проверены. Bounds fix проверен отдельно; G-011/H-008/H-009 не закрываются. |
| 2026-10-02 | E-004 | done — объявленные GLB автоматически компилируются и попадают в staged runtime assets; source/tool/rules/product hashes проверяются до упаковки. | Debug 261/261 unit/tooling/audio и 31/31 content, graphics Debug/optimized по 4/4, ASan 11/11; финальный tooling 1/1. No-op, material update, bad import rollback, registration/removal и stale product checks прошли. ZIP 230246-770: 94 файла/хеши; внешний startup, lifecycle/episode и Sandbox exit 0. Формат/материалы прежние, новых dependencies нет; G-011/H-008/H-009 не закрываются. |

| 2026-10-04 | Пересмотр PROJECT_PLAN / N4 | Добавлены E-010–E-014 и G-012 с зависимостями; E-010 ready, следующие pending, E-015 deferred. E-009 добавлена в единую таблицу как уже выполненная по карточке от 3 октября. | Внешний strategic analysis сопоставлен с текущими AABB/query callers, player spatial index, LevelSession, prop surface data, manifest, scene fence wait и evidence N1–N3. Изменены только план и roadmap: 42 локальные ссылки/якоря и восемь строк/карточек tickets проверены, diff-check прошёл. Новые build/run/tests/profiles, реализация Jolt и ZIP не выполнялись. G-011/H-008/H-009 сохраняют статусы. |
| 2026-10-04 | E-010 / N4.1 | done; E-011 ready | Общий AABB WorldQuery, prepared static/door/actor records и typed hit; перенесены combat/LOS/focus/player/rover consumers с сохранением movement math и spatial index. MSVC Debug build, focused unit 58/58 + rover regression 1/1, content 33/33, Vulkan lifecycle/episode 2/2, scoped format и diff-check прошли. Commands/evidence в карточке E-010; dependencies/formats не менялись, Jolt и ZIP не добавлялись. Human playtest, другой GPU и performance не проверялись. |
| 2026-10-04 | E-011 / N4.2 | done; E-012 ready | Изолированный Jolt static fixture с ray/box cast/overlap, layers, triangle surfaces и staged/active lifetime; pinned 5.6.0#1 SSE2 overlay, прежний baseline. Debug focused 65/65, content 33/33, Vulkan 2/2, ASan 6/6, optimized unity build и queries 6/6; CLI, scoped format и diff-check прошли. Карточка E-011 содержит commands/evidence/ограничения. Обычные игровые карты остаются AABB; G-011/H-008/H-009 открыты, ZIP не создавались. |
| 2026-10-07 | E-013 / N4.4 | done; E-014 ready | Opt-in native collision placements, прежний box movement и mesh sweeps, общий gameplay query path. Debug: 120 unit, 31 physics, 33 content passed; финальный native/lifecycle 4/4, probe reuse 1/1. ASan physics 31/31; evidence/ограничения в карточке. ZIP, human playtest и performance не выполнялись. |

| 2026-10-08 | G-016 | awaiting_playtest — HUD/турели, бой/атмосфера pressure_transfer и цепочка «Двух секторов» технически доведены | Debug content 47/47, affected graphics/lifecycle 5/5, optimized affected content 13/13; обе сборки и Check Map успешны. Игровые кадры просмотрены, два эталона явно обновлены. Повторный слепой проход обоих сценариев, слуховая оценка и авторская приёмка остаются открытыми; протокол и runtime hashes записаны. ZIP не создавался. |
| 2026-10-08 | H-008 — локальная часть D5 | in_progress — актуальный optimized baseline после G-016 | Target build и workload/capture exit 0; 12 421 CPU frames, восемь состояний, GPU/query/fence разбор, 0 игровых uploads/waits. [Evidence](../profiling/H-008-2026-10-08.md). Непрерывный проход, звук, FPS budget и слабый ПК остаются открытыми. |
| 2026-10-08 | D5 — mesh reload/pass regression | Выбранный synthetic fixture исправлен; D5 остаётся открыт | `graphics.mesh_pipeline` 1/1 с validation, три reload и 11 pass comparisons без color differences; frozen lighting inputs восстановлены из `d57f8d3`, samples/tolerances/runtime не менялись. Общий mesh test проходит этот блок и падает дальше на `power_restoration-0`; [команды, причина и границы](project-planning-2026-09-29--10-10.md#d5--приёмка-эпизода-и-measured-hardening). Отложенные человеческие/GPU проверки не закрыты. |
| 2026-10-08 | D5 — power world baseline | Четыре power comparisons исправлены; D5 остаётся открыт | Старые inputs проходят старые samples на текущем renderer; art-pass изменения установлены по map/config/atlas. Просмотрены и явно обновлены четыре power samples; финальный rebuilt test с validation даёт нулевые различия и останавливается дальше на `pressure_transfer-0`. [Causal run, hashes и команды](project-planning-2026-09-29--10-10.md#d5--приёмка-эпизода-и-measured-hardening). Допуски и runtime не менялись; непрерывный проход/звук/слабый ПК отложены. |

| 2026-10-09 | IMG-04–06 | awaiting_playtest — containment light balance, mapped-normal specular filtering and native emission implemented | Debug/Play builds; 20/20 native/import/tooling and 9/9 graphics checks, no skips/validation warnings; final authored HDR/SDR states and 48-frame camera arcs reviewed. 79 model receipts per build, byte-identical grip geometry/parts and fresh GLB import verified. Author acceptance during the containment route remains open; [evidence and limits](../art/img04-06-lighting-stability-emission.md). |

После каждой выполненной задачи добавлять короткую запись и обновлять единую таблицу статусов в разделе 6. Не копировать сюда полные логи. Для `awaiting_playtest` явно написать, какой человек/сценарий/ПК ещё нужен; для `done` перечислить реально выполненные проверки.

### E-003 — команды и артефакты

Это история проверки E-003. Ручная запись product в authored `assets` ниже
заменена build products и автоматическим staging в E-004; текущий workflow — `DEVELOPMENT.md`.

Build commands выполнены в Visual Studio developer shell, `VCPKG_ROOT` соответствует
существующему cache. Configure `msvc-debug`, `msvc-relwithdebinfo`, `msvc-asan`
успешен; baseline/tool versions не менялись. Первую compile error от попытки
копировать move-only fastgltf Material устранили выбором const-reference ветки.
Первый import обнаружил initializer-list constructor fastgltf math vector;
scalar constructors исправлены, последующие regression tests и ASan прошли.

```powershell
cmake --build --preset tests-debug --target relaycore_tests relaycore_content_validation relaycore_game_lifecycle relaycore_power_restoration_graphics relaycore_vulkan_mesh_tests relaycore_demo relaycore_sandbox relaycore_model_import --parallel 2
cmake --build --preset demo-relwithdebinfo --target relaycore_model_import relaycore_demo relaycore_sandbox relaycore_game_lifecycle relaycore_power_restoration_graphics relaycore_vulkan_mesh_tests --parallel 2
cmake --build --preset tests-asan --target relaycore_tests --parallel 2
build/msvc-debug/relaycore_model_import.exe content/models/service_cylinder.glb assets/models/service_cylinder.rcmodel
ctest --preset test-debug --output-on-failure --no-tests=error --output-junit model-assets-unit.xml
ctest --preset validation-debug --output-on-failure --no-tests=error --output-junit model-assets-content.xml
ctest --preset graphics-debug -R '^graphics\.(vulkan_mesh|vulkan_color_baseline|game_lifecycle|power_restoration)$' --output-on-failure --no-tests=error --output-junit model-assets-graphics-debug.xml
ctest --test-dir build/msvc-relwithdebinfo -R '^graphics\.(vulkan_mesh|vulkan_color_baseline|game_lifecycle|power_restoration)$' --output-on-failure --no-tests=error --output-junit model-assets-graphics-optimized.xml
ctest --preset test-debug -R '^relaycore\.(native|GLB|Blender|OBJ|Static props|shotgun body|editor)' --output-on-failure --no-tests=error --output-junit model-assets-editor-final.xml
ctest --preset test-asan -R '^relaycore\.(native|GLB|Blender|OBJ|Static props|shotgun body)' --output-on-failure --no-tests=error --output-junit model-assets-asan.xml
pwsh -NoProfile -File scripts/package-playtest.ps1 -Preset msvc-relwithdebinfo -PassThru
pwsh -NoProfile -File build/model-assets-standalone-check.ps1
git diff --check
```

Все результаты/logs: `build/model-assets-*.log`, JUnit — в соответствующих build
directories. Final editor graphics report `model-assets-editor-graphics-final.xml`
(2/2); новый map capture — `model-assets-map-final.xml` (1/1). Просмотрены
`build/msvc-debug/tests/vulkan-mesh-captures/native-cylinder-hardware_srgb.bmp`
и `build/msvc-debug/tests/power-captures/sandbox-native-model.bmp`.
ZIP `build/playtest/RelayCore-playtest-20261002-224440-223.zip`, SHA256
`319B72526F5435F5492581E4DB4B4C8A2735ADC51E627190A6FAEE1C5EBF458E`.
Это локальный technical candidate с manifest uncommitted state, version 1.3.4;
публикации и human playthrough не было. Проверен RTX 4060 Ti, driver 32.0.15.9649;
нового FPS comparison/полного ASan suite/другого GPU/remote CI не было.

### E-004 — команды и артефакты

Configure `msvc-debug`, `msvc-relwithdebinfo`, `msvc-asan` прошёл в VS developer shell
с прежним toolchain/VCPKG_ROOT. Изменение build dependency graph потребовало
перегенерации CMake; baseline/version/настройки renderer не менялись.

```powershell
cmake --build --preset demo-debug --target relaycore_model_assets relaycore_demo_assets relaycore_tests relaycore_content_validation relaycore_sandbox relaycore_vulkan_mesh_tests relaycore_game_lifecycle relaycore_power_restoration_graphics -j 2
cmake --build --preset demo-relwithdebinfo --target relaycore_demo relaycore_sandbox relaycore_vulkan_mesh_tests relaycore_game_lifecycle relaycore_power_restoration_graphics -j 2
cmake --build --preset tests-asan --target relaycore_tests -j 2
ctest --preset test-debug --output-on-failure --output-junit e004-unit.xml
ctest --preset validation-debug --output-on-failure --output-junit e004-content.xml
ctest --preset test-debug -R '^relaycore.tooling.model_assets$' --output-on-failure --output-junit e004-model-assets-final.xml
ctest --preset graphics-debug -R '^graphics\.(vulkan_mesh|vulkan_color_baseline|game_lifecycle|power_restoration)$' --output-on-failure --output-junit e004-graphics-debug.xml
ctest --test-dir build/msvc-relwithdebinfo -R '^graphics\.(vulkan_mesh|vulkan_color_baseline|game_lifecycle|power_restoration)$' --output-on-failure --output-junit e004-graphics-optimized.xml
ctest --preset test-asan -R '^relaycore\.(native|GLB|Blender|OBJ|Static props|shotgun body|tooling.model_assets)' --output-on-failure --output-junit e004-asan.xml
pwsh -NoProfile -File scripts/package-playtest.ps1 -PassThru
pwsh -NoProfile -File build/e004-standalone-check.ps1
git diff --check
```

Logs `build/e004-*.log`, JUnit в соответствующих build directories: failures/errors/skip
нет. Финальный tooling test повторён после добавления проверки второго declared model.
Native bytes совпадают с E-003: SHA256
`2F3D9990A4E939671B4ABD8FF2267C1B4FD68D0696E7D732E2C2FCBFC9C52CF4`.
Просмотрен `build/msvc-debug/tests/power-captures/sandbox-native-model.bmp`.
ZIP `build/playtest/RelayCore-playtest-20261002-230246-770.zip`, SHA256
`AA333A33DCAA3C3A9B61B4F49BF2F355D2EC46B13EEFB221A12BC7F517E1250F`.
Распакован вне checkout; actual standalone без asset override создал Vulkan device
и завершился по WM_CLOSE с exit 0. Optimized lifecycle/episode на внешних assets и
Debug Sandbox также exit 0. GLB/receipts/compiler отсутствуют в payload. Это локальный
кандидат 1.3.4; публикации, human playthrough, нового GPU и FPS comparison не было.

**Авторская поправка креплений и combat HUD — 2026-10-09.** Пульты RELEASE и
LIMITER получили стальные стойки, основания с анкерами и опоры табличек в
`containment_lining`; исходные interaction planes и 130 физических solids карты
сохранены. Первая проба физических стоек меняла pursuit Хранителя и ломала
кассета-first route; сравнение без стоек прошло, поэтому финальные крепления
входят только в существующую render-модель. Export script, editable blend и GLB
обновлены; Blender 5.2.2 fresh import: 7 meshes/materials, 63 754 triangles.

RmlUi показывает HP и полоску над живыми rover/shooter/turret, проверяет frustum
и LOS, а фактически снятое здоровье показывает всплывающим числом на 0,9 с.
Дробь суммируется по каждой цели, cooldown/miss не создают число, restore очищает
transient feedback. Хранитель сохраняет специальное разрешение аппаратом: HP
у него нет; над ним видны броня/подавление/восстановление/сдерживание, попадание
показывает «Броня», capture — существующий процент захвата.

Проверка: `cmake --build --preset demo-dev --target relaycore_demo relaycore_tests
relaycore_content_validation relaycore_containment_loop_graphics
relaycore_player_ui_graphics relaycore_level_check -j 4` в x64 MSVC environment;
`ctest --test-dir build/msvc-dev -R '^(relaycore\.(enemy HUD|routed rover|chapter rover|rover|lunge rover|destroying a lunge|nonlethal hits|shotgun)|validation\.containment|graphics\.(player_ui|containment_loop))' --output-on-failure --no-tests=error --output-junit combat-hud-results.xml`
— 31/31 passed, 0 skipped, включая SDR/HDR и ImGui disabled.
Check Map: valid, 0 warnings; `scripts/check-model-assets.ps1` проверил receipts
и staged products; `git diff --check` проходит. Просмотрены финальные
`build/msvc-dev/tests/containment-captures/apparatus-{standby,enemy-damage,armored-hit}-{sdr,hdr}.bmp`
и `build/msvc-dev/tests/player-ui-captures/enemy-health-damage.bmp`.
При более широком первоначальном фильтре отдельно упал
`relaycore.physics.native opening drives shots LOS rover movement focus doors and trigger entry`
на точном сравнении float hit positions (`tests/native_collision_gameplay_tests.cpp:188`);
он не входит в финальные 31 проверку и не исправлялся этим UI/asset pass.
Человеческая приёмка главы и читаемость в непрерывном бою остаются открытыми;
G-019 сохраняет `in_progress`, VIS-43–46 — `awaiting_playtest`.

## Исходные сравнительные оценки

Оценки 1–5 служат сравнению, а не вычислению общего балла. Больший Complexity/Risk означает больший объём/риск.

| ID | Value | Gameplay impact | Engine value | Learning value | Complexity | Risk |
| --- | --- | --- | --- | --- | --- | --- |
| H-001 | 5 | 2 | 4 | 3 | 1 | 1 |
| G-001 | 5 | 5 | 2 | 4 | 1 | 2 |
| G-002 | 5 | 5 | 1 | 3 | 2 | 2 |
| E-001 | 4 | 3 | 4 | 4 | 2 | 3 |
| G-003 | 5 | 5 | 2 | 4 | 3 | 2 |
| G-004 | 5 | 5 | 2 | 4 | 3 | 3 |
| G-005 | 4 | 4 | 3 | 5 | 3 | 3 |
| G-006 | 4 | 4 | 2 | 3 | 3 | 2 |
| H-002 | 5 | 4 | 4 | 5 | 2 | 3 |
| H-003 | 3 | 1 | 3 | 2 | 1 | 1 |
