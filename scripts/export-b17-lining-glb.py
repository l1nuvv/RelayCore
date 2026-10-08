"""VIS-14: fitted B-17 finish, retaining the authored laboratory/service floors.

Runtime metres, +Y up, one non-colliding placement at the world origin.
The existing window, equipment and passage remain the dimension authorities.
"""
from pathlib import Path
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("power", ROOT / "scripts/export-power-restoration-glb.py")
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)
h, B = p.h, p.B
h.GRAPHICS = ROOT / "build/blender-review/b17-lining/materials"
h.GRAPHICS.mkdir(parents=True, exist_ok=True)


def wall(name, x, z, length, axis, m):
    dims = lambda thickness, height: (thickness, height, length) if axis == "x" else (length, height, thickness)
    B(name + " plaster", (x, 2.29, z), dims(.024, 2.40), m["wall"], .001)
    # Cover the existing 80 mm dado and its offset rear-wall plinth.
    B(name + " enamel dado", (x, .55, z), dims(.104, 1.10), m["paint"], .002)
    B(name + " folded dado cap", (x, 1.115, z), dims(.120, .035), m["steel"], .003)
    B(name + " coved plinth", (x, .075, z), dims(.208, .15), m["dark"], .004)


def lining(m):
    for x in (-5.80, 5.80):
        wall("Laboratory side", x, 0, 9.70, "x", m)
    wall("Instrument wall", 0, 4.80, 11.60, "z", m)
    for x in (-3.62, 3.62):
        wall("Entry wing", x, -4.845, 4.44, "z", m)
    B("Entry transom plaster", (0, 3.12, -5), (2.80, .96, .324), m["wall"], .002)
    for x in (-1.405, 1.405):
        B("Entry folded jamb cap", (x, 1.32, -5), (.15, 2.64, .385), m["steel"], .004)
        B("Entry jamb impact shoe", (x, .23, -5.205), (.15, .46, .035), m["dark"], .003)
    B("Entry folded lintel cap", (0, 2.64, -5), (2.97, .15, .385), m["steel"], .004)
    B("Laboratory threshold", (0, .009, -5), (2.64, .018, .38), m["steel"], .001)
    for x in (-1.405, 1.405):
        wall("Access corridor", x, -7, 3.70, "x", m)
    wall("Access end wall", 0, -8.845, 2.82, "z", m)
    B("Observation transom finish", (-2, 2.86, 2.8), (.205, 1.38, 4.10), m["wall"], .002)
    for x in (-5.58, 5.58):
        for z in (-2.8, 2.6):
            B("Column enamel impact wrap", (x, .55, z), (.48, 1.10, .48), m["paint"], .004)
            B("Column plaster wrap", (x, 2.20, z), (.474, 2.20, .474), m["wall"], .002)
            B("Column dado collar", (x, 1.115, z), (.49, .035, .49), m["steel"], .003)
            B("Column plinth", (x, .075, z), (.50, .15, .50), m["dark"], .004)
    for z in (-2.8, 2.6):
        B("Beam plaster jacket", (0, 3.31, z), (11.52, .424, .474), m["wall"], .002)
    B("Laboratory ceiling field", (0, 3.46, 0), (11.68, .024, 9.68), m["ivory"], .001)
    B("Access ceiling field", (0, 3.46, -7), (2.82, .024, 3.70), m["ivory"], .001)
    for x in (-5.78, 5.78):
        B("Ceiling perimeter angle", (x, 3.435, 0), (.045, .045, 9.68), m["steel"], .002)
    for z in (-4.82, 4.82):
        B("Ceiling perimeter angle", (0, 3.435, z), (11.60, .045, .045), m["steel"], .002)
    for x in (-3.6, -1.2, 1.2, 3.6):
        B("Ceiling main tee", (x, 3.435, 0), (.028, .028, 9.64), m["steel"], .002)
    for z in (-3.6, -1.2, 1.2, 3.6):
        B("Ceiling cross tee", (0, 3.435, z), (11.56, .028, .028), m["steel"], .002)
    for z in (-6, -7.2, -8.4):
        B("Access ceiling cross tee", (0, 3.435, z), (2.82, .028, .028), m["steel"], .002)


if __name__ == "__main__":
    m = p.materials()
    lining(m)
    h.export("b17_lining")
