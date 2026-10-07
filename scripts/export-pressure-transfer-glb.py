"""Blender --background --factory-startup --disable-autoexec --python this_file.

Room lining is local to [24, 0, 26]; other floor origins are centered on equipment.
Run with -- --model=<name> to regenerate only one product.
"""
from pathlib import Path
from math import sin, cos, pi
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "content/models"
GRAPHICS = ROOT / "content/graphics/pressure_transfer"
GRAPHICS.mkdir(parents=True, exist_ok=True)
SELECTED = {arg.split("=", 1)[1] for arg in sys.argv if arg.startswith("--model=")}


def material(name, color, roughness, metallic=0, sheet=None, cell=0, texture=None):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.use_backface_culling = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if sheet or texture:
        source = bpy.data.images.load(str(ROOT / (sheet or texture)), check_existing=True)
        w, h = source.size
        pixels = np.asarray(source.pixels[:], dtype=np.float32).reshape(h, w, 4)
        if sheet:
            edge = w // 4
            tile = pixels[(1 - cell // 4) * edge:(2 - cell // 4) * edge,
                          (cell % 4) * edge:(cell % 4 + 1) * edge].copy()
        else:
            tile = pixels.copy()
        # Retain the source's restrained surface variation, with one coherent palette.
        luminance = tile[:, :, :3].mean(axis=2)
        modulation = np.clip(luminance / max(float(luminance.mean()), .001), .80, 1.18)
        tile[:, :, :3] = modulation[:, :, None] * np.array(color)
        tile[:, :, 3] = 1
        height, width = tile.shape[:2]
        image = bpy.data.images.new(name, width=width, height=height)
        image.pixels.foreach_set(tile.ravel())
        image.filepath_raw = str(GRAPHICS / (name + ".png"))
        image.file_format = "PNG"
        image.save()
        image.pack()
        tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
        tex.image = image
        mat.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    return mat


def setup():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = "METRIC"
    bpy.context.scene.unit_settings.scale_length = 1
    return {
        "paint": material("transfer_green_enamel", (.29, .345, .29), .68,
                          sheet="content/graphics/prop_materials_source.png"),
        "dark": material("transfer_dark_steel", (.045, .057, .052), .72, .35),
        "steel": material("transfer_galvanized", (.31, .34, .33), .64, .7),
        "ivory": material("transfer_instrument_ivory", (.72, .69, .57), .76),
        "rubber": material("transfer_rubber", (.018, .024, .023), .91),
        "red": material("transfer_valve_red", (.32, .055, .033), .65),
        "brass": material("transfer_bonded_brass", (.42, .26, .08), .56, .68),
        "concrete": material("transfer_quiet_concrete", (.39, .405, .38), .97,
                             texture="content/third_party/polyhaven/pressure_transfer/concrete_wall_001_diff_1k.jpg"),
        "floor": material("transfer_floor_coating", (.29, .315, .29), .89,
                          sheet="assets/textures/facility_source.png"),
        "yellow": material("transfer_service_ochre", (.49, .29, .065), .8),
    }


def mesh_object(name, vertices, faces, mat):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    mesh.materials.append(mat)
    return obj


def box(name, pos, size, mat, bevel=.008):
    bpy.ops.mesh.primitive_cube_add(size=1, location=pos)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(mat)
    if bevel:
        mod = obj.modifiers.new("Manufactured edge", "BEVEL")
        mod.width = min(bevel, min(size) * .2)
        mod.segments = 2
        bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def rod(name, a, b, radius, mat, sides=24):
    direction = Vector(b) - Vector(a)
    bpy.ops.mesh.primitive_cylinder_add(vertices=sides, radius=radius,
                                      depth=direction.length, location=(Vector(a) + Vector(b)) / 2)
    obj = bpy.context.object
    obj.name = name
    obj.rotation_mode = "QUATERNION"
    obj.rotation_quaternion = direction.to_track_quat("Z", "Y")
    obj.data.materials.append(mat)
    for face in obj.data.polygons:
        face.use_smooth = len(face.vertices) == 4
    return obj


def sweep(name, points, radius, mat, sides=16):
    vertices, faces = [], []
    for i, point in enumerate(points):
        tangent = Vector(points[min(i + 1, len(points) - 1)]) - Vector(points[max(0, i - 1)])
        tangent.normalize()
        axis = Vector((0, 0, 1)) if abs(tangent.z) < .9 else Vector((1, 0, 0))
        u = tangent.cross(axis).normalized()
        v = tangent.cross(u)
        vertices.extend(Vector(point) + radius * (u * cos(j * 2 * pi / sides)
                                                  + v * sin(j * 2 * pi / sides)) for j in range(sides))
    for i in range(len(points) - 1):
        for j in range(sides):
            a, b = i * sides + j, i * sides + (j + 1) % sides
            faces.append((a, b, b + sides, a + sides))
    faces += [tuple(reversed(range(sides))), tuple(range(len(vertices) - sides, len(vertices)))]
    obj = mesh_object(name, vertices, faces, mat)
    for face in obj.data.polygons[:-2]:
        face.use_smooth = True
    return obj


def vessel(name, x, m):
    # Revolved dished ends; shared poles avoid coincident zero-radius rings.
    profile = [(0, 1.85), (.10, 1.87), (.22, 1.94), (.29, 2.06), (.30, 2.15),
               (.30, 2.97), (.29, 3.06), (.22, 3.18), (.10, 3.25), (0, 3.27)]
    vertices, rings, faces = [], [], []
    sides = 40
    for radius, height in profile:
        if radius == 0:
            rings.append([len(vertices)])
            vertices.append((x, -.42, height))
        else:
            rings.append(list(range(len(vertices), len(vertices) + sides)))
            vertices.extend((x + radius * cos(j * 2 * pi / sides),
                             -.42 + radius * sin(j * 2 * pi / sides), height) for j in range(sides))
    for lower, upper in zip(rings, rings[1:]):
        for j in range(sides):
            k = (j + 1) % sides
            if len(lower) == 1:
                faces.append((lower[0], upper[k], upper[j]))
            elif len(upper) == 1:
                faces.append((lower[j], lower[k], upper[0]))
            else:
                faces.append((lower[j], lower[k], upper[k], upper[j]))
    obj = mesh_object(name, vertices, faces, m["paint"])
    for face in obj.data.polygons:
        face.use_smooth = True
    for height in (2.14, 2.98):
        bpy.ops.mesh.primitive_torus_add(major_segments=40, minor_segments=8,
                                       major_radius=.301, minor_radius=.012,
                                       location=(x, -.42, height))
        bpy.context.object.name = name + " welded head seam"
        bpy.context.object.data.materials.append(m["steel"])
    for height in (2.25, 2.88):
        box(name + " wall saddle", (x, -.11, height), (.44, .23, .10), m["dark"])
        box(name + " anchor plate", (x, .005, height), (.50, .04, .24), m["steel"])


def flange(name, center, mat, radius=.12):
    x, y, z = center
    rod(name, (x, y - .035, z), (x, y + .035, z), radius, mat, 32)
    for i in range(6):
        a = i * 2 * pi / 6
        rod(name + " bolt", (x + radius * .76 * cos(a), y - .045, z + radius * .76 * sin(a)),
            (x + radius * .76 * cos(a), y - .07, z + radius * .76 * sin(a)), .012, mat, 6)


def wheel(name, x, y, z, m):
    rod(name + " valve stem", (x, y + .16, z), (x, y, z), .027, m["steel"])
    bpy.ops.mesh.primitive_torus_add(major_segments=32, minor_segments=8,
                                   major_radius=.155, minor_radius=.018,
                                   location=(x, y, z), rotation=(pi / 2, 0, 0))
    bpy.context.object.name = name
    bpy.context.object.data.materials.append(m["red"])
    for i in range(3):
        a = i * 2 * pi / 3
        rod(name + " spoke", (x, y, z), (x + .14 * cos(a), y, z + .14 * sin(a)), .012, m["red"], 10)


def gauge(name, x, y, z, m, anomalous=False):
    rod(name + " radial stem", (x, y + .06, z - .24), (x, y + .06, z), .022, m["brass"])
    rod(name + " case", (x, y + .10, z), (x, y, z), .16, m["steel"], 40)
    rod(name + " dial", (x, y - .002, z), (x, y - .006, z), .142, m["ivory"], 40)
    for i in range(13):
        a = (-.75 + i / 12 * 1.5) * pi
        inner, outer = .107, .128
        rod(name + " graduation", (x + inner * sin(a), y - .011, z + inner * cos(a)),
            (x + outer * sin(a), y - .011, z + outer * cos(a)), .0028, m["dark"], 6)
    a = (-.48 if anomalous else .24) * pi
    rod(name + " needle", (x, y - .018, z),
        (x + .104 * sin(a), y - .018, z + .104 * cos(a)), .004, m["red"], 8)
    rod(name + " pivot", (x, y - .02, z), (x, y - .025, z), .012, m["dark"], 12)


def label(text, pos, size, mat):
    font = bpy.data.curves.new(text, "FONT")
    font.body = text
    font.align_x = "CENTER"
    font.size = size
    font.extrude = 0
    obj = bpy.data.objects.new(text, font)
    bpy.context.collection.objects.link(obj)
    obj.location = pos
    obj.rotation_euler = (pi / 2, 0, 0)
    font.materials.append(mat)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.convert(target="MESH")
    return bpy.context.object


def export(name):
    # UV0 is explicit world-scale projection; all graph inputs are native-supported.
    groups = {}
    for obj in list(bpy.context.scene.objects):
        if obj.type != "MESH":
            continue
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.select_all(action="DESELECT")
        obj.select_set(True)
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        groups.setdefault(obj.data.materials[0], []).append(obj)
    for mat, objects in groups.items():
        bpy.ops.object.select_all(action="DESELECT")
        for obj in objects:
            obj.select_set(True)
        bpy.context.view_layer.objects.active = objects[0]
        if len(objects) > 1:
            bpy.ops.object.join()
        obj = objects[0]
        obj.name = name + " / " + mat.name
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        tiny = []
        for face in bm.faces:
            a, b, c = [obj.matrix_world @ vertex.co for vertex in face.verts]
            if (b - a).cross(c - a).length <= 1.01e-7:
                tiny.append(face)
        bmesh.ops.delete(bm, geom=tiny, context="FACES")
        bm.to_mesh(obj.data)
        bm.free()
        obj.data.update()
        uv = obj.data.uv_layers.active or obj.data.uv_layers.new(name="UVMap")
        # Project finalized triangles. Thin bevel polygons can have stale/unstable
        # pre-triangulation normals and collapse a UV axis into zero tangents.
        for face in obj.data.polygons:
            points = [obj.matrix_world @ obj.data.vertices[i].co for i in face.vertices]
            normal = (points[1] - points[0]).cross(points[2] - points[0])
            axis = max(range(3), key=lambda a: abs(normal[a]))
            axes = ((1, 2), (0, 2), (0, 1))[axis]
            for index, point in zip(face.loop_indices, points):
                metres = 1.5 if mat.name == "transfer_quiet_concrete" else 2
                uv.data[index].uv = (point[axes[0]] / metres, point[axes[1]] / metres)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / (name + ".blend")))
    bpy.ops.export_scene.gltf(filepath=str(OUT / (name + ".glb")), export_format="GLB",
                              export_yup=True, export_normals=True, export_texcoords=True,
                              export_tangents=False, export_animations=False, export_skins=False,
                              export_cameras=False, export_lights=False, export_extras=False)


