"""Blender --background --disable-autoexec --python scripts/export-service-cylinder-glb.py."""
from pathlib import Path

import bpy

root = Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=str(root / "assets/models/service_cylinder.obj"),
                      forward_axis="NEGATIVE_Z", up_axis="Y")
obj = bpy.context.object
obj.name = "Vessel"
body = bpy.data.materials.new("Painted vessel")
body.use_nodes = True
body.use_backface_culling = True
shader = body.node_tree.nodes.get("Principled BSDF")
shader.inputs["Metallic"].default_value = 0
shader.inputs["Roughness"].default_value = 1
image = body.node_tree.nodes.new("ShaderNodeTexImage")
image.image = bpy.data.images.load(str(root / "assets/models/service_cylinder.bmp"))
body.node_tree.links.new(image.outputs["Color"], shader.inputs["Base Color"])
valve = bpy.data.materials.new("Valve metal")
valve.use_nodes = True
valve.use_backface_culling = True
shader = valve.node_tree.nodes.get("Principled BSDF")
shader.inputs["Base Color"].default_value = (.22, .25, .23, 1)
shader.inputs["Metallic"].default_value = 0
shader.inputs["Roughness"].default_value = 1
obj.data.materials.clear()
obj.data.materials.append(body)
obj.data.materials.append(valve)
for polygon in obj.data.polygons:
    height = sum((obj.matrix_world @ obj.data.vertices[i].co).z
                 for i in polygon.vertices) / len(polygon.vertices)
    polygon.material_index = 1 if height >= 1.30 else 0
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.separate(type="MATERIAL")
bpy.ops.object.mode_set(mode="OBJECT")
parts = list(bpy.context.selected_objects)
assembly = bpy.data.objects.new("Service cylinder", None)
bpy.context.collection.objects.link(assembly)
for part in parts:
    part.parent = assembly
    if part.data.materials[0] == valve:
        part.name = "Valve"
        bpy.context.view_layer.objects.active = part
        bpy.context.scene.cursor.location = (0, 0, 1.30)
        bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
output = root / "content/models/service_cylinder.glb"
output.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.export_scene.gltf(filepath=str(output), export_format="GLB", export_yup=True,
                          export_normals=True, export_texcoords=True, export_tangents=False,
                          export_animations=False, export_skins=False, export_cameras=False,
                          export_lights=False, export_extras=False)
print(f"Exported {len(parts)} static parts to {output}")
