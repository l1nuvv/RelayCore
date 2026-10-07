"""Blender --background --disable-autoexec --python scripts/export-native-lab-glb.py."""
from pathlib import Path
from math import cos, sin, pi, sqrt, atan2
import sys

import bpy
import bmesh

root = Path(__file__).resolve().parents[1]
output = root / "content/models"
output.mkdir(parents=True, exist_ok=True)


def material(name, color, finish=None):
    value = bpy.data.materials.new(name)
    value.use_nodes = True
    value.use_backface_culling = True
    shader = value.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1)
    shader.inputs["Metallic"].default_value = 0
    shader.inputs["Roughness"].default_value = 1
    if finish:
        index, metallic, roughness = {
            "paint": (0, 0, .58), "plastic": (1, 0, .47),
            "galvanized": (2, .85, .65), "brass": (3, .85, .48),
            "rubber": (4, 0, .90), "canvas": (5, 0, .97),
            "ceramic": (6, 0, .30), "blackened": (7, .7, .62),
            "concrete": (0, 0, .95),
        }[finish]
        image_name = "Laboratory " + finish
        image = bpy.data.images.get(image_name)
        if image is None:
            filename = root / ("assets/textures/facility_source.png" if finish == "concrete"
                               else "content/graphics/prop_materials_source.png")
            source = bpy.data.images.load(str(filename), check_existing=True)
            if tuple(source.size) != (2048, 1024):
                source.scale(2048, 1024)
            width, height = source.size
            pixels = list(source.pixels)
            cell = width // 4
            image = bpy.data.images.new(image_name, width=cell, height=cell, alpha=True)
            cropped = []
            # Blender's pixel origin is at the lower left; sheet numbering is top to bottom.
            for y in range(cell):
                offset = ((height // 2 * (1 - index // 4) + y) * width + index % 4 * cell) * 4
                cropped.extend(pixels[offset:offset + cell * 4])
            image.pixels = cropped
            image.pack()
        texture = value.node_tree.nodes.new("ShaderNodeTexImage")
        texture.image = image
        value.node_tree.links.new(texture.outputs["Color"], shader.inputs["Base Color"])
        shader.inputs["Base Color"].default_value = (1, 1, 1, 1)
        shader.inputs["Metallic"].default_value = metallic
        shader.inputs["Roughness"].default_value = roughness
        value["uv_metres"] = {"paint": 1.5, "plastic": 2, "galvanized": 2, "brass": 2,
                              "rubber": 3, "canvas": 4, "ceramic": 1, "blackened": 2,
                              "concrete": 1}[finish]
    return value


def box(name, center, size, surface, parent, bevel=0.01):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(surface)
    if bevel:
        modifier = obj.modifiers.new("Construction edges", "BEVEL")
        modifier.width = bevel
        modifier.segments = 2 if bevel >= .01 else 1
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.parent = parent
    return obj


def graphic_material(name, filename):
    surface = material(name, (1, 1, 1))
    texture = surface.node_tree.nodes.new("ShaderNodeTexImage")
    texture.image = bpy.data.images.load(str(root / "content/graphics" / filename), check_existing=True)
    texture.image.pack()
    surface.node_tree.links.new(texture.outputs["Color"],
                               surface.node_tree.nodes.get("Principled BSDF").inputs["Base Color"])
    return surface


def poster(name, center, size, surface, parent, uv=(0, 0, 1, 1)):
    x, y, z = center
    w, h = size
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata([(x - w / 2, y, z - h / 2), (x + w / 2, y, z - h / 2),
                      (x + w / 2, y, z + h / 2), (x - w / 2, y, z + h / 2)], [], [(0, 1, 2, 3)])
    layer = mesh.uv_layers.new()
    u0, v0, u1, v1 = uv
    for loop, coordinate in zip(layer.data, ((u0, v0), (u1, v0), (u1, v1), (u0, v1))):
        loop.uv = coordinate
    mesh.materials.append(surface)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.parent = parent



def export(name, tangents=False):
    selected = {argument.removeprefix("--model=") for argument in sys.argv if argument.startswith("--model=")}
    if selected and name not in selected:
        return
    # Blender Z-up / front -Y becomes runtime Y-up / front +Z. Origins sit on the floor.
    # Merge repeated fixed details by material, preserving compact bare-metal hardware parts.
    surfaces = {}
    for obj in list(bpy.context.scene.objects):
        if obj.type == "MESH":
            surface = obj.data.materials[0]
            if "uv_metres" in surface:
                uv = obj.data.uv_layers.active or obj.data.uv_layers.new()
                density = surface["uv_metres"]
                for polygon in obj.data.polygons:
                    axis = max(range(3), key=lambda a: abs(polygon.normal[a]))
                    world_normal = obj.matrix_world.to_3x3() @ polygon.normal
                    world_axis = max(range(3), key=lambda a: abs(world_normal[a]))
                    axes = ((1, 2), (0, 2), (0, 1))[world_axis]
                    cylindrical = obj.get("cylindrical_uv", False) and axis != 2
                    angles = [atan2(obj.data.vertices[i].co.y, obj.data.vertices[i].co.x)
                              for i in polygon.vertices] if cylindrical else []
                    seam = angles and max(angles) - min(angles) > pi
                    for loop_index in polygon.loop_indices:
                        point = obj.data.vertices[obj.data.loops[loop_index].vertex_index].co
                        if cylindrical:
                            angle = atan2(point.y, point.x)
                            if seam and angle < 0:
                                angle += 2 * pi
                            uv.data[loop_index].uv = (angle * obj["uv_radius"] * density, point.z * density)
                        else:
                            point = obj.matrix_world @ point
                            uv.data[loop_index].uv = (point[axes[0]] * density, point[axes[1]] * density)
            if surface.node_tree.nodes.get("Principled BSDF").inputs["Metallic"].default_value != 1:
                surfaces.setdefault(surface, []).append(obj)
    for surface, objects in surfaces.items():
        if len(objects) == 1:
            continue
        bpy.ops.object.select_all(action="DESELECT")
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        bpy.ops.object.join()
        objects[0].name = surface.name
    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue
        mesh = bmesh.new()
        mesh.from_mesh(obj.data)
        bmesh.ops.triangulate(mesh, faces=list(mesh.faces))
        # Converted small lettering can contain subpixel slivers below the runtime area contract.
        tiny = []
        for face in mesh.faces:
            a, b, c = (obj.matrix_world @ vertex.co for vertex in face.verts)
            if (b - a).cross(c - a).length <= 1.01e-7:
                tiny.append(face)
        bmesh.ops.delete(mesh, geom=tiny, context="FACES")
        mesh.to_mesh(obj.data)
        mesh.free()
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(output / f"{name}.blend"))
    temporary = output / f"{name}.export.glb"
    bpy.ops.export_scene.gltf(
        filepath=str(temporary), export_format="GLB", export_yup=True,
        export_normals=True, export_texcoords=True, export_tangents=tangents,
        export_animations=False, export_skins=False, export_cameras=False,
        export_lights=False, export_extras=False,
    )
    temporary.replace(output / f"{name}.glb")


def cylinder(name, center, radius, depth, surface, parent, rotation=(0, 0, 0), sides=20):
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius, depth=depth, location=center,
                                      rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(surface)
    obj["cylindrical_uv"] = True
    obj["uv_radius"] = radius
    for polygon in obj.data.polygons:
        polygon.use_smooth = len(polygon.vertices) == 4
    obj.parent = parent
    return obj


def tube(name, center, radius, thickness, surface, parent, rotation=(0, 0, 0)):
    bpy.ops.mesh.primitive_torus_add(major_segments=32, minor_segments=6, location=center,
                                   major_radius=radius, minor_radius=thickness, rotation=rotation)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(surface)
    for polygon in obj.data.polygons:
        polygon.use_smooth = True
    obj.parent = parent
    return obj


def label(text, center, size, surface, parent):
    bpy.ops.object.text_add(location=center, rotation=(pi / 2, 0, 0))
    obj = bpy.context.object
    obj.name = text
    obj.data.body = text
    obj.data.size = size
    obj.data.align_x = "CENTER"
    obj.data.align_y = "CENTER"
    obj.data.materials.append(surface)
    bpy.ops.object.convert(target="MESH")
    obj.parent = parent


def assembly(name):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    obj = bpy.data.objects.new(name, None)
    bpy.context.collection.objects.link(obj)
    return obj


def meter(x, y, z, width, height, parent):
    box("Bakelite meter bezel", (x, y, z), (width, .025, height), dark, parent, .006)
    box("Printed meter scale", (x, y - .018, z), (width * .86, .009, height * .80), ivory, parent, .003)
    for tick in range(7):
        angle = (tick - 3) * .24
        mark = box("Scale graduation", (x + sin(angle) * width * .32, y - .025,
                                        z - height * .22 + cos(angle) * height * .48),
                   (.006, .004, .018), dark, parent, 0)
        mark.rotation_euler.y = angle
    needle = box("Meter needle", (x - width * .04, y - .028, z),
                 (.006, .004, height * .48), dark, parent, 0)
    needle.rotation_euler.y = -.25


def knob(x, y, z, parent, radius=.026):
    cylinder("Bakelite rotary switch", (x, y, z), radius, .035, dark, parent, (pi / 2, 0, 0), 12)
    box("Switch index", (x, y - .020, z + radius * .40), (.006, .004, radius * .65), ivory, parent, 0)


if "--cabinet-only" not in sys.argv:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    steel = material("Desaturated green painted steel", (.18, .24, .21), "paint")
    top = material("Ivory laboratory laminate", (.55, .52, .42), "plastic")
    rubber = material("Dark rubber feet", (.035, .04, .038), "rubber")
    bench = bpy.data.objects.new("Lab bench - 2.4m x 0.9m x 0.92m", None)
    bpy.context.collection.objects.link(bench)
    box("Worktop", (0, .006, .88), (2.4, .888, .08), top, bench, .015)
    box("Rear splash lip", (0, .42, .99), (2.4, .06, .14), steel, bench)
    box("Front apron", (0, -.32, .75), (2.2, .06, .18), steel, bench)
    box("Rear brace", (0, .32, .25), (2.15, .06, .1), steel, bench)
    for x in (-1.05, 1.05):
        for y in (-.32, .32):
            box("Steel leg", (x, y, .44), (.07, .07, .82), steel, bench, .005)
            box("Rubber foot", (x, y, .03), (.1, .1, .06), rubber, bench, .005)
    # Keep the middle open: the native collision product and rover route use this space.
    for x in (-1.05, 1.05):
        box("End frame rail", (x, 0, .16), (.07, .70, .07), steel, bench, .005)
    box("Worktop front edge", (0, -.444, .88), (2.36, .012, .035), rubber, bench, .003)
    for x in (-.75, .75):
        box("Shallow tool drawer", (x, -.055, .755), (.40, .62, .12), steel, bench, .008)
        box("Recessed drawer pull", (x, -.373, .755), (.18, .018, .025), rubber, bench, .004)
    box("Bench inventory plate", (0, -.356, .76), (.20, .006, .07), top, bench, .002)
    label("RC / LAB", (0, -.361, .76), .025, rubber, bench)
    export("lab_bench")

bpy.ops.wm.read_factory_settings(use_empty=True)
steel = material("Blue grey electrical cabinet", (.20, .25, .27), "paint")
steel.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (.65, .85, 1, 1)
steel.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .68
# A restrained rolled-paint surface. Periodic height derivatives encode only the
# tangent-space slope; no lighting, colour, AO or wear is baked into this map.
image = bpy.data.images.new("Cabinet paint normal", width=256, height=256, alpha=True)
image.colorspace_settings.name = "Non-Color"
pixels = []
for y in range(256):
    for x in range(256):
        u, v = 2 * pi * x / 256, 2 * pi * y / 256
        dx = .08 * cos(31 * u + 19 * v) + .04 * cos(17 * u - 23 * v)
        dy = .08 * 19 / 31 * cos(31 * u + 19 * v) - .04 * 23 / 17 * cos(17 * u - 23 * v)
        length = sqrt(dx * dx + dy * dy + 1)
        pixels.extend((.5 - dx / length * .5, .5 - dy / length * .5, .5 + .5 / length, 1))
image.pixels.foreach_set(pixels)
image.filepath_raw = str(output / "power_cabinet_normal.png")
image.file_format = "PNG"
image.save()
image.pack()
texture = steel.node_tree.nodes.new("ShaderNodeTexImage")
texture.image = image
normal = steel.node_tree.nodes.new("ShaderNodeNormalMap")
normal.inputs["Strength"].default_value = 1
steel.node_tree.links.new(texture.outputs["Color"], normal.inputs["Color"])
steel.node_tree.links.new(normal.outputs["Normal"], steel.node_tree.nodes.get("Principled BSDF").inputs["Normal"])
trim = material("Painted steel frame and meter bezels", (.13, .16, .17))
trim.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .75
handle = material("Brushed steel handle", (.32, .35, .37))
handle.node_tree.nodes.get("Principled BSDF").inputs["Metallic"].default_value = 1
handle.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .38
plinth = material("Dark coated cabinet plinth", (.07, .08, .075))
plinth.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .9
dark = material("Vent recesses and meter needles", (.035, .04, .038))
ivory = material("Ivory analog instrument", (.62, .58, .46))
ivory.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .6
cabinet = bpy.data.objects.new("Power cabinet - 1.2m x 0.8m x 2.1m", None)
bpy.context.collection.objects.link(cabinet)
box("Plinth", (0, 0, .07), (1.2, .8, .14), plinth, cabinet)
box("Enclosure", (0, .015, 1.1), (1.16, .73, 1.96), steel, cabinet, .025)
box("Door backing", (0, -.375, 1.1), (1.1, .04, 1.86), trim, cabinet)
box("Lower door", (0, -.40, .66), (1.02, .04, .90), steel, cabinet)
box("Instrument fascia", (0, -.40, 1.64), (1.02, .04, .72), steel, cabinet)
for x in (-.27, .27):
    meter(x, -.435, 1.83, .30, .23, cabinet)
    knob(x, -.445, 1.51, cabinet, .045)
    label("A" if x < 0 else "V", (x, -.443, 1.65), .045, ivory, cabinet)
box("Door handle", (.40, -.46, .90), (.04, .09, .22), handle, cabinet)
for z in (.34, .40, .46):
    box("Vent slot", (0, -.426, z), (.64, .015, .018), dark, cabinet, 0)
for z in (.57, 1.17):
    box("External hinge", (-.51, -.426, z), (.035, .025, .12), trim, cabinet, .004)
box("Equipment identification plate", (0, -.428, 1.24), (.68, .009, .15), ivory, cabinet, .003)
label("RC / POWER DISTRIBUTION", (0, -.435, 1.26), .032, dark, cabinet)
label("ISOLATE BEFORE SERVICING", (0, -.435, 1.21), .026, dark, cabinet)
export("power_cabinet", tangents=True)

if "--cabinet-only" in sys.argv:
    sys.exit(0)

terminal = assembly("CRT workstation - 0.66m wide")
ivory = material("Aged ivory ABS", (.47, .43, .33), "plastic")
dark = material("Bakelite and rubber", (.022, .028, .025), "rubber")
screen = material("Phosphor green CRT glass", (.035, .11, .062))
ink = material("Phosphor trace", (.22, .39, .22))
steel = material("Grey green folded steel", (.14, .19, .16), "paint")
box("Monitor foot", (0, .07, .035), (.40, .30, .07), steel, terminal)
box("Monitor pedestal", (0, .07, .12), (.16, .16, .16), ivory, terminal)
box("Deep CRT enclosure", (0, .075, .38), (.62, .45, .43), ivory, terminal, .035)
box("Recessed screen surround", (-.035, -.161, .405), (.49, .025, .33), dark, terminal, .025)
box("Curved screen face", (-.035, -.178, .405), (.445, .018, .285), screen, terminal, .025)
display = graphic_material("CRT two channel measurement", "instrument_display.png")
poster("CRT calibrated waveform", (-.035, -.189, .405), (.425, .265), display, terminal)
label("RELAY / FIELD MONITOR", (-.035, -.177, .197), .014, dark, terminal)
for x in (-.28, .28):
    for z in (.20, .56):
        cylinder("Recessed enclosure screw", (x, -.154, z), .008, .006, dark, terminal, (pi / 2, 0, 0), 8)
for x in (.20, .25):
    knob(x, -.174, .185, terminal, .017)
for z in (.28, .32, .36, .40, .44, .48):
    box("Side cooling slot", (.311, .06, z), (.003, .24, .009), dark, terminal, 0)
box("Keyboard chassis", (0, -.30, .035), (.66, .20, .055), ivory, terminal, .01)
for row in range(4):
    for col in range(14):
        box("Key cap", (-.284 + col * .043, -.363 + row * .038, .068),
            (.034, .028, .015), dark if col > 10 else ivory, terminal, .003)
box("Space bar", (-.05, -.388, .068), (.24, .021, .015), dark, terminal, .003)
export("crt_terminal")

rack = assembly("Analog instrument rack - 0.8m x 0.7m x 2.1m")
steel = material("Institutional sage enamel", (.14, .19, .16), "paint")
ivory = material("Ivory instrument faces", (.56, .53, .42), "ceramic")
dark = material("Bakelite and ventilation apertures", (.022, .027, .025))
brass = material("Oxidized brass connector", (.32, .22, .09), "brass")
box("Rack enclosure", (0, .025, 1.05), (.8, .65, 2.1), steel, rack, .02)
box("Recessed rack opening", (0, -.31, 1.08), (.72, .025, 1.90), dark, rack)
for z in (.35, .71, 1.07, 1.43, 1.79):
    box("Replaceable rack module", (0, -.335, z), (.66, .032, .32), ivory, rack, .005)
    meter(-.16, -.36, z + .035, .22, .16, rack)
    for x in (.08, .20):
        knob(x, -.365, z - .065, rack)
    for x in (-.30, .30):
        box("Module handle", (x, -.39, z), (.018, .06, .18), dark, rack, .004)
    label("RESONANCE / INPUT", (.10, -.355, z + .105), .018, dark, rack)
for x in (-.22, 0, .22):
    cylinder("Cable connector", (x, -.35, .16), .032, .055, brass, rack, (pi / 2, 0, 0))
export("instrument_rack")

vent = assembly("Wall ventilation cassette - 1.4m x 0.55m x 0.6m")
steel = material("Galvanized duct casing", (.25, .28, .26), "galvanized")
dark = material("Duct interior", (.027, .032, .030))
box("Folded duct casing", (0, 0, .30), (1.4, .50, .6), steel, vent, .015)
box("Filter aperture", (0, -.259, .30), (1.27, .025, .46), dark, vent)
for z in (.11, .18, .25, .32, .39, .46):
    slat = box("Stamped louvre", (0, -.275, z), (1.25, .055, .035), steel, vent, .004)
    slat.rotation_euler.x = -.35
for x in (-.66, .66):
    for z in (.045, .555):
        cylinder("Cover fixing", (x, -.257, z), .016, .012, dark, vent, (pi / 2, 0, 0), 8)
export("ventilation_unit")

apparatus = assembly("Resonance containment apparatus - 2m diameter")
steel = material("Blackened instrument steel", (.055, .069, .063), "blackened")
brass = material("Aged brass conductors", (.37, .25, .105), "brass")
ivory = material("Ceramic isolators", (.58, .55, .44), "ceramic")
dark = material("Rubber and printed legends", (.018, .022, .019))
cylinder("Containment plinth", (0, 0, .10), .87, .20, steel, apparatus, sides=32)
for radius in (1.19, 1.32):
    tube("Floor continuity conductor", (0, 0, .018), radius, .012, brass, apparatus)
for side in range(4):
    angle = side * pi / 2
    contact = box("Floor conductor bridge", (cos(angle) * 1.25, sin(angle) * 1.25, .018),
                  (.20, .028, .018), brass, apparatus, .002)
    contact.rotation_euler.z = angle
cylinder("Ceramic specimen platform", (0, 0, .24), .41, .12, ivory, apparatus, sides=32)
for z, radius in ((.33, .66), (1.25, .75), (2.18, .66)):
    tube("Continuous copper field winding", (0, 0, z), radius, .032, brass, apparatus)
for side in range(4):
    angle = side * pi / 2 + pi / 4
    x, y = cos(angle) * .68, sin(angle) * .68
    cylinder("Support stanchion", (x, y, 1.14), .046, 2.10, steel, apparatus)
    for z in (.40, .48, .56, 1.94, 2.02, 2.10):
        cylinder("Ceramic insulator rib", (x, y, z), .086, .035, ivory, apparatus)
    for z in (.67, 1.82):
        cylinder("Conductor contact", (x, y, z), .073, .08, brass, apparatus)
cylinder("Upper field electrode", (0, 0, 2.25), .24, .14, brass, apparatus)
cylinder("Lower field electrode", (0, 0, .43), .18, .18, brass, apparatus)
# A restrained specimen held between electrodes by fine suspension wires.
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=.18, location=(0, 0, 1.25))
specimen = bpy.context.object
specimen.name = "Suspended resonant specimen"
specimen.scale = (.8, .65, 1.6)
specimen.data.materials.append(steel)
specimen.parent = apparatus
cylinder("Upper specimen suspension", (0, 0, 1.89), .006, .70, brass, apparatus, sides=12)
cylinder("Lower specimen restraint", (0, 0, .73), .006, .46, brass, apparatus, sides=12)
for side in range(4):
    angle = side * pi / 2 + pi / 4
    for z in (.33, 1.25, 2.18):
        cylinder("Winding stanchion clamp", (cos(angle) * .68, sin(angle) * .68, z),
                 .067, .07, brass, apparatus)
