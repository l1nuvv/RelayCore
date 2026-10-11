"""Material-only IMG-06 pass; run after the containment grip exporter.

Blender --background --disable-autoexec --threads 2 --python-exit-code 1
--python scripts/author-containment-emission.py
"""
from pathlib import Path
import importlib.util
import json
import struct

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]


def backlight(material):
    material.name = "containment_crosshead_backlight"
    shader = material.node_tree.nodes.get("Principled BSDF")
    shader.inputs["Base Color"].default_value = (.025, .05, .035, 1)
    image = bpy.data.images.get("containment_crosshead_emission")
    if image is not None:
        bpy.data.images.remove(image)
    image = bpy.data.images.new("containment_crosshead_emission", width=128, height=32)
    image.colorspace_settings.name = "sRGB"
    y, x = np.mgrid[:32, :128]
    # Broad phosphor bands retain variation at gameplay distance; no baked shadows.
    value = .65 + .35 * np.sin(np.pi * (x + .5) / 128) ** 2
    value *= .85 + .15 * np.sin(np.pi * (y + .5) / 32) ** 2
    pixels = np.ones((32, 128, 4), dtype=np.float32)
    pixels[:, :, :3] = value[:, :, None] * np.array([.38, .84, .60])
    image.pixels.foreach_set(pixels.ravel())
    directory = ROOT / "content/graphics/img04-06"
    directory.mkdir(parents=True, exist_ok=True)
    image.filepath_raw = str(directory / "containment_crosshead_emission.png")
    image.file_format = "PNG"
    image.save()
    image.pack()
    node = material.node_tree.nodes.get("Crosshead emission") or material.node_tree.nodes.new("ShaderNodeTexImage")
    node.name = "Crosshead emission"
    node.image = image
    material.node_tree.links.new(node.outputs["Color"], shader.inputs["Emission Color"])
    shader.inputs["Emission Strength"].default_value = .8


if __name__ == "__main__":
    path = ROOT / "content/models/containment_grip"
    bpy.ops.wm.open_mainfile(filepath=str(path.with_suffix(".blend")))
    material = bpy.data.materials.get("containment_crosshead_backlight") or bpy.data.materials["power_instrument_ivory"]
    backlight(material)
    objects = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    before = [(obj.name, len(obj.data.vertices), len(obj.data.polygons)) for obj in objects]
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(path.with_suffix(".blend")))
    bpy.ops.export_scene.gltf(filepath=str(path.with_suffix(".glb")), export_format="GLB",
        use_selection=True, export_yup=True, export_normals=True, export_texcoords=True, export_tangents=True,
        export_animations=False, export_skins=False, export_extras=False, export_cameras=False, export_lights=False)
    assert before == [(obj.name, len(obj.data.vertices), len(obj.data.polygons)) for obj in objects]
    data = path.with_suffix(".glb").read_bytes()
    size = struct.unpack_from("<I", data, 12)[0]
    document = json.loads(data[20:20 + size])
    assert not document.get("extensionsUsed"), document.get("extensionsUsed")
    spec = importlib.util.spec_from_file_location("native_export", ROOT / "scripts/export-pressure-transfer-glb.py")
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    native.repair_exported_tangents(path.with_suffix(".glb"))
    print("Geometry preserved:", before, flush=True)
    print("Emission:", [(m["name"], m.get("emissiveFactor"), m.get("emissiveTexture"))
                        for m in document["materials"] if "emissiveTexture" in m], flush=True)
