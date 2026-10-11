"""G-019 D. Native static assemblies in runtime metres (+Y up, face +Z).

Blender --background --factory-startup --disable-autoexec --threads 2
--python-exit-code 1 --python scripts/export-containment-glb.py
Use -- --model=<name> for one assembly. Moving assembly origins are their pivots.
"""
from pathlib import Path
from math import sin, cos, pi, atan2, hypot
import importlib.util
import sys
import tomllib

import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("power", ROOT / "scripts/export-power-restoration-glb.py")
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
h = p.h
h.GRAPHICS = ROOT / "content/graphics/containment"
h.GRAPHICS.mkdir(parents=True, exist_ok=True)
B, R, S = h.bay_box, p.rod, p.sign
SELECTED = {a.split("=", 1)[1] for a in sys.argv if a.startswith("--model=")}


def materials():
    m = p.materials()
    m["paint"] = scanned_metal("containment_sage_enamel", (.34, .40, .35), .69, .05,
                               "green_metal_rust", 1.0)
    m["blue"] = scanned_metal("containment_bluegray_enamel", (.30, .38, .40), .65, .05,
                              "blue_metal_plate", 2.5)
    m["steel"] = scanned_metal("containment_worn_steel", (.40, .43, .42), .55, .82,
                               "blue_metal_plate", 2.5, .35)
    m["wall"] = h.material("containment_plaster", (.48, .47, .425), .94,
                           texture="content/third_party/polyhaven/pressure_transfer/concrete_wall_001_diff_1k.jpg",
                           variation=1.3)
    m["wall"]["uv_metres"] = 1.5
    m["brass"] = h.material("containment_aged_brass", (.40, .26, .09), .52, .65)
    return m


def scanned_metal(name, color, roughness, metallic, asset, metres, normal_strength=.7):
    """Retain photographic wear rather than reducing it to near-uniform paint."""
    mat = h.material(name, color, roughness, metallic)
    shader = mat.node_tree.nodes.get("Principled BSDF")
    source_dir = ROOT / "content/third_party/polyhaven/containment"
    source = bpy.data.images.load(str(source_dir / f"{asset}_diff_2k.jpg"), check_existing=True)
    pixels = np.asarray(source.pixels[:], dtype=np.float32).reshape(-1, 4)
    luminance = pixels[:, :3].mean(axis=1)
    modulation = luminance / float(luminance.mean())
    chroma = pixels[:, :3] - luminance[:, None]
    pixels[:, :3] = np.clip(modulation[:, None] * np.asarray(color) + chroma*.25, 0, 1)
    pixels[:, 3] = 1
    image = bpy.data.images.new(name, width=source.size[0], height=source.size[1])
    image.colorspace_settings.name = "sRGB"
    image.pixels.foreach_set(pixels.ravel())
    image.filepath_raw = str(h.GRAPHICS / f"{name}.png")
    image.file_format = "PNG"
    image.save()
    image.pack()
    texture = mat.node_tree.nodes.new("ShaderNodeTexImage")
    texture.image = image
    mat.node_tree.links.new(texture.outputs["Color"], shader.inputs["Base Color"])
    normal_source = bpy.data.images.load(str(source_dir / f"{asset}_nor_gl_2k.jpg"), check_existing=True)
    normal_source.colorspace_settings.name = "Non-Color"
    normal_pixels = normal_source.pixels[:]
    normal_image = bpy.data.images.new(asset + " surface normal", width=normal_source.size[0], height=normal_source.size[1])
    normal_image.colorspace_settings.name = "Non-Color"
    normal_image.pixels.foreach_set(normal_pixels)
    normal_image.filepath_raw = str(h.GRAPHICS / f"{asset}_normal.png")
    normal_image.file_format = "PNG"
    normal_image.save()
    normal_image.pack()
    texture = mat.node_tree.nodes.new("ShaderNodeTexImage")
    texture.image = normal_image
    mapped = mat.node_tree.nodes.new("ShaderNodeNormalMap")
    mapped.inputs["Strength"].default_value = normal_strength
    mat.node_tree.links.new(texture.outputs["Color"], mapped.inputs["Color"])
    mat.node_tree.links.new(mapped.outputs["Normal"], shader.inputs["Normal"])
    mat["uv_metres"] = metres
    return mat


