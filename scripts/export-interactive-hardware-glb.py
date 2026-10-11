"""VIS-17/18 normalized door leaves and panel housings, in runtime basis.

Doors fit the unit AABB, including handles. Panel XY fits the unit rectangle;
depth is in metres, matching the existing actuator at Z=.086. Guides and
feedback actuators remain runtime geometry. Use -- --model=<name> to select.
"""
from pathlib import Path
from math import pi, sin, cos
from random import Random
import importlib.util
import sys

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("hardware", ROOT / "scripts/export-pressure-transfer-glb.py")
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
B = h.bay_box
SELECTED = {a.split("=", 1)[1] for a in sys.argv if a.startswith("--model=")}


def materials():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = "METRIC"
    palette = {
        "paint": h.material("hardware_sage_enamel", (.24, .29, .25), .68),
        "ivory": h.material("hardware_instrument_ivory", (.60, .58, .49), .73),
        "steel": h.material("hardware_satin_steel", (.31, .34, .33), .48, .65),
        "dark": h.material("hardware_dark_steel", (.045, .057, .052), .72, .25),
        "ochre": h.material("hardware_safety_ochre", (.49, .29, .065), .72),
        "wear": h.material("hardware_contact_steel", (.52, .55, .53), .76, .20),
        "scuff": h.material("hardware_kick_transfer", (.18, .20, .18), .88),
        "rust": h.material("hardware_breached_edge_oxide", (.50, .31, .18), .94,
                           sheet="content/graphics/prop_materials_source.png", cell=3),
        "primer": h.material("hardware_chipped_primer", (.19, .18, .14), .92),
        "crack": h.material("hardware_fractured_enamel", (.045, .052, .046), .97),
    }
    h.surface_normal(palette["rust"], .18, .07)
    return palette


def rod(name, a, b, radius, material, sides=24):
    h.rod(name, (a[0], -a[2], a[1]), (b[0], -b[2], b[1]), radius, material, sides)


def text(value, position, size, material, facing="+z"):
    h.bay_label(value, position, size, material, facing)


def pull_handle(x, sign, material):
    points = [Vector((x, -sign * (.442 + .035 * sin(i*pi/12)), -.045 + .14*i/12)) for i in range(13)]
    vertices, faces = [], []
    sides = 24
    # A fixed transverse axis avoids frame flips along the shallow arch.
    u = Vector((1, 0, 0))
    for i, point in enumerate(points):
        tangent = points[min(i+1, len(points)-1)] - points[max(0, i-1)]
        v = tangent.cross(u).normalized()
        vertices.extend(point + .012 * (u*cos(j*2*pi/sides) + v*sin(j*2*pi/sides)) for j in range(sides))
    for i in range(len(points)-1):
        for j in range(sides):
            a, b = i*sides+j, i*sides+(j+1)%sides
            faces.append((a, b, b+sides, a+sides))
    faces += [tuple(reversed(range(sides))), tuple(range(len(vertices)-sides, len(vertices)))]
    obj = h.mesh_object("Recessed pull bridge", vertices, faces, material)
    for face in obj.data.polygons[:-2]:
        face.use_smooth = True
    return points


