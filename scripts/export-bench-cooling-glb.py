"""VIS-08/09 render skins in runtime metres; existing map collision owns play.

Blender --background --factory-startup --disable-autoexec --threads 2 --python
scripts/export-bench-cooling-glb.py [-- --model=cooling_exchanger]
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
h.GRAPHICS = ROOT / "content/graphics/bench_cooling"
h.GRAPHICS.mkdir(parents=True, exist_ok=True)
B, R, S = h.bay_box, p.rod, p.sign
SELECTED = {a.split("=", 1)[1] for a in sys.argv if a.startswith("--model=")}


def bench(m):
    level = tomllib.loads((ROOT / "assets/levels/bench_relay.toml").read_text(encoding="utf-8"))
    for group in level["solid_group"]:
        for solid in group["boxes"]:
            x, y, z = solid["origin"]
            w, height, d = solid["size"]
            if height != 3.4:
                continue
            B("Workshop sealed plaster", (x, y, z), (w+.026, height, d+.026), m["wall"], .002)
            B("Workshop enamel dado", (x, .55, z), (w+.040, 1.1, d+.040), m["paint"], .002)
            B("Dado folded cap", (x, 1.12, z), (w+.054, .035, d+.054), m["steel"], .003)
            B("Workshop skirting", (x, .065, z), (w+.058, .13, d+.058), m["dark"], .003)
    for z in (1, 12.5):
        for x in (-1.55, 1.55):
            B("Gate folded jamb", (x, 1.30, z), (.095, 2.60, .29), m["steel"])
        B("Gate lintel", (0, 2.65, z), (3.2, .10, .29), m["paint"])
        for side in (-1, 1):
            S("01 / REPAIR BAY" if z == 1 else "SERVICE / EXIT", (0, 2.94, z+side*.16), 2.8, m,
              "+z" if side > 0 else "-z")
    for z in (-1.5, 5.5, 10.5):
        B("Fluorescent mounting tray", (0, 3.365, z), (2.02, .06, .48), m["steel"])
        for x in (-.97, .97):
            B("Fluorescent end cap", (x, 3.30, z), (.10, .10, .43), m["paint"])
        for dz in (-.205, .205):
            B("Fluorescent folded edge", (0, 3.30, z+dz), (1.90, .08, .045), m["steel"])
    for z in (-2, 2.5, 6.5, 10, 14):
        B("Ceiling cross tee", (0, 3.388, z), (7.78, .022, .035), m["steel"], .002)
    for x in (-2, 2):
        B("Ceiling runner", (x, 3.388, 5.5), (.035, .022, 18.78), m["steel"], .002)
    p.cabinet(-2.6, 4, 1.4, .8, 2.1, m, "B-01 / TOOL STORE")
    S("RELAY WORKSHOP", (0, 2.5, -3.87), 2.7, m, "+z")
    S("RELAY / 02", (-3.865, 2.12, 11.2), 1.55, m, "+x")
    S("BAY RELEASE", (2.3, 1.99, .84), 1.4, m)
    S("EXIT RELEASE", (2.3, 1.99, 12.34), 1.4, m)
    # The two lab_bench models and every part of their opening remain untouched.
    S("BENCH / LOW CLEARANCE", (3.87, 2.1, 7), 2.4, m, "-x")
    R("Relay conduit", (-3.84, 1.7, 11.2), (-3.84, 3.1, 11.2), .022, m["steel"])
    R("Relay overhead conduit", (-3.84, 3.1, 11.2), (-3.84, 3.1, 1), .022, m["steel"])


def exchanger(m):
    # One continuous insulated casing replaces the three stretched cabinet props.
    # All fittings stay close to the original 3.2 x 2.2 x 10 cover envelope.
    B("CH-01 insulated shell", (0, 1.1, 13), (3.23, 2.22, 10.03), m["blue"])
    for x in (-1.55, 1.55):
        B("Continuous skid rail", (x, .13, 13), (.16, .26, 10.08), m["dark"])
    for z in (8.15, 10.5, 13, 15.5, 17.85):
        B("Skid cross member", (0, .10, z), (3.25, .20, .13), m["steel"])
    for side in (-1, 1):
        x = side*1.635
        for i, z in enumerate((9.25, 11.75, 14.25, 16.75)):
            B("Removable fin cassette", (x, 1.14, z), (.025, 1.48, 2.32), m["steel"])
            B("Recessed heat exchanger field", (x+side*.018, 1.18, z), (.018, 1.15, 2.05), m["dark"], .002)
            for dz in range(18):
                B("Folded radiator fin", (x+side*.033, 1.18, z-.97+dz*.114), (.035, 1.10, .026), m["steel"], .002)
            for edge in (-1, 1):
                B("Cassette retaining rail", (x+side*.028, 1.14, z+edge*1.12), (.035, 1.44, .035), m["paint"])
            S("HX / %02d" % (i+1), (x+side*.055, .40, z), .72, m, "+x" if side > 0 else "-x")
        R("Insulated longitudinal header", (side*1.57, 2.04, 8.12), (side*1.57, 2.04, 17.88), .105, m["dark"], 40)
        for z in (8.2, 10.5, 13, 15.5, 17.8):
            B("Header saddle", (side*1.54, 1.95, z), (.23, .24, .10), m["steel"])
        S("CH-01 / PROCESS COOLING", (side*1.662, 1.91, 13), 2.7, m, "+x" if side > 0 else "-x")
    for end in (-1, 1):
        z = 13+end*5.04
        B("End service bulkhead", (0, 1.14, z), (3.18, 1.97, .035), m["paint"])
        for x in (-.77, .77):
            R("Tube sheet gasket", (x, 1.14, z), (x, 1.14, z+end*.018), .63, m["dark"], 64)
            R("Tube sheet inspection cover", (x, 1.14, z+end*.019), (x, 1.14, z+end*.045), .59, m["steel"], 64)
            for i in range(12):
                a = 2*pi*i/12
                bx, by = x+.54*cos(a), 1.14+.54*sin(a)
                R("Captive end bolt", (bx, by, z+end*.045), (bx, by, z+end*.06), .018, m["dark"], 8)
            R("End water connection", (x, 1.14, z+end*.045), (x, 1.14, z+end*.08), .14, m["blue"])
            side = 1 if x > 0 else -1
            points = [(x, 1.14, 13+end*5.085), (x, 1.14, 13+end*5.12)]
            points += [(x, 1.14+.12*(1-cos(i*pi/16)), 13+end*(5.12+.12*sin(i*pi/16)))
                       for i in range(1, 9)]
            points.append((x, 1.84, 13+end*5.24))
            points += [(x+side*.2*(1-cos(i*pi/16)), 1.84+.2*sin(i*pi/16), 13+end*5.24)
                       for i in range(1, 9)]
            points.append((side*1.37, 2.04, 13+end*5.24))
            points += [(side*(1.37+.2*sin(i*pi/16)), 2.04, 13+end*(5.24-.2*(1-cos(i*pi/16))))
                       for i in range(1, 9)]
            points.append((side*1.57, 2.04, 13+end*4.88))
            h.sweep("Continuous flanged water return", [(a, -c, b) for a, b, c in points], .075, m["steel"], 32)
        facing = "+z" if end > 0 else "-z"
        S("CH-01 / COOLING", (0, 1.94, z+end*.04), 1.32, m, facing)
        S("ISOLATE BEFORE SERVICE", (0, .36, z+end*.04), 2.4, m, facing)
    # Top covers have explicit seams and lifting eyes, without raising the cover.
    for z in (9.25, 11.75, 14.25, 16.75):
        B("Insulated roof cassette", (0, 2.221, z), (2.8, .018, 2.40), m["blue"], .002)


def service(m):
    # Fitted finish sits just inside the four original room walls.
    for axis, coordinate in (("x", -6.888), ("x", 6.888), ("z", .112), ("z", 29.888)):
        length = 29.8 if axis == "x" else 13.8
        position = lambda y: (coordinate, y, 15) if axis == "x" else (0, y, coordinate)
        size = lambda depth, height: (depth, height, length) if axis == "x" else (length, height, depth)
        B("Cooling sealed plaster", position(2.45), size(.024, 2.70), m["wall"], .001)
        B("Cooling protective enamel", position(.55), size(.030, 1.10), m["paint"], .002)
        B("Cooling folded dado cap", position(1.115), size(.042, .030), m["steel"], .002)
        B("Cooling coved skirting", position(.075), size(.048, .15), m["dark"], .003)
    # Supply and return bridge above both aisles to wall sleeves. Clearance below
    # the pipes exceeds the existing player/rover envelopes and sightlines.
    for side, height, text in ((-1, 3.18, "SUPPLY"), (1, 3.48, "RETURN")):
        x, z = side*1.17, 18.24
        points = [(x, -z, 2.04), (x, -z, height-.20)]
        points += [(x+side*.20*(1-cos(i*pi/16)), -z, height-.20+.20*sin(i*pi/16))
                   for i in range(1, 9)]
        points.append((side*6.85, -z, height))
        h.sweep(text + " overhead water feed", points, .075, m["steel"], 32)
        R(text + " branch union", (x, 2.02, z), (x, 2.12, z), .11, m["steel"])
        R(text + " wall sleeve", (side*6.83, height, z), (side*6.90, height, z), .13, m["dark"])
        for sx in (side*2.6, side*5.4):
            R("Pipe ceiling hanger", (sx, height+.075, z), (sx, 3.80, z), .015, m["dark"], 12)
            B("Pipe saddle", (sx, height, z), (.07, .19, .19), m["paint"])
        S(text, (side*1.95, height-.24, z-.02), .85, m)
    B("Service desk laminate", (-4.5, 1.033, 3.5), (1.94, .006, .94), m["ivory"], .001)
    B("Desk enamel housing", (-4.5, .49, 3.5), (1.83, .98, .83), m["paint"])
    for y in (.28, .54):
        B("Desk drawer front", (-4.5, y, 3.06), (1.67, .20, .025), m["steel"])
        B("Recessed drawer pull", (-4.5, y+.03, 3.04), (.25, .035, .025), m["dark"])
    S("CH-01 / SERVICE", (-4.5, .85, 3.028), 1.5, m)
    B("Isolator enclosure", (-6.80, 1.10, 18.5), (.225, 2.22, 1.53), m["paint"])
    B("Isolator removable panel", (-6.675, 1.25, 18.5), (.025, 1.65, 1.32), m["steel"])
    # Terminal face is at x=-6.555; keep the new plate behind it.
    S("01 / ISOLATE", (-6.65, 2.02, 18.5), 1.25, m, "+x")
    for z in (17.9, 19.1):
        for y in (.5, 1.90):
            R("Isolator captive screw", (-6.65, y, z), (-6.63, y, z), .018, m["dark"], 8)
    R("Isolator electrical drop", (-6.78, 2.22, 18.5), (-6.78, 3.35, 18.5), .026, m["dark"])
    R("Isolator cable return", (-6.78, 3.35, 18.5), (-6.78, 3.35, 13), .026, m["dark"])
    # Thin opaque replacement plates cover the shared props' pressure lettering;
    # the original cabinet/manifold meshes remain reusable on pressure_transfer.
    B("Coolant header identification", (-6.770, 3.35, 23), (.012, .26, 3.5), m["ivory"], .001)
    # Bring the lettering ahead of the receiver tops instead of hiding it behind
    # them at an ordinary standing eye height. The old nameplate stays covered.
    B("Coolant header stand-off fascia", (-6.120, 3.43, 23), (.035, .26, 3.5), m["ivory"], .002)
    for z in (21.36, 24.64):
        R("Header sign bracket", (-6.85, 3.43, z), (-6.137, 3.43, z), .025, m["steel"], 16)
    h.bay_label("COOLANT SUPPLY / RETURN", (-6.097, 3.37, 23), .16, m["dark"], "+x")
    B("Auxiliary cooling identification", (4.3, 1.86, 11.080), (1.7, .20, .012), m["ivory"], .001)
    h.bay_label("COOLING AUXILIARY", (4.3, 1.81, 11.091), .11, m["dark"], "+z")
    B("Cooling start identification", (6.332, 1.82, 23), (.012, .20, 1.66), m["ivory"], .001)
    h.bay_label("02 / COOLING START", (6.321, 1.77, 23), .11, m["dark"], "-x")
    S("03 / EXIT RELEASE", (3, 2.10, 25.84), 1.85, m)


if __name__ == "__main__":
    for name, build in (("bench_workshop", bench), ("cooling_exchanger", exchanger), ("cooling_service", service)):
        if SELECTED and name not in SELECTED:
            continue
        m = p.materials()
        build(m)
        h.export(name)