def ellipsoid(name, center, size, surface):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=20,
                                       location=(center[0], -center[2], center[1]))
    obj = bpy.context.object
    obj.name = name
    obj.scale = (size[0]/2, size[2]/2, size[1]/2)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    obj.data.materials.append(surface)
    for face in obj.data.polygons:
        face.use_smooth = True
    return obj


def cable(name, points, radius, surface):
    return h.sweep(name, [(x, -z, y) for x, y, z in points], radius, surface, 12)


def collar(name, center, radius, surface, major=.035):
    bpy.ops.mesh.primitive_torus_add(major_segments=40, minor_segments=8,
                                   location=(center[0], -center[2], center[1]),
                                   major_radius=radius, minor_radius=major)
    obj = bpy.context.object
    obj.name = name
    obj.data.materials.append(surface)
    for face in obj.data.polygons:
        face.use_smooth = True
    return obj


def grip(m):
    B("Travelling boxed crosshead", (0, 0, 0), (5.6, .50, 2.40), m["paint"], .045)
    for z in (-1.18, 1.18):
        B("Crosshead folded flange", (0, .14, z), (5.62, .18, .07), m["steel"])
        for x in (-2.4, -1.2, 0, 1.2, 2.4):
            ellipsoid("Crosshead bolt", (x, .14, z*1.03), (.10, .10, .04), m["dark"])
    for x in (-2.60, 2.60):
        B("Capture jaw", (x, -.60, 0), (.40, 1.40, 2.40), m["steel"], .035)
        for z in (-.8, 0, .8):
            B("Replaceable jaw pad", (x*.94, -.98, z), (.09, .50, .42), m["dark"])
        B("Guide shoe", (x*1.50, .08, 0), (.38, .56, .65), m["steel"])
        R("Crosshead to guide", (x, .08, 0), (x*1.50, .08, 0), .11, m["steel"])
    B("Ram thrust socket", (0, .31, 0), (.70, .15, .70), m["steel"])
    spec = importlib.util.spec_from_file_location("crosshead_emission", ROOT / "scripts/author-containment-emission.py")
    emission = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(emission)
    sign_materials = dict(m)
    sign_materials["ivory"] = m["ivory"].copy()
    emission.backlight(sign_materials["ivory"])
    S("C-19 / RESTRAINT", (0, 0, -1.235), 2.5, sign_materials)


def ram(m):
    # Unit stroke: runtime scales only its vertical length between socket and head.
    R("Polished ram", (0, 0, 0), (0, 1, 0), .16, m["steel"], 40)


def cassette(m):
    B("Cassette folded shell", (0, .12, 0), (.38, .24, .46), m["paint"], .018)
    B("Contact face", (0, .12, .237), (.30, .18, .025), m["ivory"])
    for x in (-.11, 0, .11):
        R("Contact pin", (x, .12, .24), (x, .12, .275), .025, m["brass"], 20)
    for x in (-.15, .15):
        R("Recessed grip post", (x, .25, -.12), (x, .29, -.12), .025, m["steel"])
    R("Cassette carry handle", (-.15, .29, -.12), (.15, .29, -.12), .025, m["steel"])
    S("C-19", (0, .12, -.237), .28, m)


