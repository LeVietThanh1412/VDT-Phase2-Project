"""Read-only discovery; rerun from the repository with Python 3."""
from pathlib import Path
import json,hashlib,re,subprocess
root=Path(__file__).resolve().parents[2]
out=root/'reports/research'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
rows=[]
for p in sorted((root/'Source').rglob('*')):
 if not p.is_file():continue
 text=p.read_text(encoding='utf-8-sig')
 lines=text.splitlines()
 rows.append(dict(path=p.relative_to(root).as_posix(),lines=len(lines),sha256=sha(p),
  declarations=[dict(line=i,text=s.strip()) for i,s in enumerate(lines,1) if re.match(r'(class|struct|enum class) ',s.strip()) or re.match(r'^(void|bool|float|double|int32|FString|FVector|TSharedPtr|TArray).*::',s)]))
base=subprocess.check_output(['git','rev-parse','origin/main'],cwd=root,text=True).strip()
inv=dict(audited_main=base,source_files=len(rows),source_lines=sum(x['lines'] for x in rows),files=rows,
 assets={d:sum(1 for p in (root/'Content'/d).rglob('*.uasset')) for d in sorted(p.name for p in (root/'Content').iterdir() if p.is_dir())},
 config=[dict(path=p.relative_to(root).as_posix(),sha256=sha(p)) for p in (root/'Config').glob('*.ini')])
(out/'SOURCE_INVENTORY.json').write_text(json.dumps(inv,ensure_ascii=False,indent=2),encoding='utf-8')
def compact(obj,depth=0):
 if depth>3:return '<nested evidence>'
 if isinstance(obj,dict):return {k:compact(v,depth+1) for k,v in obj.items() if k not in ['rows','samples','trace','timeline','events','frames','snapshots','history','commands','stdout','stderr']}
 if isinstance(obj,list):return [compact(x,depth+1) for x in obj[:6]]+([f'<{len(obj)} total entries>'] if len(obj)>6 else [])
 return obj
results=[]
for p in sorted((root/'Docs/Testing').glob('*.json')):
 try: data=json.loads(p.read_text(encoding='utf-8-sig'))
 except Exception as e:data={'parse_error':str(e)}
 results.append(dict(path=p.relative_to(root).as_posix(),sha256=sha(p),summary=compact(data)))
for folder in ['MinimapInput/20261114','DynamicMinimap/20261114']:
 for n in ['PIE_INPUT.json','FOLLOW_PIE.json','PHYSICAL_DIAGNOSTIC.json']:
  p=root/'Saved'/folder/n
  if p.is_file():results.append(dict(path=p.relative_to(root).as_posix(),sha256=sha(p),summary=compact(json.loads(p.read_text(encoding='utf-8-sig')))))
(out/'EVIDENCE_INDEX.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in inv.items() if k in ['audited_main','source_files','source_lines','assets']},ensure_ascii=False,indent=2))
print('Evidence records',len(results))
