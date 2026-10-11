"""VIS-13 fitted B-17 services; runtime coordinates in metres, origin [0,0,0].

Blender --background --factory-startup --disable-autoexec --threads 2 --python
scripts/export-b17-services-glb.py
Existing apparatus, instruments, furniture and collision remain map-owned.
"""
from pathlib import Path
from math import pi, sin, cos
import importlib.util

import bpy

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("transfer", ROOT / "scripts/export-pressure-transfer-glb.py")
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
B = h.bay_box


def rod(name, a, b, radius, mat, sides=24):
    return h.rod(name, (a[0], -a[2], a[1]), (b[0], -b[2], b[1]), radius, mat, sides)


def cable(name, points, mat, radius=.018):
    h.sweep(name, [(x, -z, y) for x, y, z in points], radius, mat, 16)


def bezier(a, b, c, d):
    return [tuple((1-t)**3*a[i]+3*(1-t)**2*t*b[i]+3*(1-t)*t*t*c[i]+t**3*d[i]
                  for i in range(3)) for t in (j/24 for j in range(25))]


def plate(text, pos, width, m, facing="-z", height=.23):
    axis = 0 if facing.endswith("x") else 2
    size = [.025, height, .025]
    size[2 if axis == 0 else 0] = width
    B(text + " backing", pos, size, m["ivory"], .003)
    surface = list(pos)
    surface[axis] += .016 * (1 if facing.startswith("+") else -1)
    h.bay_label(text, (surface[0], surface[1]-.038, surface[2]), .085, m["rubber"], facing)


def window(m):
    # The opaque frames overlap the original seals; no new glass or collision.
    for y in (1.055, 2.165):
        B("Observation folded glazing rail", (-2, y, 2.8), (.25, .075, 4.16), m["steel"], .006)
        for x in (-2.065, -1.935):
            B("Continuous glazing gasket", (x, y+(.042 if y < 2 else -.042), 2.8),
              (.025, .018, 4.02), m["rubber"], .003)
    B("Observation sill", (-2, 1.018, 2.8), (.34, .035, 4.17), m["ivory"], .006)
    for z in (.78, 2.8, 4.78):
        B("Glazing mullion cover", (-2, 1.61, z), (.165, 1.16, .095), m["steel"], .005)
        for y in (1.10, 2.12):
            for x in (-2.09, -1.91):
                rod("Glazing rail fastener", (x-.006, y, z), (x+.006, y, z), .012, m["rubber"], 8)
    for x, facing in ((-2.108, "-x"), (-1.892, "+x")):
        plate("01 / OBSERVATION", (x, 2.39, 1.85), 1.86, m, facing)
    B("Observation lower enamel", (-2, .53, 2.8), (.205, .96, 4.08), m["sage"], .003)
    B("Observation skirting", (-2, .07, 2.8), (.23, .14, 4.10), m["rubber"], .003)
    B("Containment sign mounting pad", (-1.895, .775, 2.5), (.06, .40, .82), m["sage"], .004)