def shooter(m, destroyed=False):
    # Shared enemy hit envelope is .84 x .95 x .84 metres.
    B("Mobile emitter chassis", (0, .32, -.02), (.58, .32, .54), m["paint"], .065)
    for x in (-.32, .32):
        for z in (-.24, .24):
            R("Traction wheel", (x-.065, .19, z), (x+.065, .19, z), .17, m["dark"], 40)
            R("Wheel hub", (x-.07, .19, z), (x+.07, .19, z), .063, m["steel"])
        B("Wheel guard", (x, .38, 0), (.16, .08, .69), m["paint"], .025)
    R("Emitter rotation bearing", (0, .46, 0), (0, .59, 0), .19, m["steel"], 40)
    B("Emitter recoil cradle", (0, .67, .02), (.31, .24, .45), m["steel"], .035)
    R("Ranged emitter barrel", (0, .75, -.09), (0, .75, .34), .092, m["dark"], 40)
    R("Copper emitter winding", (0, .75, .03), (0, .75, .20), .12, m["brass"], 40)
    R("Emitter aperture", (0, .75, .335), (0, .75, .355), .060,
      m["dark"] if destroyed else m["red"], 32)
    B("Sight bridge", (0, .89, .03), (.20, .07, .20), m["paint"], .015)
    cable("Flexible emitter feed", [(-.20, .44, -.18), (-.25, .60, -.26),
          (-.16, .70, -.27), (0, .75, -.18)], .022, m["dark"])
    S("M-19", (0, .31, -.302), .36, m)
    if destroyed:
        B("Ruptured casing", (.13, .52, -.09), (.21, .05, .25), m["rust"])
        for obj in list(bpy.context.scene.objects):
            if obj.type == "MESH":
                obj.location.z *= .65
                obj.scale.z *= .65


