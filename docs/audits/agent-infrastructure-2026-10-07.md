# RelayCore agent infrastructure audit — 2026-10-07

Scope: текущий working tree, agent instructions и developer tooling; engine/game code не изменялся. Целевой пользовательский профиль — Codex / GPT-6.1 Sol / High. Настройки модели и пользовательские глобальные skills не менялись.

## Результат

Минимальный достаточный набор — один root `AGENTS.MD`, шесть специализированных skills и существующие project docs/scripts. Количество skills сохранилось: generic implement/review заменены Vulkan investigation и gameplay/playtest. Новые hooks, nested AGENTS, override, PLANS, verify wrapper и сторонние bundles не нужны.

Подтверждены пробелы в маршрутизации, а не неспособность модели писать C++: исходный verify skill не объяснял graphics skip/пустую выборку, assets почти целиком описывал visual quality вместо native pipeline, profile не направлял к воспроизводимым workload. Полезные процедуры уже находились в developer docs, но агент не получал короткого входа к ним. G-011/G-012/H-008 evidence показывает повторяемость playtest/profile workflows; Vulkan migration evidence и текущие graphics tests — повторяемость GPU investigation. Частоту неверных решений агентов этот аудит не измерял.

P0 не выявлен. Реализованные изменения относятся к P2: меньше постоянного контекста, точнее выбор специальных знаний и interpretation проверок. Оснований заявлять измеренное улучшение качества или скорости работы агента нет.

## Инвентаризация до изменений

Все шесть исходных `SKILL.md` и три references прочитаны полностью. Root AGENTS, hook, installer и pre-commit regression fixture прочитаны полностью; presets и CI сопоставлены с CMake/test registrations. Большие планы изучены по назначению, routing, текущим карточкам и relevant evidence, без повторного чтения всей исторической хроники.

В доступных attachments был только текст задания. Отдельный архив skills не найден: источником были актуальные repo-local файлы, распаковка не выполнялась. `.codex/`, nested `AGENTS.md`, `AGENTS.override.md`, repo prompts, planning/PR templates и skill-local scripts не найдены в релевантном source tree. Build/vendor/generated documentation исключены из inventory инструкций. `core.hooksPath` уже настроен на `.githooks`.

