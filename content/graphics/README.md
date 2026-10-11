# Лабораторная графика B-17

Оригинальные SVG-мастера и непрозрачные PNG без запечённого света, теней и AO:

| Файл | Размер | Использование |
| --- | --- | --- |
| `lab_notes` | 1600 × 900 | Печатная процедура изоляции, схема контактов C1–C4, сравнение сигнала с 50 Hz |
| `lab_pictograms` | 2048 × 1024 | Восемь предупреждений; панель в игре использует первые четыре |
| `instrument_display` | 1024 × 640 | Экран двухканального измерителя на пульте |

PNG упакованы в соответствующие GLB как base color. Размер доски 1.8 × 1.05 м;
панель предупреждений 0.80 × 1.1 м. Графика предназначена для осмотра с 1–3 м.
У этого экрана авторинг использует непрозрачный base-color материал без emissive-карты. Native pipeline в целом поддерживает emissive texture/factor с IMG-06; включать его для данного прибора автоматически не требуется.

Воспроизведение: `node scripts/build-lab-graphics.cjs` с доступным модулем `sharp`,
затем экспорт моделей Blender. Шрифты: Arial, системный запасной sans-serif.
Заимствованных логотипов и готовых изображений интерфейсов нет.

Материальные источники созданы встроенным imagegen по визуальным референсам
пользователя. Сохранены `assets/textures/facility_source.png` и
`assets/models/security_rover_source.png`; это исходники albedo.
Набор запросов: плоский лист 4 × 2 из бетона, шалфейной окрашенной стали,
керамической плитки, рифлёного настила, потолочных панелей, листовой стали, деревянного
ящика и предупреждающей полосы; отдельно лист 2 × 2 из шалфейной краски,
тёмной резины, состаренной латуни и грязновато-белой эмали для робота.
`prop_materials_source.png` содержит отдельный лист 4 × 2: шалфейная эмаль,
ABS-пластик, оцинковка, латунь, резина, защитная ткань, керамика и чернёная
сталь. Экспортёр вырезает материалы, сохраняет их в GLB и задаёт плотность UV,
metallic и roughness. Это локальные материалы оборудования без запечённых теней.
Ограничения запросов: ортогональный вид, без надписей, перспективы, бликов,
света, теней и AO; сдержанный локальный износ и читаемость в игровом масштабе.
Атлас собирается `scripts/build-facility-atlas.ps1`; текстура робота уменьшается
до 512 × 512 RGB BMP с сохранением раскладки квадрантов.

Проверка: штатный импорт моделей, проверки контента и проходов, игровые кадры
через `relaycore_power_restoration_graphics --art-review` в каталоге тестов.

## VIS-25: sector graphics and occult research

`sector_signage/{pressure,cooling,power}_{procedure,pictograms}.{svg,png}`
are opaque vector/PNG masters with a common border, margins, Arial typography,
step numbers and technical palette. They replace borrowed B-17 procedures and
warnings in the three sectors. `node scripts/build-sector-graphics.cjs` with
`sharp` available through `NODE_PATH` regenerates only these six graphics.

`sector_signage/blood_pentagram.png` is the owner-approved transparent crimson
paint master: regular star, concentric circles, interior glyphs and drips, with
no board, backing or SERVICE RECORD heading. `occult_research_board.png` is a
separate opaque red-marker decipherment study: partial circle, individual glyphs,
tentative connections, crossed-out hypotheses and unfinished work.
Both were created with built-in imagegen using the owner's reference; prompts
and technical integration are recorded in `docs/art/vis25-sector-signage.md`.

Blender reproduction: `--background --factory-startup --disable-autoexec
--threads 2 --python-exit-code 1 --python scripts/export-sector-signage-glb.py`.
This reuses the original board/notice construction, creates the research-board
variant, and cuts wall-paint geometry from the transparent master's coverage.
The original transparent PNG remains intact; native RGB is opaque, and uncovered
wall remains visible because no geometry is emitted there. No alpha pipeline or
new collision is introduced. The wall motif is 1.7 m across its authored square;
the research board uses the existing 1.8 by 1.05 m frame.

## Instrument faces (VIS-27–28)

`instruments/` contains original opaque SVG/PNG waveform, electrical reference,
archived surveillance and printed pressure/cooling circuit masters. Generate with
`scripts/build-instrument-graphics.cjs` (existing sharp dependency); then run
`scripts/export-instrument-graphics-glb.py` in Blender to update the five models.
The retained observation_frame.png comes from RelayCore's own B-17 runtime capture.
Static references are explicitly labelled; current gameplay states remain on
the existing interactive hardware. See docs/art/vis27-28-instruments-language.md.
