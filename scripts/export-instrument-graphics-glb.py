"""Update existing instrument artwork; retain authored construction and material finishes."""
from pathlib import Path
import bpy
import sys
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
MODELS = ROOT / 'content/models'
GRAPHICS = ROOT / 'content/graphics/instruments'
SELECTED = {arg.split('=', 1)[1] for arg in sys.argv if arg.startswith('--model=')}
spec = importlib.util.spec_from_file_location('pressure_export', ROOT / 'scripts/export-pressure-transfer-glb.py')
pressure_export = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pressure_export)

def save(name):
    for image in list(bpy.data.images):
        if image.users == 0:
            bpy.data.images.remove(image)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(MODELS / f'{name}.blend'))
    bpy.ops.export_scene.gltf(filepath=str(MODELS / f'{name}.glb'), export_format='GLB',
        export_yup=True, export_normals=True, export_texcoords=True, export_tangents=True, export_animations=False,
        export_skins=False, export_cameras=False, export_lights=False, export_extras=False)
    pressure_export.repair_exported_tangents(MODELS / f'{name}.glb')

for name, graphic in [('crt_terminal','waveform'), ('control_console','electrical'),
                      ('surveillance_monitor','surveillance')]:
    if SELECTED and name not in SELECTED:
        continue
    bpy.ops.wm.open_mainfile(filepath=str(MODELS / f'{name}.blend'))
    image = bpy.data.images.load(str(GRAPHICS / f'{graphic}.png'))
    image.pack()
    count = 0
    for surface in {slot.material for obj in bpy.context.scene.objects if obj.type == 'MESH' for slot in obj.material_slots if slot.material}:
        if surface.users and surface.use_nodes:
            for node in surface.node_tree.nodes:
                if node.type == 'TEX_IMAGE' and node.image and Path(node.image.filepath).name in (
                        'instrument_display.png', f'{graphic}.png'):
                    node.image = image
                    surface.name = f'RC / {graphic} / static instrument face'
                    count += 1
    if count != 1:
        raise RuntimeError(f'{name}: expected one display slot, found {count}')
    save(name)

for name, graphic in [('pressure_control_casing','pressure'), ('cooling_control_casing','cooling')]:
    if SELECTED and name not in SELECTED:
        continue
    bpy.ops.wm.open_mainfile(filepath=str(MODELS / 'pressure_control_casing.blend'))
    for obj in list(bpy.data.objects):
        if obj.name == 'Printed circuit legend':
            bpy.data.objects.remove(obj, do_unlink=True)
    surface = bpy.data.materials.new(f'RC / {graphic} / printed circuit legend')
    surface.use_nodes = True
    surface.use_backface_culling = True
    shader = surface.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Roughness'].default_value = .92
    image = bpy.data.images.load(str(GRAPHICS / f'{graphic}.png'))
    image.pack()
    node = surface.node_tree.nodes.new('ShaderNodeTexImage')
    node.image = image
    surface.node_tree.links.new(node.outputs['Color'], shader.inputs['Base Color'])
    # Below the interactive terminal, between the hinges; flush to the cabinet face.
    mesh = bpy.data.meshes.new('Printed circuit legend')
    mesh.from_pydata([(-.54,-.289,.19),(.54,-.289,.19),(.54,-.289,.865),(-.54,-.289,.865)],[],[(0,1,2,3)])
    uv = mesh.uv_layers.new()
    for loop, coordinate in zip(uv.data,[(0,0),(1,0),(1,1),(0,1)]):
        loop.uv = coordinate
    mesh.materials.append(surface)
    obj = bpy.data.objects.new('Printed circuit legend',mesh)
    bpy.context.collection.objects.link(obj)
    save(name)

for name, graphic in [('electrical_notes_board','electrical_paper'),
                      ('pressure_notes_board','pressure'), ('cooling_notes_board','cooling')]:
    if SELECTED and name not in SELECTED:
        continue
    bpy.ops.wm.open_mainfile(filepath=str(MODELS / 'lab_notes_board.blend'))
    image = bpy.data.images.load(str(GRAPHICS / f'{graphic}.png'))
    image.pack()
    count = 0
    for surface in {slot.material for obj in bpy.context.scene.objects if obj.type == 'MESH'
                    for slot in obj.material_slots if slot.material}:
        if surface.use_nodes:
            for node in surface.node_tree.nodes:
                if node.type == 'TEX_IMAGE' and node.image and Path(node.image.filepath).name == 'lab_notes.png':
                    node.image = image
                    surface.name = f'RC / {graphic} / printed reference'
                    count += 1
    if count != 1:
        raise RuntimeError(f'{name}: expected one printed graphic slot, found {count}')
    save(name)
