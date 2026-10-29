"""Compact evidence/provenance manifest; does not turn failed tests green."""
import hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[2];folder=root/'Docs/QuangNinhIntegration'
names=['SEEDED_SPAWN_SUITE.json','SEEDED_NAVIGATION_REGRESSIONS.json','SEEDED_OPERATIONAL_ROUTE_20261028.json','LAND_COLLISION_OPERATIONAL_LOD.json','SEEDED_ALL_CLASSES_STARTUP.json','SEEDED_REPEATED_PATROL.json']
reports=[]
for name in names:
    p=folder/name
    if not p.exists():continue
    data=p.read_bytes();j=json.loads(data)
    reports.append(dict(file=str(p.relative_to(root)),sha256=hashlib.sha256(data).hexdigest(),status=j.get('status'),error=j.get('error')))
sources={}
for filename in ['NavalBattlefieldSubsystem.cpp','NavalBattlefieldSubsystem.h','SeaTerrainCollisionComponent.cpp','SeaTerrainCollisionComponent.h','ShipEnemyNavigationComponent.cpp','ShipEnemyNavigationComponent.h','NavalMatchSettings.h','NavalMatchSettings.cpp','ShipSetupWidget.cpp','ShipSetupWidget.h','NavalBattleUI.cpp']:
    p=root/'Source/VDTPhase2Project'/filename;sources[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
suite=json.loads((folder/'SEEDED_SPAWN_SUITE.json').read_text(encoding='utf-8'))
for r in suite.get('runs',[]):
    p=folder/r['report']
    if p.exists():reports.append(dict(file=str(p.relative_to(root)),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),status=r['status'],error=r.get('error')))
summary=dict(status='IN_PROGRESS' if any(r['status'] in ['RUNNING','WAITING_FOR_SEED_SUITE'] for r in reports) else 'PASSED' if reports and all(r['status'] in ['PASSED','LAND_COLLISION_VERIFIED'] for r in reports) else 'FAILED',scope='Seeded spawn, initial physical routes, repeated patrol and island-support evidence only. Does not certify later defense/assault, tags, buoyancy tuning or magazines.',sources=sources,reports=reports)
summary['seed_coverage']=dict(completed_starts=len(suite.get('runs',[])),distinct_seeds=len({r['seed'] for r in suite.get('runs',[])}),distinct_layouts=len({r['layout_sha256'] for r in suite.get('runs',[]) if r.get('layout_sha256')}),same_seed_replays_equal=suite.get('same_seed_replays_equal'),minimum_observed_ai_displacement_m=min((d for r in suite.get('runs',[]) for d in (r.get('travel_m') or {}).values()),default=None))
dest=folder/'SEEDED_PLACEMENT_EVIDENCE.json';dest.write_text(json.dumps(summary,indent=2),encoding='utf-8');print(summary['status'])
