"""Summarize runtime evidence, including independent coastline/spacing checks."""
import json,math
from pathlib import Path
from collections import Counter
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
from common import metric_geometry
from shapely.geometry import shape,Point
root=Path(__file__).resolve().parents[2];docs=root/'Docs/NavalCombat'
land=metric_geometry(shape(json.loads((root/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']))
def read(name):return json.loads((docs/name).read_text()) if (docs/name).exists() else None
report={}
initial=read('INITIALIZATION_NEW.json')
if initial:
    r=initial[-1];s=r['ships'];report['initialization']=dict(count=len(s),teams=dict(Counter(x['team'] for x in s)),
        players=sum(x['player'] for x in s),controllers=dict(Counter(x.get('controller') for x in s)),
        total_pawns=r.get('total_pawns'),all_physics_buoyancy=all(x['physics'] and x['buoyancy'] for x in s),hud=r['hud'])
for mode in ['PATROL','BATTLE']:
    data=read('VALIDATION_'+mode+'.json')
    if not data or not data['rows']:continue
    rows=data['rows'];names={x['name'] for r in rows for x in r['ships']};summary={}
    for name in sorted(names):
        track=[(r['t'],x) for r in rows for x in r['ships'] if x['name']==name]
        poses=[x['en_m'] for _,x in track];coast=[Point(p).distance(land) for p in poses]
        steps=[math.dist(a[1]['en_m'],b[1]['en_m'])/max(.01,b[0]-a[0]) for a,b in zip(track,track[1:])]
        summary[name]=dict(legs=max(x.get('legs',0) for _,x in track),shots=max(x['shots'] for _,x in track),
            hits=max(x['hits'] for _,x in track),retreats=max(x.get('retreats',0) for _,x in track),resumed=max(x.get('resumed',0) for _,x in track),
            damage_events=max(x['damage_events'] for _,x in track),deaths=max(x['death_events'] for _,x in track),
            nav_states=dict(Counter(x.get('nav','player') for _,x in track)),tactical_states=dict(Counter(x.get('tactic','player') for _,x in track)),
            initialized_path=any(x.get('path',False) for _,x in track),max_displacement_m=max(math.dist(poses[0],p) for p in poses),
            minimum_coast_distance_m=min(coast),land_samples=sum(land.covers(Point(p)) for p in poses),maximum_sample_speed_mps=max(steps,default=0),
            navigation_failures=dict(Counter(x.get('failure') for _,x in track if x.get('failure'))),
            invalid_target_samples=sum(bool(x.get('target')) and (x['target']==name or x.get('target_team')==x['team']) for _,x in track))
    report[mode.lower()]=dict(status=data['status'],seconds=rows[-1]['t'],error=data['error'],ships=summary,
        minimum_pair_spacing_m=min(r['min_spacing_m'] for r in rows if r['min_spacing_m'] is not None),final_hud=rows[-1]['hud'])
for key,file in [('damage_ui','DAMAGE_UI_PIE.json'),('defeat','DEFEAT_PIE.json')]:
    data=read(file)
    if data:report[key]=data if key=='defeat' else {k:v for k,v in data.items() if k in ['error','phase','rows','results']}
ranges={}
for file in ['RANGES_TURRETS_PIE.json','RANGES_BLOCKER_LIMITS_PIE.json']:
    data=read(file)
    if data:
        for r in data['results']:ranges[r['name']]=dict(passed=r['passed'],shots=r['shots'],file=file)
report['ranges_turrets']=ranges
controls=read('PLAYER_CONTROLS_PIE.json')
if controls and controls['rows']:
    rows=controls['rows'];report['player_controls']=dict(error=controls['error'],
        moved_m=math.dist(rows[0]['position'][:2],rows[-1]['position'][:2])/100,
        yaw_delta_degrees=(rows[-1]['yaw']-rows[0]['yaw']+180)%360-180,
        look_before=rows[0]['look'],look_after=rows[-1]['look'],shots=max(r['shots'] for r in rows),
        used_gun_camera=any(r['gun_camera'] for r in rows),always_player_controlled=all(r['player'] for r in rows),
        no_navigation_component=all(not r['has_navigation'] for r in rows))
retreat=read('RETREAT_PIE.json')
if retreat and retreat['rows']:
    rows=retreat['rows'];escaping=[r for r in rows if 'RETREAT:' in r['tactic']]
    report['focused_retreat']=dict(status=retreat.get('status'),error=retreat['error'],
        states=dict(Counter(r['tactic'] for r in rows)),shots=max(r['shots'] for r in rows),
        retreats=max(r['retreats'] for r in rows),resumed=max(r['resumed'] for r in rows),
        retreat_start_distance_m=escaping[0]['distance'] if escaping else None,
        maximum_retreat_distance_m=max((r['distance'] for r in escaping),default=None),
        original_anchors_retained=all(r['a']=='TP_Friendly_Patrol_A' and r['b']=='TP_Friendly_Patrol_B' for r in rows),
        final_tactical_override=rows[-1]['tactical_goal'])
occlusion=read('ISLAND_OCCLUSION_PIE.json')
if occlusion:report['real_island_occlusion']=dict(passed=occlusion.get('passed'),error=occlusion['error'],last=occlusion['rows'][-1] if occlusion['rows'] else None)
(docs/'VERIFICATION_SUMMARY.json').write_text(json.dumps(report,indent=2),encoding='utf8')
for key in ['initialization','patrol','battle']:
    if key in report:print(key,json.dumps(report[key],indent=2))
