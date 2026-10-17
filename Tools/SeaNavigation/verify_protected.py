"""Check protected assets, allowing only the explicitly authorized navigation assets."""
import hashlib
import json
import pathlib
ROOT=pathlib.Path(__file__).resolve().parents[2]

def read_json(path):
    return json.loads(pathlib.Path(path).read_text(encoding='utf-8'))

def write_json(path,value):
    pathlib.Path(path).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')

baseline=read_json(ROOT/'Docs/ShipEnemyNavigation/protected_baseline.json')
allowed={
    'Content/ShipEnemy/BluePrint/BP_ShipEnemy.uasset',
    'Content/__ExternalActors__/Levels/MainMapLevel/B/8F/CHJKNRGBDUPAW44FPQ44PZ.uasset',
    'Content/__ExternalActors__/Levels/MainMapLevel/1/MS/LC9BZZ0JPJBJN9U5MJNJTN.uasset',
    'Content/__ExternalActors__/Levels/MainMapLevel/6/KT/OKTK4YNX6YEY0QR4PIW9N0.uasset',
}
records=[]
for name,expected in baseline['files'].items():
    path=ROOT/name
    actual=hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else 'MISSING'
    records.append({'file':name,'before_sha256':expected,'after_sha256':actual,
                    'unchanged':actual==expected,'authorized_navigation_edit':name in allowed})
violations=[r['file'] for r in records if not r['unchanged'] and not r['authorized_navigation_edit']]
report={'status':'PASS' if not violations else 'FAIL','checkpoint':baseline['checkpoint'],
        'protected_files_checked':len(records),'violations':violations,'files':records}
write_json(ROOT/'Docs/ShipEnemyNavigation/protection_check.json',report)
print(json.dumps({k:v for k,v in report.items() if k!='files'}))
if violations:raise SystemExit(1)
