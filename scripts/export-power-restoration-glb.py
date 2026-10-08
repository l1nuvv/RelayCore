"""Reproducible visual shells; authored power solids remain the collision owner.

Blender --background --factory-startup --disable-autoexec --threads 2 --python
scripts/export-power-restoration-glb.py [-- --model=power_intake]
Runtime coordinates are used below; all six placements have origin [0,0,0].
"""
from pathlib import Path
from math import pi, sin, cos
import importlib.util
import sys
import tomllib

import bpy

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("transfer", ROOT / "scripts/export-pressure-transfer-glb.py")
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
h.GRAPHICS = ROOT / "content/graphics/power_restoration"
h.GRAPHICS.mkdir(parents=True, exist_ok=True)
B, T = h.bay_box, h.bay_label
SELECTED = {a.split("=", 1)[1] for a in sys.argv if a.startswith("--model=")}
LEVEL = tomllib.loads((ROOT / "assets/levels/power_restoration.toml").read_text(encoding="utf-8"))


def rod(name, a, b, radius, mat, sides=32):
    return h.rod(name, (a[0], -a[2], a[1]), (b[0], -b[2], b[1]), radius, mat, sides)


def materials():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = "METRIC"
    # The shared texture writer stores palette values as sRGB PNG bytes;
    # untextured Principled factors below are linear.
    return {
        "wall": h.material("power_plaster", (.48, .47, .425), .94,
                           texture="content/third_party/polyhaven/pressure_transfer/concrete_wall_001_diff_1k.jpg"),
        "paint": h.material("power_sage_enamel", (.42, .48, .44), .72,
                            sheet="content/graphics/prop_materials_source.png", cell=0),
        "steel": h.material("power_galvanized", (.55, .58, .57), .60, .65,
                            sheet="content/graphics/prop_materials_source.png", cell=7),
        "dark": h.material("power_blackened_steel", (.045, .055, .052), .78, .25),
        "ivory": h.material("power_instrument_ivory", (.70, .67, .56), .79),
        "blue": h.material("power_pump_bluegray", (.38, .47, .50), .68,
                           sheet="content/graphics/prop_materials_source.png", cell=0),
        "ochre": h.material("power_isolation_ochre", (.48, .29, .08), .75),
        "red": h.material("power_valve_red", (.34, .065, .04), .68),
    }


def zone(x, z):
    if x < 5.1 and z < 5.1:
        return "intake"
    if x < 5.1 and z < 23:
        return "service"
    if z >= 23 and x < 10:
        return "filtration"
    if z >= 23:
        return "maintenance"
    if z >= 15 and x >= 9:
        return "switchgear"
    if 5 <= x <= 11 and 5 <= z <= 11:
        return "pump"
    return "service"


def architecture(which, m):
    # Skin existing wall segments, including both sides of portal returns.
    # Floors retain the authored, different service/maintenance/filter materials.
    for group in LEVEL["solid_group"]:
        for solid in group["boxes"]:
            x, y, z = solid["origin"]
            w, height, d = solid["size"]
            if zone(x, z) != which:
                continue
            material = solid["material"].lower()
            if height == 3.6 and material in ("concrete", "wallpanel", "filtration_wall"):
                B("Sealed plaster wall", (x, y, z), (w+.026, height, d+.026), m["wall"], .002)
            elif height == 1.1 and material == "wallpanel":
                B("Enamel dado", (x, y, z), (w+.036, height, d+.036), m["paint"], .002)
                B("Dado cap", (x, 1.115, z), (w+.048, .035, d+.048), m["steel"], .003)
                B("Coved skirting", (x, .075, z), (w+.055, .15, d+.055), m["dark"], .004)


def sign(text, pos, width, m, facing="-z", accent="ivory"):
    axis = 0 if facing.endswith("x") else 2
    dims = [.035, .36, .035]
    dims[2 if axis == 0 else 0] = width
    B(text + " plate", pos, dims, m[accent])
    surface = list(pos)
    surface[axis] += .022 * (1 if facing.startswith("+") else -1)
    T(text, (surface[0], surface[1]-.065, surface[2]), .13, m["dark"], facing)


