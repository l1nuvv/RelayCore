"""Loaded by export-native-lab-glb.py; CC0 anatomy and authored laboratory workwear."""
import gzip
import random
from collections import defaultdict
from mathutils import Vector

scientist = assembly("Laboratory technician - cotton coat and fitted PPE")
cloth = material("Ivory cotton twill", (.66, .65, .60))
cloth.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .88
# Fine cotton variation stays below the garment seams in the visual hierarchy.
cotton = bpy.data.images.new("Fine ivory cotton albedo", width=256, height=256)
random_source = random.Random(17)
pixels = []
for y in range(256):
    for x in range(256):
        variation = random_source.uniform(-.008,.008)+.002*sin(x*pi/2)*sin(y*pi/2)
        pixels.extend((.66+variation,.65+variation,.60+variation,1))
cotton.pixels = pixels
cotton.pack()
texture = cloth.node_tree.nodes.new("ShaderNodeTexImage")
texture.image = cotton
cloth.node_tree.links.new(texture.outputs["Color"],cloth.node_tree.nodes.get("Principled BSDF").inputs["Base Color"])
cloth["uv_metres"] = 4
seam = material("Cotton hems and pocket welts", (.56, .55, .50))
trousers = material("Charcoal work trousers", (.075, .085, .080))
rubber = material("Smooth charcoal protective rubber", (.027, .033, .030))
rubber.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .66
leather = material("Dark brown safety boot leather", (.055, .043, .033))
skin = material("Technician skin", (.48, .30, .22))
metal = material("Dull nickel fasteners", (.30, .31, .29))
metal.node_tree.nodes.get("Principled BSDF").inputs["Metallic"].default_value = .7
lens = material("Smoked safety lenses", (.10, .15, .13))
lens.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = .16
positions, groups = [], defaultdict(list)
group = ""
for line in (root / "content/third_party/makehuman/base.obj").read_text().splitlines():
    fields = line.split()
    if not fields:
        continue
    if fields[0] == "v":
        positions.append(Vector(tuple(map(float, fields[1:4]))))
    elif fields[0] == "g":
        group = fields[1]
    elif fields[0] == "f":
        groups[group].append(tuple(int(v.split("/")[0]) - 1 for v in fields[1:]))
with gzip.open(root / "content/third_party/makehuman/male.target.gz", "rt") as source:
    for line in source:
        fields = line.split()
        if fields and fields[0].isdigit():
            positions[int(fields[0])] += Vector(tuple(map(float, fields[1:4])))
body_indices = {i for face in groups["body"] for i in face}
floor = min(positions[i].y for i in body_indices)
scale = 1.78 / (max(positions[i].y for i in body_indices) - floor)
positions = [Vector((p.x * scale, -p.z * scale, (p.y - floor) * scale)) for p in positions]


def joint(name):
    indices = {i for face in groups[name] for i in face}
    return sum((positions[i] for i in indices), Vector()) / len(indices)


def shaped_mesh(name, vertices, faces, finish, thickness=0, subdiv=0):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    mesh.materials.append(finish)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    obj.parent = scientist
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    bpy.context.view_layer.objects.active = obj
    if subdiv:
        modifier = obj.modifiers.new("Tailored cloth curvature", "SUBSURF")
        modifier.levels = subdiv
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    if thickness:
        modifier = obj.modifiers.new("Material thickness at openings", "SOLIDIFY")
        modifier.thickness = thickness
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return obj


def anatomy(name, faces, finish, transform=lambda p: p, expand=0, subdiv=0):
    indices = sorted({i for face in faces for i in face})
    remap = {old: new for new, old in enumerate(indices)}
    obj = shaped_mesh(name, [positions[i] for i in indices],
                      [tuple(remap[i] for i in f) for f in faces], finish)
    for vertex in obj.data.vertices:
        vertex.co = transform(vertex.co + vertex.normal * expand)
    if subdiv:
        bpy.context.view_layer.objects.active = obj
        modifier = obj.modifiers.new("Anatomical surface curvature", "SUBSURF")
        modifier.levels = subdiv
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return obj