| Материал | Назначение / загрузка / scope | Пересечение и drift | Реальная польза |
| --- | --- | --- | --- |
| `AGENTS.MD` | Постоянный root contract | Повтор project contract; design bibliography и visual rules нужны лишь части задач | Высокая: ownership, scope, dependencies, verification и Git |
| `relaycore-implement/SKILL.md` | Почти любая реализация C++/CMake | Повтор scope/minimal patch/tests/anti-slop/handoff из root; новых repo-specific операций нет | Низкая дополнительная; удалить |
| `relaycore-review/SKILL.md` | Read-only review | Общие finding threshold и anti-slop; root уже запрещает незапрошенные правки | Низкая дополнительная; удалить |
| `relaycore-verify/SKILL.md` | Выбор локальных проверок | Generic советы; не объяснял current build/test/staging boundaries | Высокая после уточнения |
| `relaycore-verify/references/check-matrix.md` | Conditional verification detail | Исторический список preset names быстро устаревает; таблица слишком общая | Сохранить decision matrix, source commands остаются в CMake |
| `relaycore-profile/SKILL.md` | Performance investigation | Большая часть generic; нет current workload routing и границ измерения | Сохранить, добавить специфику RelayCore |
| `relaycore-assets/SKILL.md` | Asset authoring/review | Routing захватывал shaders/maps вообще и пересекался с art; native products не раскрыты | Перевести в source/product/staging workflow |
| `relaycore-assets/references/visual-direction.md` | Visual vocabulary | Почти тот же owner, что ART_DIRECTION + art skill | Удалить копию |
| `relaycore-assets/references/quality-gate.md` | Asset delivery checklist | Visual часть повторяет art; техническая часть короткая | Встроить нужные technical checks в assets skill |
| `relaycore-art-direction/SKILL.md` | Только visual decisions/review | Scope чёткий; owner vocabulary — ART_DIRECTION | Сохранить; поправить ссылку на удалённый quality gate |
| `.githooks/pre-commit` | Git commit entry point | Bash → pwsh / Windows PowerShell fallback; правильный `-NoProfile` | Быстрые staged checks, без build |
| `tools/install-hooks.ps1` | Явная установка `core.hooksPath` | Не является автоматическим bootstrap; repo path authoritative | Сохранить, повторно не запускался |
| `tools/pre-commit.ps1` | Staged C++ format, conflict markers, JSON | Список VS-2022 fallback paths может устареть; PATH имеет приоритет | Проверяет индекс, не исправляет working tree |
| `tests/pre_commit_tests.ps1` | Изолированный временный Git fixture | Проверяет staged/working divergence, все format errors, JSON empty/null/BOM, conflict markers | Высокая; прогон реально выполнен |
| `CMakePresets.json` | Configure/build/test/workflow entry points | Имена и graph должны оставаться здесь, не в скопированном wrapper | Главный источник команд |
| Root/subsystem `CMakeLists.txt`, `cmake/RelayCoreOptions.cmake`, `cmake/RelayCoreDependencies.cmake` | Target graph/options/dependencies | Изменения могут инвалидировать команды skills; skills читают current definitions | Ownership graph и compiler/tool setup |
| `.github/workflows/ci.yml` | Hosted build/unit/content/tooling; opt-in GPU runner | Hosted compilation graphics не доказывает GPU execution; полный graphics gate отдельно | Не дублировать CI локально |
| `.run/*` | CLion Play, Sandbox, Check Map, Release | Interpreter path машинозависим; presets/targets должны совпадать | Существующие человеческие entry points |
| `.clang-format`, `.clang-tidy`, `vcpkg.json`, `vcpkg-configuration.json`, local overlays | Format/static analysis/dependency inputs | Authoritative config; версии не копировать в skills | Сохраняют воспроизводимость без prompt правил |
| `README.md` | Human onboarding/links, читается по необходимости | Некоторые описания возможностей исторические | Навигация, вторичный источник |
| `docs/DEVELOPMENT.md` | Setup, tests, profiling, Vulkan capture, native asset procedures | Много dated examples и version/path assumptions; сверять с source | Основной owner процедур |
| `docs/PLAYABLE_SLICE_ROADMAP.md` | Queue/status/selected ticket при gameplay/milestone работе | Исторические команды и текущие статусы сосуществуют | Authoritative queue; читать intro/table/ticket |
| `docs/PROJECT_PLAN.md` | Multi-stage programme и handoff | Уже решает постоянное planning; одна ссылка на удалённый skill исправлена | Сохраняется, не вводить конкурирующий план |
| `docs/ENGINE_ARCHITECTURE_REFERENCE.md` | Material architecture/capability changes | Future targets не являются текущими capabilities | Ownership/dependency boundaries |
| `docs/VULKAN_MIGRATION_PLAN.md` | Named gate/historical parity evidence | Migration завершена; старые GL команды не текущий workflow | Conditional evidence, не стартовая очередь |
| `docs/DEPENDENCY_STACK.md` | Dependency/subsystem foundation decisions | Snapshot versions устаревают, manifest/overlays authoritative | Запрещает раннее внедрение будущего stack |
| `docs/ART_DIRECTION.md`, `docs/art/occult-facility-style-board.png` | Visual vocabulary/style board при art work | Proposed scene не task authorization | Единственный owner визуального направления |
| `docs/ARCHITECTURE.md` | Поиск runtime/module ownership | Проверять current code; не загружать перед каждой правкой | Краткая карта системы |
| `docs/EDITOR_AND_CAMPAIGN.md` | Map/editor/transitions/user copies | Versioned примеры могут отставать от loaders | Authoring UX, не доказательство текущей схемы |
| `docs/COLLISION_ASSETS.md` | Native collision compilation/pairing/editor inspection | Format numbers/commands меняются с compiler | Единственный подробный collision workflow |
| `docs/UPDATES.md` | Explicit package/release/update task | Versions/candidate retry и внешний publish имеют side effects | Достаточный owner release workflow |
| `docs/RELEASE_AUTOMATION_PLAN.md` | Историческое основание release implementation | Не переигрывать завершённый план | Evidence, не ежедневная инструкция |
| `docs/playtests/*`, `docs/profiling/*`, `docs/PLAYTEST_GUIDE.txt` | Selected scenario/evidence | Guide может описывать иной ticket; historical build не current tree | Различает deterministic, human и performance acceptance |
| Asset/content READMEs, `tests/graphics-baselines/README.md`, RC-022/color docs | Local format/accepted visuals по затронутому пути | Legacy OBJ/GL descriptions не native pipeline contract | Читать конкретный asset/baseline, не все сразу |

