# RelayCore — производство статических моделей

Владелец текущего GLB → native workflow: импорт, source/export, manifest, compile/receipts, staging и reload. Collision products имеют [отдельный контракт](COLLISION_ASSETS.md); художественное направление — [ART_DIRECTION](ART_DIRECTION.md), окружение/build/tests — [DEVELOPMENT](DEVELOPMENT.md). Исторические результаты задачи ищите в её карточке, а не здесь.

## Статические модели: GLB → native product

### Blender: проектные навыки

Навыки установлены локально в `.agents/skills` и хранятся вместе с проектом.
Начальная точка — [relaycore-blender](../.agents/skills/relaycore-blender/SKILL.md):
она связывает моделирование, материалы, визуальную проверку и экспорт с правилами
RelayCore. Состав, исходные версии и локальные адаптации записаны в
[BLENDER_SOURCES.md](../.agents/skills/BLENDER_SOURCES.md).

Основной путь — установленный Blender и его Python через CLI. MCP, Bun и отдельный
Python-пакет `bpy` не требуются для моделирования, проверки геометрии и рендера.
Задайте `$env:BLENDER_EXECUTABLE` для текущего процесса либо найдите `blender` в PATH;
путь конкретного компьютера не хранится в проекте. Сначала проверьте `--version`.
В этой установке инструменты проверены с Blender 5.2.2 LTS.

```powershell
& $env:BLENDER_EXECUTABLE --background --factory-startup --disable-autoexec `
  --threads 2 --python-exit-code 1 `
  --python .agents/skills/blender-asset-validation/scripts/inspect_asset.py -- `
  --input content/models/power_cabinet.glb --output build/blender-review/metrics.json
