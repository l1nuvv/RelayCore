"""Blender --background --factory-startup --disable-autoexec --python this_file.

Room lining is local to [24, 0, 26]; other floor origins are centered on equipment.
Run with -- --model=<name> to regenerate only one product.
"""
from pathlib import Path
from math import sin, cos, pi
from random import Random
import sys
import json
import struct

import bpy
import bmesh
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "content/models"
GRAPHICS = ROOT / "content/graphics/pressure_transfer"
GRAPHICS.mkdir(parents=True, exist_ok=True)
SELECTED = {arg.split("=", 1)[1] for arg in sys.argv if arg.startswith("--model=")}


def material(name, color, roughness, metallic=0, sheet=None, cell=0, texture=None, variation=1):
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
        modulation = 1 + (modulation - 1) * variation
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


def surface_normal(mat, metres, strength):
    """Periodic manufactured grain; RGB is linear glTF +Y tangent space."""
    edge = 256
    height = np.random.default_rng(20261008).normal(size=(edge, edge))
    height = (height + np.roll(height, 1, 0) + np.roll(height, -1, 0)
              + np.roll(height, 1, 1) + np.roll(height, -1, 1)) / 5
    dx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * strength
    dy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * strength
    normal = np.stack((-dx, -dy, np.ones_like(height)), axis=2)
    normal /= np.linalg.norm(normal, axis=2)[:, :, None]
    pixels = np.ones((edge, edge, 4), dtype=np.float32)
    pixels[:, :, :3] = normal * .5 + .5
    image = bpy.data.images.new(mat.name + " normal", width=edge, height=edge)
    image.colorspace_settings.name = "Non-Color"
    image.pixels.foreach_set(pixels.ravel())
    image.filepath_raw = str(GRAPHICS / (mat.name + "_normal.png"))
    image.file_format = "PNG"
    image.save()
    image.pack()
    nodes = mat.node_tree.nodes
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = image
    mapped = nodes.new("ShaderNodeNormalMap")
    mat.node_tree.links.new(texture.outputs["Color"], mapped.inputs["Color"])
    mat.node_tree.links.new(mapped.outputs["Normal"], nodes.get("Principled BSDF").inputs["Normal"])
    mat["uv_metres"] = metres
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


def wear_patch(name, center, u, v, mat, seed):
    """Opaque fitted surface mark; u cross v points away from its receiver."""
    rng = Random(seed)
    center, u, v = Vector(center), Vector(u), Vector(v)
    points = [center]
    for i in range(16):
        angle = 2*pi*i/16
        radius = rng.uniform(.65, 1)
        points.append(center + radius*(u*cos(angle) + v*sin(angle)))
    # Runtime +Y up -> Blender +Z up, as in bay_box.
    return mesh_object(name, [(p.x, -p.z, p.y) for p in points],
                       [(0, i+1, (i+1) % 16+1) for i in range(16)], mat)


def surface_crack(name, points, normal, width, mat):
    points, normal = [Vector(p) for p in points], Vector(normal)
    vertices = []
    for i, point in enumerate(points):
        tangent = points[min(i+1, len(points)-1)]-points[max(i-1, 0)]
        side = tangent.cross(normal).normalized()*width/2
        vertices.extend((point-side, point+side))
    return mesh_object(name, [(p.x, -p.z, p.y) for p in vertices],
                       [(2*i, 2*i+1, 2*i+3, 2*i+2) for i in range(len(points)-1)], mat)


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


