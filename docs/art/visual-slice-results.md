# Visual slices — результаты реализации

Детали IMG/VIS из прежнего общего roadmap. Действующие статусы и открытая приёмка — в [roadmap](../PLAYABLE_SLICE_ROADMAP.md); команды и кадры — в связанных отчётах `art/`.

## IMG-01

Native v4 metallic/roughness maps; Guardian and containment ram authored. Debug/Play updated; 23 native/tooling + 7 graphics checks passed. [Evidence](img01-03-materials-reflections-ao.md)

## IMG-02

Static cube bake, GGX mips, box projection and SH fallback; first probe in containment hall. Bounds/camera/mips/invalidation checked; matched captures retained. [Evidence](img01-03-materials-reflections-ao.md)

## IMG-03

Native AO maps/strength; Guardian/ram geometry AO, indirect lighting only. Direct-light isolation and strength checked; visual acceptance in motion open. [Evidence](img01-03-materials-reflections-ao.md)

## IMG-04

Containment hall practical light balance and conditional control light; reflection rebaked. Matched HDR/SDR, power and flashlight views reviewed; author acceptance during the containment route remains open. [Evidence](img04-06-lighting-stability-emission.md)

## IMG-05

Bounded mapped-normal GGX roughness filtering; isolated 48-frame HDR/SDR camera arcs show a modest 4.9–6.0% reduction in the measured temporal-difference proxy. Author review of highlights, small text and peripheral detail in motion remains open. [Evidence](img04-06-lighting-stability-emission.md)

## IMG-06

Native v5 emissive texture/factor, old readers and powered/off crosshead plate; Debug/Play staged. 20 native/tooling + 9 graphics checks passed, no skips; author visual acceptance of the containment power states remains open. [Evidence](img04-06-lighting-stability-emission.md)

## VIS-47