def leaf(m, lift):
    # Recessed pressed panels sit inside the rolled perimeter, on both faces.
    B("Folded leaf core", (0, 0, 0), (1, 1, .68), m["dark"], .012)
    for sign in (-1, 1):
        z = sign * .37
        for x in (-.475, .475):
            B("Rolled upright", (x, 0, z), (.05, 1, .18), m["steel"], .008)
        for y in (-.475, .475):
            B("Rolled end cap", (0, y, z), (.90, .05, .18), m["steel"], .008)
        for x in (-.233, .233):
            for y, height in ((.17, .55), (-.265, .29)):
                B("Pressed enamel infill", (x, y, sign * .367), (.434, height, .075), m["paint"], .018)
        B("Recessed center joint", (0, 0, sign * .40), (.018, .90, .025), m["dark"], .003)
        for i, (x, y) in enumerate(((-.430, .421), (.430, -.141))):
            h.wear_patch("Infill edge chip", (x, y, sign*.405),
                         (.004, 0, 0), (0, sign*.008, 0), m["wear"], 220+i)
        rng = Random(2323+sign)
        for i in range(20):
            x = -.405+rng.uniform(-.007, .09)
            y = -.080+rng.uniform(-.003, .018)
            flake = rng.uniform(.006, .018)
            h.wear_patch("Flaked pressed-panel edge", (x, y, sign*.405),
                         (flake, 0, 0), (0, sign*.005, 0), m["primer"], 300+i)
            if i % 2 == 0:
                h.wear_patch("Exposed steel within paint loss", (x, y, sign*.4055),
                             (flake*.55, 0, 0), (0, sign*.0025, 0), m["wear"], 300+i)
        for i in range(16):
            x = -.475+rng.uniform(-.008, .008)
            y = -.43+rng.uniform(0, .13)
            h.wear_patch("Lower rolled edge corrosion", (x, y, sign*.4605),
                         (.007, 0, 0), (0, sign*rng.uniform(.009, .025), 0), m["rust"], 340+i)
        h.surface_crack("Fractured infill corner", [(-.431, -.088, sign*.405), (-.410, -.052, sign*.405),
                        (-.418, -.021, sign*.405), (-.395, .012, sign*.405)],
                        (0, 0, sign), .0018, m["crack"])
        h.surface_crack("Infill crack branch", [(-.410, -.052, sign*.405), (-.372, -.043, sign*.405),
                        (-.354, -.024, sign*.405)], (0, 0, sign), .0012, m["crack"])
        B("Replaceable kick plate", (0, -.37, sign * .424), (.85, .095, .025), m["steel"], .005)
        for i, (x, y, length) in enumerate(((-.27, -.35, .045), (.06, -.385, .08), (.25, -.361, .035))):
            h.wear_patch("Kick plate shoe scrape", (x, y, sign*.4375),
                         (length, .004, 0), (0, sign*.006, 0), m["scuff"], 230+i)
        B("Drive identity recess", (0, .32, sign * .415), (.48, .086, .025), m["dark"], .007)
        text("LIFT" if lift else "SLIDE", (0, .301, sign * .432), .046, m["ivory"], "+z" if sign > 0 else "-z")
        if lift:
            for x in (-.36, .36):
                B("Lift clamp foot", (x, .405, sign * .43), (.10, .075, .06), m["steel"], .006)
            B("Lower safety edge", (0, -.455, sign * .455), (.86, .026, .035), m["ochre"], .004)
        else:
            x = -.33 if sign > 0 else .33
            for y in (-.045, .095):
                B("Pull handle shoe", (x, y, sign * .431), (.075, .035, .05), m["steel"], .005)
            points = pull_handle(x, sign, m["steel"])
            # Follow the existing arched grip, keeping all marks inside its AABB.
            vertices = []
            for i in range(3, 10):
                half_width = .003+.003*sin((i-3)*pi/6)
                normal = -sign*(points[i+1]-points[i-1]).cross(Vector((1, 0, 0))).normalized()
                surface = points[i]+normal*((.012**2-half_width**2)**.5+.0005)
                vertices.extend((surface-Vector((half_width, 0, 0)), surface+Vector((half_width, 0, 0))))
            faces = [(2*i, 2*i+1, 2*i+3, 2*i+2) for i in range(6)]
            h.mesh_object("Pull polished contact", vertices,
                          faces if sign > 0 else [tuple(reversed(f)) for f in faces], m["wear"])
        for x in (-.41, .41):
            for y in (-.37, .41):
                rod("Captive face screw", (x, y, sign * .43), (x, y, sign * .446), .010, m["dark"], 8)


def panel(m, kind):
    # Profiled folded shell with a return lip; the active disk stays unobstructed.
    B("Wall gasket", (0, 0, -.067), (.98, .98, .016), m["dark"], .006)
    B("Folded control enclosure", (0, 0, -.018), (1, 1, .092), m["paint"], .022)
    B("Removable faceplate", (0, 0, .034), (.88, .91, .025), m["ivory"], .010)
    rod("Actuator sealing ring", (0, -.11, .047), (0, -.11, .069), .29, m["dark"], 48)
    # Flatten the ring vertically to match the elliptical existing actuator.
    obj = bpy.context.object
    for vertex in obj.data.vertices:
        vertex.co.y *= .74
    # Cylinders are horizontal in Blender; scale local radial Y, not world depth.
    obj.data.update()
    B("Status lens surround", (0, .30, .052), (.42, .115, .016), m["dark"], .006)
    if kind == "door":
        B("Reader slot", (0, .16, .052), (.48, .032, .015), m["dark"], .004)
        text("DOOR", (0, -.425, .050), .075, m["dark"])
    elif kind == "isolator":
        for x in (-.36, .36):
            B("Isolation guard", (x, -.10, .074), (.065, .47, .075), m["ochre"], .010)
        text("ISOLATE", (0, -.425, .050), .061, m["dark"])
        text("I / O", (0, .14, .050), .060, m["dark"])
    elif kind == "start":
        B("Startup legend", (0, .16, .052), (.46, .09, .015), m["dark"], .005)
        text("RUN", (0, .138, .062), .055, m["ivory"])
        text("START", (0, -.425, .050), .070, m["dark"])
        for x in (-.36, .36):
            for y in (-.24, -.16, -.08):
                B("Side ventilation", (x, y, .050), (.045, .018, .008), m["dark"], .002)
    else:
        B("Recessed instrument bezel", (0, .23, .050), (.72, .36, .018), m["dark"], .010)
        B("Printed analog scale", (0, .16, .061), (.61, .16, .008), m["ivory"], .006)
        for i in range(9):
            B("Scale division", (-.24 + i*.06, .17, .067), (.008, .035 if i%2 else .055, .003), m["dark"], 0)
        B("Measurement pointer", (.066, .14, .069), (.012, .085, .004), m["dark"], 0)
        text("MONITOR", (0, -.425, .050), .059, m["dark"])
    for x in (-.38, .38):
        for y in (-.40, .41):
            rod("Faceplate captive fastener", (x, y, .047), (x, y, .055), .022, m["steel"], 12)
            B("Screw slot", (x, y, .056), (.028, .006, .002), m["dark"], 0)


if __name__ == "__main__":
    for name in ("door_lift", "door_slide", "panel_door", "panel_isolator", "panel_start", "panel_instrument"):
        if SELECTED and name not in SELECTED:
            continue
        m = materials()
        if name.startswith("door_"):
            leaf(m, name == "door_lift")
        else:
            panel(m, name.removeprefix("panel_"))
        h.export(name)