def repair_exported_tangents(path):
    # MikkTSpace can give zero corner weights on metre-long, millimetre-wide
    # bevel triangles. Recover their basis from the exported geometry and UVs
    # in double precision, without changing geometry or the metre UV scale.
    data = bytearray(path.read_bytes())
    json_size = struct.unpack_from("<I", data, 12)[0]
    document = json.loads(data[20:20 + json_size])
    binary_start = 28 + json_size

    def accessor(index):
        entry = document["accessors"][index]
        view = document["bufferViews"][entry["bufferView"]]
        dtype = {5126: "<f4", 5125: "<u4", 5123: "<u2"}[entry["componentType"]]
        width = {"VEC4": 4, "VEC3": 3, "VEC2": 2, "SCALAR": 1}[entry["type"]]
        offset = binary_start + view.get("byteOffset", 0) + entry.get("byteOffset", 0)
        stride = view.get("byteStride", np.dtype(dtype).itemsize * width)
        return np.ndarray((entry["count"], width), dtype=dtype, buffer=data,
                          offset=offset, strides=(stride, np.dtype(dtype).itemsize))

    repaired = 0
    for mesh in document["meshes"]:
        for primitive in mesh["primitives"]:
            attributes = primitive["attributes"]
            if "TANGENT" not in attributes:
                continue
            tangents = accessor(attributes["TANGENT"])
            normals = accessor(attributes["NORMAL"]).astype(np.float64)
            normals /= np.linalg.norm(normals, axis=1)[:, None]
            invalid = ((np.abs(np.linalg.norm(tangents[:, :3], axis=1) - 1) >= .01)
                       | (np.abs(np.sum(normals * tangents[:, :3], axis=1)) >= .01))
            positions = accessor(attributes["POSITION"]).astype(np.float64)
            uv = accessor(attributes["TEXCOORD_0"]).astype(np.float64)
            for triangle in accessor(primitive["indices"]).reshape(-1, 3):
                if not invalid[triangle].any():
                    continue
                a, b, c = triangle
                edge1, edge2 = positions[b] - positions[a], positions[c] - positions[a]
                delta1, delta2 = uv[b] - uv[a], uv[c] - uv[a]
                determinant = delta1[0] * delta2[1] - delta1[1] * delta2[0]
                if abs(determinant) < 1e-15:
                    raise ValueError(f"{path}: degenerate UV triangle cannot supply a tangent")
                tangent = (edge1 * delta2[1] - edge2 * delta1[1]) / determinant
                # Blender flips exported V while retaining its +Y normal image
                # convention and bitangent sign. Match its unaffected corners.
                bitangent = -(edge2 * delta1[0] - edge1 * delta2[0]) / determinant
                for index in triangle:
                    if not invalid[index]:
                        continue
                    normal = normals[index]
                    direction = tangent - normal * np.dot(normal, tangent)
                    length = np.linalg.norm(direction)
                    if length < 1e-12:
                        raise ValueError(f"{path}: degenerate surface cannot supply a tangent")
                    direction /= length
                    sign = -1 if np.dot(np.cross(normal, direction), bitangent) < 0 else 1
                    tangents[index] = (*direction, sign)
                    invalid[index] = False
                    repaired += 1
            if invalid.any():
                raise ValueError(f"{path}: unreferenced invalid tangent")
    if repaired:
        path.write_bytes(data)
        print(f"Recovered {repaired} degenerate exported tangents: {path.name}")


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
                metres = mat.get("uv_metres", 1.5 if mat.name == "transfer_quiet_concrete" else 2)
                uv.data[index].uv = (point[axes[0]] / metres, point[axes[1]] / metres)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / (name + ".blend")))
    bpy.ops.export_scene.gltf(filepath=str(OUT / (name + ".glb")), export_format="GLB",
                              export_yup=True, export_normals=True, export_texcoords=True,
                              export_tangents=any("uv_metres" in mat for mat in groups),
                              export_animations=False, export_skins=False,
                              export_cameras=False, export_lights=False, export_extras=False)
    if any("uv_metres" in mat for mat in groups):
        repair_exported_tangents(OUT / (name + ".glb"))


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
    for x, side in ((-.9, -1), (.9, 1)):
        sweep("Differential impulse takeoff", [(x, -.42, 2.24), (x, -.58, 2.24),
                                               (side*.34, -.58, 2.24),
                                               (side*.34, -.30, 2.24),
                                               (side*.20, -.15, 2.31)], .014, m["brass"])
        rod("Impulse union", (side*.20, -.15, 2.30), (side*.20, -.15, 2.36), .025, m["steel"], 12)
    sweep("Brass differential loop", [(-.20, -.15, 2.31), (-.20, -.15, 2.10),
                                      (.20, -.15, 2.10), (.20, -.15, 2.31)], .017, m["brass"])
    label("DIFFERENTIAL / P1-P2", (0, -.118, 2.78), .048, m["ivory"])
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
    ceiling = material("bay_ceiling_ivory", (.48, .47, .42), .94)
    rbox("Plain bypass ceiling", (8, 3.787, 9), (15.78, .018, 17.78), ceiling, 0)
    for x in (.11, 15.89):
        rbox("Concrete wall field", (x, 2.42, 9), (.016, 2.70, 17.78), m["concrete"], 0)
        rbox("Painted service dado", (x, .55, 9), (.025, 1.06, 17.78), m["paint"], 0)
        rbox("Dado coping", (x, 1.10, 9), (.045, .035, 17.78), m["steel"])
        for z in (3.8, 9.8, 15.8):
            rbox("Wall pilaster", (x, 2, z), (.17, 3.6, .30), m["concrete"])
    for z in (3.8, 9.8, 15.8):
        rbox("Shallow ceiling rib", (8, 3.69, z), (15.78, .18, .32), m["paint"])
    for center, width in ((1.05, 1.88), (11, 9.78)):
        rbox("Exit wall concrete field", (center, 2.42, 17.89), (width, 2.70, .02), m["concrete"], 0)
        rbox("Exit wall service dado", (center, .55, 17.89), (width, 1.06, .025), m["paint"], 0)
        rbox("Exit wall coping", (center, 1.10, 17.89), (width, .035, .045), m["steel"], 0)
    # Complete the entry return and bridge the floor/ceiling skins at the opening.
    service_wall("Bypass entry wing", (2.055, .114), 3.89, "z", m)
    service_wall("Bypass entry return", (11.945, .114), 7.89, "z", m)
    for x in (3.96, 7.96):
        bay_box("Bypass gate reveal", (x, 1.56, 0), (.07, 3.12, .28), m["steel"])
    bay_box("Bypass gate return lintel", (6, 3.46, .125), (3.98, .68, .045), m["paint"])
    bay_box("Bypass gate lintel reveal", (6, 3.12, 0), (3.92, .045, .28), m["steel"], 0)
    bay_box("Gallery return plate", (6, 3.43, .158), (3.35, .32, .025), m["ivory"])
    bay_label("02 / ISOLATION GALLERY", (6, 3.37, .177), .18, m["dark"], "+z")
    bay_box("Bypass gate threshold", (6, .009, 0), (3.92, .018, .22), m["steel"], 0)
    bay_box("Bypass gate soffit", (6, 3.787, 0), (3.92, .018, .22), ceiling, 0)
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


