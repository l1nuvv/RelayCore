"""Adapt GAME-READY STUDIOS' free Biomechanical Mutant to the C-19 restraint.

See content/third_party/game_ready_mutant/source.md. The source rig is used only
offline to lower the arms. Runtime receives opaque static pivot assemblies.
"""
from pathlib import Path
from math import pi
import importlib.util
import json
import struct

import bpy
import bmesh
import numpy as np
from mathutils import Vector, Matrix

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "content/third_party/game_ready_mutant"
OUT = ROOT / "content/models"
GRAPHICS = ROOT / "content/graphics/guardian"
GRAPHICS.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location("containment", ROOT / "scripts/export-containment-glb.py")
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def skin(part):
    mat = bpy.data.materials.new("Guardian " + part + " exposed anatomy")
    mat.use_nodes = True
    mat.use_backface_culling = True
    shader = mat.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Roughness"].default_value = .64
    shader.inputs["Metallic"].default_value = .12
    for suffix in ("a", "n"):
        image = bpy.data.images.load(str(SOURCE / f"T_Biomech_Mutant_Skin_1_{part}_Bloody_{suffix}.png"))
        image.colorspace_settings.name = "sRGB" if suffix == "a" else "Non-Color"
        image.scale(2048, 2048)
        if suffix == "n":
            pixels = np.asarray(image.pixels[:], dtype=np.float32).reshape(-1, 4)
            normals = pixels[:, :3]*2-1
            repaired = int(np.count_nonzero(normals[:, 2] <= .01))
            # The imported map has near-tangent and back-facing seam texels.
            # Native opaque materials require a positive tangent hemisphere.
            normals[:, 2] = np.maximum(normals[:, 2], .01)
            normals /= np.linalg.norm(normals, axis=1)[:, None]
            pixels[:, :3] = normals*.5+.5
            pixels[:, 3] = 1
            image.pixels.foreach_set(pixels.ravel())
            print(f"Normal hemisphere adaptation {part}: {repaired} texels")
        image.filepath_raw = str(GRAPHICS / f"guardian_{part.lower()}_{suffix}.png")
        image.file_format = "PNG"
        image.save()
        image.pack()
        texture = mat.node_tree.nodes.new("ShaderNodeTexImage")
        texture.image = image
        if suffix == "a":
            mat.node_tree.links.new(texture.outputs["Color"], shader.inputs["Base Color"])
        else:
            normal = mat.node_tree.nodes.new("ShaderNodeNormalMap")
            normal.inputs["Strength"].default_value = .85
            mat.node_tree.links.new(texture.outputs["Color"], normal.inputs["Color"])
            mat.node_tree.links.new(normal.outputs["Normal"], shader.inputs["Normal"])
    return mat


def export(objects, name):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    # Preserve the artist's UVs on the organic mesh; only fittings need projection.
    for obj in objects:
        if obj.type != "MESH":
            continue
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.triangulate(bm, faces=list(bm.faces))
        tiny = [f for f in bm.faces if (f.verts[1].co-f.verts[0].co).cross(
                f.verts[2].co-f.verts[0].co).length <= 1.01e-7]
        bmesh.ops.delete(bm, geom=tiny, context="FACES")
        bm.to_mesh(obj.data)
        bm.free()
        obj.data.update()
        if obj.data.uv_layers.active is None:
            uv = obj.data.uv_layers.new(name="UVMap")
            for face in obj.data.polygons:
                points = [obj.matrix_world @ obj.data.vertices[i].co for i in face.vertices]
                normal = (points[1]-points[0]).cross(points[2]-points[0])
                axis = max(range(3), key=lambda a: abs(normal[a]))
                axes = ((1, 2), (0, 2), (0, 1))[axis]
                metres = obj.data.materials[0].get("uv_metres", 1)
                for i, point in zip(face.loop_indices, points):
                    uv.data[i].uv = (point[axes[0]] / metres, point[axes[1]] / metres)
    # Save only the selected assembly as an editable source scene.
    keep = set(objects)
    others = [o for o in bpy.context.scene.objects if o not in keep]
    for obj in others:
        bpy.context.scene.collection.objects.unlink(obj)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / f"{name}.blend"))
    bpy.ops.export_scene.gltf(filepath=str(OUT / f"{name}.glb"), export_format="GLB",
                             use_selection=True, export_yup=True, export_normals=True,
                             export_texcoords=True, export_tangents=True,
                             export_animations=False, export_skins=False, export_extras=False,
                             export_cameras=False, export_lights=False)
    path = OUT / f"{name}.glb"
    data = path.read_bytes()
    json_size = struct.unpack_from("<I", data, 12)[0]
    document = json.loads(data[20:20+json_size])
    # Solid cast panels have no tangent-space map. Omit their unused MikkTSpace
    # basis where smooth normals across rolled UV edges reverse handedness.
    for exported_mesh in document["meshes"]:
        for primitive in exported_mesh["primitives"]:
            material = document["materials"][primitive["material"]]
            if "normalTexture" not in material:
                primitive["attributes"].pop("TANGENT", None)
    encoded = json.dumps(document, separators=(",", ":")).encode("utf-8")
    encoded += b" "*((-len(encoded)) % 4)
    chunks = struct.pack("<II", len(encoded), 0x4E4F534A)+encoded+data[20+json_size:]
    path.write_bytes(struct.pack("<III", 0x46546C67, 2, 12+len(chunks))+chunks)
    c.h.repair_exported_tangents(OUT / f"{name}.glb")
    for obj in others:
        bpy.context.scene.collection.objects.link(obj)