### Tooling inventory

Операции вызываются явно человеком, build graph или выбранным workflow; автоматически в контекст модели не загружаются. Для каждой группы перечислены фактические файлы; это существующие production tools, не skill scripts.

| Файлы | Scope и польза | Owner / overlap / drift |
| --- | --- | --- |
| `scripts/release.ps1`, `release-functions.ps1`, `publish-release.ps1`, `package-playtest.ps1` | Version/candidate/retry, notes, build acceptance, package SHA256, publish | Scripts + UPDATES; высокий drift при копировании последовательности в skill. Prepare уже меняет VERSION; `-Release` публикует |
| `tests/release_scripts_tests.ps1`, `release_workflow_tests.ps1`, `tests/updater_tests.cs` | Isolated release fixtures и installation checks | Tests own regression; не вызывать production release для проверки docs |
| `scripts/check-model-assets.ps1`, `tests/model_assets_tests.ps1` | Source/tool/declaration/product/staged hashes | Receipts + compiler own format; native asset skill только маршрутизирует |
| `cmake/RelayCoreModelAssets.cmake`, `compile_model_asset.cmake`, `tools/model_import_main.cpp`, `model_import.cpp`, `model_import.h` | Explicit GLB declarations/importer/rcmodel | Pipeline targets + diagnostics authoritative; список моделей не копировать в инструкции |
| `cmake/RelayCoreCollisionAssets.cmake`, `compile_collision_asset.cmake`, `check_collision_assets.cmake`, `tools/collision_compile.cpp`, `tests/collision_assets_tests.ps1` | Authored collision → product/receipt/pairing/staging | Owner COLLISION_ASSETS + CMake/compiler; высокий drift у скопированных команд |
| `cmake/RelayCoreIrradianceAssets.cmake`, `compile_irradiance_asset.cmake`, `tools/irradiance_bake_main.cpp` | Probe generation и fingerprint | Bake inputs/targets; expensive workflow не routine verification |
| `cmake/sync_runtime_assets.cmake` | Manifest-controlled staging/removal, duplicate ownership rejection | Один synchronizer для runtime/fixtures; не делать ручные copies как обход |
| `tools/level_check.cpp`, `tests/level_check_tests.ps1` | Headless session/entry diagnostics | Current CLI/schema authoritative; valid exit не human route acceptance |
| `tools/physics_fixture.cpp`, `physics_fixture_data.h` | Isolated queries/geometry/SVG | Physics fixture без irradiance/shaders; не полный игровой маршрут |
| `tools/vulkan_toolchain_check.cpp` | Loader/header/layer probe | Не device/surface compatibility proof |
| `tools/sandbox_main.cpp`, `scripts/run-lab-b17.ps1` | Sandbox/B-17 runtime launch | Existing build/staged assets required; не новый agent launcher |
| `scripts/run-clang-tidy.ps1` | Compile database, all production cpp | Это полный production source scan, несмотря на CI label “targeted”; не запускать каждый раз |
| `scripts/build-facility-atlas.ps1`, `build-lab-graphics.cjs` | Atlas and authored graphics generation | Local texture/content conventions; explicit regeneration, graphics tool needs sharp |
| `scripts/build-service-cylinder.py`, `build-power-fuse.py`, `build-security-rover.py`, `build-security-turret.py` | Specific geometry/products | Existing generators/consumers; не общий DCC framework |
| `scripts/export-service-cylinder-glb.py`, `export-native-lab-glb.py`, `export-polyhaven-lab.py`, `export-shotgun.py` | Specific GLB/Blender exports | Importer subset, DCC version/material settings; exporter может менять несколько outputs |
| `scripts/build-player-audio.py`, `build-player-footsteps.py`, `build-combat-audio.py`, `build-environment-audio.py` | Specific audio generation | Audio consumers/format; no separate workflow evidence sufficient for another skill |
| `tools/updater/RelayCoreUpdater.cs`, `updates.json.in` | Update runtime/build config | Product functionality: прочитаны как граница release tooling, изменения вне текущей задачи |

## Решения A–I и прочих кандидатов

