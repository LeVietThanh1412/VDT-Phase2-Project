"""Read-only Editor process samples alongside a bounded PIE evidence file."""
import json,time,sys
from pathlib import Path
import psutil
root=Path(__file__).resolve().parents[2]
probe=root/sys.argv[1];dest=root/sys.argv[2]
editors=[p for p in psutil.process_iter(['name']) if p.info['name']=='UnrealEditor.exe']
assert len(editors)==1
p=editors[0];start=time.monotonic();rows=[];status='RUNNING'
while time.monotonic()-start<4200:
 try:
  m=p.memory_info();cpu=p.cpu_times()
  rows.append(dict(seconds=time.monotonic()-start,rss=m.rss,private=m.private,handles=p.num_handles(),threads=p.num_threads(),cpu_seconds=cpu.user+cpu.system))
  try:status=json.loads(probe.read_text())['status']
  except (OSError,json.JSONDecodeError):pass
 except psutil.NoSuchProcess:status='EDITOR_EXITED'
 dest.write_text(json.dumps(dict(pid=p.pid,status=status,samples=rows),indent=2))
 if status!='RUNNING':break
 time.sleep(10)
else:status='MONITOR_TIME_LIMIT'
dest.write_text(json.dumps(dict(pid=p.pid,status=status,samples=rows),indent=2))
print(status,len(rows))
