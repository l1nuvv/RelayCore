"""Run with Blender --background --disable-autoexec source.blend --python this_script."""
import collections
import pathlib
import struct

import bpy
from mathutils import Vector

root = pathlib.Path(__file__).resolve().parents[1] / 'assets' / 'models'
obj = next(o for o in bpy.data.objects if o.type == 'MESH')
mesh = obj.data
mesh.calc_loop_triangles()
groups = {g.index: g.name for g in obj.vertex_groups}
parts = {}
for vertex in mesh.vertices:
    group = max(vertex.groups, key=lambda g: g.weight)
    if group.weight < .999:
        raise RuntimeError('Expected rigid weapon parts, found blended bone weights')
    parts[vertex.index] = groups[group.group]
gun_vertices = [obj.matrix_world @ v.co for v in mesh.vertices if not parts[v.index].startswith('shell')]
length = max(v.x for v in gun_vertices) - min(v.x for v in gun_vertices)
scale = 1.02 / length
anchor = obj.matrix_world @ Vector((0, 0, max(v.co.z for v in mesh.vertices if not parts[v.index].startswith('shell'))))
normal_matrix = obj.matrix_world.to_3x3().inverted().transposed()
def point(v):
    v = (obj.matrix_world @ v - anchor) * scale
    return (-v.y, v.z, -v.x)
def normal(v):
    v = (normal_matrix @ v).normalized()
    return (-v.y, v.z, -v.x)
colors = []
for mat in mesh.materials:
    surface = next(n for n in mat.node_tree.nodes if n.type in ('BSDF_PRINCIPLED', 'BSDF_DIFFUSE'))
    name = 'Base Color' if surface.type == 'BSDF_PRINCIPLED' else 'Color'
    color = surface.inputs[name].default_value[:3]
    colors.append(tuple(round(255 * (12.92*c if c <= .0031308 else 1.055*c**(1/2.4)-.055)) for c in color))
size = 32
pixels = bytearray()
for y in range(size):
    for x in range(size):
        index = (y//8)*4 + x//8
        r,g,b = colors[index] if index < len(colors) else (0,0,0)
        pixels.extend((b,g,r))
header = b'BM' + struct.pack('<IHHI',54+len(pixels),0,0,54)
header += struct.pack('<IiiHHIIiiII',40,size,size,1,24,0,len(pixels),2835,2835,0,0)
(root/'shotgun.bmp').write_bytes(header+pixels)
counts = collections.Counter()
for part in ('body','pump'):
    triangles = []
    for triangle in mesh.loop_triangles:
        names = {parts[i] for i in triangle.vertices}
        if any(n.startswith('shell') for n in names):
            continue
        if (part == 'pump') != ('pump' in names):
            continue
        if 'pump' in names and len(names) != 1:
            raise RuntimeError('A triangle crosses the pump/body boundary')
        triangles.append(triangle)
    lines = ['# Stevens Model 520; rigid '+part+'; metres, +Y up, -Z muzzle', 'o shotgun_'+part]
    for triangle in triangles:
        for vertex in triangle.vertices:
            lines.append('v {:.7f} {:.7f} {:.7f}'.format(*point(mesh.vertices[vertex].co)))
    for triangle in triangles:
        material = mesh.polygons[triangle.polygon_index].material_index
        u = ((material%4)*8+4)/size
        v = ((material//4)*8+4)/size
        for _ in range(3):
            lines.append(f'vt {u:.7f} {v:.7f}')
    for triangle in triangles:
        for loop in triangle.loops:
            lines.append('vn {:.7f} {:.7f} {:.7f}'.format(*normal(mesh.corner_normals[loop].vector)))
    for i in range(len(triangles)):
        lines.append('f '+' '.join(f'{j}/{j}/{j}' for j in range(i*3+1,i*3+4)))
    (root/f'shotgun_{part}.obj').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    counts[part] = len(triangles)
print('SHOTGUN_EXPORT',dict(counts),'length_m=1.02','source_colors=',colors)