def portal(x, z, width, m, axis="z", title=None):
    for side in (-1, 1):
        pos = (x+side*(width/2+.05), 1.45, z) if axis == "z" else (x, 1.45, z+side*(width/2+.05))
        B("Portal folded jamb", pos, (.10, 2.9, .28) if axis == "z" else (.28, 2.9, .10), m["steel"])
    B("Portal lintel", (x, 2.96, z), (width+.2, .13, .28) if axis == "z" else (.28, .13, width+.2), m["paint"])
    if title:
        for side in (-1, 1):
            pos = (x, 3.27, z+side*.16) if axis == "z" else (x+side*.16, 3.27, z)
            sign(title, pos, width-.15, m, ("+z" if side > 0 else "-z") if axis == "z" else ("+x" if side > 0 else "-x"))


def cabinet(x, z, width, depth, height, m, title):
    B("Switchboard folded housing", (x, height/2, z), (width+.03, height+.02, depth+.025), m["paint"])
    B("Switchboard plinth", (x, .09, z), (width+.045, .18, depth+.045), m["dark"])
    front = z-depth/2-.055
    B("Removable access panel", (x, 1.43, front), (width-.15, height-.46, .025), m["steel"])
    for dx in (-width/2+.09, width/2-.09):
        B("Folded vertical rail", (x+dx, height/2, front-.025), (.035, height-.15, .035), m["dark"])
    for y in (.32, .40, .48):
        B("Ventilation slot", (x, y, front-.018), (width-.30, .025, .012), m["dark"], .002)
    B("Service latch", (x+width*.27, 1.12, front-.035), (.045, .20, .045), m["dark"])
    sign(title, (x, height-.20, front-.027), width-.14, m)
    for side in (-1, 1):
        side_x = x+side*(width/2+.025)
        B("Side access panel", (side_x, height*.53, z), (.025, height-.38, depth-.16), m["paint"])
        for y in (.30, .38, .46, .54):
            B("Side cooling slot", (side_x+side*.016, y, z), (.012, .024, depth-.34), m["dark"], .002)
        for dz in (-depth/2+.13, depth/2-.13):
            for y in (.18, height-.18):
                rod("Side panel captive screw", (side_x, y, z+dz), (side_x+side*.025, y, z+dz), .017, m["steel"], 8)
    B("Rear service cover", (x, height/2, z+depth/2+.028), (width-.12, height-.28, .025), m["paint"])


def intake(m):
    # The new laminate and stretcher frame wrap the original fuse table.
    B("Fuse bench laminate", (-3.6, .929, -.5), (1.73, .122, 1.13), m["ivory"])
    for x in (-4.2, -3):
        for z in (-.9, -.1):
            B("Bench square steel leg", (x, .43, z), (.115, .86, .115), m["steel"])
        B("Bench side stretcher", (x, .25, -.5), (.09, .07, .80), m["steel"])
    B("Bench back stretcher", (-3.6, .25, -.9), (1.2, .07, .07), m["steel"])
    B("Fuse parts tray", (-3.6, .985, -.5), (.64, .018, .46), m["dark"], .004)
    sign("SPARE FUSE / F-01", (-3.6, .87, .078), 1.38, m, "+z")
    cabinet(-4.2, 3.9, .9, 1.3, 2.8, m, "INTAKE / A-01")
    portal(0, 4.8, 3.0, m, title="SECTOR AIRLOCK")
    portal(5, 0, 2.8, m, "x", "01 / SERVICE")
    sign("COOLING RESEARCH / INTAKE", (0, 2.72, -4.82), 3.25, m, "+z")
    sign("AIRLOCK RELEASE", (2.1, 1.99, 4.78), 1.4, m)
    rod("Console cable riser", (-4.75, .45, 2.5), (-4.75, 3.22, 2.5), .025, m["steel"])
    rod("Console overhead feed", (-4.75, 3.22, 2.5), (-4.75, 3.22, -4.8), .025, m["steel"])


def service(m):
    for z in (3, 13):
        B("Corridor ceiling rib", (12.5, 3.32, z), (2.72, .09, .24), m["steel"])
    for z in (2, 5, 11, 14):
        B("Cable tray saddle", (13.77, 3.12, z), (.18, .16, .08), m["steel"])
    for x in (13.65, 13.80):
        rod("Overhead cable tray rail", (x, 3.13, 1), (x, 3.13, 14.8), .028, m["dark"], 12)
    portal(12.5, 15, 3, m, title="02 / SWITCHGEAR")
    sign("< PUMP / SWITCHGEAR >", (13.81, 2.45, 8), 2.42, m, "-x")
    sign("INTAKE / AIRLOCK  >", (11.18, 2.25, 2.1), 2.18, m, "+x")
    portal(3.5, 5, 2, m, title="RETURN / INTAKE")
    for z in (6.5, 9.5):
        B("Return passage ceiling rib", (3.5, 3.35, z), (2.7, .10, .16), m["steel"])
    sign("INTAKE  >", (2.16, 2.2, 8), 1.65, m, "+x")


