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
shell([(.69,.19,.18,.01),(.75,.23,.22,.02),(.91,.20,.18,0),(.95,.15,.12,-.02)],3)
# Front collision bumper and sensor brow.
shell([(.25,.27,.08,.39),(.36,.30,.08,.39),(.40,.27,.06,.39)],2)
shell([(.78,.15,.035,.20),(.85,.15,.035,.20)],1)
for x in [-.32,.32]:
    for z in [-.25,.25]: wheel(x,z)

out = Path(__file__).resolve().parents[1]/'assets/models/security_rover.obj'
with out.open('w', encoding='ascii') as f:
    f.write('# RelayCore security rover: own mesh, front +Z, Y up\n')
    for p,uv,n in vertices: f.write('v '+' '.join(f'{x:.6f}' for x in p)+'\n')
    for p,uv,n in vertices: f.write('vt '+' '.join(f'{x:.6f}' for x in uv)+'\n')
    for p,uv,n in vertices: f.write('vn '+' '.join(f'{x:.6f}' for x in n)+'\n')
    for i in range(0,len(vertices),3): f.write('f '+' '.join(f'{j}/{j}/{j}' for j in range(i+1,i+4))+'\n')
print(f'{out}: {len(vertices)//3} triangles')
