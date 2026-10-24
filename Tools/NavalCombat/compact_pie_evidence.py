"""Keep reviewable PIE summaries and lossless compressed frame telemetry.

Usage: python Tools/NavalCombat/compact_pie_evidence.py path/to/evidence.json ...
Does not alter native assets or external terrain data.
"""
import gzip
import json
import sys
from pathlib import Path

for argument in sys.argv[1:]:
    path = Path(argument)
    data = json.loads(path.read_text())
    if 'rows' not in data:
        print(path, 'already compact')
        continue
    raw = path.with_suffix(path.suffix + '.gz')
    raw.write_bytes(gzip.compress(path.read_bytes(), mtime=0))
    rows = data.pop('rows')
    data.update(samples=len(rows), raw_evidence=raw.name)
    if rows:
        data.update(first_sample=rows[0], last_sample=rows[-1])
    path.write_text(json.dumps(data, indent=2))
    print(path, 'samples', len(rows), 'raw gzip bytes', raw.stat().st_size)
