"""Build the fitted B-17 analog observation camera around its authored optical pose."""
from pathlib import Path
import json
import bpy
from mathutils import Vector
root=Path(__file__).resolve().parents[1]
pose=json.loads((root/'content/graphics/instruments/observation_camera.json').read_text())
def point(v): return Vector((v[0],-v[2],v[1]))
eye=point(pose['eye'])
direction=(point(pose['target'])-eye).normalized()
rotation=direction.to_track_quat('-Y','Z')
bpy.ops.wm.read_factory_settings(use_empty=True)
def material(name,color,metal,rough):
    m=bpy.data.materials.new(name)
    m.use_nodes=True
    m.use_backface_culling=True
    p=m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Metallic'].default_value=metal
    p.inputs['Roughness'].default_value=rough
    return m
enamel=material('Camera / ivory enamel',(.49,.50,.42),0,.62)
steel=material('Camera / galvanized mounting steel',(.30,.33,.30),.8,.57)
dark=material('Camera / lens hood and rubber cable',(.018,.024,.021),0,.85)
glass=material('Camera / recessed optical glass',(.025,.055,.046),0,.16)
def box(name,local,size,mat,bevel=.006):
    bpy.ops.mesh.primitive_cube_add(size=1,location=eye+rotation@Vector(local))
    obj=bpy.context.object
    obj.name=name
    obj.rotation_mode='QUATERNION'
    obj.rotation_quaternion=rotation
    obj.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    obj.data.materials.append(mat)
    if bevel:
        mod=obj.modifiers.new('Folded housing edges','BEVEL')
        mod.width=bevel
        mod.segments=2
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj
def rod(name,a,b,radius,mat,sides=20):
    delta=b-a
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides,radius=radius,depth=delta.length,location=(a+b)/2)
    obj=bpy.context.object
    obj.name=name
    obj.rotation_mode='QUATERNION'
    obj.rotation_quaternion=delta.to_track_quat('Z','Y')
    obj.data.materials.append(mat)
    return obj
box('Folded steel camera body',(0,.205,0),(.20,.32,.15),enamel,.012)
box('Removable rear access cover',(0,.370,0),(.182,.012,.132),steel,.004)
box('Black recessed front bezel',(0,.042,0),(.18,.016,.13),dark,.004)
for x in (-.095,.095):
    box('Projecting side lens hood',(x,.012,.008),(.010,.105,.142),enamel,.002)
box('Projecting top lens hood',(0,.012,.079),(.20,.105,.010),enamel,.002)
rod('Lens barrel',eye+rotation@Vector((0,.003,0)),eye+rotation@Vector((0,.048,0)),.048,dark,32)
rod('Recessed optical face',eye+rotation@Vector((0,.002,0)),eye+rotation@Vector((0,.004,0)),.035,glass,32)
for x in (-.067,.067):
    for z in (-.047,.047):
        rod('Captive cover screw',eye+rotation@Vector((x,.377,z)),eye+rotation@Vector((x,.381,z)),.006,steel,12)
joint=eye+rotation@Vector((0,.235,-.10))
rod('Camera pivot trunnion',joint+rotation@Vector((-.10,0,0)),joint+rotation@Vector((.10,0,0)),.023,steel)
base=Vector((joint.x,joint.y,.78))
rod('Telescoping observation stand',base,joint,.018,steel)
rod('Lower stand collar',base,base+Vector((0,0,.27)),.026,dark)
for dx,dz in [(-.32,-.25),(.32,-.25),(0,.36)]:
    foot=Vector((base.x+dx,base.y-dz,.035))
    rod('Splayed tripod leg',base+Vector((0,0,.17)),foot,.018,steel)
    rod('Rubber floor foot',foot-Vector((0,0,.015)),foot+Vector((0,0,.015)),.033,dark)
back=eye+rotation@Vector((.045,.385,-.045))
for a,b in [(back,base+Vector((.04,0,.28))),(base+Vector((.04,0,.28)),base+Vector((.04,0,-.70)))]:
    rod('Coaxial service cable',a,b,.008,dark,12)
box('Camera identification plate',(0,.21,.078),(.15,.075,.002),dark,0)
bpy.ops.object.text_add(location=eye+rotation@Vector((0,.21,.080)))
obj=bpy.context.object
obj.name='CAM 02 identification'
obj.rotation_mode='QUATERNION'
obj.rotation_quaternion=rotation
obj.data.body='CAM 02'
obj.data.align_x='CENTER'
obj.data.align_y='CENTER'
obj.data.size=.027
obj.data.resolution_u=2
obj.data.materials.append(enamel)
bpy.ops.object.convert(target='MESH')
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(root/'content/models/b17_observation_camera.blend'))
bpy.ops.export_scene.gltf(filepath=str(root/'content/models/b17_observation_camera.glb'),export_format='GLB',
    export_yup=True,export_normals=True,export_texcoords=True,export_animations=False,
    export_skins=False,export_cameras=False,export_lights=False,export_extras=False)
