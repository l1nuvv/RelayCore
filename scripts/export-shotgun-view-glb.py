"""VIS-35: preserve the supplied Stevens kit; add rigid gloves and sleeves.

Blender --background --factory-startup --disable-autoexec --python this_file.
Use -- --graybox for the contact checkpoint without writing authored outputs.
Runtime basis: metres, +Y up, -Z muzzle. Left hand belongs to the pump.
"""
from pathlib import Path
from math import cos, sin, pi
import sys

import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
REVIEW = ROOT / "build/blender-review/vis35"
GRAYBOX = "--graybox" in sys.argv
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system = "METRIC"
bpy.context.preferences.filepaths.save_version = 0


def point(p):
    return Vector((p[0], -p[2], p[1]))


def material(name, color, roughness, metallic=0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.use_backface_culling = True
    surface = mat.node_tree.nodes.get("Principled BSDF")
    surface.inputs["Base Color"].default_value = (*color, 1)
    surface.inputs["Roughness"].default_value = roughness
    surface.inputs["Metallic"].default_value = metallic
    return mat


wood = material("Oiled dark walnut", (.095, .049, .024), .62)
steel = material("Blued service steel", (.075, .088, .095), .40, .60)
recess = material("Dark steel recess", (.030, .036, .040), .56, .45)
edge = material("Working steel", (.16, .18, .19), .34, .70)
glove = material("Sage protective glove", (.19, .225, .18), .86)
cloth = material("Institutional cotton sleeve", (.52, .50, .415), .92)
seam = material("Cotton seam", (.37, .365, .30), .94)
texture_dir = REVIEW / "graybox" if GRAYBOX else ROOT / "content/graphics/weapons"
texture_dir.mkdir(parents=True, exist_ok=True)
grain = bpy.data.images.new("Shotgun walnut grain", width=512, height=128)
pixels = []
for y in range(128):
    v = y / 128
    for x in range(512):
        u = x / 512
        band = sin(2*pi*(v*18 + .28*sin(2*pi*u) + .13*sin(6*pi*u)))
        fine = sin(2*pi*(v*61 + .18*sin(4*pi*u)))
        factor = 1 + .12*band + .035*fine
        # Albedo only: the grain has no baked shadow or specular highlight.
        linear = [.095*factor, .049*factor, .024*factor]
        pixels.extend([12.92*c if c <= .0031308 else 1.055*c**(1/2.4)-.055 for c in linear] + [1])
grain.pixels = pixels
grain.filepath_raw = str(texture_dir / "shotgun_walnut.png")
grain.file_format = "PNG"
grain.save()
grain.pack()
node = wood.node_tree.nodes.new("ShaderNodeTexImage")
node.image = grain
wood.node_tree.links.new(node.outputs["Color"], wood.node_tree.nodes.get("Principled BSDF").inputs["Base Color"])


def sweep(name, centers, radii, mat, sides=24):
    centers = [Vector(p) for p in centers]
    vertices, faces = [], []
    for i, center in enumerate(centers):
        tangent = (centers[min(i+1, len(centers)-1)] - centers[max(i-1, 0)]).normalized()
        u = tangent.cross(Vector((0, 1, 0)))
        if u.length < .01:
            u = tangent.cross(Vector((1, 0, 0)))
        u.normalize()
        v = tangent.cross(u).normalized()
        rx, ry = radii[i] if isinstance(radii[i], tuple) else (radii[i], radii[i])
        vertices += [point(center + rx*cos(j*2*pi/sides)*u + ry*sin(j*2*pi/sides)*v) for j in range(sides)]
    for i in range(len(centers)-1):
        for j in range(sides):
            a, b = i*sides+j, i*sides+(j+1)%sides
            faces.append((a, b, b+sides, a+sides))
    faces += [tuple(reversed(range(sides))), tuple(range(len(vertices)-sides, len(vertices)))]
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    mesh.materials.append(mat)
    for face in mesh.polygons:
        face.use_smooth = True
    return obj


def join(objects, name):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    objects[0].name = name
    return objects[0]


parts = {}
for part in ("body", "pump"):
    bpy.ops.wm.obj_import(filepath=str(ROOT / f"assets/models/shotgun_{part}.obj"),
                        forward_axis="NEGATIVE_Z", up_axis="Y")
    obj = bpy.context.object
    obj.name = "Supplied Stevens " + part
    obj.data.materials.clear()
    for mat in (wood, steel, recess, edge):
        obj.data.materials.append(mat)
    for face in obj.data.polygons:
        uv = obj.data.uv_layers[0].data[face.loop_start].uv
        swatch = round(uv.x*4-.5) + round(uv.y*4-.5)*4
        face.material_index = 0 if swatch == 0 else (3 if swatch in (4, 5) else (2 if swatch == 6 else 1))
    parts[part] = {obj}

world_parts = parts["body"] | parts["pump"]

# Right palm encloses the narrow stock wrist behind the trigger bow.
right = [sweep("Right palm and wrist", [(.014, -.078, .040), (.022, -.090, .059),
               (.025, -.110, .087), (.034, -.131, .120), (.044, -.143, .145)],
               [(.017, .022), (.022, .026), (.023, .027), (.021, .024), (.020, .022)], glove)]
for y, z, r in ((-.086, .040, .0085), (-.103, .057, .008), (-.118, .074, .0075)):
    right.append(sweep("Right wrapped finger", [(.024, y, z+.020), (.031, y, z),
                 (.023, y-.005, z-.018), (.003, y-.007, z-.022), (-.015, y-.005, z-.009),
                 (-.018, y+.001, z+.009)], [r, r, r, r*.95, r*.85, r*.6], glove))
right.append(sweep("Right trigger finger", [(.020, -.076, .049), (.030, -.063, .018),
             (.027, -.060, -.014), (.020, -.066, -.029), (.006, -.073, -.027)],
             [.009, .009, .008, .007, .005], glove))
right.append(sweep("Right thumb and web", [(.010, -.096, .080), (-.015, -.074, .061),
             (-.024, -.051, .030), (-.021, -.047, -.004)], [.016, .014, .011, .007], glove))

# Left palm cups the underside of the wooden fore-end; fingers wrap the right side.
left = [sweep("Left palm and wrist", [(-.008, -.079, -.405), (-.018, -.087, -.389),
              (-.034, -.103, -.360), (-.051, -.120, -.325)],
              [(.025, .022), (.027, .025), (.024, .023), (.020, .022)], glove)]
for z, r in ((-.439, .0085), (-.418, .009), (-.396, .0085), (-.376, .0075)):
    left.append(sweep("Left wrapped finger", [(-.014, -.083, z), (.008, -.082, z),
                (.028, -.067, z), (.031, -.045, z+.002), (.023, -.029, z+.006),
                (.010, -.025, z+.008)], [r, r, r, r*.92, r*.82, r*.65], glove))
left.append(sweep("Left thumb and web", [(-.024, -.084, -.387), (-.034, -.064, -.373),
            (-.028, -.040, -.362), (-.016, -.026, -.365), (.002, -.027, -.378)],
            [.014, .013, .011, .009, .006], glove))

for part, objects, name in (("body", right, "Right continuous glove"), ("pump", left, "Left continuous glove")):
    obj = join(objects, name)
    if not GRAYBOX:
        remesh = obj.modifiers.new("Connected finger webs", "REMESH")
        remesh.mode, remesh.voxel_size = "VOXEL", .0013
        smooth = obj.modifiers.new("Soft glove surface", "SMOOTH")
        smooth.factor, smooth.iterations = .65, 4
        decimate = obj.modifiers.new("Viewmodel glove density", "DECIMATE")
        decimate.ratio = .10
    parts[part].add(obj)

parts["body"].add(sweep("Right cuff and sleeve", [(.044, -.143, .145), (.053, -.152, .167),
                  (.069, -.174, .214), (.095, -.206, .278), (.125, -.230, .340)],
                  [(.024, .025), (.028, .029), (.032, .034), (.038, .039), (.042, .044)], cloth, 32))
parts["pump"].add(sweep("Left cuff and sleeve", [(-.051, -.120, -.325), (-.060, -.133, -.302),
                  (-.075, -.154, -.258), (-.104, -.188, -.197), (-.136, -.221, -.130),
                  (-.170, -.270, -.030), (-.195, -.335, .080), (-.205, -.430, .190)],
                  [(.025, .026), (.029, .031), (.032, .035), (.038, .040), (.043, .045),
                   (.047, .049), (.050, .053), (.052, .055)], cloth, 32))
if not GRAYBOX:
    for part, center in (("body", (.049, -.146, .155)), ("pump", (-.056, -.126, -.312))):
        x, y, z = center
        for offset in (0, .008):
            parts[part].add(sweep(part + " cuff fold", [(x-.020, y, z+offset),
                (x-.012, y+.018, z+offset), (x+.009, y+.021, z+offset), (x+.023, y+.006, z+offset)],
                [.0012]*4, seam, 10))

for obj in parts["body"] | parts["pump"]:
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    for mod in list(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=mod.name)
    if obj.name in ("Right continuous glove", "Left continuous glove"):
        for face in obj.data.polygons:
            face.use_smooth = True
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.triangulate(bm, faces=list(bm.faces))
    tiny = [f for f in bm.faces if f.calc_area() <= 5.1e-8]
    if tiny:
        bmesh.ops.delete(bm, geom=tiny, context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    uv = obj.data.uv_layers.active or obj.data.uv_layers.new(name="UVMap")
    for face in obj.data.polygons:
        axis = max(range(3), key=lambda a: abs(face.normal[a]))
        axes = ((1, 2), (0, 2), (0, 1))[axis]
        for loop in face.loop_indices:
            p = obj.data.vertices[obj.data.loops[loop].vertex_index].co
            uv.data[loop].uv = ((p.y/.6, p.z/.12) if obj.data.materials[face.material_index] == wood
                                else (p[axes[0]]/.1, p[axes[1]]/.1))

out = REVIEW / "graybox" if GRAYBOX else ROOT / "content/models"
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(out / "shotgun_view.blend"))
if not GRAYBOX:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in world_parts:
        obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(out / "shotgun_world.glb"), export_format="GLB",
        use_selection=True, export_yup=True, export_normals=True, export_texcoords=True,
        export_animations=False, export_skins=False, export_cameras=False, export_lights=False,
        export_extras=False)
    for part, objects in parts.items():
        bpy.ops.object.select_all(action="DESELECT")
        for obj in objects:
            obj.select_set(True)
        bpy.ops.export_scene.gltf(filepath=str(out / f"shotgun_view_{part}.glb"), export_format="GLB",
            use_selection=True, export_yup=True, export_normals=True, export_texcoords=True,
            export_tangents=False, export_animations=False, export_skins=False,
            export_cameras=False, export_lights=False, export_extras=False)
        print(part, "triangles=", sum(len(o.data.polygons) for o in objects))