def bay_box(name, pos, size, mat, bevel=.008):
    x, y, z = pos
    w, h, d = size
    return box(name, (x, -z, y), (w, d, h), mat, bevel)


def bay_label(text, pos, size, mat, facing):
    x, y, z = pos
    obj = label(text, (x, -z, y), size, mat)
    obj.rotation_euler.z = {"+x": pi / 2, "-x": -pi / 2, "+z": 0, "-z": pi}[facing]
    return obj


def equipment_bay(m):
    floor = material("bay_bluegray_floor", (.27, .30, .31), .91,
                     sheet="assets/textures/facility_source.png")
    ceiling = material("bay_ceiling_ivory", (.48, .47, .42), .94)
    bay_box("Coated preparation floor", (4, .007, 5), (7.78, .012, 9.78), floor, 0)
    bay_box("Plain ceiling field", (4, 3.787, 5), (7.78, .018, 9.78), ceiling, 0)
    for x in (.114, 7.886):
        bay_box("Preparation concrete field", (x, 2.42, 5), (.022, 2.70, 9.78), m["concrete"], 0)
        bay_box("Enamel protective dado", (x, .55, 5), (.030, 1.06, 9.78), m["paint"], 0)
        bay_box("Dado folded cap", (x, 1.10, 5), (.048, .035, 9.78), m["steel"])
        bay_box("Service skirting", (x, .065, 5), (.05, .13, 9.78), m["dark"])
        for z in (2.5, 5, 7.5):
            bay_box("Concrete construction joint", (x, 2.42, z), (.028, 2.70, .012), m["dark"], 0)
    for z, segments in ((.114, ((4, 7.78),)), (9.886, ((1.30, 2.38), (6.70, 2.38)))):
        for x, width in segments:
            bay_box("End wall concrete", (x, 2.42, z), (width, 2.70, .022), m["concrete"], 0)
            bay_box("End wall protective dado", (x, .55, z), (width, 1.06, .030), m["paint"], 0)
            bay_box("End wall dado cap", (x, 1.10, z), (width, .035, .048), m["steel"])
            bay_box("End wall skirting", (x, .065, z), (width, .13, .05), m["dark"])
    for z in (2.5, 7.5):
        bay_box("Ceiling bearing rib", (4, 3.72, z), (7.78, .14, .18), m["paint"])
    # The jambs sit over the solid edges, outside the existing 3 m clear opening.
    for x in (2.464, 5.536):
        bay_box("Gate folded jamb", (x, 1.56, 9.85), (.07, 3.12, .10), m["steel"])
        bay_box("Jamb impact shoe", (x, .26, 9.79), (.09, .52, .04), m["dark"])
    bay_box("Gate lintel cladding", (4, 3.46, 9.875), (3.14, .68, .045), m["paint"])
    bay_box("Gate destination plate", (4, 3.43, 9.842), (2.68, .32, .025), m["ivory"])
    bay_label("01 / SERVICE HALL", (4, 3.37, 9.823), .16, m["dark"], "-z")
    bay_box("Preparation nameplate", (.148, 3.10, 4), (.035, .32, 2.55), m["ivory"])
    bay_label("EQUIPMENT PREPARATION", (.170, 3.05, 4), .13, m["dark"], "+x")
    # Routed high-level wiring connects the fixture feed to the gate reader.
    for z in (1, 5, 9.4):
        bay_box("Conduit wall saddle", (7.84, 3.36, z), (.10, .11, .055), m["steel"])
    rod("Gate reader feed", (7.78, -.14, 3.36), (7.78, -9.80, 3.36), .025, m["steel"], 12)
    rod("Reader lintel feed", (7.78, -9.80, 3.36), (6.1, -9.80, 3.36), .025, m["steel"], 12)
    rod("Reader cable drop", (6.1, -9.80, 3.36), (6.1, -9.80, 1.66), .025, m["steel"], 12)
    # Module origins are 3 m apart; join the recessed flanges at their seam.
    rod("Bay pipe coupling", (7.66, -5.375, 2.85), (7.66, -5.625, 2.85), .107, m["steel"], 32)
    for end, direction in ((2.5, -1), (8.5, 1)):
        points = [(7.66 + .23*(1-cos(i*pi/16)),
                   -(end + direction*.23*sin(i*pi/16)), 2.85) for i in range(9)]
        sweep("Bay pipe wall elbow", points, .085, m["steel"], 24)
        bay_box("Bay pipe wall sleeve", (7.886, 2.85, end+direction*.23), (.04, .25, .25), m["dark"])


