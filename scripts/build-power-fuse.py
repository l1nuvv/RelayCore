"""Build the intake power fuse as a textured ceramic cartridge; metres, Y up."""

from pathlib import Path
import math
import struct


root = Path(__file__).resolve().parents[1] / "assets/models"
vertices = []


def triangle(a, b, c, swatch):
    ab = tuple(b[i] - a[i] for i in range(3))
    ac = tuple(c[i] - a[i] for i in range(3))
    normal = (ab[1] * ac[2] - ab[2] * ac[1],
              ab[2] * ac[0] - ab[0] * ac[2],
              ab[0] * ac[1] - ab[1] * ac[0])
    length = math.sqrt(sum(component * component for component in normal))
    u = (swatch % 2) * 0.5 + 0.25
    v = (swatch // 2) * 0.5 + 0.25
    for point in (a, b, c):
        vertices.append((point, (u, v), tuple(component / length for component in normal)))


def cylinder(x0, x1, radius, swatch, sides=12):
    for i in range(sides):
        a = i * math.tau / sides
        b = (i + 1) * math.tau / sides
        low0 = (x0, math.cos(a) * radius, math.sin(a) * radius)
        low1 = (x0, math.cos(b) * radius, math.sin(b) * radius)
        high0 = (x1, math.cos(a) * radius, math.sin(a) * radius)
        high1 = (x1, math.cos(b) * radius, math.sin(b) * radius)
        triangle(low0, low1, high1, swatch)
        triangle(low0, high1, high0, swatch)


def disk(x, radius, swatch, facing, sides=12):
    center = (x, 0, 0)
    for i in range(sides):
        a = i * math.tau / sides
        b = (i + 1) * math.tau / sides
        edge0 = (x, math.cos(a) * radius, math.sin(a) * radius)
        edge1 = (x, math.cos(b) * radius, math.sin(b) * radius)
        if facing > 0:
            triangle(center, edge0, edge1, swatch)
        else:
            triangle(center, edge1, edge0, swatch)


cylinder(-0.34, 0.34, 0.32, 0)
cylinder(-0.08, 0.08, 0.335, 3)
for side in (-1, 1):
    start, end = (-0.48, -0.34) if side < 0 else (0.34, 0.48)
    cylinder(start, end, 0.39, 1)
    disk(end if side > 0 else start, 0.39, 1, side)
    disk(end + side * 0.006 if side > 0 else start + side * 0.006, 0.25, 2, side)

with (root / "power_fuse.obj").open("w", encoding="ascii") as output:
    output.write("# Power fuse cartridge; normalized to the authored pickup bounds\n")
    for position, _, _ in vertices:
        output.write("v " + " ".join(f"{value:.6f}" for value in position) + "\n")
    for _, uv, _ in vertices:
        output.write("vt " + " ".join(f"{value:.6f}" for value in uv) + "\n")
    for _, _, normal in vertices:
        output.write("vn " + " ".join(f"{value:.6f}" for value in normal) + "\n")
    for start in range(0, len(vertices), 3):
        output.write("f " + " ".join(f"{i}/{i}/{i}" for i in range(start + 1, start + 4)) + "\n")

swatches = ((178, 179, 161), (72, 77, 75), (165, 122, 71), (158, 92, 42))
size = 256
pixels = bytearray()
for y in range(size):
    for x in range(size):
        red, green, blue = swatches[(x // 128) + 2 * (y // 128)]
        grain = ((x * 7 + y * 11) % 9) - 4
        pixels.extend((blue + grain, green + grain, red + grain))

with (root / "power_fuse.bmp").open("wb") as output:
    output.write(struct.pack("<2sIHHI", b"BM", 54 + len(pixels), 0, 0, 54))
    output.write(struct.pack("<IiiHHIIiiII", 40, size, size, 1, 24, 0, len(pixels), 2835, 2835, 0, 0))
    output.write(pixels)

print(f"power_fuse.obj: {len(vertices) // 3} triangles; power_fuse.bmp: {size}x{size}")
