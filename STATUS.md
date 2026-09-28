# RelayCore — состояние проекта

Актуализация: 2026-09-28. Основная очередь — [PLAYABLE_SLICE_ROADMAP](docs/PLAYABLE_SLICE_ROADMAP.md); прежние RC/RF-аудиты не задают следующую работу автоматически.

H-005 завершён и с разрешения автора опубликован [релиз 1.3.2](https://github.com/l1nuvv/RelayCore-releases/releases/tag/v1.3.2): автоматическая версия и подготовка Release-кандидата, оформление описания, публикация зафиксированного архива, уведомление в игре и автоматическое закрытие установщика. Release content/updater 20/20, Debug graphics/lifecycle 2/2, реальная установка и сетевой переход проверены. [Отдельный план, результаты и замеры](docs/RELEASE_AUTOMATION_PLAN.md). Прошлые номера в протоколах остаются историческими; новый выпуск готовится через `scripts/release.ps1`.

H-004 завершён: версия 1.3.1, проверка обновлений из меню и отдельный Windows updater с проверкой архива, резервной копией и подтверждением запуска новой игры. С разрешения автора опубликован [релиз 1.3.1](https://github.com/l1nuvv/RelayCore-releases/releases/tag/v1.3.1). Проверены публичное скачивание и SHA256, ответы помощника `current`/`available`, локальная замена, сетевой переход с 1.3.0 и полный сетевой цикл исправленного помощника в длинной папке: готовность игры подтверждена, игра и установщик закрылись с кодом 0. Исправлена найденная при сетевом тесте 1.3.0 ошибка длины staging-пути. [Протокол](docs/playtests/H-004-2026-09-28.md), [команды и ограничения](docs/UPDATES.md).

H-001 и E-001 завершены. G-002 принята автором («все ок все лучше»). Два исходных прохода G-001 — примерно 90–120 секунд с подсказками и 3:26 с неуточнёнными условиями — сохранены в [протоколе](docs/playtests/G-001-2026-09-27.md); самостоятельность и целевая длительность не подтверждены.

Реализован блок G-003–G-005: раздельные боевые звуки и предупреждение турели, сохранение замаха/recovery робота после несмертельного попадания, понятное мягкое возрождение и сохранение актуального мира при выходе во время смерти. Сохранение формата 3; после загрузки смерти завершается respawn-таймер. [Протокол блока](docs/playtests/G-003-005-2026-09-28.md) содержит решения в трёх встречах, точные проверки и кандидата.

Автор подтвердил улучшение выстрела и подготовки турели словами «уже лучше». G-003–G-005 сохраняют остальные ограничения человеческой приёмки.

Выполнена локальная часть G-006/H-002: компактный HUD, сообщения без сплошной цветной вспышки, проверка оборудования/паузы через настоящий audio backend, строгая SHA256-упаковка с manifest. RelWithDebInfo: unit/audio 243/243, content 19/19; graphics power_restoration/game_lifecycle, standalone startup/close и повторные проверки на распакованных ресурсах вне проекта прошли. Точные команды и кандидат — в [протоколе](docs/playtests/G-006-H-002-2026-09-28.md).

Снят графический baseline шести видов и затем интегрированный Tracy-захват подготовленных состояний на реальном `Application`/`DemoGame`. При этом ASan выявил UAF внутри miniaudio при попытке открыть отсутствующий WAV; `AudioSystem` теперь отвергает отсутствующие loop/one-shot файлы до вызова библиотеки. ASan unit 243/243, content 19/19, graphics 2/2. [Новый протокол H-002](docs/playtests/H-002-2026-09-28.md) содержит условия и ограничения измерения.

Следующий шаг M3 — человеческая оценка G-003–G-006, другой ПК, самостоятельные прохождения и непрерывный Tracy-захват живого маршрута. H-002 — awaiting_playtest, итоговое согласование H-003 — in_progress. v1 пока не принят; новых engine-направлений автоматически не открывать.

## История аудита

Ниже сохранены прежние отметки RC-аудита. Их формулировки и числа относятся к соответствующим этапам, не заменяют актуальные проверки и не объявляют весь playable slice готовым.

- RC-004 — закрыт: поддерживаемая build matrix отражает реальные options.
- RC-005 — закрыт: README, presets и CI используют согласованный MSVC/CMake/vcpkg contract.
- RC-005.1 — закрыт без изменений: source tree не требовал правки для этого пункта.
- RC-006 — закрыт: `engine.toml` проходит строгую type/range validation.
- RC-006a — закрыт: удалён неподдерживаемый binding `game.controls.copy_inspector_source`.
- RC-007 — закрыт: отсутствие audio backend переводит runtime в silent mode с warning.
- RC-008 — закрыт: shadow resources имеют RAII и staging перед renderer commit.
- RC-009 — закрыт: reusable trigger использует enter/exit semantics.
- RC-010 — закрыт: vertical collision покрыта защитой от tunneling.
- RC-011 — закрыт: ImGui backend initialization проверяется и откатывается при ошибке.
- RC-012 — закрыт: gameplay input отделён от ImGui с учётом relative mouse mode и реального UI interaction.
- RC-013a — закрыт: CI выполняет Debug, RelWithDebInfo и ASan workflows.
- RC-013b — закрыт: CI запускает targeted clang-tidy.
- RC-014 — пропущен: проект приватный и не планируется к распространению.
- RC-015 — закрыт: README, architecture map и этот status file фиксируют безопасную точку возврата.
- RC-016 — закрыт: временные shader/program objects управляются RAII на compile/link exception paths.
- RC-017 — закрыт: quit немедленно прекращает текущий frame, а minimized/hidden/zero-extent window не выполняет busy render loop.
- RC-018a — закрыт: allocating diagnostics setters имеют корректный exception contract, а per-update snapshots передаются в Renderer перемещением.
- RC-018b — закрыт: runtime/audio/inspector diagnostics строятся только при активных F2/F6/F4 потребителях.
- RC-018 — закрыт: diagnostics APIs имеют корректный exception contract, snapshots перемещаются в Renderer, а тяжёлые builders учитывают visibility.
- RC-019 — закрыт: filesystem-writing tests используют уникальные per-case RAII temp directories и проходят параллельный CTest без конфликтов.
- RC-020 — закрыт: удалены неиспользуемые FlyCameraController и basic shaders, а ложная PCH-конфигурация удалена; RELAYCORE_PROJECT_ROOT уже отсутствовал в текущем дереве.
- RC-021 — закрыт: PerspectiveCamera отвергает non-finite и вырожденные view/projection inputs и сохраняет прежнее valid state при ошибках mutating API.
- RC-022a — закрыт: добавлен воспроизводимый numeric GL baseline текущего gamma-space pipeline и документирована процедура ручного visual capture; решение о sRGB migration отложено до review измерений. RC-022b открывается только после отдельного решения о migration.
- RC-022b — закрыт: Texture2D получил явный linear/sRGB encoding contract, а controlled GL baseline проверяет hardware sRGB texture decode и framebuffer encode; production pipeline остаётся legacy gamma-space.
- RC-022p1 — закрыт: добавлены детерминированные reference level/config и воспроизводимая процедура local visual capture для будущего legacy/sRGB A/B review.
- RC-022p2 — закрыт: формализованы visual-sRGB base_color и linear tint/brightness multipliers; legacy composition сохранена, а conventional linear composition покрыта unit tests.

- RC-022c — закрыт: добавлен opt-in hardware-sRGB production pipeline с behavioral default-framebuffer verification, полным linear scene RGB handling и явной ImGui state boundary; legacy_gamma сохранён как default.
- RC-022d — закрыт: local legacy_gamma/hardware_srgb A/B review подтвердил отсутствие очевидного double encoding и сохранность ImGui/HUD boundary, но существующие lighting/material values остаются legacy-tuned; legacy_gamma остаётся production default, hardware_srgb — explicit opt-in.

- RC-022e — закрыт: iteration 3 принята как стабильный deterministic hardware-sRGB reference retuning. Сужены footprints локальных источников, уменьшена доминантность glass probe, сохранены улучшения глобального разделения из iteration 2. Renderer, pipeline math и production default не менялись; legacy_gamma остаётся default, hardware_srgb — explicit opt-in.

- RC-022f — закрыт: аудит production assets подтвердил, что shared retuning небезопасен. Три production-уровня содержат 212 material-использований, 129 tint, 129 brightness и 35 локальных render-light наборов radius/intensity; дополнительно используются общие material base colors и глобальные bounce/fog/shadow параметры. Выбрана стратегия pipeline-specific authoring profiles.

## Незавершённое историческое направление

Следующую работу выбирают из [действующего плана](docs/PLAYABLE_SLICE_ROADMAP.md). Старые гипотезы аудита сначала перепроверяются по коду и текущей игровой потребности.

RC-022g — iteration 1: добавлены data model, строгий TOML parser, global/material application helpers и stable-ID validation contract для pipeline-specific authoring overlay. Runtime selection, извлечение authoring IDs из уровней и применение level-local overrides не включены в текущую очередь; возвращаться к ним только при конкретной потребности.

## Проверки после изменений

Выбирать узкую проверку по изменённой области; полный workflow не обязателен после каждого патча. Доступные расширенные проверки:

```powershell
cmake --workflow --preset debug-discipline-workflow
cmake --workflow --preset relwithdebinfo-discipline-workflow
cmake --workflow --preset asan-discipline-workflow
pwsh -File scripts/run-clang-tidy.ps1 -BuildDirectory build/msvc-debug
```

`clang-tidy` требует предварительно созданную compile database preset `msvc-debug`, например через `cmake --preset msvc-debug`.

## Known limitations

- Есть ZIP для частного плейтеста (`scripts/package-playtest.ps1`); install/CPack pipeline отсутствует.
- Проект не предназначен для публичной поставки.
- Нет deterministic GL/backend failure-injection harness.
- clang-tidy baseline намеренно ограничен явным набором checks.
- Полноценный Doxygen reference не входит в обязательный workflow.
- ImGui остаётся encoded-space UI composition в hardware-sRGB режиме; migration его backend/font/shader намеренно отложена.