def lining(m):
    level = tomllib.loads((ROOT / "assets/levels/containment_loop.toml").read_text(encoding="utf-8"))
    for solid in level["solid_group"][0]["boxes"]:
        x, y, z = solid["origin"]
        w, height, d = solid["size"]
        if solid["material"] in ("concrete", "WallPanel") and height >= 2:
            B("Institutional plaster", (x, y, z), (w+.018, height+.012, d+.018), m["wall"], .002)
            if y-height/2 < .1:
                B("Protective enamel dado", (x, .58, z), (w+.034, 1.16, d+.034), m["paint"], .002)
                B("Dado folded cap", (x, 1.175, z), (w+.047, .03, d+.047), m["steel"], .002)
        if x in (-20, -15) and height == 2.9:
            for side in (-1, 1):
                B("Archive cabinet side", (x+side*.616, 1.45, z), (.035, 2.9, 5.02), m["paint"])
                for dy in (.45, 1.18, 1.91, 2.64):
                    B("Recessed archive drawer", (x+side*.640, dy, z), (.015, .66, 4.88), m["blue"], .003)
                    for dz in (-1.8, -.6, .6, 1.8):
                        B("Drawer handle", (x+side*.66, dy, z+dz), (.035, .07, .34), m["steel"])
            B("Archive top cap", (x, 2.92, z), (1.25, .06, 5.05), m["steel"])
        if height == 1.2 and w == 3 and d == 2:
            B("Workbench folded top", (x, 1.225, z), (3.04, .05, 2.04), m["steel"])
            for side in (-1, 1):
                B("Service cabinet front", (x, .62, z+side*1.015), (2.94, 1.08, .035), m["paint"])
                for dx in (-.95, 0, .95):
                    B("Tool drawer", (x+dx, .86, z+side*1.043), (.87, .36, .025), m["blue"])
                    B("Drawer pull", (x+dx, .88, z+side*1.062), (.25, .065, .035), m["dark"])
    B("Cassette storage pedestal", (-24, .40, 51), (1.025, .80, .825), m["paint"])
    B("Cassette storage top", (-24, .8125, 51), (1.06, .025, .86), m["steel"])
    S("C-19 / LIMITER", (-24, .52, 50.57), .85, m)
    for entity in level["entity"]:
        if entity["classname"] != "render_light":
            continue
        x, y, z = entity["origin"]
        B("Suspended fixture tray", (x, y+.16, z), (1.5, .12, .40), m["steel"])
        B("Fixture diffuser", (x, y+.08, z), (1.38, .04, .29), m["ivory"])
        for dx in (-.55, .55):
            ceiling = 4.125 if -6 < x < 6 and z < 12 else 7.98
            R("Fixture hanger", (x+dx, y+.23, z), (x+dx, ceiling, z), .016, m["steel"], 12)
    for x in (-5, 5):
        B("Gantry column casing", (x, 3.6, 30), (.83, 7.2, .83), m["paint"], .025)
        B("Anchored foot plate", (x, .09, 30), (1.20, .18, 1.20), m["steel"])
        for dx in (-.43, .43):
            for dz in (-.43, .43):
                R("Foundation anchor", (x+dx, .17, 30+dz), (x+dx, .23, 30+dz), .055, m["dark"], 8)
        R("Jaw guide rail", (x*.78, .95, 28), (x*.78, 6.95, 28), .095, m["steel"], 40)
        for y in (1.0, 3.8, 6.7):
            R("Guide support", (x, y, 30), (x*.78, y, 28), .075, m["steel"])
    B("Gantry head casing", (0, 7, 30), (10.04, .64, 1.54), m["paint"], .035)
    R("Hydraulic cylinder", (0, 6.9, 28), (0, 7.65, 28), .37, m["blue"], 48)
    for y in (6.93, 7.60):
        collar("Cylinder flange", (0, y, 28), .39, m["steel"], .055)
    R("Cylinder head support", (0, 7.10, 28), (0, 7.10, 30), .22, m["steel"])
    cable("Hydraulic supply", [(0, 7.60, 28), (2, 7.60, 28), (4.7, 7.3, 29),
          (4.7, 1.5, 29)], .048, m["dark"])
    for x in (-3.42, 3.42):
        B("Trap perimeter rail", (x, .255, 28), (.05, .02, 7.84), m["ochre"], .002)
    for z in (24.10, 31.90):
        B("Trap perimeter rail", (0, .255, z), (6.84, .02, .06), m["ochre"], .002)
    for z, x in ((26, -5.45), (29, -5.47)):
        B("Control pedestal foot", (x, .055, z), (.68, .11, .88), m["steel"], .008)
        B("Control pedestal stem", (x, .63, z), (.27, 1.16, .30), m["steel"], .008)
        for dx in (-.25, .25):
            for dz in (-.34, .34):
                B("Pedestal anchor bolt", (x+dx, .12, z+dz), (.055, .025, .055), m["dark"], .004)
    B("Release control enclosure", (-5.45, 1.39, 26), (.28, .68, .80), m["paint"], .015)
    B("Release sign post", (-5.49, 2.15, 26), (.09, 1.50, .12), m["steel"], .004)
    B("Limiter sign post", (-5.58, 1.97, 29), (.09, .70, .12), m["steel"], .004)
    # The existing socket/panel retains its focus and interaction plane.
    B("Cassette dock enclosure", (-5.47, 1.38, 29), (.27, .70, .80), m["paint"])
    S("C-19 / LIMITER", (-5.64, 2.0, 29), 1.55, m, "-x")
    S("C-19 / RELEASE", (-5.55, 2.6, 26), 1.55, m, "-x")
    for text, pos, width, facing in (
        ("OBSERVATION / C-19", (0, 3.5, 11.79), 3.2, "-z"),
        ("01 / WORKSHOP", (-18, 3.4, 12.19), 3.0, "+z"),
        ("02 / ARCHIVE", (-16, 3.8, 25.81), 2.9, "-z"),
        ("03 / LABORATORY", (16, 3.8, 27.81), 3.3, "-z"),
        ("C-19 / SERVICE DRIVE", (23, 6.1, 32), 2.4, "-x"),
        ("EXIT / NEXT SECTOR", (0, 3.5, 41.79), 3.2, "-z"),
        ("ARCHIVE / RETURN >", (-12.19, 2.8, 36), 2.6, "-x"),
    ):
        S(text, pos, width, m, facing)
    observation_and_hall(m)
    workshop(m)
    archive(m)
    laboratory(m)
    upper_services(m, level)
    final_contacts(m)


