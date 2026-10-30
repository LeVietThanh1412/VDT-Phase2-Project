"""Render measured prematch coordinates on the fixed candidate-1 chart.

This displays assignments, not a claim that straight lines are traveled routes.
"""
import argparse,json
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser();parser.add_argument('report');parser.add_argument('--output',default='SEEDED_LAYOUT_VERIFIED.png');args=parser.parse_args()
report=json.loads(Path(args.report).read_text(encoding='utf-8'));layout=report['layout']
source=Image.open(root/'Docs/QuangNinhIntegration/SEEDED_FIELD_CANDIDATE_1.png').convert('RGB')
background=source.crop((source.width/12,source.height/12,source.width*11/12,source.height*11/12)).resize((1000,1000))
canvas=Image.new('RGB',(1390,1050),'#101e2b');canvas.paste(background,(0,50));d=ImageDraw.Draw(canvas)
try:
 font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',17);small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',14);title=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',23)
except OSError:font=small=title=ImageFont.load_default()
def xy(p):return ((p[0]-13250)/2.5,50+(4750-p[1])/2.5)
d.text((15,8),f"QUANGNINH / SEED {layout['seed']} / {report['status']}",font=title,fill='white')
d.text((1020,60),'MEASURED INITIAL SETUP',font=font,fill='white')
d.text((1020,93),'2.5 km square / candidate 1',font=small,fill='#c8d5dd')
d.text((1020,119),'Circle: spawn   Diamond: A* goal',font=small,fill='#c8d5dd')
d.text((1020,145),'Links are assignments, not routes.',font=small,fill='#c8d5dd')
for i in range(26):
    x=i*40;d.line([(x,50),(x,1050)],fill='#41575f',width=1);d.line([(0,50+x),(1000,50+x)],fill='#41575f',width=1)
d.line([(500,50),(500,1050)],fill='white',width=2)
for index,a in enumerate(layout['ships']):
    p=xy(a['spawn_en_m']);q=xy(a['target_en_m']);c='#49b8ff' if a['team']==0 else '#ff745c';short=('B' if a['team']==0 else 'R')+str(a['slot']+1)
    d.line([p,q],fill=c,width=2);d.ellipse((p[0]-8,p[1]-8,p[0]+8,p[1]+8),fill=c,outline='white',width=2)
    if p!=q:d.polygon([(q[0],q[1]-7),(q[0]+7,q[1]),(q[0],q[1]+7),(q[0]-7,q[1])],fill=c)
    d.text((p[0]+11,p[1]-14),short,font=font,fill='white',stroke_width=2,stroke_fill='#101e2b')
    y=200+index*104;d.text((1020,y),short+'  '+a['ship'],font=font,fill=c)
    d.text((1020,y+25),'Spawn EN: '+', '.join(f'{v:.1f}' for v in a['spawn_en_m']),font=small,fill='white')
    d.text((1020,y+46),'Goal EN: '+', '.join(f'{v:.1f}' for v in a['target_en_m']),font=small,fill='white')
    d.text((1020,y+67),'Hull/route clearance: '+str(a['physical_clearance']),font=small,fill='#bacbd7')
d.text((1020,865),'Existing six pawns / five AIControllers',font=small,fill='#c8d5dd')
d.text((1020,895),'Native geographic A* + physical overlay',font=small,fill='#c8d5dd')
d.text((1020,925),'No terrain or georeference relocation',font=small,fill='#c8d5dd')
output=root/'Docs/QuangNinhIntegration'/args.output;canvas.save(output);print(output)
