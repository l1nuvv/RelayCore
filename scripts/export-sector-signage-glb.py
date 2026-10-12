"""Reuse the authored B-17 board/notice construction with sector-specific artwork."""
from pathlib import Path
import sys
import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "content/models"

def variant(base, name, texture):
    bpy.ops.wm.open_mainfile(filepath=str(OUTPUT / f"{base}.blend"))
    old_name = "lab_notes.png" if base == "lab_notes_board" else "lab_pictograms.png"
    replacement = bpy.data.images.load(str(ROOT / f"content/graphics/sector_signage/{texture}.png"))
    replacement.pack()
    replaced = 0
    for surface in bpy.data.materials:
        if not surface.use_nodes:
            continue
        for node in surface.node_tree.nodes:
            if node.type == "TEX_IMAGE" and node.image and Path(node.image.filepath).name == old_name:
                node.image = replacement
                surface.name = f"{name} / institutional graphic"
                replaced += 1
    if replaced != 1:
        raise RuntimeError(f"Expected one {base} artwork slot, found {replaced}")
    for image in list(bpy.data.images):
        if image.users == 0:
            bpy.data.images.remove(image)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / f"{name}.blend"))
    temporary = OUTPUT / f"{name}.export.glb"
    bpy.ops.export_scene.gltf(filepath=str(temporary), export_format="GLB", export_yup=True,
                             export_normals=True, export_texcoords=True, export_animations=False,
                             export_skins=False, export_cameras=False, export_lights=False, export_extras=False)
    temporary.replace(OUTPUT / f"{name}.glb")


if "--pentagram-only" not in sys.argv:
    for sector in ("pressure", "cooling", "power"):
        for base, kind in (("lab_notes_board", "procedure"), ("safety_notice", "pictograms")):
            variant(base, f"{sector}_{base}", f"{sector}_{kind}")
    variant("lab_notes_board", "occult_research_board", "occult_research_board")

# The existing native contract is opaque. Cut geometry to the image silhouette
# instead of adding a rectangular backing or a new renderer alpha mode.
bpy.ops.wm.read_factory_settings(use_empty=True)
image = bpy.data.images.load(str(ROOT / "content/graphics/sector_signage/blood_pentagram.png"))
image.pack()
width, height = image.size
pixels = np.empty(width * height * 4, dtype=np.float32)
image.pixels.foreach_get(pixels)
alpha = pixels.reshape(height, width, 4)[:, :, 3]
step = 4
rows, cols = height // step, width // step
alpha = alpha[:rows * step, :cols * step]
mask = alpha.reshape(rows, step, cols, step).mean(axis=(1, 3)) > .42
# Keep the transparent source intact; the native product gets opaque RGB and
# uses the cut geometry for coverage.
pixels.reshape(height, width, 4)[:, :, 3] = 1
opaque = bpy.data.images.new("Blood paint RGB / coverage in geometry", width=width, height=height, alpha=False)
opaque.pixels.foreach_set(pixels)
opaque.pack()
rectangles, active = [], {}
for row in range(rows + 1):
    spans = set()
    if row < rows:
        edges = np.diff(np.pad(mask[row].astype(np.int8), (1, 1)))
        spans = set(zip(np.flatnonzero(edges == 1), np.flatnonzero(edges == -1)))
    for span in list(active):
        if span not in spans:
            rectangles.append((*span, active.pop(span), row))
    for span in spans:
        active.setdefault(span, row)
vertices, faces, uvs = [], [], []
for x0, x1, y0, y1 in rectangles:
    offset = len(vertices)
    for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        vertices.append(((x / cols - .5) * 1.7, 0, y / rows * 1.7))
        uvs.append((x * step / width, y * step / height))
    faces.extend(((offset, offset + 1, offset + 2), (offset, offset + 2, offset + 3)))
mesh = bpy.data.meshes.new("Blood paint silhouette / no backing")
mesh.from_pydata(vertices, [], faces)
mesh.update()
uv_layer = mesh.uv_layers.new()
for polygon in mesh.polygons:
    for index in polygon.loop_indices:
        uv_layer.data[index].uv = uvs[mesh.loops[index].vertex_index]
material = bpy.data.materials.new("Crimson blood / matte opaque cutout")
material.use_nodes = True
material.use_backface_culling = True
shader = material.node_tree.nodes.get("Principled BSDF")
shader.inputs["Base Color"].default_value = (1, 1, 1, 1)
shader.inputs["Roughness"].default_value = .86
texture = material.node_tree.nodes.new("ShaderNodeTexImage")
texture.image = opaque
material.node_tree.links.new(texture.outputs["Color"], shader.inputs["Base Color"])
# Emission follows the paint variation; geometry still owns silhouette coverage.
emission_pixels = pixels.reshape(height, width, 4).copy()
emission_pixels[:, :, 0] = .65 + .35 * emission_pixels[:, :, 0]
emission_pixels[:, :, 1] = .008
emission_pixels[:, :, 2] = .003
emission = bpy.data.images.new("Crimson glyph emission", width=width, height=height, alpha=False)
emission.pixels.foreach_set(emission_pixels.ravel())
emission.pack()
emission_texture = material.node_tree.nodes.new("ShaderNodeTexImage")
emission_texture.image = emission
material.node_tree.links.new(emission_texture.outputs["Color"], shader.inputs["Emission Color"])
shader.inputs["Emission Strength"].default_value = .35
mesh.materials.append(material)
obj = bpy.data.objects.new("Wall pentagram / 1.7m / render only", mesh)
bpy.context.collection.objects.link(obj)
bpy.context.preferences.filepaths.save_version = 0
bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / "blood_pentagram.blend"))
bpy.ops.export_scene.gltf(filepath=str(OUTPUT / "blood_pentagram.glb"), export_format="GLB", export_yup=True,
                         export_normals=True, export_texcoords=True, export_animations=False,
                         export_skins=False, export_cameras=False, export_lights=False, export_extras=False)
print(f"Wall pentagram: {len(faces)} triangles, no background, no collision")