def observation_and_hall(m):
    B("Observer worktop", (-4.3, .93, 10.1), (2.7, .06, 1.1), m["steel"], .012)
    for x in (-5.35, -3.25):
        B("Observer drawer pedestal", (x, .45, 10.1), (.5, .90, 1.02), m["paint"], .012)
        for y in (.26, .56, .79):
            B("Observer drawer", (x, y, 9.58), (.44, .19, .035), m["blue"], .004)
            R("Observer pull", (x-.12, y, 9.548), (x+.12, y, 9.548), .014, m["steel"], 12)
    B("Observer wiring channel", (-4.3, .82, 10.53), (2.1, .12, .10), m["dark"], .006)
    cable("CRT power lead", [(-4.65, 1.22, 10.35), (-4.65, 1.0, 10.69),
          (-4.65, .82, 10.70), (-4.65, .82, 10.53), (-5.834, .82, 10.53)], .016, m["dark"])
    S("C-19 / REMOTE OBSERVATION", (-4.3, 2.4, 11.80), 2.8, m)
    for x in (-10, 10):
        B("Hall instrumentation plinth", (x, .61, 30), (2.035, 1.22, 4.035), m["paint"], .012)
        for z in (28.8, 30, 31.2):
            B("Plinth service hatch", (x+(.99 if x < 0 else -.99), .62, z), (.05, .80, .95), m["blue"], .009)
        cable("Hall instrument trunk", [(x, 1.3, 30), (x, 2.8, 30),
              (x, 3.1, 31), (x, 3.1, 33), (x/10*11.65, 3.1, 33),
              (x/10*11.65, 2.65, 33)], .037, m["dark"])
    for z in (29.55, 30.45):
        port = -.22 if z < 30 else .22
        cable("Drive hydraulic header", [(24+port, 5.4, 32), (24+port, 7.32, 32),
              (24+port, 7.32, z), (5, 7.32, z), (5, 7.6, 28+port),
              (0, 7.6, 28+port)], .047, m["dark"])
        for x in (6, 10, 14, 18, 22):
            R("Header suspension", (x, 7.22, z), (x, 7.98, z), .024, m["steel"], 12)
            B("Header saddle", (x, 7.26, z), (.12, .08, .24), m["steel"], .008)
    for x in (-11.65, 11.65):
        for y in (2.4, 2.65):
            R("Hall wall service pipe", (x, y, 22.2), (x, y, 33.8), .062, m["steel"], 24)
            for z in (23, 26, 29, 32, 33.5):
                B("Wall pipe bracket", (x, y, z), (.26, .05, .07), m["paint"], .004)
        R("Service pipe wall termination", (x, 2.4, 22.2),
          (-11.87 if x < 0 else 11.87, 2.4, 22.2), .062, m["steel"], 24)
        R("Service pipe wall termination", (x, 2.65, 33.8),
          (-11.87 if x < 0 else 11.87, 2.65, 33.8), .062, m["steel"], 24)


def repair_tools(x, y, z, m):
    B("Open parts tray base", (x, y+.018, z), (.52, .036, .34), m["blue"], .006)
    for dx in (-.255, .255):
        B("Parts tray end", (x+dx, y+.065, z), (.018, .10, .34), m["blue"], .004)
    for dz in (-.165, .165):
        B("Parts tray rim", (x, y+.065, z+dz), (.50, .10, .018), m["blue"], .004)
    collar("Removed winding", (x-.10, y+.061, z), .069, m["brass"], .012)
    for dz in (-.07, .07):
        R("Spare contact", (x+.11, y+.062, z+dz), (x+.18, y+.062, z+dz), .018, m["steel"], 12)
    for dx, radius in ((-.115, .04), (.115, .055)):
        collar("Ring spanner head", (x+dx, y+.012, z-.32), radius, m["steel"], .009)
    R("Ring spanner web", (x-.065, y+.012, z-.32), (x+.051, y+.012, z-.32), .015, m["steel"], 12)
    R("Driver shaft", (x-.16, y+.023, z+.31), (x+.04, y+.023, z+.31), .006, m["steel"], 12)
    ellipsoid("Fluted driver grip", (x+.11, y+.023, z+.31), (.15, .042, .042), m["dark"])


def papers(x, y, z, m):
    B("Folded service folder", (x, y+.012, z), (.39, .024, .29), m["ochre"], .003)
    B("Job sheet", (x+.016, y+.027, z), (.34, .006, .26), m["ivory"], .001)
    for row in range(6):
        B("Job sheet ruled record", (x-.02, y+.031, z-.09+row*.03), (.20-row*.012, .001, .003), m["dark"], 0)


