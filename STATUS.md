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
- RC-012 — закрыт: gameplay input отделён от захвата ImGui.
- RC-013a — закрыт: CI выполняет Debug, RelWithDebInfo и ASan workflows.
- RC-013b — закрыт: CI запускает targeted clang-tidy.
- RC-014 — пропущен: проект приватный и не планируется к распространению.
- RC-015 — текущий documentation checkpoint: README, architecture map и этот status file фиксируют безопасную точку возврата.

## Следующая работа

Следующий пункт выбирается из готового аудита после закрытия RC-015. Каждый issue сначала перепроверяется по текущему коду. Broad refactoring без отдельного подтверждённого issue запрещён.

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
