"""Choose spawn cells from the real coastline chart and audit all initial routes.

Hints describe the bay/open-water basins inspected in MainMapLevel and the OSM
overview. Actual placements are accepted grid centers with >=350m vessel spacing.
This independent reference search is an audit, never a gameplay replacement.
"""
import sys,json,base64,math
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
import numpy as np
from shapely.geometry import shape,Point,LineString
from common import ROOT,metric_geometry
from validate_navigation_map import find_path

chart=json.loads((ROOT/'Content/SeaNavigation/halong_navigation.json').read_text())
land=metric_geometry(shape(json.loads((ROOT/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']))
buffer=shape(json.loads((ROOT/'Data/SeaNavigation/halong_buffered_enu.geojson').read_text())['geometry'])
grid=np.frombuffer(base64.b64decode(chart['cells_base64']),dtype=np.uint8).reshape(chart['height'],chart['width'])
size=chart['cell_size']
cell=lambda p:(math.floor((p[0]-chart['min_east'])/size),math.floor((p[1]-chart['min_north'])/size))
center=lambda c:(chart['min_east']+(c[0]+.5)*size,chart['min_north']+(c[1]+.5)*size)
water=[center((int(x),int(y))) for y,x in np.argwhere(grid)]
points=[]
spec=[('Player',0,(350,-1950)),('Friendly_AI_1',0,(750,-1950)),('Friendly_AI_2',0,(250,-2550)),
      ('BP_ShipEnemy',1,(-250,-1950)),('Enemy_AI_2',1,(-650,-2350)),('Enemy_AI_3',1,(950,-1050))]
for name,team,hint in spec:
    choices=sorted(water,key=lambda p:math.dist(p,hint))
    chosen=next(p for p in choices if all(math.dist(p,x['en_m'])>=350 for x in points) and Point(p).distance(land)>=250)
    assert math.dist(chosen,hint)<350,'Could not fit a safe spawn into the inspected basin'
    points.append(dict(label=name,team=team,en_m=chosen,coast_clearance_m=Point(chosen).distance(land)))
targets={'TP_ShipEnemy_AStarDestination':(-406.2,-1179.5),'TP_ShipEnemy_OpenSeaDestination':(-59.99996,-2350.69986)}
for name,hint in [('TP_Friendly_Patrol_A',(350,-2350)),('TP_Friendly_Patrol_B',(1050,-1550))]:
    targets[name]=min(water,key=lambda p:math.dist(p,hint))
offsets={'Friendly_AI_1':(150,0),'Friendly_AI_2':(-150,-150),'BP_ShipEnemy':(0,0),'Enemy_AI_2':(250,150),'Enemy_AI_3':(450,150)}
routes=[]
for ship in points[1:]:
    names=list(targets)[:2] if ship['team']==1 else list(targets)[2:]
    ship['start_at_b']=ship['label'] in ['Friendly_AI_1','Friendly_AI_2','Enemy_AI_2']
    ship['offset_m']=offsets[ship['label']]
    base=targets[names[1] if ship['start_at_b'] else names[0]]
    goal=tuple(base[i]+ship['offset_m'][i] for i in range(2))
    route=find_path(grid,cell(ship['en_m']),cell(goal),size);assert route
    en=[ship['en_m']]+[center(c) for c in route]+[goal]
    line=LineString(en);assert not line.intersects(buffer)
    ship['patrol_a'],ship['patrol_b']=names
    # Face the first actual chart waypoint; world Y points south.
    next_point=next(p for p in en[1:] if math.dist(p,ship['en_m'])>20)
    ship['world_yaw']=math.degrees(math.atan2(-(next_point[1]-ship['en_m'][1]),next_point[0]-ship['en_m'][0]))
    routes.append(dict(ship=ship['label'],en_m=en,length_m=line.length,coast_clearance_m=line.distance(land)))
for a,b in [(list(targets)[0],list(targets)[1]),(list(targets)[2],list(targets)[3])]:
    assert find_path(grid,cell(targets[a]),cell(targets[b]),size)
minimum=min(math.dist(a['en_m'],b['en_m']) for i,a in enumerate(points) for b in points[i+1:])
report=dict(coordinate_system='Fixed coastline-chart East/North meters, world placement through Cesium',
    geography='Friendly bay: south of the existing patrol island, west of the large southeastern mountain; open-water exit northeast. Red 1/2 west/southwest of the patrol island; Red 3 on its east side.',
    ships=points,targets=targets,routes=routes,minimum_spawn_spacing_m=minimum,
    settings=dict(detection_m=1800,engagement_m=1200,retreat_m=450,reengagement_m=800,safe_separation_m=400,cooldown_s=4,scan_s=.5,cruise_mps=8))
out=ROOT/'Docs/NavalCombat/BATTLEFIELD_PLAN.json';out.write_text(json.dumps(report,indent=2),encoding='utf8')
print(json.dumps({k:v for k,v in report.items() if k!='routes'},indent=2))