def workshop(m):
    repair_tools(-18.1, 1.25, 22.75, m)
    # The left side of the pickup worktop remains empty around the shotgun.
    repair_tools(-23.0, 1.25, 19.35, m)
    papers(-24.6, 1.25, 19.55, m)
    B("Wall-mounted tool board", (-27.80, 2.0, 19), (.035, 1.35, 2.5), m["blue"], .006)
    for z in (18.15, 18.55, 18.95, 19.35, 19.75):
        B("Tool suspension hook", (-27.74, 2.3, z), (.09, .035, .045), m["steel"], .004)
        R("Hanging service wrench", (-27.72, 1.80, z), (-27.72, 2.24, z), .027, m["steel"], 16)
        B("Wrench open jaw", (-27.72, 2.27, z), (.04, .09, .14), m["steel"], .006)
    S("REPAIR / C-19 DRIVE", (-27.76, 2.94, 19), 2.4, m, "+x")
    cable("Workshop test lead", [(-18.8, 1.28, 23.15), (-18.65, 1.257, 23.0),
          (-18.40, 1.257, 23.15), (-18.20, 1.311, 22.819)], .006, m["dark"])


def archive(m):
    for x, row in ((-20, "A"), (-15, "B")):
        for index, z in enumerate((32, 40, 48), 1):
            for side in (-1, 1):
                B("Archive cabinet end panel", (x, 1.45, z+side*2.53),
                  (1.25, 2.9, .04), m["paint"], .005)
                S(f"{row}-{index:02} / C-19", (x, 2.43, z+side*2.565), 1.16,
                  m, "+z" if side > 0 else "-z")
    B("Dossier cabinet folded shell", (-24, 1.31, 45), (2.53, 2.62, 2.03), m["paint"], .012)
    for y in (.40, 1.0, 1.6, 2.2):
        B("Dossier drawer front", (-24, y, 43.968), (2.40, .53, .035), m["blue"], .006)
        for x in (-24.7, -23.3):
            R("Dossier drawer pull", (x-.16, y, 43.932), (x+.16, y, 43.932), .02, m["steel"], 12)
    S("C-19 / EXPERIMENT RECORDS", (-24, 2.84, 45), 2.55, m)
    papers(-27.05, .92, 48.45, m)
    for z in (48.0, 48.12, 48.24):
        B("Archive binder", (-27.25, 1.10, z), (.24, .36, .075), m["ochre"], .006)
        B("Binder spine label", (-27.123, 1.15, z), (.008, .15, .04), m["ivory"], .001)
    S("LIMITER / ISSUE DESK", (-24, 3.10, 53.79), 2.8, m)


def laboratory(m):
    papers(24.1, 1.25, 49.45, m)
    S("FIELD ANALYSIS / C-19", (22, 3.12, 53.79), 3.4, m)
    cable("Bench field sensor lead", [(19.5, 1.40, 46.25), (19.65, 1.257, 46.60),
          (20.1, 1.257, 46.70), (20.35, 1.32, 46.65)], .008, m["dark"])
    cable("Laboratory instrument wall feed", [(27.4, .16, 49.35), (27.7, .10, 49.35),
          (27.84, .10, 49.35)], .026, m["dark"])
    B("Laboratory cable entry", (27.82, .10, 49.35), (.05, .14, .14), m["steel"], .005)


