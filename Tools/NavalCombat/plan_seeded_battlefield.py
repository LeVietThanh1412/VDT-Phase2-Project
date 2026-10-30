"""Read-only scan of the actual coastline chart within local terrain coverage."""
import sys,json,base64,math
from pathlib import Path
import numpy as np
from shapely.geometry import shape,Point,box,Polygon
from PIL import Image,ImageDraw
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
from common import ROOT,metric_geometry,ll_to_en,en_to_ll
chart=json.loads((ROOT/'Content/SeaNavigation/halong_navigation.json').read_text())
grid=np.frombuffer(base64.b64decode(chart['cells_base64']),dtype=np.uint8).reshape(chart['height'],chart['width'])
land=metric_geometry(shape(json.loads((ROOT/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']))
metadata=json.loads(Path('E:/LeVietThanh1412-Product/QuangNinh/Terrain/layer.json').read_text())
bounds=metadata['bounds'];corners=[ll_to_en(x,y) for x,y in [(bounds[0],bounds[1]),(bounds[2],bounds[1]),(bounds[2],bounds[3]),(bounds[0],bounds[3])]]
coverage=Polygon(corners);origin=Point(*ll_to_en(107.426513,20.944219));polys=list(getattr(land,'geoms',[land]));size=2500
candidates=[]
for e in range(1000,27000,500):
    for n in range(-22000,6500,500):
        square=box(e-size/2,n-size/2,e+size/2,n+size/2)
        if not coverage.contains(square.buffer(200)):continue
        cut=land.intersection(square);fraction=cut.area/square.area
        if not .12<fraction<.42:continue
        island_count=sum(square.contains(p.representative_point()) and p.area>15000 for p in polys)
        if island_count<2:continue
        x0=max(0,math.ceil((e-size/2-chart['min_east'])/100-.5));x1=min(chart['width'],math.floor((e+size/2-chart['min_east'])/100-.5)+1)
        y0=max(0,math.ceil((n-size/2-chart['min_north'])/100-.5));y1=min(chart['height'],math.floor((n+size/2-chart['min_north'])/100-.5)+1)
        sub=grid[y0:y1,x0:x1];half=sub.shape[1]//2
        if sub.size<500 or min(sub[:,:half].mean(),sub[:,half:].mean())<.35:continue
        score=island_count*2+cut.boundary.length/1000+sub.mean()*5-origin.distance(Point(e,n))/10000
        candidates.append(dict(center_en_m=[e,n],center_ll=list(map(float,en_to_ll(e,n))),width_m=size,yaw_deg=0,land_fraction=fraction,island_count=island_count,coast_length_m=cut.boundary.length,water_fraction=float(sub.mean()),score=score))
candidates.sort(key=lambda c:c['score'],reverse=True)
out=ROOT/'Docs/QuangNinhIntegration';out.mkdir(exist_ok=True)
(out/'SEEDED_FIELD_CANDIDATES.json').write_text(json.dumps(dict(terrain_bounds=bounds,candidates=candidates[:30]),indent=2))
for rank,c in enumerate(candidates[:3]):
    e,n=c['center_en_m'];im=Image.new('RGB',(1000,1000),'#174d65');d=ImageDraw.Draw(im)
    xy=lambda p:((p[0]-e+1500)/3,(n+1500-p[1])/3)
    for p in polys:
        if p.intersects(box(e-1500,n-1500,e+1500,n+1500)):
            d.polygon([xy(v) for v in p.exterior.coords],fill='#c6c8a6')
            for ring in p.interiors:d.polygon([xy(v) for v in ring.coords],fill='#174d65')
    for y,x in np.argwhere(grid):
        p=(chart['min_east']+(int(x)+.5)*100,chart['min_north']+(int(y)+.5)*100)
        if abs(p[0]-e)<1250 and abs(p[1]-n)<1250:
            sx,sy=xy(p);d.ellipse((sx-2,sy-2,sx+2,sy+2),fill='#7cbad1')
    d.rectangle((83.3,83.3,916.7,916.7),outline='white',width=3);d.line((500,83.3,500,916.7),fill='white',width=3)
    d.text((90,90),'BLUE',fill='#48baff');d.text((800,90),'RED',fill='#ff7964')
    d.text((30,25),str(c),fill='white');im.save(out/f'SEEDED_FIELD_CANDIDATE_{rank+1}.png')
print(json.dumps(candidates[:3],indent=2))
