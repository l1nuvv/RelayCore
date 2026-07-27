# RelayCore maintenance checkpoint

RelayCore — приватный учебный проект, проходящий ограниченный этап исправления аудита. Публичное распространение и разработка новых функций вне текущего scope.

Этот файл не фиксирует официальный versioned checkpoint: текущие логи не устанавливают verified snapshot.

## Статусы аудита

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

## Следующая работа

Следующий пункт выбирается из готового аудита после закрытия RC-021. Каждый issue сначала перепроверяется по текущему коду. Broad refactoring без отдельного подтверждённого issue запрещён.

RC-022f — провести аудит production-authored lighting/material values в hardware_srgb и выбрать стратегию миграции: shared retuning либо отдельный pipeline-specific authoring profile; production default не менять до проверки реальных уровней.

## Проверки после изменений

```powershell
cmake --workflow --preset debug-discipline-workflow
cmake --workflow --preset relwithdebinfo-discipline-workflow
cmake --workflow --preset asan-discipline-workflow
pwsh -File scripts/run-clang-tidy.ps1 -BuildDirectory build/msvc-debug
```

`clang-tidy` требует предварительно созданную compile database preset `msvc-debug`, например через `cmake --fresh --preset msvc-debug`.

## Known limitations

- Нет install/CPack/package pipeline.
- Проект не предназначен для публичной поставки.
- Нет deterministic GL/backend failure-injection harness.
- clang-tidy baseline намеренно ограничен явным набором checks.
- Полноценный Doxygen reference не входит в обязательный workflow.
- ImGui остаётся encoded-space UI composition в hardware-sRGB режиме; migration его backend/font/shader намеренно отложена.
