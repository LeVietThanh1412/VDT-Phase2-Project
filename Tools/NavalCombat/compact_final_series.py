"""Losslessly archive high-frequency QA rows/frames and keep reviewable metadata."""
import gzip,hashlib,json,sys
from pathlib import Path
for name in sys.argv[1:]:
    path=Path(name);raw=path.read_bytes();data=json.loads(raw)
    keys=[k for k in ['rows','frames'] if isinstance(data.get(k),list)]
    if not keys:print(path,'already compact');continue
    archive=path.with_suffix(path.suffix+'.gz');packed=gzip.compress(raw,mtime=0)
    assert gzip.decompress(packed)==raw
    archive.write_bytes(packed)
    for key in keys:
        values=data.pop(key);data[key+'_count']=len(values)
        if values:data[key+'_first']=values[0];data[key+'_last']=values[-1]
    data.update(raw_evidence=archive.name,raw_sha256=hashlib.sha256(raw).hexdigest())
    path.write_text(json.dumps(data,indent=2),encoding='utf-8')
    print(path,keys,'raw archive bytes',len(packed))