def ribbon(name, points, width, finish, across=(1, 0, 0)):
    offset = Vector(across) * width / 2
    vertices = [Vector(p) + d * offset for p in points for d in (-1, 1)]
    return shaped_mesh(name, vertices,
                       [(2*i, 2*i+1, 2*i+3, 2*i+2) for i in range(len(points)-1)], finish, .002)


def rings(name, sections, finish, sides=40, opening=0, thickness=0, subdiv=0):
    # Sections are garment patterns, independent of the anatomical body surface.
    vertices = []
    for z, cx, cy, rx, ry, fold in sections:
        for i in range(sides):
            angle = -pi/2 + opening/2 + (2*pi-opening) * i/(sides-1) if opening else i*2*pi/sides
            ripple = fold * (sin(5*angle + z*9) + .4*sin(9*angle-z*13))
            vertices.append((cx+(rx+ripple)*cos(angle), cy+(ry+ripple)*sin(angle), z))
    faces = [(r*sides+i, r*sides+(i+1)%sides, (r+1)*sides+(i+1)%sides, (r+1)*sides+i)
             for r in range(len(sections)-1) for i in range(sides-1 if opening else sides)]
    return shaped_mesh(name, vertices, faces, finish, thickness, subdiv)


# A knee-length coat hangs from the shoulders; the skirt does not follow the legs.
coat_sections = [(.64,0,.008,.252,.155,.007),(.655,0,.008,.252,.155,.007),
                 (.72,0,.008,.245,.152,.008),(.83,0,.008,.235,.148,.007),
                 (.96,0,.006,.222,.144,.006),(1.06,0,.003,.203,.137,.004),
                 (1.17,0,.001,.196,.137,.004),(1.30,0,0,.205,.146,.002),
                 (1.38,0,0,.208,.140,0),(1.43,0,0,.199,.120,0),
                 (1.47,0,-.005,.162,.101,0),(1.50,0,-.008,.080,.070,0),
                 (1.515,0,-.008,.067,.065,0)]
