"""Record immutable protected inputs before any Blueprint integration."""
import hashlib
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
paths = set(ROOT.glob("Config/*"))
paths.update(ROOT.glob("Content/Ship/BluePrint/*.uasset"))
paths.update(ROOT.glob("Content/Ship/Inputs/*.uasset"))
paths.update(ROOT.glob("Content/AntiShip/BluePrint/*.uasset"))
paths.update(ROOT.glob("Content/SeaGraph/*.uasset"))
paths.update(ROOT.glob("Content/Levels/*"))
paths.update(ROOT.glob("Content/__ExternalActors__/**/*.uasset"))
paths.add(ROOT / "Content/ShipEnemy/BluePrint/BP_ShipEnemy_Backup.uasset")
paths.add(ROOT / "Content/ShipEnemy/BluePrint/BP_ShipEnemy.uasset")
output = ROOT / "Docs/ShipEnemyNavigation/protected_baseline.json"
output.parent.mkdir(parents=True, exist_ok=True)
if output.exists():
    raise SystemExit("Baseline already exists; refusing to overwrite")
output.write_text(json.dumps({
    "checkpoint": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
    "files": {str(p.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in sorted(paths) if p.is_file()}
}, indent=2) + "\n", encoding="utf-8")
print(output)