box("Service instrumentation housing", (0, -.73, .58), (.48, .28, .40), steel, apparatus)
meter(-.10, -.886, .62, .17, .17, apparatus)
knob(.12, -.89, .55, apparatus, .032)
label("FIELD / ISOLATE", (0, -.878, .75), .031, ivory, apparatus)
export("resonance_apparatus")

sign = assembly("Facility identification sign - 1.2m x 0.6m")
ivory = material("Institutional enamel sign", (.62, .59, .48))
dark = material("Black stencil ink", (.026, .033, .029))
brass = material("Ochre safety paint", (.46, .29, .075))
box("Sign plate", (0, 0, .30), (1.2, .025, .60), ivory, sign, .006)
box("Department strip", (0, -.017, .53), (1.18, .008, .105), brass, sign, .001)
label("RESONANCE RESEARCH", (0, -.025, .53), .061, dark, sign)
label("B-17", (-.34, -.018, .31), .19, dark, sign)
label("FIELD ACTIVE", (.23, -.018, .34), .065, dark, sign)
label("VERIFY CONTINUITY", (.21, -.018, .25), .040, dark, sign)
label("AUTHORIZED PERSONNEL ONLY", (0, -.018, .08), .043, dark, sign)
export("facility_sign")

if not {arg for arg in sys.argv if arg.startswith("--model=")} or "--model=scientist_hazmat" in sys.argv:
    exec((root / "scripts/author-scientist.py").read_text(encoding="utf-8"))