coat = rings("Separate loose laboratory coat", coat_sections, cloth, 48, .04, .003, 2)
rings("Turned coat hem", [(z,cx,cy,rx+.001,ry+.001,f) for z,cx,cy,rx,ry,f in coat_sections[:2]], seam,48,.04,.002)
for side in (-1, 1):
    rings("Straight work trouser leg", [(.15,side*.14,0,.057,.055,.001),
          (.17,side*.14,0,.063,.060,.003),(.22,side*.14,0,.066,.064,.005),
          (.29,side*.14,0,.063,.062,.002),(.41,side*.135,0,.069,.067,.001),
          (.51,side*.13,-.015,.074,.070,.004),(.56,side*.125,-.008,.079,.073,.003),
          (.68,side*.12,0,.087,.083,.001),(.85,side*.10,0,.103,.095,0),
          (1.03,side*.095,0,.106,.10,0)], trousers, 32, subdiv=1)
    shoulder = joint("joint-l-shoulder" if side > 0 else "joint-r-shoulder")
    old_elbow = joint("joint-l-elbow" if side > 0 else "joint-r-elbow")
    old_wrist = joint("joint-l-hand" if side > 0 else "joint-r-hand")
    elbow = Vector((side*.285,.005,1.18))
    wrist = Vector((side*.315,-.035,.965))
    rotation = (old_wrist-old_elbow).rotation_difference(wrist-elbow)
    direction = (old_wrist-old_elbow).normalized()
    hand_faces = [f for f in groups["body"]
                  if all(side*positions[i].x > .30 and (positions[i]-old_wrist).dot(direction) > -.020 for i in f)]
    anatomy("Fitted five-finger rubber glove", hand_faces, rubber,
            lambda p: wrist + rotation @ (p-old_wrist), .002, 1)
    vertices = []
    sleeve_sections = [(Vector((side*.145,0,1.435)),.076),
                       (Vector((side*.185,0,1.425)),.084),
                       (shoulder.lerp(elbow,.30),.088),
                       (shoulder.lerp(elbow,.65),.075),(elbow+Vector((0,0,.025)),.071),
                       (elbow,.070),(elbow.lerp(wrist,.18),.068),
                       (elbow.lerp(wrist,.40),.064),(elbow.lerp(wrist,.66),.058),
                       (elbow.lerp(wrist,.87),.053),(elbow.lerp(wrist,.93),.049),
                       (elbow.lerp(wrist,.96),.047)]
    for row,(center,radius) in enumerate(sleeve_sections):
        axis = ((elbow-shoulder) if row<3 else
                (sleeve_sections[min(row+1,len(sleeve_sections)-1)][0]
                 - sleeve_sections[max(0,row-1)][0])).normalized()
        across = axis.cross(Vector((0,1,0))).normalized()
        depth = axis.cross(across)
        for i in range(32):
            a = i*2*pi/32
            fold = .003*sin(a*4+row*1.8) if row in (4,5,6,9) else 0
            vertices.append(center+(radius+fold)*(across*cos(a)+depth*sin(a)*.94))
    faces = [(r*32+i,r*32+(i+1)%32,(r+1)*32+(i+1)%32,(r+1)*32+i)
             for r in range(len(sleeve_sections)-1) for i in range(32)]
    faces.append(tuple(reversed(range(32))))
    shaped_mesh("Set-in sleeve with elbow ease and turned cuff", vertices,faces,cloth,.003,2)
    glove_axis = (wrist-elbow).normalized()
    across = glove_axis.cross(Vector((0,1,0))).normalized()
    depth = glove_axis.cross(across)
    vertices = [wrist+glove_axis*t + radius*(across*cos(i*2*pi/32)+depth*sin(i*2*pi/32))
                for t,radius in [(-.045,.041),(-.025,.042),(.010,.036),(.025,.031)] for i in range(32)]
    shaped_mesh("Glove gauntlet inside coat cuff",vertices,
                [(r*32+i,r*32+(i+1)%32,(r+1)*32+(i+1)%32,(r+1)*32+i) for r in range(3) for i in range(32)],rubber,.002)
    cx = side*.14
    boot_sections = [(.009,cx,-.060,.060,.143,0),(.032,cx,-.060,.060,.143,0),
                     (.045,cx,-.060,.058,.139,0),(.075,cx,-.062,.057,.134,0),
                     (.105,cx,-.040,.055,.105,0),(.14,cx,-.005,.049,.063,0),
                     (.18,cx,.005,.049,.051,0),(.205,cx,.005,.050,.051,0)]
    rings("Shaped safety boot outsole",boot_sections[:3],rubber,40)
    rings("Leather safety boot upper",boot_sections[2:],leather,40,thickness=.003)
    for z,y in ((.105,-.145),(.12,-.115),(.14,-.080)):
        ribbon("Boot lace",[(cx-.023,y,z),(cx+.023,y,z+.007)],.003,rubber,(0,0,1))

# The real face and ears remain visible around PPE, rather than a fabric-covered head.
head = anatomy("Anatomical head and continuous neck",[f for f in groups["body"]
               if max(positions[i].z for i in f)>1.50],skin)
editable = bmesh.new()
editable.from_mesh(head.data)
bmesh.ops.bisect_plane(editable,geom=list(editable.verts)+list(editable.edges)+list(editable.faces),
                      plane_co=(0,0,1.535),plane_no=(0,0,-1),clear_outer=True,dist=.00001)
neck_edges = [e for e in editable.edges if e.is_boundary and all(abs(v.co.z-1.535)<.0001 for v in e.verts)]
extension = bmesh.ops.extrude_edge_only(editable,edges=neck_edges)
for vertex in extension["geom"]:
    if isinstance(vertex,bmesh.types.BMVert):
        vertex.co.x *= .86
        vertex.co.z = 1.49
editable.normal_update()
editable.to_mesh(head.data)
editable.free()
bpy.context.view_layer.objects.active = head
modifier = head.modifiers.new("Anatomical surface curvature","SUBSURF")
modifier.levels = 1
bpy.ops.object.modifier_apply(modifier=modifier.name)
for side in (-1,1):
    shaped_mesh("Folded coat lapel",[(side*.065,-.068,1.515),(side*.120,-.115,1.46),
                (side*.065,-.153,1.35),(side*.018,-.151,1.41)],[(0,1,2,3)],cloth,.004)