def manifold(m):
    for x in (-.9, .9):
        vessel("Wall receiver", x, m)
        rod("Receiver outlet", (x, -.42, 1.85), (x, -.42, 1.58), .06, m["steel"])
        flange("Isolation flange", (x, -.44, 1.68), m["steel"])
        wheel("Receiver isolation", x, -.70, 1.68, m)
        gauge("Receiver pressure", x, -.77, 2.48, m, x > 0)
        rod("Gauge takeoff", (x, -.42, 2.24), (x, -.71, 2.24), .022, m["brass"])
    rod("Common receiver header", (-1.5, -.42, 1.58), (1.5, -.42, 1.58), .07, m["paint"])
    # Smooth quarter bends terminate in wall sleeves, not arbitrary cut ends.
    for x, sign in ((-1.5, -1), (1.5, 1)):
        points = [(x + sign * .22 * sin(i * pi / 16), -.42 + .22 * (1 - cos(i * pi / 16)), 1.58)
                  for i in range(9)]
        points.append((x + sign * .22, .02, 1.58))
        sweep("Header wall return", points, .07, m["paint"])
        flange("Wall sleeve", (x + sign * .22, .01, 1.58), m["steel"])
    box("Bonded instrument backplate", (0, -.06, 2.45), (.68, .10, .79), m["dark"])
    for x in (-.20, .20):
        gauge("Differential reference", x, -.21, 2.55, m, x > 0)
    sweep("Brass differential loop", [(-.20, -.15, 2.31), (-.20, -.15, 2.10),
                                      (.20, -.15, 2.10), (.20, -.15, 2.31)], .017, m["brass"])
    label("DIFFERENTIAL", (0, -.118, 2.78), .073, m["ivory"])
    box("System nameplate", (0, -.05, 3.35), (3.5, .08, .26), m["ivory"])
    label("PRESSURE BYPASS", (0, -.095, 3.29), .18, m["dark"])


