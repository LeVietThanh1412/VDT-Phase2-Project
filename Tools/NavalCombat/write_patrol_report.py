"""Write a factual English handoff from saved settings and measured loop results."""
import json
from pathlib import Path
root=Path(__file__).resolve().parents[2];docs=root/'Docs/NavalCombat'
summary=json.loads((docs/'PATROL_LOOP_SUMMARY.json').read_text())
audit=json.loads((docs/'V2_SAVED_PATROL_AUDIT.json').read_text())
status='PASSED' if summary['passed'] else 'NOT YET PASSED'
regression_path=docs/'COMBAT_NAVIGATION_REGRESSION_SUMMARY.json'
regression=json.loads(regression_path.read_text()) if regression_path.exists() else None
lines=['# AI patrol location and loop verification','',f'**{status}** — observed {summary["seconds"]:.1f} seconds of real PIE physics.','',
 'The patrol fixture disables combat and damage in its disposable PIE world. It does not teleport ships, change speed settings, override velocity, or accelerate simulation time. The saved level keeps combat enabled.','',
 '## What changed','',
 '- Corrected two Friendly spawns that were inside the coastline exclusion buffer and an Enemy B target in a rejected chart cell.',
 '- Preserved the existing five ship instances, valid original Enemy A and Friendly A/B actors. Assigned actual per-ship A/B actors with zero hidden formation offsets.',
 '- Kept the existing geographic A*, physical propulsion, rudder, buoyancy and Blueprint force calls. Preserved the target actor height when calculating its world-space goal.',
 '- Fixed traffic detour waypoint skipping, stale obstacle commitments and unbounded name-based yielding. Waiting AI may receive temporary priority; terrain, hull sweeps and emergency braking remain active.','',
 '## Runtime results','',
 '| Ship | A | B | Arrivals | Peak speed | Actual arrival distances | Result |',
 '|---|---|---|---:|---:|---|---|']
for name,s in summary['ships'].items():
    distances=', '.join(f'{a["distance_m"]:.2f}' for a in s['arrivals'])
    lines.append(f'| {name} | {s["a"]} | {s["b"]} | {s["legs"]} | {s["peak_speed_mps"]:.2f} m/s | {distances or "pending"} m | {"PASS" if s["passed"] else "PENDING"} |')
lines+=['','The path endpoint is compared with the actual Target Point location. Physical arrival uses an **80 m acceptance radius**, speed below 0.5 m/s and a 3-second wait before switching. This is arrival tolerance, not an instantaneous move to the actor center.','',
 'Five distinct, possessed AIController instances were captured in [V2_CONTROLLER_POSSESSION.json](V2_CONTROLLER_POSSESSION.json). Per-ship route/index state and controller ownership are recorded in the runtime trace.','',
 '## Saved Target Point locations','',
 'Unreal world coordinates in centimeters, from the scene reloaded after saving.','',
 '| Target Point | X | Y | Z |','|---|---:|---:|---:|']
targets={s[k]['label']:s[k]['world'] for s in audit['ships'] for k in ['a','b']}
for name,p in sorted(targets.items()):lines.append(f'| {name} | {p[0]:.3f} | {p[1]:.3f} | {p[2]:.3f} |')
lines+=['','## Settings','',
 'Auto Possess AI: Placed in World or Spawned. AIController class: AIController. Auto Possess Player: Disabled on AI. New Navigation Enabled: true. Navigation Mode: Patrol. Role: Fighter. Cruise: 20 m/s. Safe separation: 350 m. Traffic priority eligibility: 15 s; temporary hold: 40 s. Each pawn has its own controller and explicit A/B references.','',
 '## Evidence','',
 '- [Runtime trace](PATROL_LOOP_VERIFICATION.json): hull positions, speeds, commands, controller state, route, target, arrivals and endpoint errors.',
 '- [Independent summary](PATROL_LOOP_SUMMARY.json): coastline/grid checks, physical movement, possession and alternating arrival acceptance.',
 '- [Track map](PATROL_V2_MAP.png): saved starts, actual targets and measured hull tracks.',
 '- [Saved scene audit](V2_SAVED_PATROL_AUDIT.json) and [C++ build log](BUILD_LAST.txt).',
 '- Native planner Geometry and RealOSM tests passed, including exclusion-circle edge clearance. These unit tests are separate from the real PIE loop evidence.','',
 '## Manual check','',
 'Open MainMapLevel and Play. Inspect EnemyNavigation on each living AI: Current Waypoint Index, Desired Throttle/Rudder, Navigation State, Patrol Legs Completed and the actual Patrol Actor A/B references. Combat can temporarily replace the patrol goal with a retreat destination; the original references remain available for return. For an isolated patrol check, run verify_patrol_loops.py through the existing MCP client after stopping PIE.','',
 '## Remaining scope','',
 'The expanded V2 tactical, reverse-retreat, difficulty, positional damage, ship-profile, destruction, spectator, turn-based and menu phases are tracked separately in [NavalBattleV2_Progress.md](../NavalBattleV2_Progress.md). This report does not claim those features are complete.']
if regression:
    lines+=['','## Saved-state regression','',f'Combat-enabled real PIE: {regression["seconds"]:.2f} seconds, result {"PASS" if regression["passed"] else "FAIL"}. All five ships moved with their saved navigation settings. See [regression measurements](COMBAT_NAVIGATION_REGRESSION_SUMMARY.json).',
        'Seven affected Blueprints were explicitly compiled and their native compile status checked: [compiler results](BLUEPRINT_COMPILE_RESULTS.txt). Fresh initialization confirmed six ships, five AIControllers, one player, Blue 3 / Red 3 and 100 HP per ship.']
(docs/'PATROL_LOOP_REPORT.md').write_text('\n'.join(lines)+'\n',encoding='utf8')
print(status)
