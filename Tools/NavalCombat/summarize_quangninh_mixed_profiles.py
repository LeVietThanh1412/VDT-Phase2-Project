"""Independent controller/physics/chart checks of the mixed-class PIE run."""
import base64,gzip,json,math
from pathlib import Path

root=Path(__file__).resolve().parents[2]
path=root/'Docs/NavalCombat/QUANGNINH_FINAL_MIXED_PROFILES.json'
data=json.loads(path.read_text())
if 'rows' not in data:data=json.loads(gzip.decompress(path.with_name(data['raw_evidence']).read_bytes()))
chart=json.loads((root/'Content/SeaNavigation/halong_navigation.json').read_text())
cells=base64.b64decode(chart['cells_base64'])
def water(p):
    x=math.floor((p[0]-chart['min_east'])/chart['cell_size']);y=math.floor((p[1]-chart['min_north'])/chart['cell_size'])
    return 0<=x<chart['width'] and 0<=y<chart['height'] and bool(cells[y*chart['width']+x])

ships=[]
for name in sorted({a['name'] for r in data['rows'] for a in r['ships'] if 'nav' in a}):
    frames=[dict(t=r['t'],**a) for r in data['rows'] for a in r['ships'] if a['name']==name]
    settled=[r for r in frames if r['t']>12]
    ships.append(dict(name=name,hull_class=frames[0]['hull_class'],mass_kg=frames[0]['mass'],
        distance_m=sum(math.dist(a['en_m'],b['en_m']) for a,b in zip(frames,frames[1:])),
        physical_all=all(r['physics'] for r in frames),ai_all=all(r['controller']=='AIController' and not r['player'] for r in frames),
        path_seen=any(r['path'] for r in frames),failed_states=sum('FAILED' in r['nav'] for r in frames),
        startup_notices=[dict(t=r['t'],message=r['failure']) for r in frames if r['t']<=3 and r['failure']],
        runtime_failures=[dict(t=r['t'],message=r['failure']) for r in frames if r['t']>3 and r['failure']],
        land_entries=sum(not water(r['en_m']) for r in frames),min_up_dot=min(r['up_dot'] for r in settled),
        water_contact_ratio=sum(r['water_overlap'] for r in settled)/len(settled),
        height_range=[min(r['height_m'] for r in settled),max(r['height_m'] for r in settled)]))
checks=dict(
    correct_six_pawns_one_player=all(len(r['ships'])==6 and sum(a['player'] for a in r['ships'])==1 for r in data['rows']),
    five_physical_independent_AI=len(ships)==5 and all(r['physical_all'] and r['ai_all'] for r in ships),
    all_move_over_50m=all(r['distance_m']>50 for r in ships),
    routes_without_failed_states_or_persistent_errors=all(r['path_seen'] and not r['failed_states'] and not r['runtime_failures'] for r in ships),
    all_tracks_stay_in_chart_water=all(r['land_entries']==0 for r in ships),
    all_stay_upright_near_sea=all(r['min_up_dot']>.9 and -3<r['height_range'][0]<r['height_range'][1]<9 for r in ships),
    two_large_heavier=sum('LARGE' in r['hull_class'] and r['mass_kg']>200 for r in ships)==2,
)
result=dict(status=data['status'],error=data.get('error'),seconds=data['rows'][-1]['t'],ships=ships,checks=checks,
    note='Initial Blueprint sensor warm-up notices are retained separately; persistent errors and Failed navigation states remain failures.',
    passed=data['status']=='BOUNDED_RUN_COMPLETE' and not data.get('error') and all(checks.values()))
(root/'Docs/NavalCombat/QUANGNINH_FINAL_MIXED_SUMMARY.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