def switchgear(m):
    for i, x in enumerate((10, 11.4, 14.3, 15.7)):
        cabinet(x, 22.3, 1.15, .9, 2.8, m, "SW-%02d" % (i+1))
    sign("02 / SECTOR DISTRIBUTION", (13, 3.15, 22.82), 3.4, m)
    sign("RESERVE BUS  >", (16.82, 2.3, 20.7), 1.8, m, "-x")
    sign("MAIN BREAKER", (14.3, 2.18, 21.77), 1.03, m)
    for x in (10, 11.4, 14.3, 15.7):
        rod("Switchboard cable feed", (x, 2.84, 22.3), (x, 3.32, 22.3), .04, m["dark"])
    B("Switchboard cable trunk", (13, 3.33, 22.3), (6.65, .16, .25), m["steel"])
    portal(13, 23, 3, m, title="03 / MAINTENANCE")


def maintenance(m):
    portal(13, 31, 3, m, title="03 / MAINTENANCE")
    portal(17, 34, 3, m, "x", "RESERVE BUS")
    sign("RETURN / SWITCHGEAR", (11.69, 2.28, 27), 2.5, m, "+x")
    sign("MAINTENANCE ISOLATOR", (15.5, 2.5, 38.60), 2.2, m)
    B("Isolator enclosure rim", (15.5, 1.45, 38.675), (1.40, 1.68, .055), m["steel"])
    sign("RESERVE / BUS COUPLER", (24.81, 2.18, 34), 2.1, m, "-x")
    # Wall-mounted bus trunk and feeder terminate at the existing relay housing.
    B("Bus trunk casing", (24.80, 2.85, 34), (.16, .24, 4.2), m["ochre"])
    rod("Bus relay feeder", (24.80, 2.74, 34), (24.80, 1.85, 34), .038, m["dark"])
    for z in (32.4, 35.6):
        B("Bus wall bracket", (24.86, 2.75, z), (.15, .4, .10), m["steel"])


def filtration(m):
    portal(7.5, 23, 3, m, title="04 / FILTRATION")
    # Preserve the solid tank's full cover envelope, then explain its servicing.
    B("Filter folded housing", (5, .75, 27), (1.63, 1.52, 1.63), m["blue"])
    B("Filter lid gasket", (5, 1.52, 27), (1.64, .04, 1.64), m["dark"])
    B("Filter inspection lid", (5, 1.665, 27), (1.23, .31, 1.23), m["steel"])
    for x in (4.55, 5.45):
        B("Lid lifting handle", (x, 1.88, 27), (.06, .14, .36), m["dark"])
    rod("Filter supply", (5, 1.2, 27.82), (5, 1.2, 30.85), .09, m["steel"])
    rod("Filter return", (4.17, .55, 27), (1.64, .55, 27), .09, m["steel"])
    sign("F-04 / FILTER", (5, 1.1, 26.16), 1.3, m)
    sign("FILTER PURGE", (3, 2.42, 30.72), 1.7, m)
    sign("RETURN / PUMP", (1.69, 2.32, 25.4), 2, m, "+x")
    rod("Spill riser", (6.16, 1.8, 18), (6.16, 2.82, 18), .065, m["steel"])
    rod("Spill header takeoff", (6.16, 2.82, 18), (7.5, 2.82, 18), .065, m["steel"])
    rod("Spill outlet", (6.16, 1.8, 18), (6.59, 1.8, 18), .065, m["steel"])
    B("Spill flange backing", (6.16, 1.8, 18), (.06, .24, .24), m["dark"])


