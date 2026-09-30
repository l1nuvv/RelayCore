"""Author the original 1.52 m service-cylinder mesh; runtime imports the resulting OBJ."""
from math import cos, sin, pi, hypot
from pathlib import Path

vertices, faces = [], []


def vertex(p, n, uv):
    vertices.append((p, n, uv))
    return len(vertices)


def lathe(profile, segments=20):
    for (r0, y0), (r1, y1) in zip(profile, profile[1:]):
        length = hypot(y1-y0, r1-r0)
        radial, vertical = (y1-y0)/length, (r0-r1)/length
        ring = []
        for j in range(segments+1):
            angle = j * 2*pi/segments
            n = (radial*cos(angle), vertical, radial*sin(angle))
            ring.append([vertex((r*cos(angle), y, r*sin(angle)), n, (j/segments, y/1.52))
                         for r, y in [(r0, y0), (r1, y1)]])
        for j in range(segments):
            a, b = ring[j]
            c, d = ring[j+1]
            if r1 > 0:
                faces.append((a, b, d))
            if r0 > 0:
                faces.append((a, d, c))


# Rolled foot, straight pressure vessel, rounded shoulder and valve stem.
lathe([(0, 0), (.23, 0), (.25, .025), (.25, .09), (.235, .10),
       (.235, 1.08), (.222, 1.17), (.18, 1.24), (.10, 1.29),
       (.055, 1.30), (.055, 1.36), (.075, 1.36), (.075, 1.41),
       (.025, 1.41), (.025, 1.50), (0, 1.50)])

# Horizontal handwheel with actual open center and four spokes.
for j in range(20):
    for k in range(6):
        corners = []
        for u, v in [(j, k), (j+1, k), (j+1, k+1), (j, k+1)]:
            a, b = 2*pi*u/20, 2*pi*v/6
            n = (cos(a)*cos(b), sin(b), sin(a)*cos(b))
            p = ((.13+.018*cos(b))*cos(a), 1.49+.018*sin(b), (.13+.018*cos(b))*sin(a))
            corners.append(vertex(p, n, (u/20, v/6)))
        a, b, c, d = corners
        faces.extend([(a, c, b), (a, d, c)])
for axis in range(2):
    rings = []
    for end in [-.13, .13]:
        ring = []
        for j in range(9):
            a = 2*pi*j/8
            p = (end, 1.49+.013*cos(a), .013*sin(a))
            n = (0, cos(a), sin(a))
            if axis:
                p, n = (p[2], p[1], -p[0]), (n[2], n[1], -n[0])
            ring.append(vertex(p, n, (j/8, (end+.13)/.26)))
        rings.append(ring)
    for j in range(8):
        a, b = rings[0][j:j+2]
        c, d = rings[1][j:j+2]
        faces.extend([(a, b, d), (a, d, c)])

target = Path(__file__).resolve().parents[1] / 'assets/models/service_cylinder.obj'
target.parent.mkdir(parents=True, exist_ok=True)
with target.open('w', encoding='ascii') as out:
    out.write('# Original RelayCore service cylinder. Y up, meters, origin at foot.\n')
    for p, n, uv in vertices:
        out.write('v %.6f %.6f %.6f\n' % p)
        out.write('vt %.6f %.6f\n' % uv)
        out.write('vn %.6f %.6f %.6f\n' % n)
    for face in faces:
        out.write('f ' + ' '.join(f'{i}/{i}/{i}' for i in face) + '\n')
print(f'{target}: {len(faces)} triangles')
