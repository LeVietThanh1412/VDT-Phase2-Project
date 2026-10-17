"""Fail if any protected baseline file except the authorized active enemy changed."""
import hashlib
import json
import pathlib
ROOT=pathlib.Path(__file__).resolve().parents[2]

def read_json(path):
    return json.loads(pathlib.Path(path).read_text(encoding='utf-8'))

def write_json(path,value):
    pathlib.Path(path).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')

baseline=read_json(ROOT/'Docs/ShipEnemyNavigation/protected_baseline.json')
allowed='Content/ShipEnemy/BluePrint/BP_ShipEnemy.uasset'
records=[]
for name,expected in baseline['files'].items():
    path=ROOT/name
    actual=hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else 'MISSING'
    records.append({'file':name,'before_sha256':expected,'after_sha256':actual,
                    'unchanged':actual==expected,'authorized_enemy_edit':name==allowed})
violations=[r['file'] for r in records if not r['unchanged'] and not r['authorized_enemy_edit']]
report={'status':'PASS' if not violations else 'FAIL','checkpoint':baseline['checkpoint'],
        'protected_files_checked':len(records),'violations':violations,'files':records}
write_json(ROOT/'Docs/ShipEnemyNavigation/protection_check.json',report)
print(json.dumps({k:v for k,v in report.items() if k!='files'}))
if violations:raise SystemExit(1)
