"""Build the wall sentry's OBJ and four-swatch diffuse BMP; metres, Y up, front +Z."""

from pathlib import Path
import math
import struct


root = Path(__file__).resolve().parents[1] / "assets/models"
vertices = []


def face(points, swatch):
    a, b, c, d = points
    u = (swatch % 2) * 0.5
    v = (swatch // 2) * 0.5
    uv = [(u + 0.04, v + 0.04), (u + 0.46, v + 0.04),
          (u + 0.46, v + 0.46), (u + 0.04, v + 0.46)]
    for indices in ((0, 1, 2), (0, 2, 3)):
        p, q, r = (points[i] for i in indices)
        ab = [q[i] - p[i] for i in range(3)]
        ac = [r[i] - p[i] for i in range(3)]
        normal = (ab[1] * ac[2] - ab[2] * ac[1],
                  ab[2] * ac[0] - ab[0] * ac[2],
                  ab[0] * ac[1] - ab[1] * ac[0])
        length = math.sqrt(sum(value * value for value in normal))
        for i in indices:
            vertices.append((points[i], uv[i], tuple(value / length for value in normal)))


def box(low, high, swatch):
    x0, y0, z0 = low
    x1, y1, z1 = high
    for corners in (
        [(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)],
        [(x1,y0,z0),(x0,y0,z0),(x0,y1,z0),(x1,y1,z0)],
        [(x1,y0,z1),(x1,y0,z0),(x1,y1,z0),(x1,y1,z1)],
        [(x0,y0,z0),(x0,y0,z1),(x0,y1,z1),(x0,y1,z0)],
        [(x0,y1,z1),(x1,y1,z1),(x1,y1,z0),(x0,y1,z0)],
        [(x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1)],
    ):
        face(corners, swatch)


def cylinder_z(cx, cy, back, front, radius, swatch, sides=12):
    rings = []
    for z in (back, front):
        rings.append([(cx + math.cos(i * math.tau / sides) * radius,
                       cy + math.sin(i * math.tau / sides) * radius, z)
                      for i in range(sides)])
    for i in range(sides):
        j = (i + 1) % sides
        face((rings[0][i], rings[0][j], rings[1][j], rings[1][i]), swatch)


def disk_z(cx, cy, z, radius, swatch, sides=12):
    u = (swatch % 2) * 0.5 + 0.25
    v = (swatch // 2) * 0.5 + 0.25
    for i in range(sides):
        a = i * math.tau / sides
        b = (i + 1) * math.tau / sides
        vertices.extend((
            ((cx, cy, z), (u, v), (0, 0, 1)),
            ((cx + math.cos(a) * radius, cy + math.sin(a) * radius, z),
             (u + math.cos(a) * 0.17, v + math.sin(a) * 0.17), (0, 0, 1)),
            ((cx + math.cos(b) * radius, cy + math.sin(b) * radius, z),
             (u + math.cos(b) * 0.17, v + math.sin(b) * 0.17), (0, 0, 1)),
        ))


def octagonal_housing(rings, swatch):
    outline = [(-1,-0.55),(-0.55,-1),(0.55,-1),(1,-0.55),
               (1,0.55),(0.55,1),(-0.55,1),(-1,0.55)]
    sections = [[(x * rx, y, z * rz) for x, z in outline] for y, rx, rz in rings]
    for lower, upper in zip(sections, sections[1:]):
        for i in range(8):
            j = (i + 1) % 8
            face((lower[i], upper[i], upper[j], lower[j]), swatch)


# A recessed wall plate, gimbal and spherical instrument head. Front sensor stays at the warning cue.
box((-0.37,-0.46,-0.48), (0.37,0.35,-0.40), 1)
box((-0.22,-0.27,-0.40), (0.22,0.02,-0.24), 2)
box((-0.12,-0.13,-0.28), (0.12,0.13,-0.13), 1)
mount_vertex_count = len(vertices)
profile = []
for band in range(13):
    angle = -math.pi / 2 + .015 + band * (math.pi - .03) / 12
    profile.append((.07 + math.sin(angle) * .33, math.cos(angle) * .36, math.cos(angle) * .34))
octagonal_housing(profile, 0)
octagonal_housing([(.025,.365,.345),(.070,.365,.345)], 2)
box((-0.46,-0.16,-0.17), (-0.37,0.18,0.19), 1)
box((0.37,-0.16,-0.17), (0.46,0.18,0.19), 1)
for x in (-.31, .31):
    cylinder_z(x, .07, -.02, .08, .075, 2)
    disk_z(x, .07, .082, .045, 1)
box((-0.22,.33,-0.14), (.22,.38,.14), 2)
cylinder_z(0.04, 0.01, 0.31, 0.46, 0.13, 1)
cylinder_z(0.04, 0.01, 0.45, 0.49, 0.145, 2)
disk_z(0.04, 0.01, 0.491, 0.14, 2)
disk_z(0.04, 0.01, 0.493, 0.092, 1)
cylinder_z(-0.25, 0.24, 0.31, 0.39, 0.068, 1)
disk_z(-0.25, 0.24, 0.392, 0.056, 3)
# The armour sits proud of the dark side brackets, never on their outer planes.
box((-0.472,-0.06,-0.16), (-0.35,0.25,0.18), 2)
box((0.35,-0.06,-0.16), (0.472,0.25,0.18), 2)

def write_obj(path, damaged):
    angle = math.radians(55 if damaged else 0)
    cosine, sine = math.cos(angle), math.sin(angle)
    def bend(position, normal):
        x, y, z = position
        nx, ny, nz = normal
        y, z = y + 0.04, z + 0.24
        return ((x, y * cosine - z * sine - 0.04,
                 y * sine + z * cosine - 0.24),
                (nx, ny * cosine - nz * sine, ny * sine + nz * cosine))
    with path.open("w", encoding="ascii") as output:
        output.write("# RelayCore wall sentry; local origin at collision centre, front +Z\n")
        for index, (position, _, normal) in enumerate(vertices):
            if damaged and index >= mount_vertex_count:
                position, normal = bend(position, normal)
            output.write("v " + " ".join(f"{value:.6f}" for value in position) + "\n")
        for _, uv, _ in vertices:
            output.write("vt " + " ".join(f"{value:.6f}" for value in uv) + "\n")
        for index, (_, _, normal) in enumerate(vertices):
            if damaged and index >= mount_vertex_count:
                _, normal = bend((0, 0, 0), normal)
            output.write("vn " + " ".join(f"{value:.6f}" for value in normal) + "\n")
        for start in range(0, len(vertices), 3):
            output.write("f " + " ".join(f"{i}/{i}/{i}" for i in range(start + 1, start + 4)) + "\n")


write_obj(root / "security_turret.obj", False)
write_obj(root / "security_turret_destroyed.obj", True)

size = 256
row_bytes = (size * 3 + 3) & ~3


def write_bmp(path, signal_color):
    swatches = ((106, 103, 85), (32, 38, 35), (137, 112, 65), signal_color)
    pixels = bytearray()
    for y in range(size):
        for x in range(size):
            red, green, blue = swatches[(x // 128) + 2 * (y // 128)]
            grain = ((x * 13 + y * 7) % 11) - 5
            pixels.extend((blue + grain, green + grain, red + grain))
        pixels.extend(b"\0" * (row_bytes - size * 3))
    with path.open("wb") as output:
        output.write(struct.pack("<2sIHHI", b"BM", 54 + len(pixels), 0, 0, 54))
        output.write(struct.pack("<IiiHHIIiiII", 40, size, size, 1, 24, 0, len(pixels), 2835, 2835, 0, 0))
        output.write(pixels)


write_bmp(root / "security_turret.bmp", (167, 79, 44))
write_bmp(root / "security_turret_destroyed.bmp", (33, 31, 27))
print(f"security_turret models: {len(vertices) // 3} triangles; diffuse textures: {size}x{size}")
