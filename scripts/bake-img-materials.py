"""Blender --background --factory-startup --disable-autoexec --threads 2
--python-exit-code 1 --python scripts/bake-img-materials.py -- --model=guardian_body

Refine existing source materials without changing geometry. Run after the owning
exporter. Guardian retains its artist UVs; the ram gets a unique UV atlas for AO.
"""
from pathlib import Path
import sys
import importlib.util
import json
import struct
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
name = next(a.split("=", 1)[1] for a in sys.argv if a.startswith("--model="))
if name not in {"guardian_body", "guardian_arm", "containment_ram"}:
    raise ValueError("expected guardian_body, guardian_arm or containment_ram")
out = ROOT / "content/graphics/img01-03" / name
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(ROOT / "content/models" / f"{name}.blend"))
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.render.threads_mode = "FIXED"
scene.render.threads = 2
scene.cycles.samples = 32
scene.cycles.seed = 20261009
prefs = bpy.context.preferences.addons['cycles'].preferences
prefs.compute_device_type = 'OPTIX'
prefs.get_devices()
for device in prefs.devices:
    device.use = device.type == 'OPTIX'
if not any(d.use for d in prefs.devices):
    raise RuntimeError("OptiX device required for material baking")
scene.cycles.device = 'GPU'
print("Bake device", [(d.name, d.type) for d in prefs.devices if d.use], flush=True)
scene.render.bake.margin = 8
scene.render.bake.use_clear = False
if scene.world is None:
    scene.world = bpy.data.worlds.new('Material bake world')
scene.world.color = (1, 1, 1)
scene.world.use_nodes = True
scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value = (1,1,1,1)
scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value = 1
scene.world.light_settings.distance = .22
bpy.context.preferences.filepaths.save_version = 0
objects = [o for o in scene.objects if o.type == 'MESH']

def image(label, edge, linear=True):
    result = bpy.data.images.new(label, width=edge, height=edge, alpha=True)
    result.colorspace_settings.name = 'Non-Color' if linear else 'sRGB'
    result.generated_color = (1,1,1,1)
    return result

def save(img, label):
    img.filepath_raw = str(out / f"{label}.png")
    img.file_format = 'PNG'
    img.save()
    img.pack()

def target(material, img):
    node = material.node_tree.nodes.new('ShaderNodeTexImage')
    node.image = img
    material.node_tree.nodes.active = node
    return node

if name == 'containment_ram':
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
        obj.data.uv_layers.active.name = 'SurfaceUV'
        for mat in obj.data.materials:
            for node in list(mat.node_tree.nodes):
                if node.type == 'TEX_IMAGE':
                    uv = mat.node_tree.nodes.new('ShaderNodeUVMap')
                    uv.uv_map = 'SurfaceUV'
                    mat.node_tree.links.new(uv.outputs['UV'], node.inputs['Vector'])
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    obj = bpy.context.object
    obj.data.uv_layers.new(name='MaterialUV')
    obj.data.uv_layers.active_index = 1
    obj.data.uv_layers[1].active_render = True
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=.012)
    bpy.ops.object.mode_set(mode='OBJECT')
    objects = [obj]
    mat = obj.data.materials[0]
    for polygon in obj.data.polygons:
        polygon.material_index = 0
    shader = mat.node_tree.nodes.get('Principled BSDF')
    base = image('ram base color atlas', 1024, False)
    base_node = target(mat, base)
    scene.render.bake.use_pass_direct = False
    scene.render.bake.use_pass_indirect = False
    scene.render.bake.use_pass_color = True
    bpy.ops.object.bake(type='DIFFUSE')
    save(base, 'base_color')
    normal = image('ram normal atlas', 1024)
    normal_node = target(mat, normal)
    bpy.ops.object.bake(type='NORMAL')
    save(normal, 'normal')
    # Replace source texture nodes only after transfer to the new atlas.
    for link in list(shader.inputs['Base Color'].links) + list(shader.inputs['Normal'].links):
        mat.node_tree.links.remove(link)
    mat.node_tree.links.new(base_node.outputs['Color'], shader.inputs['Base Color'])
    normal_map = mat.node_tree.nodes.new('ShaderNodeNormalMap')
    mat.node_tree.links.new(normal_node.outputs['Color'], normal_map.inputs['Color'])
    mat.node_tree.links.new(normal_map.outputs['Normal'], shader.inputs['Normal'])
    # glTF always consumes TEXCOORD_0 in this native contract.
    obj.data.uv_layers.remove(obj.data.uv_layers['SurfaceUV'])