def bay_furniture(m):
    # Closed cabinet skins wrap the original bench/cupboard collision solids.
    worktop = material("bay_worktop_laminate", (.35, .37, .34), .86,
                       sheet="content/graphics/prop_materials_source.png", cell=1)
    bay_box("Preparation worktop", (2, 1.031, 4), (1.94, .016, .94), worktop, .004)
    bay_box("Worktop folded edge", (2, 1.006, 3.532), (1.94, .04, .025), m["steel"], .004)
    for x in (1.087, 2.913):
        bay_box("Bench enamel cheek", (x, .51, 4), (.025, 1, .84), m["paint"])
    bay_box("Bench rear skin", (2, .51, 4.414), (1.8, 1, .028), m["paint"])
    bay_box("Bench plinth", (2, .07, 3.585), (1.8, .14, .03), m["dark"])
    bay_box("Bench door reveal", (2, .56, 3.585), (1.8, .84, .03), m["dark"])
    for x in (1.55, 2.45):
        bay_box("Upper instrument drawer", (x, .85, 3.563), (.865, .22, .028), m["paint"])
        bay_box("Lower storage door", (x, .44, 3.563), (.865, .56, .028), m["paint"])
        for y in (.84, .66):
            for anchor in (x - .12, x + .12):
                bay_box("Handle folded mount", (anchor, y, 3.532), (.035, .026, .05), m["steel"], .004)
            bay_box("Drawer channel grip", (x, y, 3.501), (.27, .035, .025), m["steel"], .005)
        for y in (.25, .61):
            bay_box("Storage hinge", (x - .38, y, 3.538), (.035, .085, .028), m["steel"], .004)
    bay_box("Bench identification", (2, .985, 3.538), (.62, .055, .012), m["ivory"], .002)
    bay_label("PREPARATION", (2, .972, 3.530), .032, m["dark"], "-z")
    # Cupboard: 1.5 m wide, 0.3 m deep, 2.2 m high; two hinged shallow doors.
    bay_box("Cupboard upper folded edge", (.26, 2.213, 7), (.32, .026, 1.54), m["paint"])
    for z in (6.238, 7.762):
        bay_box("Cupboard return cheek", (.26, 1.1, z), (.32, 2.2, .026), m["paint"])
    bay_box("Cupboard dark reveal", (.413, 1.1, 7), (.026, 2.2, 1.5), m["dark"])
    for z, side, title in ((6.625, -1, "SERVICE"), (7.375, 1, "TOOLS")):
        bay_box("Service tools door", (.435, 1.15, z), (.025, 2.03, .714), m["paint"])
        for y in (.36, 1.92):
            bay_box("Cupboard hinge", (.455, y, z + side * .31), (.045, .14, .04), m["steel"], .004)
        for y in (1.02, 1.18):
            bay_box("Cupboard handle mount", (.467, y, z - side * .23), (.05, .025, .025), m["steel"], .004)
        bay_box("Cupboard pull", (.497, 1.1, z - side * .23), (.025, .20, .032), m["steel"], .005)
        for y in (.24, .30, .36, .42):
            bay_box("Cupboard ventilation slot", (.451, y, z), (.009, .016, .48), m["dark"], .002)
        bay_box("Service tools plate", (.456, 1.82, z), (.014, .20, .62), m["ivory"], .003)
        bay_label(title, (.468, 1.78, z), .084, m["dark"], "+x")
    bay_box("Cupboard toe protection", (.432, .06, 7), (.045, .12, 1.5), m["dark"])


