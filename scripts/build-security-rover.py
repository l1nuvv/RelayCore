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

def shell(rings, tile, cap_top=True):
    # Octagonal cross sections create a bevelled, sloping shell.
    pattern = [(-1,-.65),(-.65,-1),(.65,-1),(1,-.65),(1,.65),(.65,1),(-.65,1),(-1,.65)]
    points = [[(x*w,y,z*l+offset) for x,z in pattern] for y,w,l,offset in rings]
    for lower, upper in zip(points,points[1:]):
        for i in range(8):
            j=(i+1)%8
            quad(lower[i],upper[i],upper[j],lower[j],tile)
    for i in range(1,7):
        triangle(points[0][0],points[0][i],points[0][i+1],tile)
        if cap_top:
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
vertices = []
# The broken upper shell exposes the drive tray instead of compressing an intact rover.
shell([(.10,.22,.27,0),(.18,.27,.32,0),(.28,.27,.32,0)],0,cap_top=False)
shell([(.17,.235,.285,0),(.19,.235,.285,0)],1)
for x in (-.14,.14):
    begin=len(vertices)
    shell([(.19,.035,.12,-.08),(.24,.035,.12,-.08)],2)
    for i in range(begin,len(vertices)):
        p,uv,n=vertices[i];vertices[i]=((p[0]+x,p[1],p[2]),uv,n)
# A peeled service cover remains attached along the rear edge.
quad((-.25,.28,-.29),(.24,.28,-.29),(.18,.37,-.08),(-.20,.32,-.03),0)
quad((-.25,.278,-.29),(-.20,.318,-.03),(.18,.368,-.08),(.24,.278,-.29),1)
for x,z in [(-.12,.02),(-.04,.05),(.05,.04),(.13,.02)]:
    for i in range(8):
        a,b=i*math.tau/8,(i+1)*math.tau/8
        quad((x+math.cos(a)*.016,.19,z+math.sin(a)*.016),
             (x+math.cos(a)*.016,.235,z+math.sin(a)*.016),
             (x+math.cos(b)*.016,.235,z+math.sin(b)*.016),
             (x+math.cos(b)*.016,.19,z+math.sin(b)*.016),2)
wreck = vertices

def broken_head(p):
    x, y, z = p
    y,z=(y-.82)*.8,(z-.02)*.8
    pitch,roll=math.radians(-25),math.radians(70)
    y,z=y*math.cos(pitch)-z*math.sin(pitch),y*math.sin(pitch)+z*math.cos(pitch)
    return (x*.75*math.cos(roll)-y*math.sin(roll)+.36,
            x*.75*math.sin(roll)+y*math.cos(roll)+.205,z+.03)

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
                uv = (.65, .65)
            wreck.append((point, uv, tuple(value/length for value in normal)))

append_part(head_start, head_end, broken_head)
for index, begin in enumerate(wheel_starts):
    end = wheel_starts[index+1] if index+1 < len(wheel_starts) else len(live)
    if index == 3:
        append_part(begin, end, lambda p: (p[0]*.8+.09, p[1]*.85, p[2]*.8+.08))
    else:
        append_part(begin, end, lambda p: (p[0]*.8, p[1]*.90, p[2]*.8))

def hardware_box(center,size,tile,angle=0):
    points=[]
    for x,y,z in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
                  (-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]:
        x,y,z=x*size[0]/2,y*size[1]/2,z*size[2]/2
        points.append((center[0]+x,center[1]+y*math.cos(angle)-z*math.sin(angle),
                       center[2]+y*math.sin(angle)+z*math.cos(angle)))
    for face in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(3,7,6,2),(0,4,7,3),(1,2,6,5)]:
        quad(*(points[i] for i in face),tile)

def hardware_cylinder(a,b,radius,tile,sides=12):
    direction=[b[i]-a[i] for i in range(3)]
    length=math.sqrt(sum(x*x for x in direction));direction=[x/length for x in direction]
    reference=(0,1,0) if abs(direction[1])<.9 else (1,0,0)
    tangent=[direction[1]*reference[2]-direction[2]*reference[1],
             direction[2]*reference[0]-direction[0]*reference[2],
             direction[0]*reference[1]-direction[1]*reference[0]]
    length=math.sqrt(sum(x*x for x in tangent));tangent=[x/length for x in tangent]
    bitangent=[direction[1]*tangent[2]-direction[2]*tangent[1],
               direction[2]*tangent[0]-direction[0]*tangent[2],
               direction[0]*tangent[1]-direction[1]*tangent[0]]
    rings=[[(p[axis]+radius*(math.cos(i*math.tau/sides)*tangent[axis]
                          +math.sin(i*math.tau/sides)*bitangent[axis]))
            for axis in range(3)] for p in (a,b) for i in range(sides)]
    for i in range(sides):
        j=(i+1)%sides
        quad(rings[i],rings[j],rings[sides+j],rings[sides+i],tile)
        triangle(a,rings[j],rings[i],tile)
        triangle(b,rings[sides+i],rings[sides+j],tile)

for z in (-.2,.2):
    hardware_cylinder((-.26,.189,z),(.26,.189,z),.022,2)
    hardware_box((0,.225,z),(.18,.07,.085),1)
for index,(x,z) in enumerate([(-.256,-.2),(-.256,.2),(.256,-.2),(.346,.28)]):
    y=.1785 if index==3 else .189
    side=-1 if x<0 else 1
    outer=x+side*.07
    hardware_cylinder((outer,y,z),(outer+side*.009,y,z),.069,2,20)
    for i in range(6):
        angle=i*math.tau/6
        by,bz=y+math.sin(angle)*.045,z+math.cos(angle)*.045
        hardware_cylinder((outer+side*.010,by,bz),(outer+side*.016,by,bz),.007,1,6)
    for i in range(24):
        angle=i*math.tau/24
        hardware_box((x,y+math.cos(angle)*.176,z+math.sin(angle)*.176),(.10,.009,.028),1,angle)
for x in (-.245,.245):
    for z in (-.26,0,.26):
        hardware_cylinder((x,.276,z),(x,.284,z),.009,2,8)
for a,b in zip([(.12,.25,.10),(.20,.29,.08),(.26,.19,.06)],
               [(.20,.29,.08),(.26,.19,.06),(.31,.11,.02)]):
    hardware_cylinder(a,b,.008,1,8)

# Consistent planar density within each atlas tile; narrow details no longer stretch a whole tile.
for i in range(0,len(wreck),3):
    normal=wreck[i][2]
    axis=max(range(3),key=lambda a:abs(normal[a]))
    axes=((1,2),(0,2),(0,1))[axis]
    for j in range(i,i+3):
        p,uv,n=wreck[j]
        tile_x,tile_y=int(uv[0]*2)*.5,int(uv[1]*2)*.5
        uv=(tile_x+.25+p[axes[0]]*.38,tile_y+.25+p[axes[1]]*.38)
        wreck[j]=(p,uv,n)

floor_offset = .001-min(p[1] for p,_,_ in wreck)
wreck = [((p[0], p[1]+floor_offset, p[2]), uv, n) for p,uv,n in wreck]
write_obj(root/'security_rover_destroyed.obj', wreck, 'destroyed security rover')
