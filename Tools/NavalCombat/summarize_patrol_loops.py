"""Independent geometry and per-ship acceptance summary for the real PIE trace."""
import sys,json,math,base64
from pathlib import Path
from collections import Counter
from shapely.geometry import shape,Point,LineString
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
from common import ROOT,metric_geometry
docs=ROOT/'Docs/NavalCombat'
j=json.loads((docs/'PATROL_LOOP_VERIFICATION.json').read_text())
land=metric_geometry(shape(json.loads((ROOT/'Data/SeaNavigation/halong_land.geojson').read_text())['features'][0]['geometry']))
chart=json.loads((ROOT/'Content/SeaNavigation/halong_navigation.json').read_text());cells=base64.b64decode(chart['cells_base64'])
def water(p):
    x=math.floor((p[0]-chart['min_east'])/chart['cell_size']);y=math.floor((p[1]-chart['min_north'])/chart['cell_size'])
    return 0<=x<chart['width'] and 0<=y<chart['height'] and bool(cells[y*chart['width']+x])
report=dict(status=j['status'],seconds=j['rows'][-1]['t'] if j['rows'] else 0,probe_error=j['error'],assertions=j['errors'],ships={})
names={s['name'] for r in j['rows'] for s in r['ships']}
for name in sorted(names):
    track=[(r['t'],s) for r in j['rows'] for s in r['ships'] if s['name']==name];poses=[s['en_m'] for _,s in track]
    arrivals=[a for a in j['arrivals'] if a['ship']==name]
    row=dict(controller_counts=dict(Counter(s['controller'] for _,s in track)),ai_always_controlled=all(s['ai'] and s['controller'] for _,s in track),physics_always_active=all(s['physics'] for _,s in track),
        initialized_path=any(s['path'] for _,s in track),legs=max(s['legs'] for _,s in track),
        maximum_displacement_m=max(math.dist(poses[0],p) for p in poses),peak_speed_mps=max(s['speed_mps'] for _,s in track),
        moving_sample_fraction=sum(s['speed_mps']>1 for _,s in track)/len(track),
        maximum_traffic_wait_s=max(s.get('traffic_wait_s',0) for _,s in track),priority_samples=sum(s.get('traffic_priority',False) for _,s in track),
        maximum_endpoint_error_m=max((s['endpoint_error_m'] for _,s in track if s['endpoint_error_m'] is not None),default=None),
        maximum_goal_world_error_m=max((s['goal_world_error_m'] for _,s in track if s['goal_world_error_m'] is not None),default=None),
        blocked_water_samples=sum(not water(p) for p in poses),land_crossing_segments=sum(LineString([a,b]).intersects(land) for a,b in zip(poses,poses[1:])),
        minimum_coast_distance_m=min(Point(p).distance(land) for p in poses),
        maximum_sample_travel_speed_mps=max((math.dist(a[1]['en_m'],b[1]['en_m'])/(b[0]-a[0]) for a,b in zip(track,track[1:]) if b[0]>a[0]),default=0),
        navigation_failures=dict(Counter(s['failure'] for _,s in track if s['failure'] and 'Waiting for 12' not in s['failure'])),arrivals=arrivals,
        a=track[-1][1]['a'],b=track[-1][1]['b'])
    sequence=[a['arrived_at'] for a in arrivals]
    row['alternating_arrivals']=len(sequence)>=3 and all(a!=b for a,b in zip(sequence,sequence[1:])) and set(sequence)=={row['a'],row['b']}
    row['passed']=row['alternating_arrivals'] and row['ai_always_controlled'] and row['physics_always_active'] and row['maximum_displacement_m']>100 and row['maximum_endpoint_error_m']<1 and row['blocked_water_samples']==0 and row['land_crossing_segments']==0 and not row['navigation_failures']
    report['ships'][name]=row
report['passed']=len(report['ships'])==5 and all(s['passed'] for s in report['ships'].values()) and not report['probe_error'] and not report['assertions']
(docs/'PATROL_LOOP_SUMMARY.json').write_text(json.dumps(report,indent=2))
print('status',report['status'],'seconds',round(report['seconds']),'passed',report['passed'])
for name,s in report['ships'].items():print(name,'legs',s['legs'],'peak',round(s['peak_speed_mps'],1),'moved',round(s['maximum_displacement_m']),'blocked',s['blocked_water_samples'],'failures',s['navigation_failures'],'endpoint_error_m',s['maximum_endpoint_error_m'])