def service_wall(name, center, length, axis, m):
    x, z = center
    for part, height, tall, thickness, mat in (
        ("concrete", 2.42, 2.70, .022, m["concrete"]),
        ("dado", .55, 1.06, .030, m["paint"]),
        ("coping", 1.10, .035, .048, m["steel"]),
        ("skirting", .065, .13, .050, m["dark"]),
    ):
        size = (thickness, tall, length) if axis == "x" else (length, tall, thickness)
        bay_box(name + " / " + part, (x, height, z), size, mat, 0)


def exit_corridor(m):
    ceiling = material("bay_ceiling_ivory", (.48, .47, .42), .94)
    bay_box("Exit coated floor", (28, .007, 48), (3.78, .012, 7.78), m["floor"], 0)
    bay_box("Exit ceiling", (28, 3.787, 48), (3.78, .018, 7.78), ceiling, 0)
    for x in (26.114, 29.886):
        service_wall("Exit service wall", (x, 48), 7.78, "x", m)
    service_wall("Technical sector end wall", (28, 51.886), 3.78, "z", m)
    for z in (46, 50):
        bay_box("Exit ceiling rib", (28, 3.72, z), (3.78, .14, .18), m["paint"])
    # Sleeve the existing opening, with plates on both sides of the raised door.
    for x in (26.04, 29.96):
        bay_box("Exit reveal", (x, 1.56, 44), (.08, 3.12, .28), m["steel"])
        for z in (43.84, 44.16):
            bay_box("Exit frame flange", (x, 1.56, z), (.14, 3.12, .055), m["steel"])
        bay_box("Exit impact shoe", (x, .25, 43.80), (.16, .50, .035), m["dark"])
    for z, facing, title in ((43.86, "-z", "EXIT / TECHNICAL SECTOR"),
                              (44.14, "+z", "03 / PRESSURE BYPASS")):
        bay_box("Exit lintel cladding", (28, 3.46, z), (3.98, .68, .045), m["paint"])
        bay_box("Exit destination plate", (28, 3.43, z + (-.04 if facing == "-z" else .04)),
                (3.55, .32, .025), m["ivory"])
        bay_label(title, (28, 3.37, z + (-.06 if facing == "-z" else .06)), .17, m["dark"], facing)
    bay_box("Exit threshold", (28, .009, 44), (3.8, .018, .22), m["steel"], 0)
    bay_box("Exit lintel reveal", (28, 3.12, 44), (3.8, .045, .28), m["steel"], 0)
    bay_box("Sector destination plate", (28, 2.55, 51.85), (3.2, .52, .04), m["paint"])
    bay_label("TECHNICAL SECTOR", (28, 2.46, 51.822), .23, m["ivory"], "-z")
    bay_box("Exit reader identity plate", (31.15, 1.97, 43.84), (1.45, .25, .04), m["ivory"])
    bay_label("EXIT RELEASE", (31.15, 1.91, 43.812), .12, m["dark"], "-z")
    rod("Exit reader takeoff", (31, -43.79, 1.70), (32.05, -43.79, 1.70), .025, m["steel"], 12)
    rod("Exit reader supply", (32.05, -43.79, 1.70), (32.05, -43.79, 3.22), .025, m["steel"], 12)
    rod("Exit supply lintel run", (32.05, -43.79, 3.22), (30.15, -43.79, 3.22), .025, m["steel"], 12)
    for y in (2.1, 2.8, 3.22):
        bay_box("Exit conduit saddle", (32.05, y, 43.85), (.08, .06, .13), m["steel"])


