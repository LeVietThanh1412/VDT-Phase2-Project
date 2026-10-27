"""Select a genuine island detour INSIDE authoritative candidate 1 for PIE QA.

This is test-fixture planning only. Runtime still uses the native geographic A*.
"""
import sys,json,math,base64
from pathlib import Path
import numpy as np
from shapely.geometry import shape,Point,LineString
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
from common import ROOT,metric_geometry,en_to_ll
from validate_navigation_map import find_path

c=json.loads((ROOT/'Content/SeaNavigation/halong_navigation.json').read_text())
land=metric_geometry(shape(json.loads((ROOT/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']))
grid=np.frombuffer(base64.b64decode(c['cells_base64']),dtype=np.uint8).reshape(c['height'],c['width'])
cell=lambda p:(int((p[0]-c['min_east'])//100),int((p[1]-c['min_north'])//100))
center=lambda p:(c['min_east']+(p[0]+.5)*100,c['min_north']+(p[1]+.5)*100)
points=[center((int(x),int(y))) for y,x in np.argwhere(grid) if 13350<=center((x,y))[0]<=14450 and 2350<=center((x,y))[1]<=4150 and 200<Point(center((x,y))).distance(land)<600]
found=[]
for a in points:
    for b in points:
        direct=LineString([a,b]);dist=direct.length
        if not 750<dist<1300 or a[1]<=b[1] or not direct.intersects(land):continue
        raw=find_path(grid,cell(a),cell(b),100)
        if not raw:continue
        route=[center(p) for p in raw];line=LineString(route)
        if not all(13250<=p[0]<=15750 and 2250<=p[1]<=4750 for p in route):continue
        if line.distance(land)<200 or line.length/dist>1.8:continue
        found.append(dict(a_en_m=a,b_en_m=b,a_ll=list(map(float,en_to_ll(*a))),b_ll=list(map(float,en_to_ll(*b))),
            direct_distance_m=dist,path_length_m=line.length,minimum_shore_clearance_m=line.distance(land),
            start_shore_clearance_m=Point(a).distance(land),direct_intersects_real_island=True,reference_route_en_m=route))
assert found,'No valid fixture in selected candidate'
found.sort(key=lambda p:abs(p['start_shore_clearance_m']-250)+p['path_length_m']*.1)
out=dict(reference='SEEDED_FIELD_CANDIDATE_1.png',center_en_m=[14500,3500],width_m=2500,
    purpose='Phase 1 physical island traversal/near-coast regression; not sector implementation',selected=found[0])
(ROOT/'Docs/NavalCombat/NAVIGATION_ISLAND_FIXTURE.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
print(json.dumps(out,indent=2))