def casing(m):
    # This shell lies outside the unchanged 2.4 x 2.2 x 2 gameplay-cover solid.
    box("Cover lid", (0, 0, 2.215), (2.44, 2.04, .03), m["paint"])
    for x in (-1.212, 1.212):
        box("Folded side", (x, 0, 1.10), (.024, 2.02, 2.2), m["paint"])
    for y in (-1.014, 1.014):
        box("Service face", (0, y, 1.10), (2.4, .028, 2.2), m["dark"])
        for x in (-.60, .60):
            box("Removable enclosure door", (x, y * 1.02, 1.18), (1.16, .025, 1.98), m["paint"])
            for height in (.27, 1.94):
                box("Door hinge", (x - .50, y * 1.045, height), (.04, .045, .15), m["steel"])
            box("Recessed latch", (x + .44, y * 1.043, 1.22), (.065, .045, .22), m["dark"])
            for height in (.42, .49, .56, .63):
                box("Lower cooling louvre", (x, y * 1.04, height), (.78, .025, .022), m["dark"], .002)
    for x in (-1.1, 1.1):
        box("Enclosure foot", (x, 0, .07), (.24, 1.96, .14), m["dark"])
    box("Upper identification", (0, -1.059, 1.86), (1.7, .018, .20), m["ivory"])
    label("BYPASS ISOLATION", (0, -1.071, 1.81), .12, m["dark"])