arch = assembly("Concrete service vault - 3.2m clear span")
concrete = material("Aged warm cast concrete", (.37, .36, .31), "concrete")
for x in (-1.5, 1.5):
    box("Vault pier", (x, 0, .90), (.20, .40, 1.8), concrete, arch, .008)
vertices, faces = [], []
for y in (-.20, .20):
    for radius in (1.4, 1.6):
        for step in range(25):
            angle = step * pi / 24
            vertices.append((cos(angle) * radius, y, 1.8 + sin(angle) * radius))
for step in range(24):
    faces.extend(((step, step + 1, step + 26, step + 25),
                  (step + 50, step + 75, step + 76, step + 51),
                  (step, step + 50, step + 51, step + 1),
                  (step + 25, step + 26, step + 76, step + 75)))
faces.extend(((0, 25, 75, 50), (24, 74, 99, 49)))
mesh = bpy.data.meshes.new("Continuous cast vault")
mesh.from_pydata(vertices, [], faces)
mesh.update()
obj = bpy.data.objects.new("Vault crown", mesh)
bpy.context.collection.objects.link(obj)
obj.data.materials.append(concrete)
obj.parent = arch
export("concrete_arch")

pipes = assembly("Flanged utility pipe - 3m module")
steel = material("Dull galvanized pipe", (.22, .24, .21), "galvanized")
dark = material("Pipe clamps and gasket", (.055, .065, .058))
red = material("Faded red valve wheel", (.30, .075, .045))
cylinder("Supply pipe", (0, 0, .20), .085, 3, steel, pipes, (0, pi / 2, 0))
for x in (-1.35, 1.35):
    cylinder("Bolted flange", (x, 0, .20), .14, .055, steel, pipes, (0, pi / 2, 0))
    for bolt in range(8):
        angle = bolt * pi / 4
        cylinder("Flange bolt", (x, cos(angle) * .116, .20 + sin(angle) * .116),
                 .013, .08, dark, pipes, (0, pi / 2, 0), 6)
