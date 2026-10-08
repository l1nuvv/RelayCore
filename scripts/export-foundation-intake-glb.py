"""VIS-11 fitted intake finish and pickup key; runtime coordinates in metres.

Blender --background --factory-startup --disable-autoexec --threads 2 --python
scripts/export-foundation-intake-glb.py [-- --model=foundation_intake]
The key uses normalized pickup bounds, as the existing pickup mesh does.
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
h.GRAPHICS = ROOT / "content/graphics/foundation_intake"
h.GRAPHICS.mkdir(parents=True, exist_ok=True)
B, R, S = h.bay_box, p.rod, p.sign
SELECTED = {a.split("=", 1)[1] for a in sys.argv if a.startswith("--model=")}


def intake(m):
    level = tomllib.loads((ROOT / "assets/levels/foundation_intake.toml").read_text(encoding="utf-8"))
    for group in level["solid_group"]:
        for solid in group["boxes"]:
            x, y, z = solid["origin"]
            w, height, d = solid["size"]
            if height != 3.4:
                continue
            B("Intake sealed plaster", (x, y, z), (w+.024, height, d+.024), m["wall"], .002)
            B("Intake protective enamel", (x, .55, z), (w+.036, 1.1, d+.036), m["paint"], .002)
            B("Folded dado cap", (x, 1.12, z), (w+.048, .035, d+.048), m["steel"], .003)
            B("Intake skirting", (x, .065, z), (w+.052, .13, d+.052), m["dark"], .003)
    B("Portal upper infill", (0, 3.15, 3), (2.04, .5, .278), m["wall"], .002)
    for x in (-1.04, 1.04):
        B("Portal folded jamb", (x, 1.45, 3), (.08, 2.9, .29), m["steel"], .004)
        B("Portal impact shoe", (x, .24, 2.835), (.08, .48, .04), m["dark"], .003)
    B("Portal lintel", (0, 2.95, 3), (2.16, .1, .29), m["steel"], .004)
    S("02 / PUMP SECTOR", (0, 3.16, 2.83), 2.20, m)
    S("01 / INTAKE WORKSHOP", (0, 3.16, 3.17), 2.75, m, "+z")
    S("SERVICE KEY", (1.80, 1.88, 2.83), 1.22, m)
    S("01 / INTAKE WORKSHOP", (0, 2.47, -3.848), 3.30, m, "+z")
    S("PUMPS / 02", (0, 2.15, 5.848), 2.20, m)
    R("Pump forward arrow stem", (0, 2.40, 5.822), (0, 2.65, 5.822), .022, m["ochre"], 12)
    for x in (-.10, .10):
        R("Pump forward arrow head", (x, 2.55, 5.822), (0, 2.65, 5.822), .022, m["ochre"], 12)
    B("Ceiling field", (0, 3.365, 1), (7.74, .018, 9.74), m["ivory"], .001)
    for z in (-2, 1, 3, 5):
        B("Ceiling cross tee", (0, 3.344, z), (7.74, .024, .035), m["steel"], .002)
    for x in (-2, 2):
        B("Ceiling runner", (x, 3.344, 1), (.035, .024, 9.74), m["steel"], .002)
    for z, width in ((0, 1.8), (4.5, 1.35)):
        B("Ceiling fluorescent back tray", (0, 3.30, z), (width, .07, .43), m["steel"], .004)
        B("Fluorescent opal diffuser", (0, 3.22, z), (width-.24, .025, .28), m["ivory"], .003)
        for x in (-width/2+.045, width/2-.045):
            R("Fixture rigid ceiling stem", (x, 3.335, z), (x, 3.375, z), .015, m["steel"], 16)
            B("Fixture porcelain end cap", (x, 3.25, z), (.09, .10, .39), m["paint"], .004)
        for dz in (-.19, .19):
            B("Fixture folded reflector", (0, 3.255, z+dz), (width-.16, .09, .035), m["steel"], .003)
    # Existing closed solid remains authoritative; all handles stay near its face.
    B("Issue cabinet shell", (-2, .45, 0), (1.52, .90, 1.02), m["blue"], .007)
    B("Issue worktop edge", (-2, .918, 0), (1.56, .036, 1.06), m["dark"], .004)
    B("Issue worktop laminate", (-2, .938, 0), (1.54, .004, 1.04), m["ivory"], .0005)
    for y in (.24, .51, .75):
        B("Tool drawer", (-2, y, -.522), (1.34, .19, .024), m["paint"], .003)
        B("Drawer recessed pull", (-2, y+.025, -.540), (.28, .034, .014), m["dark"], .002)
    for x in (-2.64, -1.36):
        B("Cabinet plinth foot", (x, .06, -.42), (.10, .12, .14), m["dark"], .003)
    B("Key issue tray", (-2, .942, 0), (.49, .004, .38), m["ochre"], .0005)
    for x in (-2.58, -1.42):
        B("Bench rear upright", (x, 1.58, .53), (.035, 1.30, .035), m["steel"], .003)
    B("Tool board folded backing", (-2, 1.65, .53), (1.43, .85, .035), m["dark"], .004)
    B("Tool board face", (-2, 1.65, .505), (1.37, .79, .018), m["blue"], .002)
    S("KEY / SECTOR 02", (-2, 2.25, .49), 1.43, m)
    for x, length in ((-2.52, .38), (-2.29, .32), (-2.06, .26)):
        y, z = 1.84, .464
        points = [(x+.055*cos(i*2*pi/32), y+.055*sin(i*2*pi/32), z) for i in range(33)]
        h.sweep("Ring spanner head", [(px, -pz, py) for px, py, pz in points], .014, m["steel"], 8)
        B("Ring spanner shank", (x, y-length/2-.035, z), (.036, length, .025), m["steel"], .006)
        R("Spanner peg", (x, y+.045, .50), (x, y+.045, .438), .009, m["dark"], 12)
    for x in (-1.82, -1.63):
        R("Screwdriver blade", (x, 1.40, .463), (x, 1.72, .463), .012, m["steel"], 12)
        R("Screwdriver handle", (x, 1.70, .463), (x, 1.87, .463), .035, m["red"], 24)
        R("Driver support peg", (x, 1.70, .50), (x, 1.70, .425), .009, m["dark"], 12)
    vise(m)
    R("Reader conduit", (1.30, 1.58, 2.82), (1.30, 2.55, 2.82), .018, m["steel"], 16)
    R("Reader feed to portal", (1.30, 2.55, 2.82), (1.04, 2.55, 2.82), .018, m["steel"], 16)


def vise(m):
    x = -2.48
    R("Vise swivel foot", (x, .94, -.10), (x, .98, -.10), .18, m["dark"], 48)
    R("Vise cast swivel body", (x, .98, -.10), (x, 1.07, -.10), .145, m["blue"], 48)
    B("Vise fixed body", (x, 1.11, -.01), (.24, .18, .28), m["blue"], .022)
    B("Vise fixed jaw", (x, 1.23, -.105), (.29, .14, .09), m["blue"], .012)
    B("Vise sliding guide", (x, 1.095, -.23), (.11, .08, .35), m["dark"], .006)
    B("Vise moving cheek", (x, 1.15, -.285), (.18, .14, .09), m["blue"], .012)
    B("Vise moving jaw", (x, 1.23, -.285), (.29, .14, .09), m["blue"], .012)
    for z in (-.159, -.231):
        B("Vise replaceable jaw plate", (x, 1.265, z), (.28, .062, .018), m["steel"], .002)
        for dx in (-.095, .095):
            R("Jaw retaining screw", (x+dx, 1.265, z-.011), (x+dx, 1.265, z+.011), .009, m["dark"], 12)
    R("Vise lead screw", (x, 1.13, -.40), (x, 1.13, -.01), .024, m["steel"], 24)
    R("Vise screw collar", (x, 1.13, -.43), (x, 1.13, -.37), .043, m["blue"], 32)
    R("Vise sliding tommy bar", (x-.13, 1.13, -.415), (x+.13, 1.13, -.415), .012, m["steel"], 16)
    for dx in (-.13, .13):
        R("Vise handle stop", (x+dx, 1.13, -.435), (x+dx, 1.13, -.395), .022, m["dark"], 16)
    B("Vise rear anvil", (x, 1.21, .09), (.22, .035, .10), m["steel"], .004)
    for dx in (-.13, .13):
        R("Vise bench bolt", (x+dx, .97, -.03), (x+dx, .99, -.03), .018, m["steel"], 6)


def key(m):
    brass = h.material("service_key_nickel_brass", (.62, .46, .18), .38, .65)
    # Bow opening is real negative space. Normalized unit bounds preserve the
    # authored pickup/highlight volume; the thin key rests at its lower face.
    points = [(-.275+.17*cos(i*2*pi/48), -.30*sin(i*2*pi/48), -.43) for i in range(49)]
    h.sweep("Service key pierced bow", points, .055, brass, 12)
    B("Service key shoulder", (-.105, -.43, 0), (.13, .11, .17), brass, .015)
    B("Service key shaft", (.18, -.43, 0), (.58, .11, .075), brass, .012)
    for x, depth in ((.25, .21), (.38, .29)):
        B("Service key cut tooth", (x, -.43, depth/2), (.08, .11, depth), brass, .009)


if __name__ == "__main__":
    for name, build in (("foundation_intake", intake), ("service_key", key)):
        if SELECTED and name not in SELECTED:
            continue
        m = p.materials()
        build(m)
        h.export(name)