ring = [(cos(a)*.069,-.008+sin(a)*.068,1.508+.012*max(0,sin(a))) for a in [i*pi/24 for i in range(25)]]
ribbon("Coat collar stand",ring,.018,cloth,(0,0,1))
for side in (-1,1):
    x=side*.15
    shaped_mesh("Coat patch pocket",[(x-.053,-.135,.82),(x+.053,-.135,.82),
                (x+.053,-.146,.96),(x-.053,-.146,.96)],[(0,1,2,3)],cloth,.003)
    ribbon("Pocket mouth welt",[(x-.053,-.149,.955),(x+.053,-.149,.955)],.009,seam,(0,0,1))

# Low-profile oval goggles follow the eye sockets; straps wrap around the cranium.
for side in (-1,1):
    centre=joint("joint-l-eye" if side>0 else "joint-r-eye")
    for name,rx,rz,y,finish in [("Rubber safety goggle seal",.035,.025,-.150,rubber),
                               ("Smoked oval safety lens",.029,.020,-.154,lens)]:
        vertices=[(centre.x,y,centre.z)]+[(centre.x+rx*cos(i*2*pi/40),y+.008*abs(cos(i*2*pi/40)),centre.z+rz*sin(i*2*pi/40)) for i in range(40)]
        shaped_mesh(name,vertices,[(0,1+i,1+(i+1)%40) for i in range(40)],finish,.003)
box("Goggle bridge",(0,-.153,1.673),(.015,.012,.009),rubber,scientist,.003)
points=[]
for i in range(49):
    a=-pi/2+i*2*pi/48
    points.append((.087*cos(a),-.038+.104*sin(a),1.678))
ribbon("Continuous goggle head strap",points,.014,rubber,(0,0,1))
outline=[(-.027,-.157,1.643),(-.053,-.144,1.616),(-.050,-.142,1.582),
         (-.027,-.148,1.563),(.027,-.148,1.563),(.050,-.142,1.582),
         (.053,-.144,1.616),(.027,-.157,1.643),(0,-.181,1.650)]
vertices=outline+[(x*.60,y-.027,1.603+(z-1.603)*.7) for x,y,z in outline]
faces=[(i,(i+1)%9,9+(i+1)%9,9+i) for i in range(9)]+[tuple(range(9,18))]
shaped_mesh("Fitted cupped half-face respirator",vertices,faces,rubber,.003,1)
for side in (-1,1):
    cylinder("Replaceable respirator cartridge",(side*.047,-.181,1.596),.024,.027,rubber,scientist,(pi/2,side*.40,0),32)
    for z in (1.586,1.596,1.606):
        box("Cartridge intake slot",(side*.052,-.199,z),(.026,.003,.0025),metal,scientist,.001)
    points=[(side*.050,-.144,1.615),(side*.078,-.083,1.625),
            (side*.088,-.020,1.638),(side*.062,.046,1.65),(0,.065,1.65)]
    ribbon("Respirator cheek strap",points,.012,rubber,(0,0,1))

# The apron follows the coat front and hangs freely below the waist.
vertices=[]
for row in range(19):
    t=row/18
    z=.71+.61*t
    width=.195 if t<.55 else .195-(t-.55)/.45*.093
    y=-.173+.012*max(0,min(1,(z-.98)/.16))
    for col in range(17):
        u=col/8-1
        vertices.append((u*width,y+.023*u*u+.007*sin(u*8)*max(0,1-t),z+.006*abs(u)**8*max(0,1-t*8)))
shaped_mesh("Cut rubber laboratory apron",vertices,
            [(r*17+i,r*17+i+1,(r+1)*17+i+1,(r+1)*17+i) for r in range(18) for i in range(16)],rubber,.003)
for side in (-1,1):
    ribbon("Apron shoulder strap",[(side*.082,-.159,1.32),(side*.088,-.150,1.38),
           (side*.093,-.123,1.45),(side*.088,-.090,1.487),
           (side*.085,-.025,1.510),(side*.085,.050,1.485),
           (side*.075,.092,1.45),(0,.115,1.40)],.021,rubber)
points=[(.211*cos(i*2*pi/64),.003+.145*sin(i*2*pi/64),1.06) for i in range(65)]
ribbon("Apron waist belt",points,.023,rubber,(0,0,1))
export("scientist_hazmat")