| Component | Verdict | Priority / Why |
| --- | --- | --- |
| Root AGENTS | IMPROVE | P2: сохранить policy, вынести design knowledge, убрать visual duplication, уточнить conditional docs routing |
| Existing implement/review | DO NOT ADD / remove | P2: generic process уже принадлежит root/model; нет RelayCore-specific workflow |
| A — Build/Verification | IMPROVE existing verify | P2: current presets, graphics skip/empty selection, staged assets, CI boundaries |
| B — Vulkan Investigation | ADD | P2: повторяемые GPU failure domains; короткая диагностика и вход к существующим procedures |
| C — Performance/Tracy | IMPROVE existing profile | P2: сравнимый optimized workload, CPU/GPU/wait boundaries, first-use/revisit |
| D — Asset Pipeline | IMPROVE existing assets | P2: native products/receipts/staging уже сложнее visual checklist |
| E — Gameplay/Playtest | ADD | P2: existing protocols повторяются, нужен selected-ticket routing и human-evidence boundary |
| F — Art Direction | KEEP | Только исправлена устаревшая внутренняя отсылка; direction/description сохранены |
| G — Release/Packaging | DO NOT ADD | UPDATES + resumable scripts + fixtures уже содержат порядок; новый skill почти повторял бы routing |
| H — CMake/Dependencies | DO NOT ADD | Root + DEPENDENCY_STACK + verify достаточно; generated-command audit нужен по конкретному ABI вопросу |
| I — Map/Content Authoring | DO NOT ADD | Existing editor/dev docs + playtest/verify/assets покрывают различимые операции; отдельный skill пересекался бы с тремя |
| Audio/export-specific skills | DO NOT ADD | Есть конкретные generators, но нет подтверждённого отдельного повторяемого agent workflow |
| Nested AGENTS для engine/game/renderer/assets/tools/tests | DO NOT ADD | Не найдено постоянного subtree contract сверх root; различия относятся к задачам/skills |
| AGENTS.override | DO NOT ADD | Нет конфликта инструкций, который требовал бы override |
| PLANS / execution-plan skill | DO NOT ADD | Roadmap tickets/PROJECT_PLAN/Vulkan plan уже ведут длительную работу; root разрешает task plan лишь когда owner отсутствует |
| `tools/verify.ps1` | DO NOT ADD | Presets + existing validators уже исполняют операции; wrapper создаёт второй command graph и drift |
| pre-commit / installer | KEEP | Нет подтверждённой ошибки в проверенных сценариях; не превращать в build farm |
| Additional agent hooks, personas, orchestrator, memory/MCP framework | DO NOT ADD | Не решают обнаруженный repository knowledge gap |

## Ownership, загрузка и сопровождение после изменений