def service_hall(m):
    floor = material("bay_bluegray_floor", (.27, .30, .31), .91,
                     sheet="assets/textures/facility_source.png")
    ceiling = material("bay_ceiling_ivory", (.48, .47, .42), .94)
    bay_box("Service hall coated floor", (4, .007, 17), (7.78, .012, 13.78), floor, 0)
    bay_box("Service hall ceiling", (4, 3.787, 17), (7.78, .018, 13.78), ceiling, 0)
    service_wall("West service wall", (.114, 17), 13.78, "x", m)
    service_wall("East service wall", (7.886, 15), 9.78, "x", m)
    service_wall("Hall turning wall", (4, 23.886), 7.78, "z", m)
    for x in (1.3, 6.7):
        service_wall("Gate return wall", (x, 10.114), 2.38, "z", m)
    for z in (13, 18):
        bay_box("Service ceiling rib", (4, 3.72, z), (7.78, .14, .18), m["paint"])
        bay_box("Service bearing strip", (.16, 2.42, z), (.12, 2.70, .24), m["concrete"])
    # Return faces finish the existing door without reducing its clear opening.
    for x in (2.464, 5.536):
        bay_box("Service gate return jamb", (x, 1.56, 10.15), (.07, 3.12, .10), m["steel"])
    bay_box("Service gate return lintel", (4, 3.46, 10.125), (3.14, .68, .045), m["paint"])
    bay_box("Preparation return plate", (4, 3.43, 10.158), (2.68, .32, .025), m["ivory"])
    bay_label("EQUIPMENT BAY", (4, 3.37, 10.177), .16, m["dark"], "+z")
    bay_box("Gate threshold", (4, .009, 10), (2.98, .018, .22), m["steel"], 0)
    # Gallery opening is z=20.1..23.9; the frame follows the solid corners.
    for z in (20.045, 23.955):
        bay_box("Gallery portal reveal sleeve", (8, 1.90, z), (.34, 3.78, .09), m["steel"])
        for x in (7.85, 8.15):
            bay_box("Gallery portal upright", (x, 1.90, z), (.12, 3.78, .09), m["steel"])
        bay_box("Portal impact shoe", (7.78, .28, z), (.025, .56, .12), m["dark"])
    bay_box("Gallery portal head", (8, 3.70, 22), (.42, .18, 3.82), m["steel"])
    bay_box("Gallery threshold", (8, .009, 22), (.22, .018, 3.8), m["steel"], 0)
    bay_box("Hall direction plate", (4.7, 2.65, 23.855), (3.8, .43, .04), m["ivory"])
    bay_label("<  02 / ISOLATION GALLERY", (4.7, 2.57, 23.83), .18, m["dark"], "-z")
    # Reused west-wall pipe modules terminate in sleeved wall returns.
    for seam in (14.5, 17.5):
        rod("Hall inter-module coupling", (.34, -(seam-.125), 2.85), (.34, -(seam+.125), 2.85), .107, m["steel"], 32)
    for end, direction in ((11.5, -1), (20.5, 1)):
        points = [(.34 - .23 * (1 - cos(i * pi / 16)),
                   -(end + direction * .23 * sin(i * pi / 16)), 2.85) for i in range(9)]
        sweep("Pipe swept wall return", points, .085, m["steel"], 24)
        bay_box("Pipe wall sleeve", (.114, 2.85, end + direction * .23), (.035, .24, .24), m["dark"])
    rod("Hall conduit", (7.78, -9.78, 3.36), (7.78, -18.3, 3.36), .025, m["steel"], 12)
    rod("Gallery bulkhead feed", (7.78, -18.3, 3.36), (8.11, -18.3, 3.36), .025, m["steel"], 12)
    for z in (11, 14.5, 18.3):
        bay_box("Hall conduit saddle", (7.84, 3.36, z), (.10, .11, .055), m["steel"])


