"""Standalone map of saved target actors and measured PIE hull tracks."""
import json,sys
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
from shapely.geometry import shape,box
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
from common import ROOT,metric_geometry
docs=ROOT/'Docs/NavalCombat';plan=json.loads((docs/'PATROL_V2_PLAN.json').read_text());run=json.loads((docs/'PATROL_LOOP_VERIFICATION.json').read_text())
land=metric_geometry(shape(json.loads((ROOT/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']))
im=Image.new('RGB',(1600,1120),'#101b2b');d=ImageDraw.Draw(im)
font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',20);small=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',17);title=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',30)
bounds=(-700,-3250,1550,-350);scale=950/(bounds[3]-bounds[1])
xy=lambda p:(80+(p[0]-bounds[0])*scale,90+(bounds[3]-p[1])*scale)
d.rectangle([xy((bounds[0],bounds[3])),xy((bounds[2],bounds[1]))],fill='#20485d')
clip=land.intersection(box(*bounds))
for p in clip.geoms if hasattr(clip,'geoms') else [clip]:
    if p.is_empty:continue
    d.polygon([xy(q) for q in p.exterior.coords],fill='#78968a',outline='#bed0b7')
    for r in p.interiors:d.polygon([xy(q) for q in r.coords],fill='#20485d')
d.line([xy(p) for p in plan['reference_polygon_en_m']],fill='#718795',width=2)
colors=['#48b4ff','#88ddff','#ff635c','#ffa568','#e789b9']
latest={s['name']:s for s in run['rows'][-1]['ships']}
for i,s in enumerate(plan['ships']):
    points=[xy(a['en_m']) for r in run['rows'] for a in r['ships'] if a['name']==s['label']]
    if len(points)>1:d.line(points,fill=colors[i],width=3)
    for end in ['a','b']:
        x,y=xy(plan['targets'][s[end]]);d.rectangle((x-6,y-6,x+6,y+6),fill=colors[i],outline='white')
        d.text((x+9,y-18),str(i+1)+end.upper(),fill='white',font=small,stroke_width=2,stroke_fill='#101b2b')
    x,y=xy(s['spawn_en_m']);d.ellipse((x-7,y-7,x+7,y+7),outline=colors[i],width=3)
    x,y=xy(latest[s['label']]['en_m']);d.ellipse((x-5,y-5,x+5,y+5),fill=colors[i])
x,y=xy(plan['player_en_m']);d.text((x-5,y-25),'PLAYER',fill='white',font=small,stroke_width=2,stroke_fill='#101b2b')
d.text((45,25),'MainMapLevel | Actual A/B targets and physics patrol tracks',fill='white',font=title)
d.rounded_rectangle((880,90,1550,1040),radius=18,fill='#192d43')
d.text((910,112),'SAVED PATROL ASSIGNMENTS',font=font,fill='white')
for i,s in enumerate(plan['ships']):
    y=170+i*130;d.text((910,y),f"{i+1}. {s['label']}",font=font,fill=colors[i])
    d.text((910,y+28),'A: '+s['a'],font=small,fill='white');d.text((910,y+53),'B: '+s['b'],font=small,fill='white')
    d.text((910,y+80),f"Recorded arrivals: {latest[s['label']]['legs']}",font=small,fill='#adc5d7')
d.text((910,850),'Each square is an actual Target Point actor.',font=font,fill='white')
d.text((910,883),'Patrol offsets: zero. Cruise setting: 20 m/s.',font=font,fill='white')
d.text((910,916),'Tracks: real PIE, combat disabled for this test.',font=font,fill='white')
d.text((910,960),f"Observed: {run['rows'][-1]['t']:.1f} s | {run['status']}",font=small,fill='#adc5d7')
d.text((45,1070),'North up / East right. Outline dots: saved starts. Solid dots: last sampled hull positions. Gray polygon: requested region.',font=small,fill='#c4d8e7')
im.save(docs/'PATROL_V2_MAP.png')
