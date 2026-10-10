# G-019 — наполнение принятой главы

Поручение 2026-10-11: этапы 0–7 следующего цикла. Фактический статус —
в карточке G-019 roadmap. Существующий gameplay принят, новые задачи/враги
и изменения баланса сюда не входят.

## Исходные виды и конечный список

Свежие Debug content/SDR/HDR проверки исходной карты: 12/12, без skips.
Карта, GLB и одинаковые кадры сохранены в `build/g019/artpass-before/`;
команды/результаты — `artpass-before-{build,checks}.log`, JUnit `artpass-before.xml`.
Graphics fixture дополнен рабочим местом, дробовиком, северным входом архива,
кассетой и лабораторным крылом; камеры остаются одинаковыми до/после.

| Зона | Видимый недостаток / размещения | Сохраняемое пространство |
| --- | --- | --- |
| Пост и зал | Пустой пост: fitted стол, CRT/монитор, приборы, консоль и стойка. У рамы — подключённые гидролинии/лоток, боковые приборные стойки и стеновой оккультный акцент | Центральное стекло, вход, пульт/кассета, сектор ловушки, боковое уклонение и видимый выход |
| Мастерская | Пустые столешницы: измеритель, ручной инструмент, снятые детали, документы; настенная ремонтная доска | Дробовик на первом столе, обход обоих верстаков, authored rover route |
| Архив | Однообразные фасады: индексы рядов/ячеек, папки, стол изучения и локальное оформление кассеты | Оба входа, проходы между шкафами, shortcut и crossfire |
| Лаборатория | Столы без общей функции: микроскоп/образцы, измеритель, терминал, доска исследования | Восточный подход, северное соединение, путь стрелка |
| Верхний маршрут | Необъяснённый сервис: боковые трассы/крепления, конструктивные опоры галереи, ограждение по существующим границам, шкаф привода | Обе лестницы, grip_service, обратный маршрут и проёмы |
| Детали/свет | Документы, локальные следы работы, привязанные к зонам надписи и контактные детали | Приоритет интерактивов и опасных действий |

Повторное использование: `crt_terminal`, `surveillance_monitor`, `instrument_rack`,
`control_console`, `lab_accessories`, `lab_multimeter`, `lab_microscope`,
`power_cabinet`, `electrical_notes_board`, `occult_research_board`, `blood_pentagram`.
Размеры GLB измерены с node transforms (`build/g019/kit-bounds.json`).
Небольшая адаптация: дополнения существующего `containment_lining` и exporter;
новая отдельная библиотека моделей не нужна.

Моделирование: метры, runtime +Y вверх, прежние pivots и scanned enamel/steel;
дополнительные объекты имеют видимые опоры/подключения, плавные профили труб,
обработанные кромки и оправданный размер на дистанции 1–8 м. Сверка — с
industrial-occult board: боковая инженерная инфраструктура, спокойная рабочая
обстановка и локальный красный символ при главном фокусе на Хранителе.
Новые крупные препятствия получают явную коллизию. Экспорт только lining;
hero body/arm/ram/grip сохраняются. Просмотр по зонам и fresh-import evidence
дополняют importer/route checks; отсутствие topology ошибок не заменяет просмотр.

## Результат 2026-10-11

Этапы 0–6 выполнены: пост/зал, мастерская, архив, лаборатория, верхний маршрут,
контактные детали и локальный свет. Оригинальные 130 solids, задачи, маршруты
врагов и баланс сохранены; карта содержит 89 entities. Крупная новая мебель
имеет explicit prop collision. Добавлен один свет над лабораторным верстаком;
выход, ловушка и взаимодействия остаются видимыми в фиксированных видах.

[До](g019-environment-fill-before.png) · [После](g019-environment-fill-after.png).
Полные SDR/HDR и дополнительные крупные виды: `build/g019/artpass-final/`.

Этапы 1–5: Debug native compile/staging, 10 containment cases + SDR — каждый
**11/11, zero skips**. Логи и JUnit: `build/g019/artpass-stageN-{build,checks}.log`,
`artpass-stageN.xml`. Финальный Debug build и content/SDR/HDR — **12/12,
0 failed, 0 skipped** (`artpass-final-{build,checks}.log`, `artpass-final.xml`).
JUnit также находится в `build/msvc-debug/build/g019/`: CTest разрешает его
выходной путь относительно build tree.

Реальное grounded движение в fixture проверяет оба порядка подготовки,
доступность пультов/предметов, trap/dodge и выход; save/retry/editor/AI gates
сохраняются. Это scripted traversal, не самостоятельный человеческий playtest.

Дополнительный запуск graphics fixture с `--previous-level` и `--motion` прошёл:
старый disk Continue отклонён штатной диагностикой «Карта изменилась после
сохранения. Начните новый эпизод.», player position и simulation tick сохранены.
Пользовательский прогресс не изменялся. 48 кадров дуги камеры у Хранителя
сохранены, выборка просмотрена (`artpass-continue-motion.log`, `artpass-motion.png`);
это проверка изображения в движении, а не непрерывный проход со звуком.
Vulkan validation включена, VUID/error в этом логе не обнаружены.

Check Map: **valid, 0 warnings** (`build/g019/artpass-map.json`). Fresh import GLB
и проверка сохранённого blend: **hard_gate_pass=true, issues=[]**
(`build/blender-review/g019-fill/{glb,source}.json`). GLB: 112421 triangles,
8 mesh objects/materials, 6 embedded images; degenerate faces/invalid vertices/
missing material faces — 0. Это геометрические метрики, замера frame time нет.
`demo-dev` обновлён (`artpass-play-build.log`). `check-model-assets.ps1` подтвердил
source/receipt/product/staged hashes всех **79 моделей** в Debug и Play
(`artpass-{debug,play}-assets.log`). После удаления неактивного диагностического
вывода graphics target повторно собран (`artpass-fixture-build.log`).

Просмотр выявил два локальных дефекта: collision-box стола рисовался поверх
пространства между тумбами (заменён explicit prop collider); shotgun support
1.20 м был ниже native столешницы 1.25 м (изменён только `pickup_surface_y`).
Стол архива смещён к стене: край −26.60 м оставляет 0.18 м сверх радиуса 0.42 м
врага на lane −26 м. Проходы и crossfire подтверждены повторным fixture.

Blender 5.2.2 LTS, выборочно изменён только `containment_lining`. OpenImageIO
повторно отказал в перезаписи отдельных существующих PNG при проверенных правах
и свободном месте. Scratch runner `build/g019/export-current.py` вызывает те же
`materials()/lining()/export()` с новым временным каталогом изображений; PNG
packed в blend и embedded в GLB, новых внешних runtime зависимостей нет.
Прежние source textures не меняются. Команда итерации:

```powershell
& build/g019/artpass-run.ps1 -Stage final -Final
```

Runner использует существующие presets/targets, MSVC developer environment,
Blender `--threads 2 --disable-autoexec`; native staging обновляет только нужный
reflection product и переиспользует актуальные diffuse probes. Архивов нет.

Этап 7 технически проверен в указанном объёме. Остаются непрерывный авторский
проход со звуком и приёмка наполнения: длительность, читаемость в самостоятельном
бою и звук этим запуском не подтверждены. Принятый gameplay G-019 остаётся `done`;
новое наполнение ожидает просмотра автора.
