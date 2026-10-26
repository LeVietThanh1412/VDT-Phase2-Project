"""External read-only process counters during the final patrol fixture."""
import json,sys,time
from pathlib import Path
import psutil
root=Path(__file__).resolve().parents[2]
processes=[p for p in psutil.process_iter(['name']) if p.info['name']=='UnrealEditor.exe']
assert len(processes)==1,'Expected one authoritative editor'
process=processes[0];start=time.monotonic();rows=[];status='RUNNING'
probe_path=root/(sys.argv[1] if len(sys.argv)>1 else 'Docs/NavalCombat/FINAL_INTEGRATED_PATROL.json')
dest=root/(sys.argv[2] if len(sys.argv)>2 else 'Docs/NavalCombat/FINAL_EDITOR_MEMORY.json')
while time.monotonic()-start<540:
    try:
        memory=process.memory_info();cpu=process.cpu_times()
        rows.append(dict(seconds=time.monotonic()-start,rss_bytes=memory.rss,private_bytes=memory.private,threads=process.num_threads(),handles=process.num_handles(),cpu_user=cpu.user,cpu_system=cpu.system))
    except psutil.NoSuchProcess:status='EDITOR_EXITED';break
    try:
        probe=json.loads(probe_path.read_text())
        if probe['status']!='RUNNING':status=probe['status']
    except (FileNotFoundError,PermissionError,json.JSONDecodeError):pass
    dest.write_text(json.dumps(dict(pid=process.pid,status=status,samples=rows),indent=2),encoding='utf-8')
    if status!='RUNNING':break
    time.sleep(5)
else:status='MONITOR_TIME_LIMIT'
dest.write_text(json.dumps(dict(pid=process.pid,status=status,samples=rows),indent=2),encoding='utf-8')
print(json.dumps(dict(status=status,samples=len(rows),rss_start=rows[0]['rss_bytes'] if rows else None,rss_end=rows[-1]['rss_bytes'] if rows else None)))
