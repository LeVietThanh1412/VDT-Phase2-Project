"""Export the inspected coastline, saved six-ship starts and real PIE tracks."""
import json,sys
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from shapely.geometry import shape,box
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
from common import metric_geometry
root=Path(__file__).resolve().parents[2];docs=root/'Docs/NavalCombat';plan=json.loads((docs/'BATTLEFIELD_PLAN.json').read_text())
land=metric_geometry(shape(json.loads((root/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']))
all_points=[s['en_m'] for s in plan['ships']]+list(plan['targets'].values())
minimum=[min(p[i] for p in all_points)-400 for i in range(2)];maximum=[max(p[i] for p in all_points)+400 for i in range(2)]
scale=min(1320/(maximum[0]-minimum[0]),820/(maximum[1]-minimum[1]));image=Image.new('RGB',(1600,1000),'#101c2c');draw=ImageDraw.Draw(image)
font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',20);small=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',16);title=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',32)
def xy(p):return (80+(p[0]-minimum[0])*scale,100+(maximum[1]-p[1])*scale)
draw.rectangle([xy((minimum[0],maximum[1])),xy((maximum[0],minimum[1]))],fill='#20485d')
clipped=land.intersection(box(*minimum,*maximum))
for polygon in clipped.geoms if hasattr(clipped,'geoms') else [clipped]:
    if polygon.is_empty:continue
    draw.polygon([xy(p) for p in polygon.exterior.coords],fill='#75958c',outline='#b0c1ab')
    for ring in polygon.interiors:draw.polygon([xy(p) for p in ring.coords],fill='#20485d')
runtime=docs/'VALIDATION_PATROL.json'
if runtime.exists():
    rows=json.loads(runtime.read_text())['rows']
    for ship in plan['ships'][1:]:
        positions=[xy(s['en_m']) for r in rows for s in r['ships'] if s['name']==ship['label']]
        if len(positions)>1:draw.line(positions,fill='#4cafff' if ship['team']==0 else '#ff7768',width=3)
for name,p in plan['targets'].items():
    x,y=xy(p);draw.rectangle((x-5,y-5,x+5,y+5),fill='#edcc64')
    label=('Enemy A' if 'AStar' in name else 'Enemy B') if 'ShipEnemy' in name else name.replace('TP_Friendly_Patrol_','Friendly ')
    draw.text((x+9,y-20),label,font=small,fill='#ffe49b',stroke_width=2,stroke_fill='#101c2c')
for ship in plan['ships']:
    x,y=xy(ship['en_m']);color='#4cafff' if ship['team']==0 else '#ff7768'
    draw.ellipse((x-8,y-8,x+8,y+8),fill=color,outline='white',width=2)
    draw.text((x+12,y+5),ship['label'].replace('BP_ShipEnemy','Enemy_AI_1'),font=small,fill='white',stroke_width=2,stroke_fill='#101c2c')
draw.text((55,26),'MainMapLevel | 3 BLUE vs 3 RED',font=title,fill='white')
draw.rounded_rectangle((1030,100,1540,920),radius=18,fill='#1a2b40')
panel=[('SAVED CONFIGURATION','#ffffff'),('', '#ffffff'),('BLUE: player + 2 friendly AI','#4cafff'),
    ('RED: 3 enemy AI','#ff7768'),('', '#ffffff'),('Navigation: existing geographic A*','#dae6f2'),
    ('Propulsion: existing hull physics','#dae6f2'),('Safe ship spacing: 400 m','#dae6f2'),('', '#ffffff'),
    ('Detection: 1,800 m','#dae6f2'),('Engagement: 1,200 m','#dae6f2'),('Retreat threshold: 450 m','#dae6f2'),
    ('Safe reengagement: 800 m','#dae6f2'),('Weapon cooldown: 4 seconds','#dae6f2'),('', '#ffffff'),
    ('Health: 100 HP per ship','#dae6f2'),('Projectile impact: 25 damage','#dae6f2'),('Teammate damage: 0','#dae6f2'),
    ('HP: green >60%, yellow >30%, red <=30%','#dae6f2'),('', '#ffffff'),('Both Blue AI start toward open water.','#dae6f2'),
    ('Red ships start on both sides of the island.','#dae6f2')]
for i,(label,color) in enumerate(panel):draw.text((1050,122+i*31),label,font=small if len(label)>37 else font,fill=color)
draw.line((1060,865,1060+400*scale,865),fill='white',width=4);draw.text((1060,878),'400 meters (map scale)',font=small,fill='#96aabd')
draw.text((55,944),'Dots: saved starts   Lines: actual patrol PIE tracks   Squares: original patrol anchors   N up / E right',font=font,fill='#dae6f2')
draw.text((55,975),'Local coastline chart in meters; placements transformed through Cesium. No enemy anchor moved.',font=small,fill='#96aabd')
image.save(docs/'BATTLEFIELD_OVERVIEW.png')