def services(m):
    # Back-wall signal trunk links both racks and the isolated power cabinet.
    for y in (2.72, 2.94):
        B("Folded cable tray side", (0, y, 4.67), (11.28, .025, .075), m["steel"], .003)
    for x in (-5.4, -3.5, -1.5, .5, 2.5, 4.5, 5.4):
        B("Tray wall mounting shoe", (x, 2.82, 4.82), (.09, .28, .075), m["steel"], .003)
        B("Tray cantilever", (x, 2.73, 4.69), (.06, .035, .30), m["steel"], .003)
    for x in (j*.35-5.25 for j in range(31)):
        B("Tray rung", (x, 2.75, 4.67), (.025, .025, .20), m["steel"], .002)
    for y in (2.79, 2.84, 2.89):
        rod("Contained signal loom", (-5.52, y, 4.67), (5.52, y, 4.67), .018, m["rubber"])
    for x in (3.5, 5.2):
        cable("Rack cable drop", bezier((x, 2.84, 4.67), (x, 2.35, 4.67),
                                       (x, 2.35, 4.30), (x, 2.04, 4.30)), m["rubber"], .025)
        rod("Rack gland", (x, 2.03, 4.30), (x, 2.12, 4.30), .045, m["steel"])
    cable("Observation rack supply", bezier((-5.20, 2.84, 4.67), (-5.20, 2.84, 3.25),
                                           (-5.20, 2.50, 3.25), (-5.20, 2.06, 3.25)), m["rubber"], .025)
    rod("Observation rack gland", (-5.20, 2.03, 3.25), (-5.20, 2.12, 3.25), .045, m["steel"])
    # High-level instrument feed uses a wall shoe and two rigid ceiling stems.
    B("Apparatus feed wall plate", (5.80, 2.81, 1.2), (.10, .32, .28), m["sage"], .006)
    B("Apparatus feed channel", (4.15, 2.84, 1.2), (3.30, .07, .15), m["steel"], .005)
    for x in (2.75, 4.65):
        rod("Feed ceiling stem", (x, 2.875, 1.2), (x, 3.49, 1.2), .014, m["steel"])
        B("Feed ceiling foot", (x, 3.485, 1.2), (.13, .02, .13), m["steel"], .003)
    cable("Cabinet to field feed", [(5.52, 2.84, 4.67), (5.65, 2.84, 4.4),
                                   (5.65, 2.84, 1.2), (2.75, 2.84, 1.2)], m["rubber"], .022)
    cable("Field electrode flexible tail", bezier((2.75, 2.84, 1.2), (2.45, 2.75, 1.2),
                                                 (2, 2.66, 1.2), (2, 2.35, 1.2)), m["rubber"], .022)
    rod("Field electrode gland", (2, 2.29, 1.2), (2, 2.39, 1.2), .045, m["steel"])
    # Bond the existing floor conductor into the rack and apparatus base.
    rod("Bonding return", (1, .027, 4.55), (3.5, .027, 4.55), .018, m["brass"])
    rod("Rack earth riser", (3.5, .027, 4.55), (3.5, .26, 4.55), .018, m["brass"])
    rod("Apparatus earth tail", (1, .027, 1.15), (1.3, .10, 1.15), .018, m["brass"])
    for z in (1.35, 2.5, 3.7, 4.45):
        B("Bonding conductor clamp", (1, .031, z), (.09, .038, .055), m["steel"], .003)
    # Existing utility pipes receive wall terminations instead of open ends.
    for x, direction in ((-.5, -1), (5.5, 1)):
        points = [(x+direction*.25*sin(i*pi/16), 2.65,
                   4.55+.25*(1-cos(i*pi/16))) for i in range(9)]
        cable("Service swept pipe return", points, m["steel"], .085)
        rod("Service pipe wall flange", (x+direction*.25, 2.65, 4.78), (x+direction*.25, 2.65, 4.85), .13, m["sage"])
    rod("Rear pipe inter-module coupling", (2.375, 2.65, 4.55), (2.625, 2.65, 4.55), .107, m["steel"])
    for z, direction in ((-8.5, -1), (-5.5, 1)):
        points = [(-1.23-.20*(1-cos(i*pi/16)), 2.65,
                   z+direction*.20*sin(i*pi/16)) for i in range(9)]
        cable("Access pipe wall elbow", points, m["steel"], .085)
        rod("Access pipe wall flange", (-1.46, 2.65, z+direction*.20), (-1.40, 2.65, z+direction*.20), .13, m["sage"])
    for x, z, direction in ((-5.55, -2.7, -1), (5.55, 2.2, 1)):
        B("Vent wall throat", (x+direction*.29, 2.8, z), (.18, .48, 1.22), m["rubber"], .004)
        B("Vent wall flange", (x+direction*.25, 2.8, z), (.035, .64, 1.46), m["steel"], .005)
        for dz in (-.54, .54):
            B("Vent support shoe", (x+direction*.29, 2.46, z+dz), (.18, .06, .09), m["steel"], .003)
    for x in (0, 2, 4):
        B("Pipe mounting shoe", (x, 2.65, 4.82), (.16, .34, .08), m["steel"], .003)
        rod("Pipe standoff", (x, 2.65, 4.55), (x, 2.65, 4.82), .018, m["steel"])
    # Fix the suspended fixtures to the existing ceiling.
    for x, z, axis, half in ((-4, 0, "z", .85), (2, -2, "x", .85),
                             (3.8, 3.4, "x", .72), (0, -7, "z", .65)):
        for side in (-1, 1):
            px, pz = (x, z+side*half) if axis == "z" else (x+side*half, z)
            rod("Fluorescent ceiling stem", (px, 3.40, pz), (px, 3.49, pz), .014, m["steel"])
    for x in (1.76, 2.24):
        rod("Apparatus task light stem", (x, 2.92, .15), (x, 3.49, .15), .014, m["steel"])
    # Shielded observer task light illuminates the scientist's front (+X/-Z).
    B("Observer light backplate", (-2.11, 2.38, 3.2), (.09, .22, .24), m["sage"], .006)
    rod("Observer lamp bracket", (-2.11, 2.38, 3.2), (-2.55, 2.38, 3.2), .025, m["steel"])
    B("Observer lamp folded hood", (-2.59, 2.34, 3.2), (.36, .12, .28), m["sage"], .012)
    B("Observer lamp opal diffuser", (-2.59, 2.27, 3.2), (.29, .025, .22), m["ivory"], .004)
    rod("Observer lamp conduit", (-2.13, 2.38, 3.2), (-2.13, 2.80, 3.2), .012, m["steel"])
    rod("Observer lamp trunk", (-2.13, 2.80, 3.2), (-2.13, 2.80, 4.67), .012, m["steel"])
    for y, z in ((2.55, 3.2), (2.75, 3.2), (2.8, 4.5)):
        B("Observer conduit saddle", (-2.11, y, z), (.08, .06, .06), m["steel"], .003)


