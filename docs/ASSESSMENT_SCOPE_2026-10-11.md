# RelayCore — границы актуальной оценки, 2026-10-11

**Исторический scope статического обзора от 2026-10-11.** Последующая реализация и действующие статусы находятся в [renderer-плане](RENDERER_MODERNIZATION_PLAN.md) и [roadmap](PLAYABLE_SLICE_ROADMAP.md). Описанные ниже source/working-tree observations относятся только к исходной оценке.

Проверенный тогда локальный HEAD: `c9b4dcb619def034f81eca0cca5604bb5e63709c`. Рабочее дерево содержит незакоммиченные изменения, поэтому SHA не описывает его целиком. `VERSION` и vcpkg manifest указывают продукт **1.4.1**. Это не версия Vulkan API: source на момент обзора использовал 1.3.

Исходный внешний обзор исправлял ошибку доступа к репозиторию, но его G-019 `in_progress` уже устарел относительно локальных документов. Roadmap фиксирует авторскую приёмку 2026-10-10 (`done`) и последующий технический art-pass. Независимые замеры 20–30 минут, sound-on проход, другой GPU и отдельные VIS/IMG/G-010 gates не закрываются общей авторской приёмкой. Авторитет статусов — [roadmap](PLAYABLE_SLICE_ROADMAP.md), а не этот обзор.

В этой сверке прочитаны три переданных документа, README, VERSION, manifest/CMake version contract, обзорные архитектурные/стратегические документы, release-1.4.1 evidence и актуальные G-019 записи. Выборочно проверены Vulkan version selection, upload waits, scene submission, descriptor pools, pipeline creation и swapchain formats. Прочитано art direction и просмотрен основной style board. Это не полный lifetime/concurrency аудит.

Наличие кода, записанные результаты прежних тестов и самостоятельно выполненная runtime-проверка — разные степени подтверждения. Release 1.4.1 известен по [протоколу](playtests/release-1.4.1.md); опубликованный payload и сеть повторно не проверялись. Новые builds, CTest, gameplay runs, validation, RenderDoc и Tracy не запускались. Полная графическая совместимость и визуальное качество этим обзором не подтверждены.

[План модернизации](RENDERER_MODERNIZATION_PLAN.md) перенесён в docs с локальным delta, дроблением широких milestones и уточнением temporal dependencies. На момент этого обзора implementation milestones оставались pending. README и связанные текущие документы согласованы с продуктом 1.4.1 и авторской приёмкой; датированные исторические результаты сохраняются. Палитра включена в [art direction](ART_DIRECTION.md#6-color-direction). Runtime, зависимости, VERSION и release payload в рамках этой задачи не изменяются.