def upper_services(m, level):
    for solid in level["solid_group"][0]["boxes"]:
        x, y, z = solid["origin"]
        w, height, d = solid["size"]
        if y != 4.5 or height != 1:
            continue
        B("Folded gallery guard", (x, y, z), (w+.025, height+.015, d+.025), m["paint"], .004)
        B("Gallery hand cap", (x, 5.025, z), (w+.06, .055, d+.06), m["steel"], .008)
        B("Gallery kick strip", (x, 4.08, z), (w+.04, .16, d+.04), m["steel"], .004)
        axis = 0 if w > d else 2
        length = w if axis == 0 else d
        bays = int(length/2)
        for index in range(bays+1):
            pos = [x, y, z]
            pos[axis] += -length/2 + index*length/bays
            B("Guard panel seam post", pos, (.09, 1.02, d+.05) if axis == 0
              else (w+.05, 1.02, .09), m["steel"], .004)
    for x, start in ((18, 14), (-24, 30)):
        for side in (-1, 1):
            obj = B("Stair side stringer", (x+side*1.535, 2, start+4.5),
                    (.09, .18, hypot(9, 3.9)), m["steel"], .01)
            obj.rotation_euler.x = -atan2(3.9, 9)
        for step in range(20):
            B("Stair tread folded nosing", (x, (step+1)*.2+.012, start+step*.45+.025),
              (3.04, .024, .06), m["steel"], .003)
    for side, depths in ((-1, (30, 34, 38)), (1, (20, 26, 34))):
        for z in depths:
            B("Gallery cantilever beam", (side*9.96, 3.65, z), (3.86, .30, .16), m["steel"], .008)
            B("Gallery wall anchor plate", (side*11.82, 3.40, z), (.05, .82, .54), m["paint"], .006)
            for y in (3.14, 3.64):
                for dz in (-.18, .18):
                    R("Gallery anchor bolt", (side*11.79, y, z+dz),
                      (side*11.74, y, z+dz), .025, m["steel"], 12)
    B("Drive service folded case", (24, 4.7, 32), (2.035, 1.435, 2.035), m["paint"], .015)
    B("Drive case service hatch", (24, 4.77, 30.966), (1.64, 1.06, .035), m["blue"], .006)
    for x in (23.3, 24.7):
        for y in (4.37, 5.16):
            R("Drive hatch fastener", (x, y, 30.94), (x, y, 30.92), .024, m["steel"], 12)
    S("C-19 / HYDRAULIC DRIVE", (24, 5.20, 30.922), 1.85, m)
    for x in (23.25, 24.75):
        B("Drive case foot", (x, 4.04, 32), (.24, .08, 1.85), m["steel"], .005)


def final_contacts(m):
    papers(-3.22, .96, 9.82, m)
    B("Observer power wall entry", (-5.834, .82, 10.53), (.052, .14, .14), m["steel"], .005)
    cable("Observer monitor power lead", [(-3.55, 1.15, 10.325), (-3.55, 1.0, 10.69),
          (-3.55, .82, 10.70), (-3.55, .82, 10.53)], .013, m["dark"])
    for x in (-5.3, -3.3):
        R("Observer sign glass standoff", (x, 2.4, 11.82), (x, 2.4, 11.985), .022, m["steel"], 12)
    for z in (18.1, 19.9):
        R("Workshop sign wall standoff", (-27.78, 2.94, z), (-27.85, 2.94, z), .02, m["steel"], 12)
    for x in (-25, -23):
        R("Dossier header support", (x, 2.62, 45), (x, 2.67, 45), .024, m["steel"], 12)
    for x, y, z in ((-25.1, 3.10, 53.79), (-22.9, 3.10, 53.79),
                    (20.7, 3.12, 53.79), (23.3, 3.12, 53.79)):
        R("Archive lab sign wall standoff", (x, y, z+.015), (x, y, 53.85), .02, m["steel"], 12)
    for x in (23.2, 24.8):
        R("Drive plate standoff", (x, 5.20, 30.94), (x, 5.20, 30.95), .024, m["steel"], 12)
    for side in (-1, 1):
        B("Hall instrument header junction", (side*11.65, 2.65, 33), (.20, .18, .20), m["blue"], .006)
        for z in (23, 26, 29, 32):
            R("Hall pipe coupling", (side*11.65, 2.4, z-.085),
              (side*11.65, 2.4, z+.085), .081, m["steel"], 24)


if __name__ == "__main__":
    for name, build in (
        ("containment_grip", grip), ("containment_ram", ram),
        ("limiter_cassette", cassette), ("mobile_shooter", shooter),
        ("mobile_shooter_destroyed", lambda m: shooter(m, True)),
        ("containment_lining", lining),
    ):
        if SELECTED and name not in SELECTED:
            continue
        build(materials())
        h.export(name)