Проверены 71 source/export owner и model receipt/product/staged SHA256 в Debug/optimized; native collision pairing актуален, девять карт проходят Check Map без warnings. Правило проверки сохраняется для будущих изменений; [scope и evidence](../VISUAL_REVAMP_AUDIT.md#vis-47--vis-50--проверка-pipeline-и-документации-2026-10-09)

## VIS-50

Уточнены lab_bench metallic/roughness opt-in, Vulkan atlas UV convention и отдельная native collision; DEVELOPMENT описывает текущий manifest и checker без упаковки. [Evidence](../VISUAL_REVAMP_AUDIT.md#vis-47--vis-50--проверка-pipeline-и-документации-2026-10-09)

## VIS-43

RmlUi 6.3 foundation/HUD реализованы; четыре разрешения, кириллица, hit/hurt/death, SDR/HDR и developer overlay проверены; [evidence и ограничения](../tickets/VIS-43-46.md#vis-43--переход-на-игровой-hud-и-ui)

## VIS-44

Main/pause/settings, scenario/new/continue, save/settings persistence перенесены; SDL mouse/Tab/Enter и блокировка gameplay проверены без ImGui

## VIS-45

Death/respawn, completion/finale, transition errors перенесены; lifecycle и campaign route проходят

## VIS-46

Игровые ImGui call sites удалены; обычный маршрут проверен с ImGui disabled, editor/diagnostics сохраняются

## VIS-48

Локальная оценка текущего kit: 6 460 optimized frames, 218–238 world draws / 157 222–158 328 triangles, warm p99 2,931–6,756 мс; gameplay mesh uploads отсутствуют, восемь first-use texture uploads относятся к RmlUi atlas. [Измерение и границы](vis48-49-cost-and-review.md#vis-48-one-current-measurement); непрерывный sound-on проход и слабый ПК остаются H-008

## VIS-49

Известный power mismatch воспроизведён: scene-only failing tiles сосредоточены на новой двери VIS-17; shotgun samples отражают VIS-35. Четыре эталона обновлены после просмотра, tolerances сохранены; 3/3 graphics без skips, power SDR/HDR stages проходят. [Evidence и короткий маршрут](vis48-49-cost-and-review.md); авторская приёмка в движении/бою открыта

## VIS-03

Выходная облицовка, двусторонняя рама, EXIT/sector/reader signage и корпус светильника; свет и видимый emitter зависят от `transfer_running`. Игровые solids/entities/mission сохранены; scripted exit/trigger проходят

## VIS-04

Проверены фиксированные виды стыков до/после перепуска и открытая дверь; исправлены обратная отделка входа в зал 03, порог, потолок и высота dado. Техническая проверка завершена; непрерывный маршрут и субъективная приёмка человеком остаются открытыми

## VIS-05

Вход `power_restoration`: отделка, стол предохранителя, шкаф, порталы и указатели; кадры до/после, оба состояния питания и обратный вид проверены; [исходники и evidence](power-restoration-visual-slice.md)

## VIS-06

Служебные крылья, оформленные электрощиты, резервное питание, фильтрация и обратные указатели; исходные solids/взаимодействия/различия полов сохранены; route/retry/snapshot regressions проходят

## VIS-07

Насосная: привод, двигатель, муфта, манометр и связанные трубы/узел утечки; одинаковая камера standby/power/flush/cooling и шесть нейтральных видов; исходные steam/fan/button/door states сохранены

## VIS-08

`bench_relay`: штукатурка/dado, потолочная сетка, рамы, корпуса светильников, шкаф-укрытие и указатели. Открытые `lab_bench` и native collision сохранены; rover/low-ray и обе тактики проходят. По скриншоту автора 2026-10-09 турель перенесена креплением к боковой стене; 5/5 checks и mounting capture проходят, Debug/Play staged. [Сравнения и evidence](bench-cooling-visual-slice.md)

## VIS-09

`cooling_loop`: три растянутых casing заменены единым теплообменным банком с оребрением, коллекторами, обслуживаемыми торцами и overhead supply/return; оформлены стол и isolation post. Исходные solids/обход/бой/состояния сохранены; signage завершён отдельно в VIS-10

## VIS-10

Cooling-specific таблички закрывают прежние pressure/transfer/bypass-подписи; 01 — ISOLATE, 02 — COOLING START, 03 — EXIT RELEASE согласованы с HUD/подсказками. Shared корпуса и игровая логика сохранены; [сравнение и проверки](bench-cooling-visual-slice.md#vis-10--cooling-signage-2026-10-08)

## VIS-11

Автор принял ключ и исчезающую рамку; после тисков и инструмента сектор 01 читается лучше. По повторному отзыву устранён ложный pickup из шага задания, снижен пересвет, подняты корпуса светильников для устранения полосы тени, исправлены размеры и крепление пускового пульта и дверных кнопок. Debug/optimized обновлены; восемь профильных проверок проходят, HDR/SDR и контакт панелей проверены; [отзывы, сравнения и evidence](foundation-intake-visual-slice.md). Требуется повторная визуальная приёмка автора; теснота насосной остаётся отдельным замечанием

## VIS-12

Автор принял визуальную итерацию: шкаф с насосом читается, детали закреплены, визуальных конфликтов нет, указатели понятны. Применение ключа понятно из-за отсутствия альтернатив — это наблюдение о линейности, а не подтверждение качества навигации. 7/7 технических проверок, HDR/SDR без skips, Check Map без warnings, Debug/optimized staging проверены; [сравнение, evidence и отзыв](foundation-pump-visual-slice.md). Теснота и линейность комнаты остаются вопросами level design; последние правки VIS-11 этим отзывом не приняты

## VIS-13

B-17: наблюдательное стекло с рамой/уплотнениями, рабочие группы и образцы, подключённые кабели/трубы и крепления, свет лицевой стороны учёного и установки; существующие apparatus/приборы/мебель переиспользованы. Solids/colliders/entries/mission сохранены. Debug/optimized staging, 5/5 профильных проверок и повторные 3/3 после правки контактов; HDR/SDR без skips, отдельный Debug capture с Vulkan validation, Check Map без warnings. Автор принял ревамп визуально 2026-10-08; пересвет SDR остаётся открытым замечанием к обработке изображения. [Сравнение, исходники, отзыв и границы](b17-visual-slice.md)

## VIS-14

B-17 и входной коридор: облицовка стен/колонн/балок, защитный пояс, цоколь, потолок и рама; разные полы, окно и исходные игровые данные сохранены. Room-fitted сборка на прежней палитре, 8 100 triangles, без общего генератора. 12/12 content/tooling, 4/4 graphics без skips, Debug validation capture без ошибок; Debug/optimized staging совпадает. [Сравнение и evidence](vis14-16-facility-finish.md). Субъективная авторская приёмка нового прохода открыта

## VIS-15

Муфты повторяемых труб в pressure bay/hall/gallery и B-17, отводы в стены, горловины и опоры вентиляции; исходные кабели/подвесы сохранены. Прямые/обратные виды и шесть крупных планов проверены; Check Map четырёх карт без warnings, общий validation capture чистый. [Исходники, scope и проверки](vis14-16-facility-finish.md); не заявлена проверка каждого соединения проекта

## VIS-16

Общая модель шкафов power/bench: уплотнение двери, петли, ручки с опорами, крепёж и кромка крышки. Исправлена высота панели низкого насосного корпуса; pump/exchanger семейства переиспользованы. Девять затронутых моделей прошли authored/fresh-import checks; все 44 native products/receipts/staged hashes проверены в Debug/optimized. [Сравнение и evidence](vis14-16-facility-finish.md); человеческая визуальная приёмка открыта

## VIS-17

Два native полотна lift/slide: прессованные панели, кромки, нижние накладки, безопасная кромка lift и утопленная ручка slide. Неподвижные направляющие вне полного swept depth; исходные AABB, движение, clearance и activation policy сохранены. Проверены closed/mid/open и обратные виды обеих ориентаций; [исходники и evidence](vis17-19-interactive-hardware.md). Человеческая визуальная приёмка открыта

## VIS-18

Четыре native корпуса: door/isolator/start/instrument, локальные подписи, защитные щёки и аналоговая шкала. Render-only panel_style отделён от действий; прежние положения кнопок и neutral/focused/blocked/warning/success/consumed сохранены. [Все состояния и evidence](vis17-19-interactive-hardware.md); требуется человеческая визуальная приёмка

## VIS-19

Переиспользован уже принятый автором service_key из VIS-11: отверстие головки, плечо, стержень и два зубца; узнаваемость/highlight/исчезновение подтверждены прежним отзывом. Повторно проверены HDR/SDR focus/collection, перенос ключа, пуск насосов, continue/retry; fuse/shotgun и геометрия ключа не изменены. [Проверка и сверка устаревшего аудита](vis17-19-interactive-hardware.md)

## VIS-20

Уменьшен шум бетона и краски общей палитры, cooling получил штукатурку/защитный пояс; исправлена скрытая внутри прежних стен облицовка B-17. Same-camera HDR/SDR сравнения проверены; игровые карты/solids/свет сохранены. [Изменения и evidence](vis20-22-material-pass.md); авторская визуальная приёмка открыта

## VIS-21

Для 14 моделей проверены metre UV, цвет и износ; согласованы повторы штукатурки/эмали/металла. Legacy atlas и его 32 gutters сохранены; исходные масштабы пола, дерева и ткани проверены. Ближние/маршрутные, прямые/теневые виды просмотрены; [границы и проверки](vis20-22-material-pass.md)

## VIS-22

Локальные linear +Y normals и scalar roughness/metallic для эмали, стали, резины и меди; исправлена ячейка galvanized albedo. Tangents проверены native importer, 14 authored/fresh imports проходят; Debug/optimized products совпадают, 18/18 content/tooling и 10/10 graphics без skips. [Сравнение и evidence](vis20-22-material-pass.md); человеческая приёмка и прежнее замечание к SDR остаются открытыми

## VIS-23

После замечания автора о незаметном результате усилены причинные сколы, трещины, потёртости и коррозия крепежа/нижних кромок; оформлен след существующей утечки. Девять моделей, исходные triangles/material pixels/maps и bounds дверей сохранены; Debug/optimized staging, 15 content/native, tooling fixture и 7 разных graphics cases без skips проверены. [Сравнение, отзыв и evidence](vis23-causal-wear.md); требуется визуальная приёмка автора

## VIS-24

Проверены стекло, CRT и ткань B-17; у native CRT roughness экрана/кромки изменён с 1.0 на 0.23. Imported monitor, прозрачное окно и cotton/rubber учёного переиспользованы. Геометрия/UV/images/maps сохранены; Debug/optimized receipts/staging совпадают, 5/5 content/tooling и 2/2 HDR/SDR graphics без skips проходят. [Сравнение и evidence](vis24-special-materials.md); изменение локальное и малозаметное, требуется визуальная приёмка автора

## VIS-25

Три локальные процедуры и три набора предупреждений pressure/cooling/power на общих корпусах; чужие B-17 инструкции заменены. По указанию автора — крупная кровавая пентаграмма на стенах и отдельная незавершённая доска расшифровки в B-17. Принят standalone blood master; финальные игровые виды подлежат авторской оценке. [Исходники и проверки](vis25-sector-signage.md)

## VIS-26

Девять сохранённых arrow/01–03 интегрированы в физические таблички pressure/cooling/power; четыре указателя прямого/обратного пути, согласован номер 03 щитовой. Перекрытия кабелем, приборами, турелью и колоннами исправлены; игровые solids/entities/mission сохранены. Debug/optimized assets и 61 receipt/product/staged hash совпадают; финальные 4/4 проверки включают HDR/SDR и powered/open виды, Check Map трёх карт без warnings. [Исходники, сравнение и проверки](vis26-route-signage.md). Автор прошёл маршрут и принял результат 2026-10-08: «вроде все ок». Независимый новый игрок не заявлен; дубликаты сохранены

## VIS-27

Разделены waveform/electrical/surveillance и печатные pressure/cooling; физическая CAM 02 соответствует архивному ракурсу, схемы распределены по семи игровым картам. [Исходники и проверки](vis27-28-instruments-language.md); Debug build, 42/42 focused checks и финальные 32 вида проверены

## VIS-28

Русский — обязательный язык игровых инструкций; английские institutional labels сохранены осознанно. Проверены русские строки, IDs и target chains; стандарт в ART_DIRECTION.md. [Evidence](vis27-28-instruments-language.md)

## VIS-29

Настроены 33 существующих светильника (30 authored записей) на пяти картах; intake/foundation pump сохранены после проверки. Локальный рабочий свет и аварийные акценты, пониженная общая засветка B-17; экспозиция и игровые данные сохранены. 20/20 проверок, 128 финальных HDR/SDR видов, Check Map семи карт без warnings, Debug/optimized staging и probes совпадают. [Сравнение и evidence](vis29-room-lighting.md); требуется визуальная приёмка автора в движении

## VIS-30

Согласованы 46 светильников и отдельный источник аппаратуры B-17 на семи картах: размеры/высота/направление плоскостей, четыре корпуса, крепление под кабельным коробом, on/off/damaged состояния. Удалены 24 старые полосы; настройки света и gameplay сохранены. 21/21 проверок и финальные 7/7, HDR/SDR без skips; Debug/Play staging проверен. [Кадры и evidence](vis30-light-fixtures.md); требуется визуальная приёмка автора в движении

## VIS-31

Проверены 15 contact views в HDR/SDR: bench, шкафы, трубы, двери, турель, учёный и три положения rover. Исправлен подтверждённый зазор 9 мм под обувью; геометрия выше подошвы, поза и коллизия сохранены. Тени сравнены с отключёнными. [Кадры и проверки](vis31-33-contact-atmosphere-focus.md); требуется авторская оценка в движении

## VIS-32

Проверен 31 маршрутный вид в HDR/SDR на семи картах, включая powered/open и последовательные позиции камеры. Fog/postfx сохранены по сравнению с отключённой атмосферой; добавлена регрессия неизменности непрозрачного HUD pixel. [Evidence](vis31-33-contact-atmosphere-focus.md); непрерывный человеческий проход открыт

## VIS-33

Минеральный образец B-17 отделён материалом от опоры; pressure получил две импульсные линии к паре приборов и согласованную P1/P2/Δp-схему в двух моделях. 14/14 основных и 6/6 финальных проверок без skips; Debug/Play, 66 receipts каждого дерева, пять products и три probes проверены. [Сравнение и исходники](vis31-33-contact-atmosphere-focus.md); требуется авторская визуальная приёмка

## VIS-34

Стартовое точное оружие получило native служебный пистолет с перчаткой/рукавом, отдельным затвором и recoil/recovery; UI называет слот «Пистолет». Hitscan, урон, темп и save schema сохранены. 13 различных checks проходят, graphics 3/3 без skips, 60 HDR/SDR поз/FOV/окон; fresh-import 13 514 triangles, Debug/Play и 68 receipts каждого дерева проверены. [Кадры, исходники и evidence](vis34-service-pistol.md); требуется авторская визуальная приёмка

## VIS-35

Сохранён Stevens 520; native viewmodel получил две перчатки/рукава, дерево/сталь и согласованные с прицелом положение/recoil/recovery. Левая рука следует прежнему 70-мм циклу цевья; gameplay/save и pickup сохранены. 15/15 выбранных checks, graphics 4/4 без skips, 76 HDR/SDR кадров; authored/fresh import 16 594 triangles, Debug/Play и 70 receipts каждого дерева проверены. [Кадры, исходники и границы](vis35-shotgun.md). Отдельный power baseline падает до отрисовки оружия; авторская визуальная приёмка открыта

## VIS-36

Bare world Stevens 520 использует исходные body/pump и точные native материалы viewmodel; исправлены опора/ориентация shotgun и fuse, согласованы focus/consumed состояния трёх pickups. Render-only support height сохраняет interaction/save. Проверены FOV/окна, Debug/Play и 71 receipt каждого дерева. [Кадры и проверки VIS-36–38](vis36-38-pickups-security-scientist.md); требуется авторская визуальная приёмка

## VIS-37

Уточнены drive/mounting/оптика rover, sage/steel корпус и bearings турели; live/windup/recovery/shot/destroyed состояния просмотрены в HDR/SDR. Live geometry укладывается в прежние hit bounds, gameplay/таймеры сохранены. В общем проходе VIS-36–38 — 20 различных checks, graphics без skips/validation warnings. [Исходники и evidence](vis36-38-pickups-security-scientist.md); приёмка в бою открыта

## VIS-38

Статический сотрудник B-17 получил уточнённый рукав, швы/разрез/застёжки халата, окантовку фартука и асимметричную позу с планшетом. Анатомия/PPE/обувь сохранены; fresh import 157 096 triangles, 11 materials. Neutral и runtime front/side/back проверены, B-17 probe обновлён; NPC animation не добавлялась. [Сравнение и evidence](vis36-38-pickups-security-scientist.md); требуется визуальная приёмка автора

## VIS-39

Процедурное тело первого лица: закрытые кромки/подол халата, округлённые ноги и ботинки, перекрытие сочленений, двухзвенная походка по направлению движения и приземление с опорной стопой. Исправлено положение относительно камеры; статический B-17 scientist не менялся. Debug/Play собраны, 7/7 выбранных checks без skips, 100 финальных HDR/SDR кадров; [сравнение, исходники и границы](vis39-player-body.md). Требуется авторская приёмка в непрерывном движении

## VIS-40

Короткие тонкие player/turret/pellet traces, вспышки у стволов, metal/hard impacts по текущим query normal/surface и отдельные destroyed sparks. Hitscan/урон/темп/save сохранены. 32 различных checks, graphics без skips/validation warnings, Debug/Play обновлены; [сравнение, исходники и evidence](vis40-41-combat-environment-effects.md). Требуется приёмка в бою и движении

## VIS-41

Компактный направленный пар от fittings, объёмные короткие искры, guard/mounting шести вентиляторов; исправлены скрытые источники и перекрытие трёх вентиляторов кассетами. Repair/stop/start/coast и игровые данные сохранены. 282 финальных HDR/SDR кадра, glass blend regression, Check Map и staging проверены; [evidence](vis40-41-combat-environment-effects.md). Требуется авторская приёмка в движении

## VIS-42

Три позиции ламп blocked/ready/operated в существующих пультах, сохранён pressed/consumed feedback. Реальные цепочки power/cooling/pressure-exit/pump проверены с одной камеры до операции и после истечения feedback; повторно проверены power/fault/isolation/fan и live/wreck. Debug собран, 9 unit/content и 4 graphics checks проходят без skips; [сравнение, исходники и evidence](vis42-equipment-states.md). Читаемость в непрерывном движении требует авторской приёмки