Метаданные skills доступны при выборе; body читается при подходящей задаче; reference — когда нужны детали. Это соответствует [официальной документации skills](https://developers.openai.com/codex/skills). Root остаётся всегда применимым; этот audit report не включён в routing и не должен читаться при обычной работе.

| Owner | Когда читать / supporting files | Кто обновляет / trigger drift |
| --- | --- | --- |
| `AGENTS.MD` — global policy | Почти любая работа | Автор или агент в policy change; низкий drift, команды не каталогизируются |
| `relaycore-verify` — selection/results | Нетривиальный verification или failure; единственный reference `references/check-matrix.md` | Агент, меняющий presets/test graph/CI semantics; средний drift, проверять source names |
| `relaycore-vulkan-investigate` — diagnostic order | GPU startup/validation/sync/lifetime/regression; supporting procedures остаются в DEVELOPMENT | Агент, меняющий device/presentation/validation workflow; низкий–средний drift, нет version pins |
| `relaycore-profile` — comparable evidence | Performance investigation/optimization; DEVELOPMENT + relevant report | Агент, меняющий workloads/Tracy/metrics; средний drift, не копировать capture commands |
| `relaycore-assets` — production delivery | Source/export/native product/staging; CMake/scripts/COLLISION_ASSETS/DEVELOPMENT | Агент, меняющий compiler/receipt/staging; средний drift, источники форматов остаются в tooling |
| `relaycore-playtest` — acceptance/evidence | Ticket с player criteria, protocol preparation/results; existing ticket protocol | Автор/агент, меняющий playtest/status semantics; низкий drift, нет закреплённого “текущего” scenario |
| `relaycore-art-direction` — visual decisions | Art review/concept/material/signage/lighting; ART_DIRECTION + board | Автор art direction; низкий drift, единственный style vocabulary owner |
| `docs/DESIGN_REFERENCES.md` — design knowledge | Только rationale/design задачи; перенесены существующие bibliography и gameplay type guidance | Автор при изменении design principles; низкий drift, нет новых команд |
| Existing project docs — system/procedure knowledge | Conditional routing выше | Автор/агент вместе с изменением соответствующей системы; не синхронизировать копии в skills |
| Этот audit report — snapshot | Только повторный аудит/проверка данного изменения | Не live policy и не постоянная очередь; не требует синхронизации после каждой feature |

## Routing review: 3 positive и 3 negative запроса на skill

Это ручная проверка descriptions против сценариев, не модельный eval. Несколько skills могут корректно сочетаться: art определяет вид, assets доставляет продукт, verify выбирает проверки. Exclusions разделяют намерение, а не запрещают такое сочетание.

| Skill | Должен применяться | Не должен применяться |
| --- | --- | --- |
| `relaycore-verify` | «Выбери проверки после изменения player movement»; «Почему test-debug выбрал несобранный test?»; «CTest зелёный, но graphics Skipped — gate пройден?» | «Прочитай текст отчёта без запуска проверок»; «Предложи палитру лаборатории»; «Объясни назначение двери без изменений и tests» |
| `relaycore-vulkan-investigate` | «Startup не создаёт device на другом GPU»; «Validation VUID после swapchain recreation»; «После HDR изменения baseline показывает неверный цвет» | «Измени известную shader constant с уже выбранным test»; «Найди ошибку TOML parser без GPU»; «Сравни CPU frame-time в двух Tracy captures» |
| `relaycore-profile` | «Разбери scene fence cost по Tracy»; «Сними before/after на bench workload»; «Откуда first-use spike при входе?» | «Исправь validation descriptor error»; «Сократи длинное имя функции»; «Подтверди удовольствие игрока по автотестам» |
| `relaycore-assets` | «Добавь GLB через rcmodel pipeline»; «Collision receipt stale после изменения модели»; «Подготовь tiling texture и проверь доставку в runtime» | «Сменить порядок задач в карте без новых assets»; «Предложи visual language комнаты без production»; «Исправь арифметику таймера» |
| `relaycore-playtest` | «Подготовь независимый G-012 playtest»; «Выполни slice ticket с criteria понятности маршрута»; «Запиши отзыв игрока и обнови статус» | «Исправь изолированный off-by-one reload timer»; «Экспортируй шкаф в GLB»; «Измерь draw/upload costs без оценки gameplay» |
| `relaycore-art-direction` | «Проверь occult apparatus на соответствие стилю»; «Выбери материалы и навигационные акценты»; «Предложи форму CRT prop для facility» | «Почему importer отвергает tangent?»; «Исправь CMake dependency»; «Проверь staleness collision receipt без visual decisions» |

Все шесть descriptions просмотрены против этих 36 сценариев. Perfect routing и экономия runtime tokens не заявляются.

## Поиск внешних skills

Открыты фактические upstream SKILL.md, а не только каталоги. Ничего не устанавливалось, чужие scripts не исполнялись и полный код bundles не прошёл аудит: ниже решения о пригодности, не certification безопасности.

| Источник | Вывод для RelayCore |
| --- | --- |
| [OpenAI gh-fix-ci](https://github.com/openai/skills/blob/main/skills/.curated/gh-fix-ci/SKILL.md) | Полезен при повторяющемся GitHub PR/Actions triage: bundled log collection. Сейчас такого gap не показано; его повторное approval перед implementation лишнее при уже порученном fix. Не устанавливать в repo заранее |
| [Anthropic skill-creator](https://github.com/anthropics/skills/blob/main/skills/skill-creator/SKILL.md) | Полезна идея with/without baseline и near-miss trigger cases. Полный Claude-specific eval loop не переносить в проект; системный Codex skill-creator уже доступен |
| [CppStudio router](https://github.com/tarkansarim/CppStudio/blob/main/skills/cpp-cuda-vulkan-studio/SKILL.md), [Vulkan module](https://github.com/tarkansarim/CppStudio/blob/main/skills/cpp-cuda-vulkan-studio/modules/vulkan-compute-sync/GUIDE.md) | Отдельные producer/consumer вопросы полезны, но bundle включает planner/supervisor/process lanes/CUDA и общий router. RelayCore уже владеет нужными procedures; не добавлять framework |
| [Superpowers using-superpowers](https://github.com/obra/superpowers/blob/main/skills/using-superpowers/SKILL.md) | Требует skill lookup перед любой репликой и при 1% вероятности relevance. Для данного запроса на минимальный context это плохой default |
| [cmake-vcpkg-engineering](https://github.com/tyx3211/cpp-coding-standards/blob/main/skills/cmake-vcpkg-engineering/SKILL.md) | Полезна проверка generated compile/link/cache при ABI/toolchain изменениях. Полный workflow требует baseline/binary comparison и Release даже для широкого scope; не заменять узкий RelayCore verify |
| [Mindrally cpp](https://github.com/Mindrally/skills/blob/main/cpp/SKILL.md) | Generic advice и чужие naming/tests conventions (Google Test/Catch2 вместо doctest). Не добавлять |

Подтверждённо полезных для безусловной установки внешних bundles не найдено среди изученных. При будущем регулярном CI triage первым кандидатом остаётся узкий log-inspection helper, после проверки полного содержимого/совместимости. Это не утверждение об отсутствии любых подходящих skills в интернете.

## Context efficiency и удалённое

Счётчик — whitespace-separated words, не tokenizer и не измерение billed context. До/после относительно HEAD: root **1966 → 1367** слов (−30,5%); descriptions шести skills **236 → 215** (−8,9%); root + все repo-local skill bodies/references **4636 → 3820** (−17,6%). Последняя сумма не является контекстом одного запуска: все skills одновременно не нужны. Перенесённые design references и snapshot report в неё не входят, поскольку читаются отдельно по требованию.

Удалены два generic entry points и два assets references. Принципы design/type guidance сохранены в DESIGN_REFERENCES; визуальный vocabulary остаётся в ART_DIRECTION. Новые skills не копируют схемы форматов, версии tools, весь renderer design или команды CI. Исправлена ссылка на старый implement skill в PROJECT_PLAN.

## Фактическая структура

```text
AGENTS.MD
.agents/skills/
  relaycore-art-direction/SKILL.md
  relaycore-assets/SKILL.md
  relaycore-playtest/SKILL.md
  relaycore-profile/SKILL.md
  relaycore-verify/
    SKILL.md
    references/check-matrix.md
  relaycore-vulkan-investigate/SKILL.md
docs/
  DESIGN_REFERENCES.md
  audits/agent-infrastructure-2026-10-07.md
  ... existing authoritative docs/protocols
.githooks/pre-commit
tools/{install-hooks,pre-commit}.ps1
scripts/... existing production tooling
```

## Verification и границы

- `pwsh -NoProfile -File tests/pre_commit_tests.ps1` — passed, **16,75 s** весь fixture (не один hook). Первая sandbox попытка остановилась на записи временного `.git/config`; вне sandbox fixture завершился, индекс RelayCore не изменялся.
- Bundled `skill-creator/scripts/quick_validate.py .agents/skills/relaycore-verify` не запустился: в доступном Python отсутствует `yaml`/PyYAML. Пакеты не устанавливались.
- Альтернативная stdlib-проверка всех manifests: точный ограниченный YAML grammar из двух single-line plain scalars, frontmatter envelope, уникальность/совпадение name с каталогом, длины и placeholders — passed, **6 skills**. Это не общий YAML parser; текущие manifests не используют другие YAML конструкции.
- Проверка root-relative file references и Markdown links — **22** существующих targets; sibling skill names сопоставлены с шестью каталогами. Deleted entry points не остаются в active routing.
- Поиск точных повторов paragraphs от 60 слов между agent files и relevant authoritative docs — **0**; дополнительно выполнен ручной semantic overlap review. Exact-match check не доказывает отсутствие всех смысловых повторов.
- 36 routing examples выше проверены вручную. Model A/B eval, token/time benchmark, live GPU/playtest/profiling не выполнялись.
- Итоговые diff review и `git diff --check` выполнены; runtime build не требуется для prose-only изменений. Hooks/scripts/presets/CI и engine/game code данным аудитом не менялись. Две исходные пользовательские правки `src/relaygame/world_irradiance.cpp` и `tests/power_restoration_graphics.cpp` сохранены.

Созданные validation script/JSON в ignored `build/agent-infrastructure-audit/` — локальная evidence данного аудита, не новый обязательный tool. Существующие protocol/doc snapshots могут отставать от current source; это причина conditional routing и source precedence, а не добавления ещё одного knowledge layer.
