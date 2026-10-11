"""VIS-26: wall-mounted route plates around retained gameplay route glyphs.

Reuse the facility's folded metal / enamel / screw vocabulary. All coordinates
are runtime metres; the exported room-fitted assemblies have origin [0,0,0].
The glyph positions are shared with the map installer, not inferred at runtime.
"""
from pathlib import Path
import importlib.util
import json
import bpy

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('transfer', ROOT/'scripts/export-pressure-transfer-glb.py')
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
DATA = json.loads((ROOT/'content/graphics/route_signage/placements.json').read_text('utf-8'))

def point(pos, facing, u=0, v=0, depth=0):
    # Local +u follows the retained arrow mesh's +X direction after yaw.
    n, t = {'+z': ((0,0,1),(1,0,0)), '-z': ((0,0,-1),(-1,0,0)),
            '+x': ((1,0,0),(0,0,-1)), '-x': ((-1,0,0),(0,0,1))}[facing]
    return tuple(pos[i]+u*t[i]+depth*n[i]+(v if i==1 else 0) for i in range(3))

def plate(pos, facing, title, subtitle, m, glyph=True, width=2.4, stand_off=.08):
    height = .9 if width < 1.5 else .72
    dims = (.028,height,width) if facing.endswith('x') else (width,height,.028)
    h.bay_box('Folded galvanized carrier', point(pos,facing,depth=-.026), dims, m['steel'], .006)
    dims = (.008,height-.06,width-.06) if facing.endswith('x') else (width-.06,height-.06,.008)
    h.bay_box('Sage enamel face', point(pos,facing,depth=-.008), dims,m['paint'],.003)
    if stand_off:
        for u in (-width/2+.18, width/2-.18):
            size=(stand_off-.04,.50,.08) if facing.endswith('x') else (.08,.50,stand_off-.04)
            h.bay_box('Wall stand-off bracket', point(pos,facing,u,depth=-(stand_off+.04)/2),size,m['steel'],.004)
    for u in (-width/2+.08,width/2-.08):
        for v in (-height/2+.09,height/2-.09):
            a,b = point(pos,facing,u,v,-.006), point(pos,facing,u,v,.003)
            h.rod('Captive fixing', (a[0],-a[2],a[1]), (b[0],-b[2],b[1]),.013,m['steel'],8)
    u = .36 if glyph and width >= 1.5 else 0
    lines = ((title,-.31,.12),) if width < 1.5 else ((title,.07,.145),(subtitle,-.17,.105))
    for text,v,size in lines:
        obj=h.bay_label(text,point(pos,facing,u,v,.004),size,m['ivory'],facing)
        bpy.context.view_layer.update()
        allowed=(width-.15) if width < 1.5 else (1.40 if glyph else 2.10)
        if obj.dimensions.length > 0 and max(obj.dimensions.x,obj.dimensions.y)>allowed:
            obj.scale *= allowed/max(obj.dimensions.x,obj.dimensions.y)

def build(sector, layout):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    m={key:h.material('route_'+key,rgb,rough,metal) for key,rgb,rough,metal in (
        ('steel',(.38,.41,.39),.68,.65),('paint',(.105,.16,.12),.78,0),
        ('ivory',(.77,.74,.60),.82,0))}
    for item in layout['cues']:
        plate(item['center'],item['facing'],item['title'],item['subtitle'],m,width=item.get('width',2.4),stand_off=item.get('stand_off',.08))
    for item in layout['returns']:
        plate(item['center'],item['facing'],item['title'],item['subtitle'],m,False)
    return m

if __name__ == '__main__':
    import sys
    if '--review-plate' in sys.argv:
        build('review', {'cues': [{'center':[0,0,0], 'facing':'+z',
              'title':'PRESSURE', 'subtitle':'BYPASS / PT-03'}], 'returns':[]})
        review=ROOT/'build/blender-review/route_signage/plate.blend'
        review.parent.mkdir(parents=True,exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(review))
    else:
        for sector, layout in DATA.items():
            build(sector,layout)
            h.export(sector+'_route_signage')
