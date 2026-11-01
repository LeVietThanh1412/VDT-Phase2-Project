"""Plan existing ship/target placements near main's fixed Cesium origin.

No Unreal or source dataset mutation. Routes use the existing geographic chart.
"""
import sys,json,base64,math
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw
from shapely.geometry import shape,Point,LineString
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
from common import ROOT,ll_to_en,en_to_ll,metric_geometry
from validate_navigation_map import find_path

chart=json.loads((ROOT/'Content/SeaNavigation/halong_navigation.json').read_text())
grid=np.frombuffer(base64.b64decode(chart['cells_base64']),dtype=np.uint8).reshape(chart['height'],chart['width'])
land=metric_geometry(shape(json.loads((ROOT/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']))
origin=list(map(float,ll_to_en(107.426513,20.944219)))
cell=lambda p:(math.floor((p[0]-chart['min_east'])/chart['cell_size']),math.floor((p[1]-chart['min_north'])/chart['cell_size']))
center=lambda x,y:(chart['min_east']+(x+.5)*chart['cell_size'],chart['min_north']+(y+.5)*chart['cell_size'])
water=[center(int(x),int(y)) for y,x in np.argwhere(grid)
       if math.dist(center(x,y),origin)<4000 and Point(center(x,y)).distance(land)>=350]
def pick(e,n):
    hint=(origin[0]+e,origin[1]+n)
    return min(water,key=lambda p:math.dist(p,hint))
def route(a,b):
    cells=find_path(grid,cell(a),cell(b),chart['cell_size']);assert cells,(a,b)
    points=[a]+[center(*c) for c in cells]+[b]
    line=LineString(points);assert not line.intersects(land)
    assert line.distance(land)>=250,(a,b,line.distance(land))
    return dict(points=points,length_m=line.length,land_clearance_m=line.distance(land))
specs=[
 ('Friendly_AI_1',0,(-3000,1300),(-2800,2400),'TP_Friendly_Patrol_A','TP_Friendly_Patrol_B'),
 ('Friendly_AI_2',0,(-1900,1300),(-1900,2400),'TP_Blue02_A','TP_Blue02_B'),
 ('BP_ShipEnemy',1,(-1400,2600),(-1400,1300),'TP_ShipEnemy_AStarDestination','TP_ShipEnemy_OpenSeaDestination'),
 ('BP_ShipEnemy2',1,(-900,2500),(-900,1300),'TP_Red02_A','TP_Red02_B'),
 ('BP_ShipEnemy3',1,(-400,1400),(-400,2400),'TP_Red03_A','TP_Red03_B')]
targets={};ships=[]
for label,team,a,b,an,bn in specs:
    a=pick(*a);b=pick(*b);r=route(a,b);back=route(b,a);targets[an]=a;targets[bn]=b
    direction=next(p for p in r['points'][1:] if math.dist(p,a)>20)
    ships.append(dict(label=label,team=team,spawn_en_m=a,a=an,b=bn,
                      yaw=math.degrees(math.atan2(-(direction[1]-a[1]),direction[0]-a[0])),
                      start_b=True,a_to_b=r,b_to_a=back,coast_clearance_m=Point(a).distance(land)))
player=pick(-1700,450);positions=[player]+[s['spawn_en_m'] for s in ships]
spacing=min(math.dist(a,b) for i,a in enumerate(positions) for b in positions[i+1:]);assert spacing>=400,spacing
for s in ships:
    s['spawn_llh']=[*map(float,en_to_ll(*s['spawn_en_m'])),9.7]
report=dict(origin_llh=[107.426513,20.944219,0],origin_chart_en=origin,ships=ships,targets=targets,
            target_llh={k:[*map(float,en_to_ll(*v)),3] for k,v in targets.items()},player_en_m=player,
            player_llh=[*map(float,en_to_ll(*player)),14.7],minimum_spawn_spacing_m=spacing,
            maximum_origin_distance_m=max(math.dist(p,origin) for p in positions))
out=ROOT/'Docs/QuangNinhIntegration';out.mkdir(exist_ok=True);(out/'ORIGIN_PATROL_PLAN.json').write_text(json.dumps(report,indent=2))
im=Image.new('RGB',(1100,1100),'#315a70');d=ImageDraw.Draw(im);xy=lambda p:(550+(p[0]-origin[0])/7,550-(p[1]-origin[1])/7)
for poly in getattr(land,'geoms',[land]):
    if poly.distance(Point(*origin))<5500:d.polygon([xy(p) for p in poly.exterior.coords],fill='#acbca0')
for s in ships:
    color='#56bcff' if s['team']==0 else '#ff7162'
    d.line([xy(p) for p in s['a_to_b']['points']],fill=color,width=3)
    for key in ['a','b']:
        x,y=xy(targets[s[key]]);d.ellipse((x-6,y-6,x+6,y+6),outline=color,width=3);d.text((x+9,y),s['label']+' '+key.upper(),fill=color)
x,y=xy(player);d.ellipse((x-7,y-7,x+7,y+7),fill='#ffff55');d.text((x+10,y),'Player base',fill='#ffff55')
x,y=xy(origin);d.ellipse((x-5,y-5,x+5,y+5),fill='white');d.text((x+10,y),'World origin (land)',fill='white')
im.save(out/'ORIGIN_PATROL_PLAN.png')
print(json.dumps({k:report[k] for k in ['origin_chart_en','minimum_spawn_spacing_m','maximum_origin_distance_m']},indent=2))
