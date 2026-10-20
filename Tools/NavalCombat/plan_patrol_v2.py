"""Audit compact per-ship patrols against the actual coastline chart."""
import sys,json,base64,math
from pathlib import Path
import numpy as np
from shapely.geometry import shape,Point,LineString,MultiPoint
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
from common import ROOT,metric_geometry
from validate_navigation_map import find_path
chart=json.loads((ROOT/'Content/SeaNavigation/halong_navigation.json').read_text())
audit=json.loads((ROOT/'Docs/NavalCombat/CURRENT_PATROL_AUDIT.json').read_text())
land=metric_geometry(shape(json.loads((ROOT/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']))
grid=np.frombuffer(base64.b64decode(chart['cells_base64']),dtype=np.uint8).reshape(chart['height'],chart['width'])
size=chart['cell_size']
cell=lambda p:(math.floor((p[0]-chart['min_east'])/size),math.floor((p[1]-chart['min_north'])/size))
center=lambda c:(chart['min_east']+(c[0]+.5)*size,chart['min_north']+(c[1]+.5)*size)
region=MultiPoint([(83.39935681,-3058.20231920),(1384.90097508,-2314.30109534),(-347.66365371,-1166.68053293),(726.99096226,-641.33062018)]).convex_hull
assert region.is_valid and len(region.exterior.coords)==5
water=[center((int(x),int(y))) for y,x in np.argwhere(grid) if region.covers(Point(center((x,y)))) and Point(center((x,y))).distance(land)>=250]
pick=lambda hint:min(water,key=lambda p:math.dist(p,hint))
old={s['label']:s for s in audit['ships']}
targets={
 'TP_Friendly_Patrol_A':old['Friendly_AI_1']['a']['en_m'],
 'TP_Friendly_Patrol_B':old['Friendly_AI_1']['b']['en_m'],
 'TP_ShipEnemy_AStarDestination':old['BP_ShipEnemy']['a']['en_m'],
 'TP_ShipEnemy_OpenSeaDestination':pick(old['BP_ShipEnemy']['b']['en_m']),
 'TP_Blue02_A':pick((50,-2450)), 'TP_Blue02_B':pick((750,-1250)),
 'TP_Red02_A':pick((350,-850)), 'TP_Red02_B':pick((650,-2150)),
 'TP_Red03_A':pick((850,-850)), 'TP_Red03_B':pick((950,-1950))}
specs=[('Friendly_AI_1',0,(450,-2450),'TP_Friendly_Patrol_A','TP_Friendly_Patrol_B'),
 ('Friendly_AI_2',0,(50,-2450),'TP_Blue02_A','TP_Blue02_B'),
 ('BP_ShipEnemy',1,(-50,-1050),'TP_ShipEnemy_AStarDestination','TP_ShipEnemy_OpenSeaDestination'),
 ('BP_ShipEnemy2',1,(350,-850),'TP_Red02_A','TP_Red02_B'),
 ('BP_ShipEnemy3',1,(850,-850),'TP_Red03_A','TP_Red03_B')]
def route(a,b):
    path=find_path(grid,cell(a),cell(b),size);assert path,(a,b)
    points=[a]+[center(p) for p in path]+[b]
    line=LineString(points);assert not line.intersects(land)
    return dict(points=points,length_m=line.length,minimum_land_clearance_m=line.distance(land))
ships=[]
for label,team,hint,a,b in specs:
    spawn=pick(hint);first=route(spawn,targets[b]);out=route(targets[a],targets[b]);back=route(targets[b],targets[a])
    next_point=next(p for p in first['points'][1:] if math.dist(p,spawn)>20)
    ships.append(dict(label=label,team=team,spawn_en_m=spawn,yaw=math.degrees(math.atan2(-(next_point[1]-spawn[1]),next_point[0]-spawn[0])),
        a=a,b=b,start_b=True,initial=first,a_to_b=out,b_to_a=back,coast_clearance_m=Point(spawn).distance(land)))
player=pick((50,-2050));starts=[player]+[s['spawn_en_m'] for s in ships]
spacing=min(math.dist(a,b) for i,a in enumerate(starts) for b in starts[i+1:]);assert spacing>=350
for p in targets.values():
    x,y=cell(p);assert grid[y,x]
report=dict(reference_polygon_en_m=list(region.exterior.coords),region_area_m2=region.area,targets=targets,ships=ships,
    player_en_m=player,minimum_spawn_spacing_m=spacing,cruise_mps=20,offsets=[0,0,0],arrival_radius_m=80)
(ROOT/'Docs/NavalCombat/PATROL_V2_PLAN.json').write_text(json.dumps(report,indent=2))
print(json.dumps(dict(targets=targets,minimum_spawn_spacing_m=spacing,ships=[{k:v for k,v in s.items() if k not in ['initial','a_to_b','b_to_a']} for s in ships]),indent=2))