for x in (-.8, .8):
    cylinder("Pipe clamp", (x, 0, .20), .098, .06, dark, pipes, (0, pi / 2, 0))
    box("Wall standoff", (x, .14, .20), (.07, .20, .07), dark, pipes, .004)
cylinder("Valve body", (0, 0, .20), .13, .22, steel, pipes, (0, pi / 2, 0))
cylinder("Valve spindle", (0, -.14, .20), .024, .26, dark, pipes, (pi / 2, 0, 0))
tube("Hand valve wheel", (0, -.28, .20), .17, .018, red, pipes, (pi / 2, 0, 0))
for angle in (0, pi / 2):
    spoke = box("Valve wheel spoke", (0, -.28, .20), (.34, .025, .025), red, pipes, .004)
    spoke.rotation_euler.y = angle
export("utility_pipe")



board = assembly("Laboratory procedure board - 1.8m x 1.05m")
steel = material("Oxidized aluminum board frame", (.29, .31, .27), "galvanized")
notes = graphic_material("Original B17 procedure and waveform notes", "lab_notes.png")
box("Board frame", (0, 0, .525), (1.8, .035, 1.05), steel, board, .008)
poster("Procedure surface", (0, -.021, .525), (1.74, .99), notes, board)
box("Marker tray", (0, -.06, .015), (1.60, .12, .025), steel, board, .004)
export("lab_notes_board")

