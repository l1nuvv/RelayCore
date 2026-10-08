"""VIS-12 pump sector finish; the authored solids own all collision.

Blender --background --factory-startup --disable-autoexec --threads 2 --python
scripts/export-foundation-pump-glb.py [-- --model=foundation_pump_machine]
Room origin is world zero; pump skid origin is [1.6, 0, 2.6].
"""
from pathlib import Path
from math import sin, cos, pi
import importlib.util
import sys
import tomllib

import bpy

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("power", ROOT / "scripts/export-power-restoration-glb.py")
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
h = p.h
# Reuse the accepted workshop textures byte-for-byte.
h.GRAPHICS = ROOT / "content/graphics/foundation_intake"
B, R, S = h.bay_box, p.rod, p.sign
SELECTED = {a.split("=", 1)[1] for a in sys.argv if a.startswith("--model=")}


def pipe(name, points, radius, mat):
    h.sweep(name, [(x, -z, y) for x, y, z in points], radius, mat, 24)


def flange(name, x, y, z, mat, axis="z", radius=.14):
    delta = {"x": (.035, 0, 0), "y": (0, .035, 0), "z": (0, 0, .035)}[axis]
    R(name, (x-delta[0], y-delta[1], z-delta[2]),
      (x+delta[0], y+delta[1], z+delta[2]), radius, mat, 48)


def machine(m):
    # A closed drive enclosure preserves the original full-height shelter.
    B("P-02 drive enclosure", (0, 1.1, 0), (1.624, 2.224, 1.624), m["blue"], .006)
    B("Skid plinth", (0, .09, 0), (1.65, .18, 1.65), m["dark"], .006)
    for x in (-.77, .77):
        B("Enclosure folded corner", (x, 1.15, -.824), (.055, 2.05, .035), m["steel"], .003)
    B("Drive service door", (0, 1.12, -.829), (1.36, 1.78, .026), m["paint"], .005)
    S("P-02 / PUMP DRIVE", (0, 1.91, -.857), 1.32, m)
    B("Service door latch", (.51, 1.15, -.86), (.045, .22, .035), m["dark"], .003)
    for y in (.30, .38, .46, .54):
        B("Drive intake louvre", (0, y, -.851), (1.12, .032, .012), m["dark"], .002)
    for side in (-1, 1):
        B("Enclosure side cover", (side*.824, 1.13, 0), (.026, 1.88, 1.36), m["paint"], .004)
        for z in (-.57, .57):
            for y in (.26, 1.98):
                R("Captive side screw", (side*.831, y, z), (side*.846, y, z), .018, m["dark"], 6)
    B("Pump mounting deck", (0, 2.24, 0), (1.64, .045, 1.64), m["dark"], .005)
    for z in (-.26, .43):
        B("Motor foot", (0, 2.33, z), (.58, .17, .13), m["steel"], .008)
    R("Motor end bell", (0, 2.54, .12), (0, 2.54, .60), .225, m["blue"], 64)
    for z in (.18, .25, .32, .39, .46, .53):
        R("Motor cooling flange", (0, 2.54, z), (0, 2.54, z+.022), .25, m["blue"], 64)
    R("Motor fan cover", (0, 2.54, .60), (0, 2.54, .67), .23, m["dark"], 64)
    B("Motor junction box", (.22, 2.66, .40), (.18, .20, .24), m["paint"], .008)
    R("Coupling guard", (0, 2.54, -.22), (0, 2.54, .12), .135, m["ochre"], 48)
    for z in (-.16, -.08, 0, .08):
        R("Coupling guard rib", (0, 2.54, z), (0, 2.54, z+.018), .15, m["dark"], 48)
    R("Cast pump volute", (0, 2.54, -.49), (0, 2.54, -.23), .29, m["blue"], 64)
    R("Pump cover ring", (0, 2.54, -.53), (0, 2.54, -.49), .31, m["steel"], 64)
    R("Pump inlet neck", (0, 2.54, -.73), (0, 2.54, -.52), .085, m["blue"], 48)
    flange("Suction flange", 0, 2.54, -.73, m["steel"])
    for i in range(8):
        x, y = .265*cos(i*pi/4), 2.54+.265*sin(i*pi/4)
        R("Pump cover bolt", (x, y, -.55), (x, y, -.52), .023, m["dark"], 6)
    R("Discharge neck", (.18, 2.62, -.33), (.18, 2.98, -.33), .075, m["blue"], 48)
    flange("Discharge flange", .18, 2.92, -.33, m["steel"], "y", .12)
    R("Gauge takeoff", (.18, 2.84, -.33), (.48, 2.84, -.33), .018, m["steel"], 24)
    R("Gauge rim", (.48, 2.84, -.42), (.48, 2.84, -.30), .10, m["dark"], 48)
    R("Gauge ivory dial", (.48, 2.84, -.426), (.48, 2.84, -.42), .086, m["ivory"], 48)
    R("Gauge needle", (.48, 2.84, -.431), (.51, 2.90, -.431), .005, m["dark"], 8)
    for i in range(9):
        angle = pi*.15+i*pi*.7/8
        a = (.48+.063*cos(angle), 2.84+.063*sin(angle), -.433)
        b = (.48+.075*cos(angle), 2.84+.075*sin(angle), -.433)
        R("Gauge scale tick", a, b, .003, m["dark"], 6)