def control(m):
    # Backing solid: runtime x=39.4..39.9, y=0..2, z=39..41.
    # Existing interactive terminal remains in front of the central free area.
    box("Control enclosure front", (0, -.272, 1), (2.02, .025, 2), m["paint"])
    box("Control cabinet top", (0, -.015, 2.016), (2.02, .55, .032), m["paint"])
    for x in (-1.01, 1.01):
        box("Cabinet folded cheek", (x, -.02, 1), (.025, .54, 2), m["paint"])
    for x in (-.7, .7):
        box("Control hinges", (x, -.298, .6), (.05, .04, .14), m["steel"])
    box("Meter hood", (0, -.09, 2.21), (1.6, .2, .38), m["dark"])
    for x in (-.48, 0, .48):
        gauge("Transfer meter", x, -.22, 2.22, m)
    box("Control identification", (0, -.295, 1.82), (1.66, .025, .20), m["ivory"])
    label("TRANSFER CONTROL", (0, -.310, 1.77), .12, m["dark"])
    box("Sill", (0, -.29, 1.03), (1.8, .06, .04), m["dark"])
    label("START", (0, -.290, 1.10), .09, m["ivory"])
    rod("Control cable riser", (.72, .02, 2), (.72, .02, 3.38), .025, m["steel"])
    for height in (2.6, 3.25):
        box("Riser wall clamp", (.72, .035, height), (.10, .05, .05), m["steel"])