def armor_panel(source, name, region, surface):
    """Fitted shell retains the source silhouette with an actual rolled edge."""
    data = source.copy()
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.delete(bm, geom=[f for f in bm.faces if not region(f.calc_center_median())], context="FACES")
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
    bm.normal_update()
    for v in bm.verts:
        v.co += v.normal*.022
    bm.to_mesh(data)
    bm.free()
    data.materials.clear()
    data.materials.append(surface)
    for face in data.polygons:
        face.material_index = 0
        face.use_smooth = True
    obj = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    solid = obj.modifiers.new("Cast shell thickness", "SOLIDIFY")
    solid.thickness = .025
    solid.offset = 0
    bpy.ops.object.modifier_apply(modifier=solid.name)
    bevel = obj.modifiers.new("Rolled shell edge", "BEVEL")
    bevel.width = .006
    bevel.segments = 2
    bevel.limit_method = "ANGLE"
    bpy.ops.object.modifier_apply(modifier=bevel.name)
    # Rolled edges need their own UVs; inherited organic UVs collapse there.
    while obj.data.uv_layers:
        obj.data.uv_layers.remove(obj.data.uv_layers[0])
    return obj


def build():
    m = c.materials()
    bpy.ops.import_scene.fbx(filepath=str(SOURCE / "SK_Bio_Mutant.fbx"))
    body = next(o for o in bpy.context.scene.objects if o.type == "MESH")
    rig = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
    for side, angle in (("l", -pi/5), ("r", pi/5)):
        bone = rig.pose.bones["upperarm_"+side]
        bone.matrix = Matrix.Translation(bone.head) @ Matrix.Rotation(angle, 4, "Z") \
                      @ Matrix.Translation(-bone.head) @ bone.matrix
        bpy.context.view_layer.update()
    dependency = bpy.context.evaluated_depsgraph_get()
    evaluated = body.evaluated_get(dependency)
    mesh = bpy.data.meshes.new_from_object(evaluated, depsgraph=dependency)
    # FBX importer has already resolved centimetres and source orientation.
    mesh.transform(body.matrix_world)
    bpy.data.objects.remove(body, do_unlink=True)
    rig.animation_data_clear()
    bpy.data.objects.remove(rig, do_unlink=True)
    body = bpy.data.objects.new("Guardian adapted anatomy", mesh)
    bpy.context.scene.collection.objects.link(body)
    min_z = min(v.co.z for v in mesh.vertices)
    scale = 3.55 / (max(v.co.z for v in mesh.vertices)-min_z)
    for v in mesh.vertices:
        v.co = Vector((v.co.x*scale, v.co.y*scale, (v.co.z-min_z)*scale))
    mesh.materials.clear()
    mesh.materials.append(skin("Top"))
    mesh.materials.append(skin("Bottom"))
    for f in mesh.polygons:
        f.use_smooth = True
    anatomy = mesh.copy()
    # The runtime strike pivot retains the source shoulder and organic UVs.
    pivot = Vector((.52, .16, 2.88))
    arm_mesh = mesh.copy()
    for data, keep_arm in ((mesh, False), (arm_mesh, True)):
        bm = bmesh.new()
        bm.from_mesh(data)
        remove = []
        for face in bm.faces:
            center = face.calc_center_median()
            is_arm = center.x > .52 and center.z > 1.23
            if is_arm != keep_arm:
                remove.append(face)
        bmesh.ops.delete(bm, geom=remove, context="FACES")
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context="VERTS")
        bm.to_mesh(data)
        bm.free()
    arm = bpy.data.objects.new("Guardian captive strike arm", arm_mesh)
    bpy.context.scene.collection.objects.link(arm)
    for v in arm.data.vertices:
        v.co -= pivot
    body_objects = [body]
    before = set(bpy.context.scene.objects)
    metal = c.scanned_metal("Guardian worn gunmetal", (.25, .29, .28), .56, .82,
                            "blue_metal_plate", 2.5, .35)
    enamel = c.scanned_metal("Guardian breached restraint enamel", (.34, .40, .35), .70, .05,
                             "green_metal_rust", 1.0)
    for side in (-1, 1):
        armor_panel(anatomy, "Cast thigh guard", lambda v, s=side: v.x*s > .10 and 1.05 < v.z < 1.73,
                    enamel)
        armor_panel(anatomy, "Cast shin boot", lambda v, s=side: v.x*s > .10 and .13 < v.z < .92,
                    metal)
        # Leave the knees and flexor web exposed between manufactured shells.
        c.R("Shin return piston", (side*.28, .27, -.16), (side*.28, .86, -.12), .029, m["steel"])
        c.R("Hip linkage", (side*.25, 1.13, -.13), (side*.25, 1.67, -.11), .035, m["steel"])
        for y in (.88, 1.09, 1.65):
            c.R("Leg bearing pin", (side*.16, y, -.10), (side*.37, y, -.10), .065, m["brass"])
    armor_panel(anatomy, "Left dorsal containment carapace", lambda v: 1.85 < v.z < 2.90
                and v.y > .045 and v.x < -.04,
                metal)
    armor_panel(anatomy, "Left shoulder shell", lambda v: v.x < -.36 and 2.48 < v.z < 3.0,
                enamel)
    armor_panel(anatomy, "Left forearm gauntlet", lambda v: v.x < -.40 and 1.75 < v.z < 2.20,
                metal)
    armor_panel(anatomy, "Upper pectoral restraint", lambda v: abs(v.x) < .42 and 2.65 < v.z < 2.88
                and v.y < -.10, enamel)
    c.B("Dorsal service spine", (0, 2.40, -.34), (.18, .92, .16), m["steel"], .025)
    for y in (2.0, 2.24, 2.48, 2.72):
        c.B("Spinal guide clamp", (0, y, -.40), (.34, .08, .10), m["brass"])
    # Chest bridge and return straps are fitted to the imported torso.
    for x in (-.42, .42):
        c.B("Chest restraint stanchion", (x, 2.53, .29), (.11, .68, .15), m["steel"], .018)
        c.R("Restraint return", (x, 2.28, -.34), (x, 2.28, .37), .047, m["steel"])
        c.R("Restraint return", (x, 2.79, -.34), (x, 2.79, .37), .047, m["steel"])
    c.B("Chest limiter bridge", (0, 2.55, .40), (1.02, .24, .13), m["steel"], .025)
    c.B("C-19 contact plate", (0, 2.55, .48), (.37, .17, .04), m["brass"])
    for x in (-.12, 0, .12):
        c.R("Occult contact incision", (x-.03, 2.50, .505), (x+.03, 2.60, .505), .008, m["dark"], 8)
    for x in (-.37, .37):
        c.B("Dorsal umbilical socket", (x, 2.84, -.30), (.18, .23, .14), m["steel"])
        c.cable("Severed restraint feed", [(x, 2.84, -.38), (x*1.5, 3.19, -.49),
                (x*1.55, 3.48, -.46), (x*1.7, 3.42, -.27)], .045, m["dark"])
    body_objects += list(set(bpy.context.scene.objects)-before)
    arm_objects = [arm]
    export(body_objects, "guardian_body")
    export(arm_objects, "guardian_arm")
    for obj in arm_objects:
        obj.location += pivot
    review = ROOT / "build/blender-review/g019"
    review.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(review / "guardian-assembled.blend"))
    (GRAPHICS / "adaptation.json").write_text(json.dumps({
        "source": "GAME-READY STUDIOS / bdb9c9cfcf9c4a5eafec9f58b170bf98",
        "height_m": 3.55, "runtime_arm_pivot": [pivot.x, pivot.z, -pivot.y],
        "texture_edge": 2048, "rig": "offline pose only",
    }, indent=2), encoding="utf-8")


if __name__ == "__main__":
    build()
