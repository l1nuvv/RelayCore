# Кандидат RelayCore 1.4.1 — 2026-10-10

Статус: с разрешения автора опубликован
[RelayCore 1.4.1](https://github.com/l1nuvv/RelayCore-releases/releases/tag/v1.4.1)
в `l1nuvv/RelayCore-releases`.
Человеческая приёмка G-019, другого GPU и непрерывного sound-on прохождения
этой проверкой не закрывается. Текущий workflow — [UPDATES.md](../UPDATES.md).

## Подготовка и проверки

- `pwsh -NoProfile -File scripts/release.ps1` — успешно после повтора вне
  ограниченной среды; номер при повторе остался 1.4.1. Первая попытка не смогла
  разрешить сетевые адреса vcpkg и найти `cl.exe`.
- `msvc-release`: Release, Unity/IPO включены, Tracy выключен. Собраны
  `relaycore_demo`, `relaycore_updater_tests`, `relaycore_content_validation`.
- `ctest --test-dir build/msvc-release -R '^(relaycore\.updater$|validation\.)'
  --output-on-failure` — 68/68 passed, без пропусков.
- `pwsh -NoProfile -File tests/release_scripts_tests.ps1` — passed.
- `pwsh -NoProfile -File tests/release_workflow_tests.ps1` — passed, изолированная
  копия с имитацией сборки/публикации; реальные GitHub releases не изменялись.
- Packaging сверил 207 файлов и содержимое ZIP. Запуск
  `build/msvc-release/tests/relaycore_updater_tests.exe <ZIP>` дал
  `real_update=passed`: установка, backup, запуск настоящей игры, подтверждение
  готовности и штатное закрытие.
- `pwsh -NoProfile -File scripts/release.ps1 -Publish` — успешно; publisher
  сверил GitHub digest/size загруженных ZIP и descriptor до публикации.
  Публичный `/releases/latest/download/release.json` проверен после публикации:
  repository/version/bytes/SHA256 совпадают с локальным кандидатом.
  Новый сетевой переход через клиентский updater отдельно не выполнялся;
  установка/перезапуск выше использовали тот же локальный ZIP.
- Документация: 74 файла, 452 локальные ссылки/якоря, ошибок нет;
  `git diff --check` прошёл.

## Артефакт

- ZIP: `build/playtest/RelayCore-playtest-20261010-225529-108.zip`.
- Размер: 218 076 953 байт.
- SHA256: `EB430015B05E00944C552BD49CBAEEB8E59319FAEE0CEDEE59058708262A9F09`.
- Состояние/хеши: `build/release/candidate.json`; описание:
  `build/release/release-1.4.1.md`; descriptor рядом с ZIP — `.release.json`.
- Логи: `build/release/documentation-release.log` и
  `build/release/documentation-publish.log`.
- Длительности: configure 55,85 с; build 122,49 с; tests 4,99 с;
  package 37,46 с; installation/restart 15,85 с. Это один локальный проход,
  не обещание времени сборки или прироста FPS на другой машине.

BUILD_MANIFEST фиксирует source state в момент упаковки. После него завершались
только изменения документации и этот протокол; код, конфигурация сборки и payload
не менялись. Архив сохранён без повторной упаковки. VERSION/vcpkg синхронизированы
на 1.4.1; исходники не коммитились и не отправлялись в Git.