def gallery(m):
    ceiling = material("bay_ceiling_ivory", (.48, .47, .42), .94)
    bay_box("Isolation gallery coated floor", (20, .007, 22), (23.78, .012, 7.78), m["floor"], 0)
    bay_box("Isolation gallery ceiling", (20, 3.787, 22), (23.78, .018, 7.78), ceiling, 0)
    service_wall("Gallery supply wall", (20, 18.114), 23.78, "z", m)
    service_wall("Gallery return wall", (18, 25.886), 19.78, "z", m)
    service_wall("Gallery end wall", (31.886, 22), 7.78, "x", m)
    service_wall("Gallery entry wing return", (8.114, 19.055), 1.89, "x", m)
    service_wall("Gallery portal return", (8.114, 24.945), 1.89, "x", m)
    for x in (11, 19, 26):
        bay_box("Gallery ceiling rib", (x, 3.72, 22), (.18, .14, 7.78), m["paint"])
        for z in (18.16, 25.84):
            bay_box("Gallery bearing strip", (x, 2.42, z), (.24, 2.70, .12), m["concrete"])
    for x in (27.96, 31.96):
        bay_box("Room 03 gate jamb", (x, 1.56, 25.85), (.07, 3.12, .10), m["steel"])
    bay_box("Room 03 lintel cladding", (30, 3.46, 25.875), (3.98, .68, .045), m["paint"])
    bay_box("Room 03 destination plate", (30, 3.43, 25.842), (3.35, .32, .025), m["ivory"])
    bay_label("03 / PRESSURE BYPASS", (30, 3.37, 25.823), .18, m["dark"], "-z")
    bay_box("Turn landmark plate", (31.85, 2.92, 23.9), (.04, .42, 3.1), m["ivory"])
    bay_label("03 / BYPASS  >", (31.825, 2.84, 23.9), .20, m["dark"], "-x")
    bay_box("Gallery portal identity", (8.15, 2.65, 25), (.04, .38, 1.25), m["ivory"])
    bay_label("02", (8.176, 2.56, 25), .24, m["dark"], "+x")
    rod("Gallery supply conduit", (8.11, -18.3, 3.36), (31.78, -18.3, 3.36), .025, m["steel"], 12)
    for x in (9, 14, 19, 24, 29, 31.7):
        bay_box("Gallery conduit saddle", (x, 3.36, 18.20), (.055, .11, .20), m["steel"])
    rod("Gallery reader drop", (20, -18.24, 3.36), (20, -18.24, 1.67), .025, m["steel"], 12)
    bay_box("Reader junction", (20, 3.36, 18.17), (.14, .14, .18), m["paint"])
    bay_box("Reader identity plate", (21, 1.92, 18.15), (1.55, .22, .06), m["ivory"])
    bay_label("03 / GATE RELEASE", (21, 1.87, 18.185), .11, m["dark"], "+z")
    for end, direction in ((10.5, -1), (19.5, 1)):
        points = [(end + direction * .23 * sin(i * pi / 16),
                   -(18.34 - .23 * (1 - cos(i * pi / 16))), 2.85) for i in range(9)]
        sweep("Gallery pipe wall return", points, .085, m["steel"], 24)
        bay_box("Gallery pipe wall sleeve", (end + direction * .23, 2.85, 18.114), (.24, .24, .035), m["dark"])
    for seam in (13.5, 16.5):
        rod("Gallery inter-module coupling", (seam-.125, -18.34, 2.85), (seam+.125, -18.34, 2.85), .107, m["steel"], 32)
    # Skin the original 1.2 x 2.4 x .4 turret backing; keep its LOS envelope.
    for x in (29.387, 30.613):
        bay_box("Turret backing folded cheek", (x, 1.2, 22.5744), (.026, 2.4, .42), m["paint"])
    for z in (22.3614, 22.7874):
        bay_box("Turret backing face", (30, 1.2, z), (1.20, 2.4, .026), m["paint"])
    bay_box("Turret backing cap", (30, 2.415, 22.5744), (1.24, .030, .44), m["steel"])
    bay_box("Turret mounting plate", (30, 1.55, 22.342), (1.04, 1.16, .025), m["dark"])
    for x in (29.55, 30.45):
        for y in (1.04, 2.06):
            rod("Mounting anchor", (x, -22.33, y), (x, -22.30, y), .023, m["steel"], 8)
    bay_box("Turret service plinth", (30, .065, 22.5744), (1.24, .13, .44), m["dark"])
    bay_box("Turret identification plate", (30, 2.24, 22.341), (.76, .19, .025), m["ivory"])
    bay_label("G-02 / SECURITY", (30, 2.20, 22.322), .072, m["dark"], "-z")
    rod("Turret cable riser", (30, -22.81, 2.4), (30, -22.81, 3.60), .025, m["steel"], 12)
    rod("Turret overhead feed", (30, -18.3, 3.60), (30, -22.81, 3.60), .025, m["steel"], 12)
    rod("Turret supply takeoff", (30, -18.3, 3.36), (30, -18.3, 3.60), .025, m["steel"], 12)
    for z in (19, 22.81):
        bay_box("Turret feed ceiling saddle", (30, 3.69, z), (.08, .20, .06), m["steel"])


if __name__ == "__main__":
    for name, build in (("pressure_manifold", manifold), ("transfer_cover_casing", casing),
                      ("pressure_control_casing", control), ("transfer_room_lining", room),
                      ("equipment_bay_lining", equipment_bay), ("equipment_bay_furniture", bay_furniture),
                      ("service_hall_lining", service_hall), ("isolation_gallery_lining", gallery),
                      ("transfer_exit_lining", exit_corridor)):
        if SELECTED and name not in SELECTED:
            continue
        materials = setup()
        build(materials)
        export(name)