def workstations(m):
    plate("02 / SAMPLE PREPARATION", (4.4, .77, -3.235), 2.10, m, "+z")
    plate("REFERENCE / 50 Hz", (-4.2, .77, 3.89), 1.90, m)
    plate("03 / FIELD MEASUREMENT", (3.5, 2.26, 4.25), 2.08, m)
    for x in (3.1, 3.9):
        rod("Instrument ID support", (x, 2.03, 4.25), (x, 2.18, 4.25), .012, m["steel"])
    plate("ISOLATE / B-17", (4.9, 2.28, 4.78), 1.52, m)
    B("Isolator ID wall backing", (4.9, 2.28, 4.83), (1.35, .13, .10), m["sage"], .004)
    plate("B-17 / RESONANCE RESEARCH", (0, 2.94, -5.17), 2.65, m)
    plate("EXIT / ACCESS CORRIDOR", (0, 2.94, -4.82), 2.65, m, "+z")
    # A shallow cassette tray explains prepared samples without opaque glass.
    B("Sample cassette tray", (3.55, .934, -3.28), (.62, .025, .16), m["steel"], .005)
    for z in (-3.37, -3.19):
        B("Sample tray folded edge", (3.55, .955, z), (.65, .04, .018), m["steel"], .002)
    for x in (3.23, 3.87):
        B("Sample tray end", (x, .955, -3.28), (.018, .04, .20), m["steel"], .002)
    for i, x in enumerate((3.38, 3.55, 3.72)):
        B("Ceramic sample carrier", (x, .958, -3.28), (.12, .025, .12), m["ivory"], .004)
        # Cut crystal profile, with a continuous band and two real pointed ends.
        vertices = [(x, 3.28, 1.075), (x, 3.28, .971)]
        vertices += [(x+.034*cos(j*pi/3), 3.28+.034*sin(j*pi/3), 1.005) for j in range(6)]
        faces = [(0, 2+j, 2+(j+1)%6) for j in range(6)]
        faces += [(1, 2+(j+1)%6, 2+j) for j in range(6)]
        mesh = bpy.data.meshes.new("Prepared resonance specimen")
        mesh.from_pydata(vertices, [], faces)
        mesh.update()
        obj = bpy.data.objects.new("Prepared specimen " + str(i+1), mesh)
        bpy.context.collection.objects.link(obj)
        mesh.materials.append(m["specimen"])
    # Desk cables terminate at supported service sockets behind the equipment.
    for x, z, front in ((4.4, -3.6, False), (-4.0, 4.25, True)):
        wall_z = 4.77 if front else -4.77
        socket_x = -5.45 if front else 5.45
        B("Bench service socket panel", (socket_x, 1.10, wall_z), (.45, .16, .06), m["ivory"], .004)
        cable("Terminal service lead", bezier((x, 1.0, z), (x+.2, .97, z),
                                             (socket_x, .96, wall_z), (socket_x, 1.08, wall_z)), m["rubber"], .012)
        rod("Socket conduit", (socket_x, 1.18, wall_z), (socket_x, 2.84, wall_z), .014, m["steel"])
    B("Observation socket panel", (-5.745, 1.12, .7), (.06, .16, .45), m["ivory"], .004)
    cable("Observation terminal lead", bezier((-4.9, 1.0, .7), (-5.3, 1.0, .8),
                                              (-5.5, 1.0, .7), (-5.745, 1.1, .7)), m["rubber"], .012)


if __name__ == "__main__":
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = "METRIC"
    m = {
        "sage": h.material("b17_services_sage", (.25, .31, .27), .63),
        "steel": h.material("b17_services_galvanized", (.34, .38, .37), .48, .80),
        "rubber": h.material("b17_services_rubber", (.018, .025, .023), .86),
        "ivory": h.material("b17_services_ivory", (.63, .61, .51), .36),
        "brass": h.material("b17_services_bonded_copper", (.42, .25, .08), .42, .80),
        "specimen": h.material("b17_prepared_specimen", (.12, .19, .16), .35, .25),
    }
    h.surface_normal(m["sage"], .6, .10)
    h.surface_normal(m["steel"], .4, .035)
    h.surface_normal(m["rubber"], .3, .055)
    h.surface_normal(m["brass"], .4, .02)
    window(m)
    services(m)
    workstations(m)
    h.export("b17_services")
