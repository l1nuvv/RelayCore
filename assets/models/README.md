# Статические модели

Текущий pipeline: GLB → native `.rcmodel` v5 (reader принимает v1–5), с отдельными collision/probe products. Авторинг и staging описаны в [MODEL_ASSETS](../../docs/MODEL_ASSETS.md#статические-модели-glb--native-product). Ниже сохранены источники прежних OBJ/BMP и native-семейств; OBJ не задаёт материалы текущего native viewmodel.

`shotgun_body.obj` и `shotgun_pump.obj` — Stevens Model 520 из предоставленного
автором архива `low-poly-stevens-model-520a.zip` ([источник](https://sketchfab.com/3d-models/low-poly-stevens-model-520a-275af36ee49545f3bdf117329f72745f)).
Корпус — 1206 треугольников, цевьё — 380; патроны исключены. Общая длина 1,02 м,
+Y вверх, ствол по -Z. `shotgun.bmp` — 32×32, палитра цветов исходных материалов
без освещения и AO. Две жёсткие части используют одну палитру; помпа двигается
отдельным transform в игре. OBJ остаются исходной геометрией обоих представлений.
Руки и готовые анимации в supplied исходнике отсутствуют. VIS-35 использует эти
OBJ без замены геометрии для native viewmodel с перчатками/рукавами и отдельными
материалами: `scripts/export-shotgun-view-glb.py`,
`content/models/shotgun_view.blend` и `shotgun_view_{body,pump}.glb`.
VIS-36 экспортирует из тех же двух meshes и материалов `shotgun_world.glb` без
рук. Runtime pickup использует `models/shotgun_world.rcmodel`, лежит на боку и
сохраняет исходные 1,02 м. `pickup_surface_y` указывает render-only высоту опоры;
origin/size взаимодействия и save fingerprint от этого поля не меняются.

Для повторного экспорта открыть исходный `.blend` через Blender в background:

```powershell
blender --background --factory-startup --disable-autoexec 'Stevens Model 520.blend' --python scripts/export-shotgun.py
```

`security_rover.obj` — оригинальная 0.95-метровая модель охранного робота для 1.2:
скошенный корпус, четыре колеса, парная круглая оптика и передняя защитная накладка, 684 треугольника.
Диффузная текстура `security_rover.bmp` (512 × 512) создана отдельно для него;
исходник — `security_rover_source.png`, геометрия —
`scripts/build-security-rover.py`. Робот размещается в редакторе как
`enemy_rover`, модель и текстура загружаются через кэш рендерера.
`security_rover_destroyed.obj` — отдельная модель обломков: корпус завален на бок,
верхний блок и два колеса отброшены. Она использует ту же диффузную текстуру и
создаётся тем же скриптом. Мёртвый робот не участвует в столкновениях и бою.

`security_turret.obj` — настенная турель: крепление, шаровая головка на подвесе, ствол
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

## Лабораторный набор B-17

Исходники в `content/models`: `.blend` для редактирования и `.glb` для штатного
импортера `.rcmodel`. Размеры в метрах; в игре +Y вверх, лицевая сторона +Z.
`scripts/export-native-lab-glb.py` воспроизводит стол, силовой шкаф, CRT-терминал,
приборную стойку, вентиляцию, резонансную установку, вывеску, защитный костюм,
бетонную арку, трубы с клапаном, доску, предупреждения, принадлежности и пульт.
Фигура учёного статична. Вооружение использует существующий Stevens 520.
Микроскоп `lab_microscope`, мультиметр `lab_multimeter`, монитор
`surveillance_monitor` и прибор `analog_navigation_instrument` адаптированы из
CC0-моделей Poly Haven через `scripts/export-polyhaven-lab.py`. Источники,
авторы и преобразования перечислены в `content/third_party/polyhaven/README.md`.

```powershell
blender --background --factory-startup --disable-autoexec --python scripts/export-native-lab-glb.py
# Для одного изменённого исходника:
blender --background --factory-startup --disable-autoexec --python scripts/export-native-lab-glb.py -- --model=lab_notes_board
```

Печатная графика и её воспроизведение описаны в `content/graphics/README.md`.
Набор размещён в `occult_lab_b17.toml`, а служебные детали — на пяти игровых
картах. Стол сохраняет существующую нативную коллизию и открытый проход снизу.
`scripts/run-lab-b17.ps1` запускает существующую RelWithDebInfo-сборку с профилем
B-17 и `hardware_srgb`, как в проверочных кадрах.
Новые крупные предметы B-17 используют явно заданные AABB; настенные детали
и мелкие принадлежности не перекрывают маршруты.

## Импорт OBJ

Экспортировать OBJ с триангуляцией, UV и нормалями; Y вверх, размеры в метрах.
Поддерживаются `v x y z`, `vt u v`, `vn x y z`, треугольные `f v/vt/vn` с положительными
и отрицательными индексами. Сглаживание должно быть записано в нормалях.
`o`, `g`, `s`, `mtllib`, `usemtl` не меняют геометрию: карта явно назначает одну
diffuse-текстуру всему мешу; MTL и несколько материалов пока не импортируются.
Отсутствующие UV/нормали, вырожденные треугольники, полигоны и неподдерживаемые
геометрические команды отклоняются с именем файла и строкой.

Текстура — BMP RGB/RGBA, непрозрачный albedo. UV имеют начало снизу слева.
Применяются освещение, туман, тени и цветовой режим сцены; процедурный материал
стены поверх модели не накладывается. Alpha-cutout, скелет и анимации не поддерживаются текущим static importer.

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
его материал определяет шаги и приземление. Треугольники OBJ не становятся коллизией.

Для native `models/<имя>.rcmodel` доступен отдельный opt-in
`collision_asset = "collision/<имя>.rccollision"` вместо трёх box-полей выше.
Collision product явно связан с моделью, компилируется из отдельного authored source
и проверяется при подготовке сцены; render triangles не используются автоматически.
`lab_bench` сохраняет проход снизу через `content/collision/lab_bench.toml`.
Контракт, сборка, receipts и editor inspection описаны в
[COLLISION_ASSETS.md](../../docs/COLLISION_ASSETS.md), GLB import и scalar
metallic/roughness opt-in — в [MODEL_ASSETS](../../docs/MODEL_ASSETS.md#поддерживаемый-импорт).

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