```

Проверки и рендеры сохраняйте в `build/blender-review/<asset>/<revision>`;
воспроизводимые исходники — по существующим соглашениям `scripts` и `content/models`.
Сравнивайте форму с референсами, открывайте изображения с нескольких ракурсов и
дорабатывайте конкретные дефекты. Красивый рендер Blender не подтверждает совместимость
с игровым импортёром: ограничения следующего раздела остаются обязательными.

### Поддерживаемый импорт

Общие интерактивные корпуса VIS-17/18 воспроизводятся через
`scripts/export-interactive-hardware-glb.py`: `door_lift`, `door_slide` и четыре
`panel_*` модели. Полотно нормализовано в исходный движущийся AABB, направляющие
находятся вне swept depth. У `gameplay_entity` с `kind = panel/button/terminal/console`
необязательный render-only `panel_style` принимает `door`, `isolator`, `start`,
`instrument`; остальные значения вызывают диагностику. По умолчанию panel/button
использует door, terminal/console — instrument. Позиция кнопки, feedback, focus,
collision и действия задаются прежними игровыми данными.
`panel_style` исключён из level fingerprint, чтобы смена корпуса сохраняла
совместимость прогресса; gameplay properties и геометрия по-прежнему проверяются.

`relaycore_model_import` компилирует статический GLB в `.rcmodel`. Поддерживаются
triangle primitives с POSITION/NORMAL, TEXCOORD_0 для текстур, default scene,
иерархия и transforms узлов, повторное использование meshes, зеркальные экземпляры,
несколько opaque материалов с base-color factor и встроенными PNG/JPEG. Factors
применяются в linear space; base color хранит visual sRGB с top-left UV/image origin.
Normal map требует экспортированных FLOAT VEC4 TANGENT и TEXCOORD_0; без tangent
importer отказывает с инструкцией включить экспорт в Blender. Tangent xyz — единичный
вектор, перпендикулярный normal, w = ±1; handedness совпадает внутри треугольника.
[glTF basis](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes):
`B = cross(N,T) * w`. Mirrored nodes запекают X reflection в mesh и меняют tangent.w
вместе с winding; runtime трансформирует T обычной матрицей, N inverse transpose,
ортогонализует T и учитывает знак determinant.
Normal map хранит исходные linear UNORM RGB, +Y tangent convention, положительный Z;
alpha игнорируется. UV и green channel не переворачиваются. `normalTexture.scale`
сохраняется и применяется к XY перед нормализацией. В обоих цветовых режимах normal
texture всегда linear. Shadow receiver bias/offset использует геометрическую нормаль,
освещение — mapped normal.
Sampler — repeat, linear/trilinear; при поддержке `samplerAnisotropy` устройство
включает анизотропию до `min(16, maxSamplerAnisotropy)`. Без этой optional feature
сохраняется трилинейная фильтрация; GPU startup contract не расширяется. Выбранная
степень записывается в startup log. Неподдерживаемые attributes/extensions, skins,
animation, morphs, sparse accessors и alpha/double-sided
вызывают ошибку с путём source. IMG-01/03 поддерживают embedded metallic-roughness
(G=roughness, B=metallic) и occlusion (R) textures: linear RGBA8, UV0, repeat/linear,
без texture transform. MR maps требуют `--metallic-roughness`, умножаются на factors;
AO strength в [0,1] ослабляет только непрямой свет. Неиспользуемые каналы игнорируются.
IMG-06 adds core glTF emissive textures/factors: embedded PNG/JPEG RGB is sRGB,
alpha is ignored, and the independent linear RGB factor is finite and in [0,1].
The same UV0/sampler restrictions apply. RMDL v5 retains readers for v1–4 and
stores the map and factor separately; runtime emission maps always use sRGB
sampling, including the legacy color pipeline. Emission is added independently
of direct/indirect light and AO, before fog and tone mapping. Core factor-only
materials work; glTF extensions (including emissive-strength) remain unsupported.
The first consumer is the containment crosshead's illuminated plate, authored
by `scripts/author-containment-emission.py` and the owning containment exporter.
Its factor is zero in standby and restored when `grip_ready` powers the drive; it does
not illuminate geometry. A separate conditional light belongs to the existing
control indicator. Bloom was not required by this restrained first consumer.
Verification: `graphics.emissive_maps`, native/GLB cases and containment HDR/SDR.
Metallic/roughness scalars поддерживаются
при явном выборе новой модели освещения. По умолчанию importer сохраняет прежний
base-color/normal путь; `--metallic-roughness` включает glTF factors для всех материалов
модели. В декларации CMake этот выбор задаёт
`set(RELAYCORE_MODEL_METALLIC_ROUGHNESS_power_cabinet ON)`; `lab_bench` также
явно включён в metallic/roughness список, `service_cylinder` использует прежнее
освещение. Актуальный список opt-in моделей задаёт `cmake/RelayCoreModelAssets.cmake`.
Декларация входит в SHA256 receipts, смена выбора пересобирает product.

Новая модель — GGX distribution, separable Smith visibility, Schlick Fresnel и Lambert
diffuse по [glTF Appendix B](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation).
Factors — linear значения в [0,1]; invalid/nonfinite данные отклоняются. Roughness=0
сохраняется в product, но shader регуляризует сингулярный GGX lobe: perceptual roughness
минимум 0.05 (alpha минимум 0.0025). Это явное численное приближение.
Directional/local/spot используют текущие light/shadow contracts, геометрический shadow
bias и mapped normal для BRDF. Hemisphere ambient — приближение diffuse irradiance;
environment specular использует SH/hemisphere fallback из irradiance probes либо
статический GGX cubemap внутри bounds authored reflection probe. Динамических
отражений нет; условия bake описаны в разделе Static reflection probes ниже.
Для этой модели shading, fog и postfx вычисляются в linear space в обоих color modes,
затем legacy кодирует RGB вручную, hardware использует sRGB attachment. Прежние материалы
сохраняют свою цветовую обработку. SDR saturation сохраняется в default-пути;
opt-in HDR/exposure описаны в разделе Native HDR profile ниже.

```powershell
cmake --build --preset demo-debug
# Для работы только с ресурсами или полигоном:
cmake --build --preset demo-debug --target relaycore_demo_assets
cmake --build --preset demo-debug --target relaycore_sandbox
```

Модели подключены к сборке через явный список в `cmake/RelayCoreModelAssets.cmake`.
Первый slice E-004 включал `service_cylinder.glb`, `lab_bench.glb` и
`power_cabinet.glb` из `content/models`; текущий список также содержит последующие
facility, signage, interactive hardware и weapon модели.
Для новой модели добавьте GLB в `content/models` и имя файла в этот список
(латинские буквы, цифры, `_` и `-`, без подкаталогов);
product получает путь `models/<имя>.rcmodel`. Файл не нужно добавлять в `assets`:
двойное authored/generated владение одним путём вызывает ошибку сборки.

Каждая конфигурация создаёт свои products в `build/<preset>/generated/model-assets`,
затем синхронизирует их в `build/<preset>/src/relaygame/assets` вместе с authored
ресурсами и SPIR-V. Изменение GLB, importer executable или compiler rules вызывает
перекомпиляцию; обычная повторная сборка не переписывает неизменившуюся модель.
Неподдерживаемый/повреждённый GLB останавливает сборку до staging и сохраняет
прежний рабочий product. CTest и Check Map используют полный staged runtime root.

`generated/model-receipts` хранит build-only receipts с SHA256 source/importer/
compiler rules/product. Упаковка сверяет их с текущими файлами и staged product,
поэтому отклоняет устаревшую модель даже при сохранённом timestamp исходника.
Это timestamp-based CMake dependency tracking и проверка целостности при упаковке;
content-addressed cache, streaming и автоматический Blender export сюда не входят.
GLB и receipts не попадают в ZIP. Сам CLI остаётся доступен для отдельного импорта,
например `build/msvc-debug/relaycore_model_import.exe source.glb build/test.rcmodel`.

Для проверки текущей сборки без упаковки используйте существующий checker:

```powershell
pwsh -NoProfile -File scripts/check-model-assets.ps1 -ProjectRoot . `
  -BuildRoot build/msvc-debug -RuntimeAssets build/msvc-debug/src/relaygame/assets
```

Он сверяет source/importer/rules/declaration/product SHA256 и staged product для
каждого пути из `generated/runtime-models.txt`. Stale state исправляется повторной
компиляцией исходника, а не ручной правкой receipt. `.rcmodel` остаётся build-only
product; воспроизводимый export и editable source хранятся в `scripts`/`content`.
Отдельный native collision opt-in и его receipts описаны в [COLLISION_ASSETS.md](COLLISION_ASSETS.md).