def room(m):
    level = tomllib.loads((ROOT / "assets/levels/foundation_pump.toml").read_text(encoding="utf-8"))
    for group in level["solid_group"]:
        for solid in group["boxes"]:
            x, y, z = solid["origin"]
            w, height, d = solid["size"]
            if height != 3.4:
                continue
            B("Pump sealed plaster", (x, y, z), (w+.024, height, d+.024), m["wall"], .002)
            B("Pump protective enamel", (x, .55, z), (w+.036, 1.1, d+.036), m["paint"], .002)
            B("Folded dado cap", (x, 1.12, z), (w+.048, .035, d+.048), m["steel"], .003)
            B("Pump skirting", (x, .065, z), (w+.052, .13, d+.052), m["dark"], .003)
    B("Portal upper infill", (0, 3.15, 6), (2.04, .5, .278), m["wall"], .002)
    for x in (-1.04, 1.04):
        B("Exit folded jamb", (x, 1.45, 6), (.08, 2.9, .29), m["steel"], .004)
        for z in (5.835, 6.165):
            B("Exit impact shoe", (x, .24, z), (.08, .48, .04), m["dark"], .003)
    B("Exit lintel", (0, 2.96, 6), (2.16, .12, .29), m["steel"], .004)
    S("02 / SERVICE EXIT", (0, 3.16, 5.852), 2.3, m)
    S("02 / PUMP SECTOR", (0, 3.16, 6.148), 2.3, m, "+z")
    S("01 / WORKSHOP RETURN", (0, 2.5, -3.848), 2.85, m, "+z")
    S("SERVICE EXIT", (0, 1.70, 9.848), 2.20, m)
    S("02 / PUMP SERVICES", (2.848, 2.6, 4.5), 1.85, m, "-x")
    S("ISOLATE BEFORE SERVICE", (2.848, 2.14, 4.5), 2.05, m, "-x", "ochre")
    B("Ceiling field", (0, 3.365, 3), (5.74, .018, 13.74), m["ivory"], .001)
    for z in (-2, 1, 4, 6, 9):
        B("Ceiling cross tee", (0, 3.344, z), (5.74, .024, .035), m["steel"], .002)
    for x in (-1.8, 1.8):
        B("Ceiling runner", (x, 3.344, 3), (.035, .024, 13.74), m["steel"], .002)
    for z in (1, 7):
        B("Fluorescent back tray", (0, 3.02, z), (1.35, .07, .43), m["steel"], .004)
        B("Fluorescent opal diffuser", (0, 2.94, z), (1.11, .025, .28), m["ivory"], .003)
        for x in (-.63, .63):
            R("Fixture ceiling stem", (x, 3.055, z), (x, 3.375, z), .015, m["steel"], 16)
            B("Fixture end cap", (x, 2.97, z), (.09, .10, .39), m["paint"], .004)
        for dz in (-.19, .19):
            B("Fixture folded reflector", (0, 2.975, z+dz), (1.19, .09, .035), m["steel"], .003)
    # The existing left-wall solid stays closed behind the usable's own face.
    B("Startup cabinet folded shell", (-2.80, 1.0, 1.5), (.174, 2.024, 1.324), m["blue"], .004)
    B("Startup cabinet face", (-2.702, 1.0, 1.5), (.025, 1.86, 1.17), m["paint"], .003)
    S("P-02 / KEY START", (-2.679, 1.84, 1.5), 1.13, m, "+x")
    for z in (1.0, 2.0):
        R("Control door hinge", (-2.69, .55, z), (-2.69, .76, z), .023, m["steel"], 16)
    for y in (.30, .38, .46):
        B("Control ventilation slot", (-2.682, y, 1.5), (.012, .025, .83), m["dark"], .002)
    R("Startup conduit riser", (-2.72, 2.0, 1.5), (-2.72, 3.12, 1.5), .02, m["steel"], 24)
    pipe("Motor feed", [(-2.72, 3.12, 1.5), (-2.72, 3.12, 3.0),
                        (1.86, 3.12, 3.0), (1.86, 2.73, 3.0)], .02, m["dark"])
    # The mount embeds the turret's existing rear plate, leaving its front clear.
    B("Security support shell", (1.6, 1.2, 5.7644), (.824, 2.424, .424), m["blue"], .005)
    for x in (1.23, 1.97):
        B("Security support edge", (x, 1.2, 5.54), (.05, 2.35, .035), m["steel"], .003)
    S("SECURITY / 02", (1.6, 2.15, 5.536), .85, m)
    S("EXIT RELEASE", (-1.66, 2.10, 5.852), 1.48, m)
    R("Exit reader conduit", (-1.4, 1.54, 5.85), (-1.4, 2.66, 5.85), .018, m["steel"], 24)
    R("Reader feed to jamb", (-1.4, 2.66, 5.85), (-1.04, 2.66, 5.85), .018, m["steel"], 24)
    # Suction joins the existing wall utility at y=2.5; bends have real radius.
    points = [(1.6, 2.54, 1.87), (1.6, 2.54, 1.69)]
    points += [(1.6-.18*(1-cos(i*pi/16)), 2.54, 1.69-.18*sin(i*pi/16)) for i in range(1, 9)]
    points += [(-2.60, 2.54, 1.51)]
    pipe("P-02 suction branch", points, .075, m["steel"])
    flange("Utility takeoff sleeve", -2.60, 2.54, 1.51, m["dark"], "x", .13)
    points = [(1.78, 2.98, 2.27)]
    points += [(1.78-.15*(1-cos(i*pi/16)), 2.98+.15*sin(i*pi/16), 2.27) for i in range(1, 9)]
    points += [(1.63, 3.13, 4.45), (1.63, 3.13, 5.90)]
    pipe("P-02 discharge header", points, .075, m["steel"])
    for z in (2.27, 4.45):
        R("Header ceiling hanger", (1.63, 3.13, z), (1.63, 3.37, z), .018, m["steel"], 16)
    flange("Partition pipe sleeve", 1.63, 3.13, 5.86, m["dark"], "z", .13)


if __name__ == "__main__":
    for name, build in (("foundation_pump_machine", machine), ("foundation_pump_room", room)):
        if SELECTED and name not in SELECTED:
            continue
        m = p.materials()
        build(m)
        h.export(name)