def room(m):
    # Runtime-local coordinates converted explicitly for this room shell.
    def rbox(name, pos, size, mat, bevel=.008):
        x, y, z = pos
        w, h, d = size
        return box(name, (x, -z, y), (w, d, h), mat, bevel)
    rbox("Quiet coated floor", (8, .007, 9), (15.78, .012, 17.78), m["floor"], 0)
    for x in (.11, 15.89):
        rbox("Concrete wall field", (x, 2.32, 9), (.016, 2.74, 17.78), m["concrete"], 0)
        rbox("Painted service dado", (x, .49, 9), (.025, .96, 17.78), m["paint"], 0)
        rbox("Dado coping", (x, .99, 9), (.045, .045, 17.78), m["steel"])
        for z in (3.8, 9.8, 15.8):
            rbox("Wall pilaster", (x, 2, z), (.17, 3.6, .30), m["concrete"])
    for z in (3.8, 9.8, 15.8):
        rbox("Shallow ceiling rib", (8, 3.69, z), (15.78, .18, .32), m["paint"])
    for center, width in ((1.05, 1.88), (11, 9.78)):
        rbox("Exit wall concrete field", (center, 2.34, 17.89), (width, 2.7, .02), m["concrete"], 0)
        rbox("Exit wall service dado", (center, .49, 17.89), (width, .96, .025), m["paint"], 0)
    rbox("Exit lintel sign", (4, 3.43, 17.86), (3.3, .26, .04), m["ivory"])
    text = label("TECHNICAL SECTOR", (4, -17.835, 3.37), .18, m["dark"])
    text.rotation_euler.z = pi
    # Service zone boundary, not stripes on every surface.
    rbox("Machine service boundary", (14.65, .017, 9), (.07, .008, 8), m["yellow"], 0)
    for z in (5, 13):
        rbox("Machine service boundary end", (15.24, .017, z), (1.25, .008, .07), m["yellow"], 0)
    # One routed cable tray links supply at entry to the pressure-control bay.
    for x in (14.8, 15.25):
        rbox("Tray side rail", (x, 3.42, 9), (.035, .12, 17.7), m["steel"])
    for z in [i + .5 for i in range(18)]:
        rbox("Tray rung", (15.025, 3.37, z), (.45, .025, .04), m["steel"], 0)
    for z in (1, 7, 13, 17):
        rbox("Tray ceiling support", (15.025, 3.64, z), (.58, .025, .055), m["steel"])
        for x in (14.8, 15.25):
            rbox("Tray suspension", (x, 3.56, z), (.018, .28, .018), m["steel"], 0)
    for x in (14.94, 15.02, 15.10):
        rod("Contained service cable", (x, -.15, 3.40), (x, -17.85, 3.40), .022, m["rubber"], 12)


for name, build in (("pressure_manifold", manifold), ("transfer_cover_casing", casing),
                    ("pressure_control_casing", control), ("transfer_room_lining", room)):
    if SELECTED and name not in SELECTED:
        continue
    materials = setup()
    build(materials)
    export(name)
