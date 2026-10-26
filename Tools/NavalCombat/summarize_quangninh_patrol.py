"""Independent offline checks of real five-AI physics/route telemetry.

Run with Python and NumPy after the PIE soak finishes. No GEOS/PROJ DLL is required.
"""
import argparse,base64,gzip,json,math,sys
from pathlib import Path
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'SeaNavigation'))
from coast_geometry import ROOT,CoastGeometry

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input',default='Docs/NavalCombat/QUANGNINH_PATROL_SOAK.json')
parser.add_argument('--output',default='Docs/QuangNinhIntegration/PATROL_SUMMARY.json')
arguments=parser.parse_args()
path=ROOT/arguments.input
data=json.loads(gzip.decompress(path.read_bytes()) if path.suffix=='.gz' else path.read_text())
if 'rows' not in data and data.get('raw_evidence'):
    data=json.loads(gzip.decompress(path.with_name(data['raw_evidence']).read_bytes()))
chart=json.loads((ROOT/'Content/SeaNavigation/halong_navigation.json').read_text())
cells=np.frombuffer(base64.b64decode(chart['cells_base64']),dtype=np.uint8).reshape(chart['height'],chart['width'])
land=CoastGeometry()
def in_water(p):
    x=math.floor((p[0]-chart['min_east'])/chart['cell_size']);y=math.floor((p[1]-chart['min_north'])/chart['cell_size'])
    return 0<=x<chart['width'] and 0<=y<chart['height'] and bool(cells[y,x])
names=sorted({a['name'] for row in data['rows'] for a in row['ships']})
summary=dict(status=data['status'],error=data.get('error'),seconds=data['rows'][-1]['t'],samples=len(data['rows']),ships=[],arrivals=data['arrivals'],errors=data['errors'])
for name in names:
    rows=[dict(t=row['t'],**next(a for a in row['ships'] if a['name']==name)) for row in data['rows'] if any(a['name']==name for a in row['ships'])]
    points=[a['en_m'] for a in rows]
    route_keys={tuple(tuple(p[:2]) for p in a['route']) for a in rows if len(a['route'])>=2}
    goal_errors=[a['goal_world_error_m'] for a in rows if a.get('goal_world_error_m') is not None]
    endpoint_errors=[a['endpoint_error_m'] for a in rows if a.get('endpoint_error_m') is not None]
    settled=[a for a in rows if a['t']>12]
    cruise=[a['speed_mps'] for a in settled if abs(a['rudder'])<.2 and a['distance_to_target_m']>400 and 'FOLLOW_ROUTE' in a['state']]
    sharp=[a['speed_mps'] for a in settled if abs(a['rudder'])>.6 and a['speed_mps']>3]
    longest=streak=0
    for a in settled:
        streak=0 if a.get('in_water',False) else streak+1;longest=max(longest,streak)
    record=dict(max_consecutive_out_of_water_samples=longest,water_contact_ratio=sum(a.get('in_water',False) for a in settled)/max(1,len(settled)),name=name,first_world=rows[0]['world'],last_world=rows[-1]['world'],legs=rows[-1]['legs'],distance_m=sum(math.dist(a,b) for a,b in zip(points,points[1:])),peak_speed_mps=max(a['speed_mps'] for a in rows),cruise_median_mps=float(np.median(cruise)) if cruise else None,moving_turn_median_mps=float(np.median(sharp)) if sharp else None,min_coast_distance_m=min(land.distance(p) for p in points),min_route_coast_distance_m=min((land.route_distance_bound(route) for route in route_keys),default=None),land_entries=sum(not in_water(p) for p in points),physical_all=all(a['physics'] for a in rows),ai_controller_all=all(a['ai'] and a['controller']=='AIController' for a in rows),out_of_water_after_settling=sum(not a.get('in_water',False) for a in settled),failed_state_samples=sum('FAILED' in a['state'] for a in rows),recovery_samples=sum('RECOVERY' in a['state'] for a in rows),max_live_target_error_m=max(goal_errors,default=0),max_route_endpoint_error_m=max(endpoint_errors,default=0),arrivals=[a for a in data['arrivals'] if a['ship']==name])
    summary['ships'].append(record)
summary['assertions']={
    'all_five_completed_three_arrivals':len(names)==5 and all(a['legs']>=3 for a in summary['ships']),
    'correct_six_pawns_one_player':all(r['total_pawns']==6 and r['player_pawns']==1 for r in data['rows']),
    'five_independent_ai_controllers':all(a['ai_controller_all'] for a in summary['ships']),
    'physical_movement_over_2km_each':all(a['physical_all'] and a['distance_m']>2000 for a in summary['ships']),
    'no_land_entries_or_failed_state':all(a['land_entries']==0 and a['failed_state_samples']==0 for a in summary['ships']),
    'live_target_conversion_correct':all(a['max_live_target_error_m']<.01 and a['max_route_endpoint_error_m']<.01 for a in summary['ships']),
    'every_switch_inside_actual_arrival_radius':bool(data['arrivals']) and all(a['distance_m']<=a['allowed_m']+3 for a in data['arrivals']),
    'sustained_water_contact_after_settling':all(a['max_consecutive_out_of_water_samples']<=1 and a['water_contact_ratio']>=.99 for a in summary['ships']),
    'no_probe_or_location_errors':data.get('error') is None and not data['errors'],
}
summary['passed']=data['status']=='ALL_FIVE_LOOPS_VERIFIED' and all(summary['assertions'].values())
output=ROOT/arguments.output
output.write_text(json.dumps(summary,indent=2))
print(json.dumps({k:v for k,v in summary.items() if k not in ['arrivals','ships']},indent=2))
for s in summary['ships']:print(s['name'],s['legs'],round(s['distance_m']),s['cruise_median_mps'],s['land_entries'])