Сценарий `scripts/export-service-cylinder-glb.py`
запускается в Blender через `--background --disable-autoexec --python`; он использует
исходный project OBJ/BMP, сохраняет метры/+Y вверх и выделяет корпус/вентиль.

В Sandbox баллон у устройств (`[29,0,2.5]`) использует новый путь. `.rcmodel`
доступен в каталоге моделей редактора; материалы входят в файл, поле `texture`
для него отсутствует. Коллизия остаётся отдельным authored AABB. F7 после компиляции
и сборки подготавливает новый ресурс, неудачный reload сохраняет рабочий; bounds
редактора обновляются при загрузке карты. Runtime не содержит fastgltf/WIC.

`prop_static` может задать `casts_shadow = false` для плоской настенной краски:
экземпляр остаётся видимым и освещаемым, но исключается из всех теневых проходов.
По умолчанию тени включены; допустимы только `true`/`false`. Этот render-only выбор
не меняет коллизию. Четыре размещения `blood_pentagram` используют отключение теней.

Проверочная native-комната N1 — `assets/levels/native_lab.toml`. В меню редактора
выберите «Открыть карту → native_lab.toml», затем «Играть» для preview. Три стола,
два шкафа и три баллона используют общие model products с разными поворотами и
масштабами; коллизия каждого экземпляра — явно заданный local AABB. Верхняя кнопка
основного шкафа включает рабочий свет, нижняя открывает кладовую. Это content fixture
без миссии, а не новый эпизод или завершённая production assets phase C.

`scripts/export-native-lab-glb.py` создаёт стол и шкаф в Blender, сохраняет editable
`.blend` и экспортирует GLB в `content/models`. Запуск: Blender
`--background --disable-autoexec --python scripts/export-native-lab-glb.py`.
Размеры в метрах; origin на полу; Blender +Z вверх / фасад -Y переходит в runtime
+Y вверх / фасад +Z. Окрашенная сталь шкафа использует normal map покрытия;
`power_cabinet_normal.png` — воспроизводимый source, также packed в `.blend` и встроен
в GLB. Для повторного экспорта только шкафа добавьте `-- --cabinet-only`.
После изменения GLB выполните обычную сборку, затем повторно запустите preview
или нажмите F7 в обычной игре. Во время editor preview F7 не выполняет reload:
редактор сохраняет собственное состояние для возврата.
Карта native fixture и её три `.rcmodel` входят в runtime staging и запрошенный
playtest ZIP; Blender/GLB для запуска не требуются. Fixture выбирается отдельно
от кампании и не заменяет её стартовую главу.

Compiler пишет product v5. Loader читает v1–v5; v1 остаётся base-color-only,
v2 — base-color/normal, оба используют прежнее освещение. Старые runtimes не читают
новые версии products: для обновлённого runtime
products пересобираются обычной сборкой, receipts/staging учитывают изменение importer.
Little-endian header — RMDL/version/mesh/material/part counts. В v2 mesh начинается
с flags (бит 0 = tangents), vertex/index counts; vertex — 16 float (position, color,
normal, material_kind, UV, tangent). После base-color width/height/RGBA каждого
материала идут normal width/height, float scale и RGBA; 0/0 означает отсутствие карты.
Part mesh/material и column-major transform не изменены. Нет зависимости от GLM padding.
В v3 после normal payload каждого материала добавлены uint flags (бит 0 = новая модель
освещения), float roughness и float metallic; при flags=0 записываются 1/0. V1/v2 не
приобретают новую модель при чтении или повторной записи. V4 после v3 factors
хранит MR width/height/RGBA, AO width/height/RGBA и float AO strength; 0/0 означает
отсутствующую карту. V5 дописывает emissive width/height/RGBA и три linear factor
компоненты; map является sRGB, factor независим от map. Loader сохраняет чтение
v1–v4 без изменения прежнего освещения.
Normal/MR/AO/emissive maps остаются встроенными, repeat/linear, с GPU mip chain, без TextureAsset/KTX2;
отдельные текстурные продукты и mip policy требуют следующего consumer.
Loader проверяет размеры, indices, finite geometry/transforms,
материалы и точный конец файла до создания runtime resources; лимит product 256 MiB.

Для проверки normal slice:

```powershell
ctest --preset test-debug -R '^relaycore\.(native|GLB|Blender|tooling.model_assets)' --output-on-failure
ctest --preset graphics-debug -R '^graphics\.(vulkan_mesh|vulkan_color_baseline|power_restoration)$' --output-on-failure
# RenderDoc: 120 кадров native_lab в hardware_srgb, шкаф крупным планом.
# Рабочая директория build/msvc-debug/tests; RELAYCORE_ASSET_ROOT — staged assets.
./relaycore_vulkan_mesh_tests.exe --profile-native
# Только analytical BRDF/material checks в обоих color modes:
./relaycore_vulkan_mesh_tests.exe --metallic-roughness
```