safety = assembly("Laboratory safety notice - 0.8m x 1.1m")
ivory = material("Enamel safety notice backing", (.58, .56, .48))
icons = graphic_material("Original laboratory safety pictograms", "lab_pictograms.png")
box("Safety sign plate", (0, 0, .55), (.80, .025, 1.1), ivory, safety, .006)
for index, (x, z) in enumerate(((-.19, .80), (.19, .80), (-.19, .30), (.19, .30))):
    u, v = index * .25, .5
    poster("Safety instruction", (x, -.016, z), (.36, .44), icons, safety, (u, v, u + .25, v + .5))
export("safety_notice")

accessories = assembly("Laboratory sampling and measuring set - tabletop")
ivory = material("Glazed ceramic and paper", (.62, .60, .50), "ceramic")
dark = material("Black laboratory instrument", (.035, .045, .039), "rubber")
brass = material("Brass instrument fittings", (.33, .24, .105), "brass")
amber = material("Amber reagent bottle", (.18, .10, .038))
amber.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .22
for x, radius, height in ((-.30, .047, .20), (-.17, .035, .15), (-.06, .035, .15)):
    cylinder("Reagent bottle", (x, .08, height / 2), radius, height, amber, accessories)
    cylinder("Bottle stopper", (x, .08, height + .018), radius * .62, .035, dark, accessories)
    box("Bottle label", (x, .08 - radius, height * .55), (radius * 1.3, .004, height * .32), ivory, accessories, .002)
