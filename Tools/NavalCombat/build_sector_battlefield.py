"""Build lightweight sectors for the EXACT selected candidate, without actors.

Uses source coastline polygons and the existing conservative navigation chart.
This prepares spatial data; it does not certify physical spawns or bathymetry.
"""
import sys,json,math,base64,hashlib,argparse
from pathlib import Path
from collections import deque
import numpy as np
from shapely.geometry import shape,Point,Polygon,LineString,box
from PIL import Image,ImageDraw
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
from common import ROOT,metric_geometry,en_to_ll
from validate_navigation_map import find_path

args=argparse.ArgumentParser();args.add_argument('--sector-size',type=float,default=100);opt=args.parse_args()
assert 25<=opt.sector_size<=250
source=ROOT/'Content/SeaNavigation/halong_navigation.json';c=json.loads(source.read_text())
candidate=json.loads((ROOT/'Docs/QuangNinhIntegration/SEEDED_FIELD_CANDIDATES.json').read_text())['candidates'][0]
assert candidate['center_en_m']==[14500,3500] and candidate['width_m']==2500 and candidate['yaw_deg']==0
land=metric_geometry(shape(json.loads((ROOT/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']))
grid=np.frombuffer(base64.b64decode(c['cells_base64']),dtype=np.uint8).reshape(c['height'],c['width'])
lo_e,lo_n,width,height=13250.,2250.,2500.,2500.;size=opt.sector_size
cols=math.ceil(width/size);rows=math.ceil(height/size)
cell=lambda p:(math.floor((p[0]-c['min_east'])/c['cell_size']),math.floor((p[1]-c['min_north'])/c['cell_size']))
center=lambda p:(c['min_east']+(p[0]+.5)*100,c['min_north']+(p[1]+.5)*100)
valid=lambda p:0<=cell(p)[0]<c['width'] and 0<=cell(p)[1]<c['height'] and bool(grid[cell(p)[1],cell(p)[0]])
zones={'BLUE_DEFENSE_CORE':[.12,.46,.30,.82], 'BLUE_WEST_ISLAND_PATROL':[.08,.46,.28,.90],
       'BLUE_NORTH_WATCH':[.12,.49,.08,.36], 'RED_STAGING':[.64,.92,.06,.28],
       'RED_APPROACH':[.58,.85,.32,.72], 'CENTER_CROSSING':[.43,.62,.24,.68]}
islands=[p for p in getattr(land,'geoms',[land]) if p.intersects(box(lo_e-500,lo_n-500,lo_e+width+500,lo_n+height+500))]
sectors=[]
for row in range(rows):
    for col in range(cols):
        x0=lo_e+col*size;x1=min(lo_e+width,x0+size)
        y1=lo_n+height-row*size;y0=max(lo_n,y1-size)
        polygon=box(x0,y0,x1,y1);en=((x0+x1)/2,(y0+y1)/2)
        samples=[]
        for fx in [.2,.5,.8]:
            for fy in [.2,.5,.8]:
                p=(x0+(x1-x0)*fx,y0+(y1-y0)*fy)
                if valid(p):samples.append(dict(en_m=p,shore_clearance_m=Point(p).distance(land)))
        samples.sort(key=lambda p:(-p['shore_clearance_m'],math.dist(p['en_m'],en),p['en_m']))
        anchor=samples[0]['en_m'] if samples else en;clearance=Point(anchor).distance(land)
        u=(anchor[0]-lo_e)/width;v=(lo_n+height-anchor[1])/height
        membership=[name for name,(u0,u1,v0,v1) in zones.items() if u0<=u<=u1 and v0<=v<=v1 and samples]
        sectors.append(dict(id=f'S{row:02d}_{col:02d}',row=row,column=col,center_en_m=en,
            anchor_en_m=anchor,longitude_latitude=list(map(float,en_to_ll(*anchor))),uv=[u,v],
            team_region='Blue' if x1<=14500 else 'Red' if x0>=14500 else 'Center',
            water_area_fraction=1-polygon.intersection(land).area/polygon.area,
            chart_sample_fraction=len(samples)/9,shore_clearance_m=clearance,navigable=bool(samples),
            suitable_small=bool(samples) and clearance>=65,suitable_medium=bool(samples) and clearance>=112,
            suitable_large=bool(samples) and clearance>=112,spawn_eligible=bool(samples) and clearance>=200,
            patrol_eligible=bool(samples) and clearance>=112,connected_region=0,
            nearby_islands=[i for i,p in enumerate(islands) if polygon.distance(p)<500],zones=membership,
            samples=samples))

# Connectivity here is a sector-data aid. Gameplay continues to use native A*.
# Neighbor links require an existing chart-water path contained in their union.
adj=[[] for _ in sectors]
for i,s in enumerate(sectors):
    if not s['navigable']:continue
    for dc,dr in [(1,0),(0,1),(1,1),(-1,1)]:
        x=s['column']+dc;y=s['row']+dr
        if not (0<=x<cols and 0<=y<rows):continue
        j=y*cols+x;t=sectors[j]
        if not t['navigable']:continue
        path=find_path(grid,cell(s['anchor_en_m']),cell(t['anchor_en_m']),100)
        if path and len(path)<=5 and all(lo_e<=center(p)[0]<=lo_e+width and lo_n<=center(p)[1]<=lo_n+height for p in path):adj[i].append(j);adj[j].append(i)
region=0
for i,s in enumerate(sectors):
    if not s['navigable'] or s['connected_region']:continue
    region+=1;q=deque([i]);s['connected_region']=region
    while q:
        for j in adj[q.popleft()]:
            if not sectors[j]['connected_region']:sectors[j]['connected_region']=region;q.append(j)
counts={r:sum(s['connected_region']==r for s in sectors) for r in range(1,region+1)}
main=max(counts,key=counts.get)
for s in sectors:
    s['spawn_eligible'] &= s['connected_region']==main
    s['patrol_eligible'] &= s['connected_region']==main
representatives={}
for name,bounds in zones.items():
    options=[s for s in sectors if name in s['zones'] and s['connected_region']==main and s['patrol_eligible']]
    assert options,name
    hint=((bounds[0]+bounds[1])/2,(bounds[2]+bounds[3])/2)
    representatives[name]=min(options,key=lambda s:math.dist(s['uv'],hint))['id']
connections=[]
for a,b in [('RED_STAGING','RED_APPROACH'),('RED_APPROACH','CENTER_CROSSING'),('CENTER_CROSSING','BLUE_DEFENSE_CORE'),('BLUE_DEFENSE_CORE','BLUE_WEST_ISLAND_PATROL'),('BLUE_WEST_ISLAND_PATROL','BLUE_NORTH_WATCH')]:
    sa=next(s for s in sectors if s['id']==representatives[a]);sb=next(s for s in sectors if s['id']==representatives[b])
    path=find_path(grid,cell(sa['anchor_en_m']),cell(sb['anchor_en_m']),100);assert path,(a,b)
    points=[sa['anchor_en_m']]+[center(p) for p in path]+[sb['anchor_en_m']]
    assert all(lo_e<=p[0]<=lo_e+width and lo_n<=p[1]<=lo_n+height for p in points),(a,b,'outside field')
    assert not LineString(points).intersects(land)
    connections.append(dict(start_zone=a,end_zone=b,start_sector=sa['id'],end_sector=sb['id'],reference_route_en_m=points))
data=dict(format='VDT_NAVAL_SECTORS',version=1,coordinate_system=c['coordinate_system'],
    reference_image='Docs/QuangNinhIntegration/SEEDED_FIELD_CANDIDATE_1.png',center_en_m=[14500,3500],
    width_m=width,height_m=height,yaw_deg=0,sector_size_m=size,columns=cols,rows=rows,
    navigation_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),navigation_sha1=hashlib.sha1(source.read_bytes()).hexdigest(),zones=zones,
    zone_representatives=representatives,main_connected_region=main,sectors=sectors)
dest=ROOT/'Content/SeaNavigation/quangninh_sectors.json';dest.write_text(json.dumps(data,separators=(',',':')),encoding='utf-8')
qa=dict(status='PASSED',sector_count=len(sectors),dimensions=[cols,rows],logical_only_no_target_actors=True,
    navigable=sum(s['navigable'] for s in sectors),connected_regions=counts,
    eligible_spawns=sum(s['spawn_eligible'] for s in sectors),zone_representatives=representatives,
    connectivity=connections,data_sha256=hashlib.sha256(dest.read_bytes()).hexdigest(),
    limitation='Spatial coastline/chart certification only; physical hull overlap, water coverage and underwater clearance belong to runtime spawn validation. Real bathymetry is not certified.')
(ROOT/'Docs/QuangNinhIntegration/SECTOR_GRID_QA.json').write_text(json.dumps(qa,indent=2),encoding='utf-8')
im=Image.new('RGB',(1000,1000),'#133f53');d=ImageDraw.Draw(im)
xy=lambda p:((p[0]-lo_e)/width*1000,(lo_n+height-p[1])/height*1000)
for p in islands:d.polygon([xy(v) for v in p.exterior.coords],fill='#b9bda8')
for s in sectors:
    x0=s['column']*1000/cols;y0=s['row']*1000/rows
    d.rectangle((x0,y0,x0+1000/cols,y0+1000/rows),outline='#507481',width=1)
    if s['navigable']:
        x,y=xy(s['anchor_en_m']);d.ellipse((x-3,y-3,x+3,y+3),fill='#43b8ff' if s['uv'][0]<.5 else '#ff796a')
for link in connections:d.line([xy(p) for p in link['reference_route_en_m']],fill='#f2d457',width=3)
d.line((500,0,500,1000),fill='white',width=3)
im.save(ROOT/'Docs/QuangNinhIntegration/SECTOR_GRID_PREVIEW.png')
print(json.dumps({k:v for k,v in qa.items() if k!='connectivity'},indent=2))
