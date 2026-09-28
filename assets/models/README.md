# Статические модели — 0.7

`security_rover.obj` — оригинальная 0.95-метровая модель охранного робота для 1.2:
скошенный корпус, четыре колеса и передняя защитная накладка, 620 треугольников.
Диффузная текстура `security_rover.bmp` (512 × 512) создана отдельно для него;
исходник — `security_rover_source.png`, геометрия —
`scripts/build-security-rover.py`. Робот размещается в редакторе как
`enemy_rover`, модель и текстура загружаются через кэш рендерера.
`security_rover_destroyed.obj` — отдельная модель обломков: корпус завален на бок,
верхний блок и два колеса отброшены. Она использует ту же диффузную текстуру и
создаётся тем же скриптом. Мёртвый робот не участвует в столкновениях и бою.

`security_turret.obj` — настенная турель: крепление, бронированный корпус, ствол
и отдельный датчик. Модель нормализована под размер `gameplay_entity` с `kind = "turret"`;
передняя сторона направлена по +Z, `angles` задаёт поворот вокруг Y. Собственная
диффузная текстура `security_turret.bmp` содержит окрашенный металл, тёмный корпус,
сталь и сигнальный цвет. `security_turret_destroyed.obj` сохраняет крепление
на стене и опускает повреждённую головку со стволом; его текстура
`security_turret_destroyed.bmp` гасит датчик. Модели и текстуры воспроизводятся скриптом
`scripts/build-security-turret.py` без внешних зависимостей.

`power_fuse.obj` — керамический силовой картридж с металлическими контактами
для предмета `power_fuse` на столе входного помещения. Диффузная текстура
`power_fuse.bmp` и геометрия создаются скриптом `scripts/build-power-fuse.py`.

`service_cylinder.obj` — оригинальный баллон высотой 1.508 м с округлым плечом,
открытым маховиком и спицами, 832 треугольника. Исходник геометрии:
`scripts/build-service-cylinder.py`. Экземпляры стоят у стены стартовой комнаты
в `power_restoration.toml` и `test_room.toml`.

## Импорт

Экспортировать OBJ с триангуляцией, UV и нормалями; Y вверх, размеры в метрах.
Поддерживаются `v x y z`, `vt u v`, `vn x y z`, треугольные `f v/vt/vn` с положительными
и отрицательными индексами. Сглаживание должно быть записано в нормалях.
`o`, `g`, `s`, `mtllib`, `usemtl` не меняют геометрию: карта явно назначает одну
diffuse-текстуру всему мешу; MTL и несколько материалов пока не импортируются.
Отсутствующие UV/нормали, вырожденные треугольники, полигоны и неподдерживаемые
геометрические команды отклоняются с именем файла и строкой.

Текстура — BMP RGB/RGBA, непрозрачный albedo. UV имеют начало снизу слева.
Применяются освещение, туман, тени и цветовой режим сцены; процедурный материал
стены поверх модели не накладывается. Alpha-cutout, скелет и анимации не входят в 0.7.

```toml
[[entity]]
classname = "prop_static"
origin = [-4, 0, -3]
angles = [30, 0, 0] # yaw, pitch, roll, градусы
scale = [1, 1, 1]   # необязательно, положительные компоненты
model = "models/service_cylinder.obj"
texture = "models/service_cylinder.bmp"
collision_mins = [-0.25, 0, -0.25]
collision_maxs = [0.25, 1.52, 0.25]
collision_material = "Metal"
```

Пути относительны к `assets`. Коллизия необязательна; при её наличии все три
поля обязательны. Это отдельный локальный бокс, после трансформации превращаемый
в охватывающий мировой AABB. Он блокирует движение, выстрелы и взаимодействие;
его материал определяет шаги и приземление. Точная коллизия по треугольникам не используется.

## Владение и перезагрузка

`Renderer::static_models()` владеет кэшем модели и текстуры до уничтожения рендерера.
Подготовка `WorldSceneRenderer` получает общие неизменяемые ресурсы; экземпляры
хранят только ссылку и матрицу. В кадре нет загрузки или проверки файлов.
При подготовке/перезагрузке сцены сравниваются размер и время изменения обоих
файлов: неизменённая пара переиспользуется, изменённая загружается до замены записи.
Старые сцены сохраняют старый ресурс до освобождения ссылки. Ошибка загрузки
не заменяет рабочую сцену. При внешнем редактировании нужно сохранять новое время
изменения файла. Цветовые режимы используют разные GPU-текстуры.

Игра переводит сущности карты и текущее состояние в очередь отрисовки.
Движковый `Renderer` получает меш, матрицу, проход, необязательную diffuse-текстуру,
свет и камеру. Он не читает задания, инвентарь или условия открытия дверей.
`RelayGame::WorldSceneRenderer` — игровая подготовка представления, а не движковый рендерер.

## Текстура баллона

`service_cylinder_source.png` создан встроенным imagegen. `service_cylinder.bmp`
— уменьшенная до 256×256 копия для текущего загрузчика SDL. Это самостоятельный
материал окрашенного металла, не фрагмент атласа стен.

Prompt: “Create a seamless tileable square base-color diffuse texture for an original
retro industrial game asset: muted desaturated olive-green enamel painted metal on
a portable compressed-gas cylinder. Flat orthographic material scan, fills entire
image edge to edge, very restrained fine paint mottling, subtle small chips showing
gray steel, most paint intact, medium value olive gray-green. No lighting, no shadow,
no ambient occlusion, no highlights, no objects, no borders, no text, no logos, no
labels, no seams, no perspective. This is a pure albedo material for UV mapping a
1.4 meter high cylinder in a Half-Life-era inspired original indoor laboratory game;
readable at 256x256. Save the generated asset for use in the local project.”