box("Sample rack base", (.20, .08, .025), (.32, .17, .05), ivory, accessories, .008)
for x in (.09, .16, .23, .30):
    cylinder("Ceramic sample vial", (x, .08, .12), .022, .19, ivory, accessories, sides=12)
    cylinder("Sample cap", (x, .08, .22), .024, .025, dark, accessories, sides=12)
box("Instrument foot", (-.24, -.12, .035), (.18, .20, .07), dark, accessories)
cylinder("Instrument support", (-.24, -.08, .23), .016, .40, brass, accessories)
box("Sensor head", (-.24, -.13, .40), (.12, .16, .09), ivory, accessories, .008)
cylinder("Optical pickup", (-.24, -.22, .40), .032, .05, dark, accessories, (pi / 2, 0, 0))
box("Laboratory notebook", (.17, -.17, .016), (.30, .22, .032), dark, accessories, .003)
box("Paper cover", (.17, -.17, .034), (.28, .20, .004), ivory, accessories, .001)
export("lab_accessories")

console = assembly("Analog control desk - 1.5m operator station")
steel = material("Faded green console enamel", (.17, .25, .17), "paint")
ivory = material("Ivory control fascia and meter scales", (.55, .53, .41), "ceramic")
dark = material("Bakelite switches and meter bezels", (.022, .029, .023))
red = material("Red stop buttons", (.37, .07, .035))
amber = material("Amber indicator lenses", (.50, .29, .055))
display = graphic_material("Original two channel field display", "instrument_display.png")
box("Operator desk pedestal", (0, .05, .40), (1.5, .80, .80), steel, console, .022)
box("Desk toe recess", (0, -.36, .12), (1.30, .06, .22), dark, console)
fascia = box("Sloped control surface", (0, -.05, .96), (1.48, .90, .07), ivory, console, .012)
fascia.rotation_euler.x = .30
box("Upright instrument bridge", (0, .31, 1.46), (1.5, .18, .86), steel, console, .018)
box("Display bezel", (0, .207, 1.59), (.90, .04, .53), dark, console, .012)
poster("CRT field display", (0, .182, 1.59), (.84, .47), display, console)
for x in (-.60, .60):
    meter(x, .20, 1.68, .22, .19, console)
    knob(x, .19, 1.39, console, .039)