for obj in objects:
    organic = 'guardian' in name and any('exposed anatomy' in m.name for m in obj.data.materials if m)
    if not organic and name != 'containment_ram':
        continue
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    used_materials = {p.material_index for p in obj.data.polygons}
    for material_index, mat in enumerate(obj.data.materials):
        if material_index not in used_materials:
            continue
        if not mat or ('guardian' in name and 'exposed anatomy' not in mat.name):
            continue
        shader = mat.node_tree.nodes.get('Principled BSDF')
        base_node = shader.inputs['Base Color'].links[0].from_node
        base = base_node.image
        edge = 1024 if name == 'containment_ram' else 512
        ao = image(mat.name + ' occlusion', edge)
        ao_node = target(mat, ao)
        bpy.ops.object.bake(type='AO')
        # Retain the default white value outside the UV islands. AO strength
        # at runtime is intentionally moderate; it never darkens direct lights.
        save(ao, f'occlusion_{material_index}')
        sample = base.copy()
        sample.scale(edge, edge)
        pixels = np.asarray(sample.pixels[:], dtype=np.float32).reshape(-1,4)
        bpy.data.images.remove(sample)
        if 'guardian' in name:
            blood = np.clip((pixels[:,0]-pixels[:,1]) * 3, 0, 1)
            roughness = .72 - .35*blood
            metallic = np.full(len(pixels), .04)
        else:
            # Preserve the existing steel substrate; darker oxide is rougher.
            luma = pixels[:,:3].mean(axis=1)
            oxide = np.clip((.24-luma)*4,0,1)
            roughness = .36 + .4*oxide
            metallic = .92 - .45*oxide
        packed = pixels.copy()
        packed[:,0] = np.asarray(ao.pixels[:], dtype=np.float32).reshape(-1,4)[:,0]
        packed[:,1] = roughness
        packed[:,2] = metallic
        packed[:,3] = 1
        orm = image(mat.name + ' ORM', edge)
        orm.pixels.foreach_set(packed.ravel())
        save(orm, f'orm_{material_index}')
        orm_node = target(mat, orm)
        split = mat.node_tree.nodes.new('ShaderNodeSeparateColor')
        mat.node_tree.links.new(orm_node.outputs['Color'], split.inputs['Color'])
        mat.node_tree.links.new(split.outputs['Green'], shader.inputs['Roughness'])
        mat.node_tree.links.new(split.outputs['Blue'], shader.inputs['Metallic'])
        # Exporter-recognized glTF material output for AO.
        group = bpy.data.node_groups.get('glTF Material Output') or bpy.data.node_groups.new('glTF Material Output', 'ShaderNodeTree')
        if not group.interface.items_tree:
            group.interface.new_socket(name='Occlusion', in_out='INPUT', socket_type='NodeSocketFloat')
        output = mat.node_tree.nodes.new('ShaderNodeGroup')
        output.node_tree = group
        mat.node_tree.links.new(split.outputs['Red'], output.inputs['Occlusion'])
        print(mat.name, 'roughness', float(roughness.min()), float(roughness.max()),
              'metallic', float(metallic.min()), float(metallic.max()),
              'AO', float(packed[:,0].min()), float(packed[:,0].max()), flush=True)

bpy.ops.object.select_all(action='DESELECT')
for obj in objects:
    obj.select_set(True)
bpy.ops.wm.save_as_mainfile(filepath=str(ROOT / 'content/models' / f'{name}.blend'))
bpy.ops.export_scene.gltf(filepath=str(ROOT / 'content/models' / f'{name}.glb'), export_format='GLB',
    use_selection=True, export_yup=True, export_normals=True, export_texcoords=True, export_tangents=True,
    export_animations=False, export_skins=False, export_extras=False, export_cameras=False, export_lights=False)
spec = importlib.util.spec_from_file_location('native_export', ROOT / 'scripts/export-pressure-transfer-glb.py')
native = importlib.util.module_from_spec(spec)
spec.loader.exec_module(native)
path = ROOT / 'content/models' / f'{name}.glb'
data = path.read_bytes()
json_size = struct.unpack_from('<I',data,12)[0]
document = json.loads(data[20:20+json_size])
for mesh in document['meshes']:
    for primitive in mesh['primitives']:
        mat = document['materials'][primitive['material']]
        if 'normalTexture' not in mat:
            primitive['attributes'].pop('TANGENT',None)
for mat in document['materials']:
    if 'occlusionTexture' in mat:
        mat['occlusionTexture']['strength'] = .55
encoded = json.dumps(document,separators=(',',':')).encode()
encoded += b' '*((-len(encoded))%4)
chunks = struct.pack('<II',len(encoded),0x4E4F534A)+encoded+data[20+json_size:]
path.write_bytes(struct.pack('<III',0x46546C67,2,12+len(chunks))+chunks)
native.repair_exported_tangents(path)
print('Exported',path,flush=True)
