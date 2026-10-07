"""Adapt the four CC0 laboratory props to RelayCore's scalar-material pipeline."""
from pathlib import Path
import json
import sys

import bmesh
import bpy
from math import sqrt
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
asset = next((argument.removeprefix("--asset=") for argument in sys.argv
              if argument.startswith("--asset=")), "vintage_microscope")
output_name = {
    "vintage_microscope": "lab_microscope",
    "retro_multimeter": "lab_multimeter",
    "television_02": "surveillance_monitor",
    "vintage_spacecraft_instrument": "analog_navigation_instrument",
}[asset]
source = root / "content/third_party/polyhaven" / asset
document = json.loads((source / f"{asset}_1k.gltf").read_text())
glass_names = {entry["name"] for entry in document["materials"] if entry.get("alphaMode") == "BLEND"}
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(source / f"{asset}_1k.gltf"))

normal_sources = [image for image in bpy.data.images if "nor_gl" in image.name]
for image in normal_sources:
    image.colorspace_settings.name = "Non-Color"
    data = list(image.pixels)
    repaired = 0
    for offset in range(0, len(data), 4):
        x, y, z = (data[offset + channel] * 2 - 1 for channel in range(3))
        length = sqrt(x * x + y * y + z * z)
        if z <= 0 or length <= .5:
            # The source JPEG has black unused texels outside UV islands. Runtime validates all texels.
            if max(data[offset:offset + 3]) < .1 or length < .05:
                x, y, z = 0, 0, 1
            else:
                z = max(z, .02)
            length = sqrt(x * x + y * y + z * z)
            data[offset:offset + 3] = [component / length * .5 + .5 for component in (x, y, z)]
            repaired += 1
    converted = bpy.data.images.new(asset + " validated tangent normals", width=image.size[0], height=image.size[1])
    converted.colorspace_settings.name = "Non-Color"
    converted.pixels = data
    converted.pack()
    for surface in bpy.data.materials:
        for node in surface.node_tree.nodes:
            if node.type == "TEX_IMAGE" and node.image == image:
                node.image = converted
    print(f"Normalized {repaired} unused, short or nonpositive-Z normal texels from {image.name}")

arm = bpy.data.images.load(str(source / f"textures/{asset}_arm_1k.jpg"), check_existing=True)
arm.colorspace_settings.name = "Non-Color"
width, height = arm.size
pixels = list(arm.pixels)

def mask_at(uv):
    x, y = int(uv.x * width) % width, int(uv.y * height) % height
    offset = (y * width + x) * 4
    return pixels[offset + 2]

variants = {}
for original in list(bpy.data.materials):
    metal = original.copy()
    metal.name = original.name + " exposed metal"
    variants[original] = (original, metal)
    for surface, metallic, roughness in ((original, 0, .57), (metal, .85, .43)):
        shader = surface.node_tree.nodes.get("Principled BSDF")
        for socket in ("Metallic", "Roughness", "Alpha"):
            for link in list(shader.inputs[socket].links):
                surface.node_tree.links.remove(link)
        shader.inputs["Metallic"].default_value = metallic
        shader.inputs["Roughness"].default_value = .16 if original.name in glass_names else roughness
        shader.inputs["Alpha"].default_value = 1
        surface.use_backface_culling = True

display = None
if asset == "television_02":
    display = bpy.data.materials.new("B17 two-channel monitor")
    display.use_nodes = True
    display.use_backface_culling = True
    shader = display.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (1, 1, 1, 1)
    shader.inputs["Roughness"].default_value = .22
    texture = display.node_tree.nodes.new("ShaderNodeTexImage")
    texture.image = bpy.data.images.load(str(root / "content/graphics/instrument_display.png"))
    display.node_tree.links.new(texture.outputs["Color"], shader.inputs["Base Color"])

objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
points = [obj.matrix_world @ vertex.co for obj in objects for vertex in obj.data.vertices]
minimum = Vector(tuple(min(point[axis] for point in points) for axis in range(3)))
maximum = Vector(tuple(max(point[axis] for point in points) for axis in range(3)))
offset = Vector(((minimum.x + maximum.x) / 2, (minimum.y + maximum.y) / 2, minimum.z))
bpy.ops.object.select_all(action="DESELECT")
for obj in objects:
    previous = list(obj.data.materials)
    # These sources address TEXCOORD_0; discard their unused secondary UV sets.
    for layer in list(obj.data.uv_layers)[1:]:
        obj.data.uv_layers.remove(layer)
    obj.data.uv_layers.active_index = 0
    uv = obj.data.uv_layers[0].data
    indices = []
    surfaces = [surface for original in previous for surface in variants[original]]
    for polygon in obj.data.polygons:
        center = sum((uv[index].uv for index in polygon.loop_indices), Vector((0, 0))) / len(polygon.loop_indices)
        indices.append(polygon.material_index * 2 + int(mask_at(center) > .5))
    obj.data.materials.clear()
    for surface in surfaces:
        obj.data.materials.append(surface)
    for polygon, index in zip(obj.data.polygons, indices):
        polygon.material_index = index
    obj.location -= offset
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    obj.select_set(False)
    if display:
        obj.data.materials.append(display)
        for polygon in obj.data.polygons:
            points = [obj.data.vertices[index].co for index in polygon.vertices]
            if polygon.normal.y < -.8 and all(-.161 < point.x < .161 and .139 < point.z < .378
                                              and point.y < -.143 for point in points):
                polygon.material_index = len(obj.data.materials) - 1
                for loop in polygon.loop_indices:
                    point = obj.data.vertices[obj.data.loops[loop].vertex_index].co
                    obj.data.uv_layers[0].data[loop].uv = ((point.x + .16) / .32, (point.z - .14) / .237)
    mesh = bmesh.new()
    mesh.from_mesh(obj.data)
    if asset == "retro_multimeter":
        covers = [face for face in mesh.faces if previous[face.material_index // 2].name in glass_names]
        bmesh.ops.delete(mesh, geom=covers, context="FACES")
    bmesh.ops.triangulate(mesh, faces=list(mesh.faces))
    tiny = [face for face in mesh.faces if (face.verts[1].co - face.verts[0].co).cross(
        face.verts[2].co - face.verts[0].co).length <= 1.01e-7]
    bmesh.ops.delete(mesh, geom=tiny, context="FACES")
    mesh.to_mesh(obj.data)
    mesh.free()

for image in bpy.data.images:
    if image.source == "FILE":
        image.pack()
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(root / f"content/models/{output_name}.blend"))
bpy.ops.export_scene.gltf(
    filepath=str(root / f"content/models/{output_name}.glb"), export_format="GLB",
    export_yup=True, export_normals=True, export_texcoords=True, export_tangents=True,
    export_animations=False, export_skins=False, export_cameras=False, export_lights=False,
    export_extras=False,
)