for row in range(3):
    for col in range(6):
        x, y = -.55 + col * .22, -.28 + row * .17
        z = .96 + (y + .05) * .30
        switch = box("Mechanical push button", (x, y, z + .06), (.07, .07, .037),
                     red if row == 0 and col == 0 else amber if row == 1 else dark, console, .004)
        switch.rotation_euler.x = .30
label("RELAY / FIELD CONTROL", (0, .208, 1.20), .041, ivory, console)
export("control_console")

fixture = assembly("Fluorescent ceiling fitting - stamped steel and prismatic diffuser frame")
enamel = material("Aged ivory fixture enamel", (.62, .62, .56))
enamel.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .65
reflector = material("Formed aluminum reflector", (.48, .49, .46))
reflector.node_tree.nodes.get("Principled BSDF").inputs["Metallic"].default_value = .75
reflector.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .55
fastener = material("Fixture clips and screws", (.13, .14, .13))
box("Ballast and wiring pan", (0,0,-.025), (1.30,.36,.05), enamel, fixture, .006)
for side in (-1,1):
    box("Folded diffuser retaining rail", (0,side*.17,-.075), (1.30,.025,.10), enamel, fixture, .003)
    box("Fixture end cap", (side*.638,0,-.075), (.024,.34,.10), enamel, fixture, .003)
    panel = box("Angled reflector", (0,side*.10,-.06), (1.23,.12,.008), reflector, fixture, .002)
    panel.rotation_euler.x = side*.30
    for x in (-.40,.40):
        box("Diffuser spring clip", (x,side*.183,-.10), (.035,.015,.055), fastener, fixture, .002)
    for x in (-.59,.59):
        cylinder("Captive screw", (x,side*.145,-.127), .006,.004,fastener,fixture,sides=12)
export("fluorescent_fixture")
