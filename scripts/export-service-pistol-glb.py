"""Original full-size service pistol; metres, +Y up, -Z muzzle in RelayCore.

Blender --background --factory-startup --disable-autoexec --python this_file.
Use -- --graybox for the proportion checkpoint in build/blender-review/vis34.
The rigid frame includes the glove/sleeve; the slide travels along local +Z.
"""
from pathlib import Path
from math import cos, sin, pi
import sys

import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
REVIEW = ROOT / "build/blender-review/vis34"
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
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    return mat


steel = material("Parkerized blue steel", (.085, .105, .115), .43, .65)
edge = material("Polished working steel", (.30, .34, .35), .31, .75)
grip = material("Brown molded phenolic", (.105, .061, .035), .68)
dark = material("Recesses", (.012, .017, .020), .88)
glove = material("Sage protective glove", (.19, .225, .18), .86)
cloth = material("Institutional cotton sleeve", (.52, .50, .415), .92)
seam = material("Cotton seam", (.37, .365, .30), .94)
ivory = material("Sight paint", (.74, .70, .52), .64)


def mesh(name, vertices, faces, mat, bevel=0, smooth=False):
    data = bpy.data.meshes.new(name)
    data.from_pydata([point(v) for v in vertices], [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    data.materials.append(mat)
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data)
    bm.free()
    for face in data.polygons:
        face.use_smooth = smooth
    if bevel:
        mod = obj.modifiers.new("Machined edge radius", "BEVEL")
        mod.width, mod.segments = bevel, 2
        mod = obj.modifiers.new("Face weighted normals", "WEIGHTED_NORMAL")
        mod.keep_sharp = True
    return obj


def profile(name, yz, half_width, mat, bevel=.001):
    n = len(yz)
    vertices = [(x, y, z) for x in (-half_width, half_width) for y, z in yz]
    faces = [tuple(reversed(range(n))), tuple(range(n, 2*n))]
    faces += [(i, (i+1)%n, (i+1)%n+n, i+n) for i in range(n)]
    return mesh(name, vertices, faces, mat, bevel)


def box(name, center, size, mat, bevel=.001):
    x, y, z = center
    sx, sy, sz = [v/2 for v in size]
    obj = profile(name, [(y-sy, z-sz), (y+sy, z-sz), (y+sy, z+sz), (y-sy, z+sz)], sx, mat, bevel)
    obj.location.x = x
    return obj


def sweep(name, centers, radii, mat, sides=20):
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
        vertices += [center + rx*cos(j*2*pi/sides)*u + ry*sin(j*2*pi/sides)*v for j in range(sides)]
    for i in range(len(centers)-1):
        for j in range(sides):
            a, b = i*sides+j, i*sides+(j+1)%sides
            faces.append((a, b, b+sides, a+sides))
    faces += [tuple(reversed(range(sides))), tuple(range(len(vertices)-sides, len(vertices)))]
    return mesh(name, vertices, faces, mat, smooth=True)


def tube(name, y, z0, z1, outer, inner, mat):
    sides = 40
    vertices = [(r*cos(i*2*pi/sides), y+r*sin(i*2*pi/sides), z)
                for z, r in ((z0, outer), (z1, outer), (z1, inner), (z0, inner)) for i in range(sides)]
    faces = []
    for ring in range(4):
        for i in range(sides):
            a, b = ring*sides+i, ring*sides+(i+1)%sides
            next_ring = (ring+1)%4*sides
            faces.append((a, b, next_ring+(i+1)%sides, next_ring+i))
    return mesh(name, vertices, faces, mat, smooth=True)


# Full-size pistol with a 219 mm slide, sloped backstrap and open trigger bow.
profile("Frame and dust cover", [(.026, -.160), (.027, .054), (.014, .071),
        (-.012, .056), (-.030, .019), (-.025, -.021), (-.014, -.060), (.005, -.100), (.006, -.157)], .016, steel, .002)
profile("Sloping grip core", [(-.017, .009), (-.016, .051), (-.028, .066),
        (-.115, .093), (-.125, .078), (-.124, .035), (-.105, .021)], .0165, steel, .003)
outer = [(-.019, -.067), (-.022, -.078), (-.054, -.076), (-.069, -.060), (-.069, -.020), (-.048, .009), (-.025, .010)]
inner = [(-.027, -.060), (-.029, -.068), (-.051, -.067), (-.060, -.054), (-.060, -.023), (-.044, .001), (-.032, .002)]
n = len(outer)
vertices = [(x, y, z) for x in (-.010, .010) for loop in (outer, inner) for y, z in loop]
faces = []
for i in range(n):
    j = (i+1)%n
    faces += [(i, j, j+n, i+n), (i+2*n, i+3*n, j+3*n, j+2*n),
              (i, i+2*n, j+2*n, j), (i+n, j+n, j+3*n, i+3*n)]
mesh("Open trigger guard", vertices, faces, steel, .0015)

for sign in (-1, 1):
    obj = profile("Molded grip panel", [(-.033, .029), (-.030, .056), (-.046, .069),
                  (-.112, .086), (-.118, .077), (-.115, .039), (-.098, .029)], .003, grip, .002)
    obj.location.x = sign*.018
box("Magazine heel", (0, -.122, .060), (.038, .008, .058), dark, .002)
tube("Bore and muzzle crown", .047, -.172, -.040, .0085, .0046, edge)
tube("Dark bore interior", .047, -.169, -.100, .00465, .0042, dark)
box("Recoil spring guide", (0, .001, -.155), (.011, .011, .015), edge, .003)

# One connected glove surface around the grip, with a bent trigger finger.
hand_parts = []
hand_parts.append(sweep("Palm and wrist", [(.006, -.052, .061), (.018, -.072, .064),
                (.021, -.098, .075), (.033, -.118, .103), (.048, -.135, .130)],
                [(.018, .021), (.022, .025), (.022, .024), (.020, .022), (.019, .021)], glove, 28))
for y, r in ((-.062, .0085), (-.082, .008), (-.100, .007)):
    hand_parts.append(sweep("Wrapped glove finger", [(.020, y, .062), (.032, y, .033),
                (.028, y-.002, .008), (.012, y-.003, .001), (-.008, y-.002, .006), (-.018, y, .022)],
                [r, r, r, r*.94, r*.88, r*.65], glove))
hand_parts.append(sweep("Trigger finger", [(.019, -.048, .053), (.032, -.040, .025),
            (.032, -.039, -.015), (.026, -.044, -.040), (.012, -.046, -.045), (.004, -.041, -.035)],
            [.008, .008, .0078, .0075, .0065, .0045], glove))
hand_parts.append(sweep("Thumb web and thumb", [(.004, -.076, .070), (-.019, -.058, .053),
            (-.025, -.039, .023), (-.023, -.025, -.002), (-.022, -.024, -.022)],
            [.014, .013, .011, .009, .006], glove))
bpy.ops.object.select_all(action="DESELECT")
for obj in hand_parts:
    obj.select_set(True)
bpy.context.view_layer.objects.active = hand_parts[0]
bpy.ops.object.join()
hand = hand_parts[0]
hand.name = "Continuous protective glove"
if not GRAYBOX:
    remesh = hand.modifiers.new("Joined finger webs", "REMESH")
    remesh.mode, remesh.voxel_size = "VOXEL", .0013
    smooth = hand.modifiers.new("Soft glove folds", "SMOOTH")
    smooth.factor, smooth.iterations = .65, 4
    decimate = hand.modifiers.new("Preserve glove silhouette at viewmodel density", "DECIMATE")
    decimate.ratio = .10

sweep("Cuff and forearm sleeve", [(.047, -.134, .129), (.055, -.145, .146),
      (.069, -.162, .182), (.088, -.183, .237), (.108, -.205, .295)],
      [(.023, .024), (.027, .027), (.030, .031), (.034, .035), (.037, .039)], cloth, 32)
frame = set(bpy.context.scene.objects)

# The slide is a hollow moving shell; barrel and recoil guide stay on the frame.
section = [(-.016, .027), (-.016, .052), (-.010, .066), (.010, .066), (.016, .052), (.016, .027)]
verts = [(x, y, z) for z in (-.160, .059) for x, y in section]
faces = [(i, (i+1)%6, (i+1)%6+6, i+6) for i in range(6)]
faces.append(tuple(range(6, 12)))
mesh("Faceted machined slide", verts, faces, steel, .0012)
# Front annular web leaves the muzzle visible, including during slide travel.
for sign in (-1, 1):
    box("Front slide cheek", (sign*.013, .046, -.159), (.006, .038, .004), steel)
box("Front slide bridge", (0, .062, -.159), (.024, .007, .004), steel)
box("Front slide lower bridge", (0, .030, -.159), (.024, .006, .004), steel)
box("Front sight blade", (0, .071, -.139), (.004, .012, .013), dark, .0008)
for sign in (-1, 1):
    box("Notched rear sight", (sign*.008, .071, .040), (.007, .012, .013), dark, .0008)

if not GRAYBOX:
    for sign in (-1, 1):
        for i in range(9):
            box("Rear cocking groove", (sign*.0163, .041, .004+i*.0048), (.0009, .021, .0015), dark, .0002)
        box("Rear sight paint", (sign*.008, .073, .047), (.0025, .0025, .0006), ivory, .0002)
    box("Front sight paint", (0, .074, -.132), (.0022, .003, .0006), ivory, .0002)
    box("Ejection port well", (.004, .0663, -.030), (.013, .0008, .036), dark, .0006)
    box("Visible chamber hood", (.003, .0668, -.028), (.010, .0005, .027), edge, .0003)
slide = set(bpy.context.scene.objects) - frame

if not GRAYBOX:
    profile("Exposed hammer", [(.026, .061), (.030, .070), (.039, .078),
            (.045, .076), (.043, .068), (.033, .061)], .005, dark, .001)
    sweep("Curved trigger", [(0, -.026, -.009), (0, -.041, -.018), (0, -.052, -.030)], [.003, .003, .003], edge, 16)
    box("Slide stop", (-.019, .008, -.014), (.007, .007, .038), steel)
    box("Safety paddle", (-.020, .006, .039), (.007, .010, .020), dark, .0018)
    for sign in (-1, 1):
        for y, z in ((-.042, .044), (-.108, .061)):
            obj = box("Grip screw", (sign*.0215, y, z), (.0018, .005, .005), edge, .001)
        for i in range(7):
            obj = box("Molded grip ribs", (sign*.0212, -.050-i*.008, .046+i*.002), (.001, .002, .025), dark, .0003)
    sweep("Sleeve top seam", [(.047, -.113, .132), (.062, -.129, .169), (.083, -.148, .226), (.105, -.167, .289)], [.00065]*4, seam, 8)
    for i in range(3):
        sweep("Cuff stitched fold", [(.029, -.131-i*.004, .131+i*.006), (.043, -.114-i*.004, .137+i*.006),
              (.062, -.118-i*.004, .145+i*.006), (.072, -.134-i*.004, .149+i*.006)], [.0009]*4, seam, 10)
frame = set(bpy.context.scene.objects) - slide


def finalize(objects):
    for obj in objects:
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        for mod in list(obj.modifiers):
            bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        for vertex in obj.data.vertices:
            if vertex.co.z < -.02:
                vertex.co.z = -.02 + (vertex.co.z + .02) * .78
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        # Native products require cross-product area > 1e-7 m², including bevel slivers.
        tiny = [f for f in bm.faces if f.calc_area() <= 5.1e-8]
        if tiny:
            bmesh.ops.delete(bm, geom=tiny, context="FACES")
        bm.to_mesh(obj.data)
        bm.free()
        obj.data.update()
        # Acquire UV data after replacing topology; previous RNA handles are stale.
        uv = obj.data.uv_layers.new(name="UVMap")
        for face in obj.data.polygons:
            axis = max(range(3), key=lambda a: abs(face.normal[a]))
            axes = ((1, 2), (0, 2), (0, 1))[axis]
            for loop in face.loop_indices:
                p = obj.data.vertices[obj.data.loops[loop].vertex_index].co
                uv.data[loop].uv = (p[axes[0]]/.1, p[axes[1]]/.1)


finalize(frame | slide)
out = REVIEW / "graybox" if GRAYBOX else ROOT / "content/models"
out.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=str(out / "service_pistol.blend"))
if not GRAYBOX:
    for name, objects in (("pistol_frame", frame), ("pistol_slide", slide)):
        groups = {}
        for obj in sorted(objects, key=lambda o: o.name):
            groups.setdefault(obj.data.materials[0], []).append(obj)
        exported = []
        for mat, group in groups.items():
            bpy.ops.object.select_all(action="DESELECT")
            for obj in group:
                obj.select_set(True)
            bpy.context.view_layer.objects.active = group[0]
            if len(group) > 1:
                bpy.ops.object.join()
            group[0].name = name + " / " + mat.name
            exported.append(group[0])
        bpy.ops.object.select_all(action="DESELECT")
        for obj in exported:
            obj.select_set(True)
        bpy.ops.export_scene.gltf(filepath=str(out / (name+".glb")), export_format="GLB", use_selection=True,
            export_yup=True, export_normals=True, export_texcoords=True, export_tangents=False,
            export_animations=False, export_skins=False, export_cameras=False, export_lights=False,
            export_extras=False)
        print(name, "triangles=", sum(len(o.data.polygons) for o in exported))