def pump(m):
    # Existing 1.4m square solid is a drive enclosure; the visible motor/pump
    # rides on its top, avoiding any change to the player's cover or passage.
    cabinet(6.4, 9.5, 1.4, 1.4, 1.8, m, "P-02 / DRIVE")
    B("Equipment skid", (6.4, 1.83, 9.5), (1.50, .06, 1.50), m["dark"])
    for x in (5.96, 6.84):
        B("Pump mounting foot", (x, 1.96, 9.45), (.16, .22, .76), m["steel"])
    rod("Finned motor casing", (6.4, 2.19, 9.47), (6.4, 2.19, 10.02), .25, m["blue"], 48)
    for z in (9.52, 9.60, 9.68, 9.76, 9.84, 9.92):
        rod("Motor cooling fin", (6.4, 2.19, z), (6.4, 2.19, z+.022), .28, m["blue"], 48)
    B("Motor terminal box", (6.4, 2.47, 9.80), (.26, .16, .26), m["paint"])
    rod("Coupling guard", (6.4, 2.19, 9.12), (6.4, 2.19, 9.47), .15, m["ochre"])
    rod("Pump casing", (6.4, 2.19, 8.94), (6.4, 2.19, 9.13), .34, m["blue"], 48)
    rod("Pump bolted cover", (6.4, 2.19, 8.90), (6.4, 2.19, 8.94), .36, m["steel"], 48)
    for i in range(8):
        x, y = 6.4+.295*cos(i*pi/4), 2.19+.295*sin(i*pi/4)
        rod("Casing bolt", (x, y, 8.88), (x, y, 8.91), .026, m["dark"], 6)
    rod("Pump suction", (6.4, 2.19, 8.90), (6.4, 2.19, 8.65), .10, m["steel"])
    # A swept elbow, not a diagonal intersection, connects the outlet to wall feed.
    points = [(6.4, -8.65, 2.19), (6.4, -8.48, 2.19)]
    points += [(6.4-.18*(1-cos(i*pi/16)), -(8.48-.18*sin(i*pi/16)), 2.19) for i in range(1, 9)]
    points += [(5.31, -8.30, 2.19), (5.31, -7.2, 2.19), (5.31, -7.2, 2.40), (5.4, -7.2, 2.40)]
    h.sweep("Suction pipe and leaking service joint", points, .075, m["steel"], 24)
    rod("Discharge riser", (6.4, 2.48, 9.03), (6.4, 3.05, 9.03), .085, m["steel"])
    rod("Discharge header", (5.09, 3.05, 9.03), (6.4, 3.05, 9.03), .085, m["steel"])
    rod("Discharge wall sleeve", (5.09, 3.05, 9.03), (5.15, 3.05, 9.03), .13, m["dark"])
    rod("Pressure gauge stem", (6.4, 2.69, 9.03), (6.74, 2.69, 9.03), .025, m["steel"])
    rod("Pressure gauge bezel", (6.73, 2.69, 9.03), (6.82, 2.69, 9.03), .125, m["dark"], 48)
    rod("Pressure gauge face", (6.82, 2.69, 9.03), (6.824, 2.69, 9.03), .107, m["ivory"], 48)
    rod("Pressure gauge needle", (6.83, 2.65, 9.07), (6.83, 2.75, 8.98), .006, m["dark"], 8)
    rod("Leaking union", (5.37, 2.4, 7.2), (5.45, 2.4, 7.2), .13, m["dark"])
    B("Cooling control backplate", (7, 1.42, 5.42), (1.14, 1.40, .045), m["steel"])
    sign("P-02 / COOLING START", (7, 2.43, 5.44), 2.15, m, "+z")
    rod("Control cable", (7, 2.8, 5.32), (7, 3.15, 5.32), .025, m["dark"])
    rod("Motor overhead feed", (6.4, 3.15, 5.32), (6.4, 3.15, 9.8), .025, m["dark"])
    rod("Motor feed drop", (6.4, 3.15, 9.8), (6.4, 2.55, 9.8), .025, m["dark"])
    rod("Control feed elbow", (7, 3.15, 5.32), (6.4, 3.15, 5.32), .025, m["dark"])
    portal(11, 8, 2, m, "x", "P-02 / PUMP")
    sign("FILTER / 04  >", (8, 2.4, 10.81), 2, m)


if __name__ == "__main__":
    for name, build in (("intake", intake), ("service", service), ("switchgear", switchgear),
                        ("maintenance", maintenance), ("filtration", filtration), ("pump", pump)):
        product = "power_" + name
        if SELECTED and product not in SELECTED:
            continue
        m = materials()
        architecture(name, m)
        build(m)
        h.export(product)
