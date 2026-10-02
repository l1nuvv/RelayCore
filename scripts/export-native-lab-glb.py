"""Blender --background --disable-autoexec --python scripts/export-native-lab-glb.py."""
from pathlib import Path
from math import cos, pi, sqrt
import sys

import bpy

root = Path(__file__).resolve().parents[1]
output = root / "content/models"
output.mkdir(parents=True, exist_ok=True)


def material(name, color):
    value = bpy.data.materials.new(name)
    value.use_nodes = True
    value.use_backface_culling = True
    shader = value.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (*color, 1)
    shader.inputs["Metallic"].default_value = 0
    shader.inputs["Roughness"].default_value = 1
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
        modifier.segments = 1
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    obj.parent = parent
    return obj


def export(name, tangents=False):
    # Blender Z-up / front -Y becomes runtime Y-up / front +Z. Origins sit on the floor.
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(output / f"{name}.blend"))
    bpy.ops.export_scene.gltf(
        filepath=str(output / f"{name}.glb"), export_format="GLB", export_yup=True,
        export_normals=True, export_texcoords=True, export_tangents=tangents,
        export_animations=False, export_skins=False, export_cameras=False,
        export_lights=False, export_extras=False,
    )


if "--cabinet-only" not in sys.argv:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    steel = material("Desaturated green painted steel", (.18, .24, .21))
    top = material("Ivory laboratory laminate", (.55, .52, .42))
    rubber = material("Dark rubber feet", (.035, .04, .038))
    bench = bpy.data.objects.new("Lab bench - 2.4m x 0.9m x 0.92m", None)
    bpy.context.collection.objects.link(bench)
    box("Worktop", (0, 0, .88), (2.4, .9, .08), top, bench, .015)
    box("Rear splash lip", (0, .42, .99), (2.4, .06, .14), steel, bench)
    box("Front apron", (0, -.32, .75), (2.2, .06, .18), steel, bench)
    box("Rear brace", (0, .32, .25), (2.15, .06, .1), steel, bench)
    for x in (-1.05, 1.05):
        for y in (-.32, .32):
            box("Steel leg", (x, y, .44), (.07, .07, .82), steel, bench, .005)
            box("Rubber foot", (x, y, .03), (.1, .1, .06), rubber, bench, .005)
    export("lab_bench")

bpy.ops.wm.read_factory_settings(use_empty=True)
steel = material("Blue grey electrical cabinet", (.20, .25, .27))
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
    box("Meter bezel", (x, -.435, 1.83), (.30, .04, .23), trim, cabinet)
    box("Meter face", (x, -.46, 1.83), (.25, .012, .18), ivory, cabinet, .003)
    needle = box("Meter needle", (x, -.471, 1.83), (.012, .008, .13), dark, cabinet, 0)
    needle.rotation_euler.y = -.3 if x < 0 else .25
box("Door handle", (.40, -.46, .90), (.04, .09, .22), handle, cabinet)
for z in (.34, .40, .46):
    box("Vent slot", (0, -.426, z), (.64, .015, .018), dark, cabinet, 0)
export("power_cabinet", tangents=True)
