"""Build the security rover's triangulated OBJ; metres, Y up, front +Z."""
from pathlib import Path
import math

vertices = []

def triangle(a, b, c, tile):
    ab = [b[i] - a[i] for i in range(3)]
    ac = [c[i] - a[i] for i in range(3)]
    n = [ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0]]
    length = math.sqrt(sum(x*x for x in n))
    n = [x/length for x in n]
    x, y = (tile % 2)*.5, (1-tile//2)*.5
    for p, uv in zip((a,b,c), ((.04,.04),(.46,.04),(.46,.46))):
        vertices.append((p, (x+uv[0], y+uv[1]), n))

def quad(a,b,c,d,tile):
    triangle(a,b,c,tile)
    start = len(vertices)
    triangle(a,c,d,tile)
    x, y = (tile % 2)*.5, (1-tile//2)*.5
    for i, uv in enumerate(((.04,.04),(.46,.46),(.04,.46))):
        p, _, n = vertices[start+i]
        vertices[start+i] = p, (x+uv[0], y+uv[1]), n

def shell(rings, tile):
    # Octagonal cross sections create a bevelled, sloping shell.
    pattern = [(-1,-.65),(-.65,-1),(.65,-1),(1,-.65),(1,.65),(.65,1),(-.65,1),(-1,.65)]
    points = [[(x*w,y,z*l+offset) for x,z in pattern] for y,w,l,offset in rings]
    for lower, upper in zip(points,points[1:]):
        for i in range(8):
            j=(i+1)%8
            quad(lower[i],upper[i],upper[j],lower[j],tile)
    for i in range(1,7):
        triangle(points[0][0],points[0][i],points[0][i+1],tile)
        triangle(points[-1][0],points[-1][i+1],points[-1][i],tile)

def wheel(x,z):
    rings=[]
    for dx,r in [(-.085,.16),(-.06,.20),(.06,.20),(.085,.16)]:
        rings.append([(x+dx,.21+math.sin(i*math.tau/12)*r,z+math.cos(i*math.tau/12)*r) for i in range(12)])
    for a,b in zip(rings,rings[1:]):
        for i in range(12):
            j=(i+1)%12
            quad(a[i],b[i],b[j],a[j],1)
    for k in [0,-1]:
        center=(rings[k][0][0],.21,z)
        for i in range(12):
            a,b=rings[k][i],rings[k][(i+1)%12]
            triangle(center,b,a,2) if k==0 else triangle(center,a,b,2)

shell([(.18,.27,.34,0),(.28,.34,.42,0),(.48,.34,.42,0),(.60,.25,.31,-.04)],0)
shell([(.58,.13,.14,-.04),(.68,.13,.14,-.04),(.72,.10,.10,-.04)],2)
head_start = len(vertices)
shell([(.69,.16,.14,.01),(.74,.21,.18,.02),(.88,.21,.18,.02),(.95,.15,.12,-.02)],3)
# Twin circular optical pickups echo the sealed protective goggles and analog instruments.
for cx in (-.092, .092):
    for radius, z, tile in ((.061, .208, 2), (.043, .212, 1)):
        for side in range(16):
            a, b = side * math.tau / 16, (side + 1) * math.tau / 16
            triangle((cx, .83, z), (cx + math.cos(a) * radius, .83 + math.sin(a) * radius, z),
                     (cx + math.cos(b) * radius, .83 + math.sin(b) * radius, z), tile)
head_end = len(vertices)
# Front collision bumper and sensor brow.
shell([(.25,.27,.08,.39),(.36,.30,.08,.39),(.40,.27,.06,.39)],2)
shell([(.78,.15,.035,.20),(.85,.15,.035,.20)],1)
wheel_starts = []
for x in [-.32,.32]:
    for z in [-.25,.25]:
        wheel_starts.append(len(vertices))
        wheel(x,z)

def write_obj(path, mesh, label):
    with path.open('w', encoding='ascii') as f:
        f.write(f'# RelayCore {label}, front +Z, Y up\n')
        for p,uv,n in mesh: f.write('v '+' '.join(f'{x:.6f}' for x in p)+'\n')
        for p,uv,n in mesh: f.write('vt '+' '.join(f'{x:.6f}' for x in uv)+'\n')
        for p,uv,n in mesh: f.write('vn '+' '.join(f'{x:.6f}' for x in n)+'\n')
        for i in range(0,len(mesh),3): f.write('f '+' '.join(f'{j}/{j}/{j}' for j in range(i+1,i+4))+'\n')
    print(f'{path}: {len(mesh)//3} triangles')

root = Path(__file__).resolve().parents[1]/'assets/models'
write_obj(root/'security_rover.obj', vertices, 'security rover: own mesh')

live = vertices
wreck = []
angle = math.radians(65)

def roll(p):
    x, y, z = p
    return (x*math.cos(angle)-(y-.45)*math.sin(angle)-.1,
            x*math.sin(angle)+(y-.45)*math.cos(angle)+.35, z)

def broken_head(p):
    x, y, z = p
    return (x+.58, y-.48, z+.22)

def append_part(begin, end, transform, dark=False):
    for i in range(begin, end, 3):
        points = [transform(live[j][0]) for j in range(i, i+3)]
        ab = [points[1][k]-points[0][k] for k in range(3)]
        ac = [points[2][k]-points[0][k] for k in range(3)]
        normal = (ab[1]*ac[2]-ab[2]*ac[1],
                  ab[2]*ac[0]-ab[0]*ac[2],
                  ab[0]*ac[1]-ab[1]*ac[0])
        length = math.sqrt(sum(value*value for value in normal))
        for j, point in zip(range(i, i+3), points):
            uv = live[j][1]
            if dark:
                uv = (uv[0], uv[1]+.5)
            wreck.append((point, uv, tuple(value/length for value in normal)))

append_part(0, head_start, roll)
append_part(head_start, head_end, broken_head, dark=True)
append_part(head_end, wheel_starts[0], roll)
for index, begin in enumerate(wheel_starts):
    end = wheel_starts[index+1] if index+1 < len(wheel_starts) else len(live)
    if index < 2:
        append_part(begin, end, roll)
    else:
        shift = -.35 if index == 2 else .40
        append_part(begin, end, lambda p, shift=shift: (p[0]+shift, p[1]-.10, p[2]+.3))

floor_offset = .025-min(p[1] for p,_,_ in wreck)
wreck = [((p[0], p[1]+floor_offset, p[2]), uv, n) for p,uv,n in wreck]
write_obj(root/'security_rover_destroyed.obj', wreck, 'destroyed security rover')
