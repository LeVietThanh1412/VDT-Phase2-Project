# AI patrol location and loop verification

**PASSED** — observed 1595.8 seconds of real PIE physics.

The patrol fixture disables combat and damage in its disposable PIE world. It does not teleport ships, change speed settings, override velocity, or accelerate simulation time. The saved level keeps combat enabled.

## What changed

- Corrected two Friendly spawns that were inside the coastline exclusion buffer and an Enemy B target in a rejected chart cell.
- Preserved the existing five ship instances, valid original Enemy A and Friendly A/B actors. Assigned actual per-ship A/B actors with zero hidden formation offsets.
- Kept the existing geographic A*, physical propulsion, rudder, buoyancy and Blueprint force calls. Preserved the target actor height when calculating its world-space goal.
- Fixed traffic detour waypoint skipping, stale obstacle commitments and unbounded name-based yielding. Waiting AI may receive temporary priority; terrain, hull sweeps and emergency braking remain active.

## Runtime results

| Ship | A | B | Arrivals | Peak speed | Actual arrival distances | Result |
|---|---|---|---:|---:|---|---|
| BP_ShipEnemy | TP_ShipEnemy_AStarDestination | TP_ShipEnemy_OpenSeaDestination | 3 | 20.06 m/s | 79.76, 79.61, 79.69 m | PASS |
| BP_ShipEnemy2 | TP_Red02_A | TP_Red02_B | 5 | 19.83 m/s | 79.67, 79.14, 79.76, 79.03, 79.65 m | PASS |
| BP_ShipEnemy3 | TP_Red03_A | TP_Red03_B | 9 | 19.46 m/s | 79.69, 79.41, 79.51, 79.08, 79.42, 79.47, 79.61, 79.43, 79.63 m | PASS |
| Friendly_AI_1 | TP_Friendly_Patrol_A | TP_Friendly_Patrol_B | 10 | 19.65 m/s | 79.65, 79.88, 78.80, 78.90, 79.71, 79.60, 79.70, 79.42, 79.70, 79.80 m | PASS |
| Friendly_AI_2 | TP_Blue02_A | TP_Blue02_B | 6 | 20.05 m/s | 79.78, 79.12, 79.75, 79.54, 79.34, 79.21 m | PASS |

The path endpoint is compared with the actual Target Point location. Physical arrival uses an **80 m acceptance radius**, speed below 0.5 m/s and a 3-second wait before switching. This is arrival tolerance, not an instantaneous move to the actor center.

Five distinct, possessed AIController instances were captured in [V2_CONTROLLER_POSSESSION.json](V2_CONTROLLER_POSSESSION.json). Per-ship route/index state and controller ownership are recorded in the runtime trace.

## Saved Target Point locations

Unreal world coordinates in centimeters, from the scene reloaded after saving.

| Target Point | X | Y | Z |
|---|---:|---:|---:|
| TP_Blue02_A | 5000.000 | 244999.982 | -47.331 |
| TP_Blue02_B | 74999.998 | 124999.997 | -16.723 |
| TP_Friendly_Patrol_A | 34999.998 | 234999.984 | 0.000 |
| TP_Friendly_Patrol_B | 104999.995 | 154999.993 | 0.000 |
| TP_Red02_A | 35000.000 | 84999.999 | -6.655 |
| TP_Red02_B | 64999.996 | 214999.987 | -39.745 |
| TP_Red03_A | 74999.999 | 84999.999 | -10.102 |
| TP_Red03_B | 94999.994 | 194999.989 | -37.043 |
| TP_ShipEnemy_AStarDestination | -40620.000 | 117950.000 | -6.184 |
| TP_ShipEnemy_OpenSeaDestination | 14999.998 | 294999.968 | -68.769 |

## Settings

Auto Possess AI: Placed in World or Spawned. AIController class: AIController. Auto Possess Player: Disabled on AI. New Navigation Enabled: true. Navigation Mode: Patrol. Role: Fighter. Cruise: 20 m/s. Safe separation: 350 m. Traffic priority eligibility: 15 s; temporary hold: 40 s. Each pawn has its own controller and explicit A/B references.

## Evidence

- [Runtime trace](PATROL_LOOP_VERIFICATION.json): hull positions, speeds, commands, controller state, route, target, arrivals and endpoint errors.
- [Independent summary](PATROL_LOOP_SUMMARY.json): coastline/grid checks, physical movement, possession and alternating arrival acceptance.
- [Track map](PATROL_V2_MAP.png): saved starts, actual targets and measured hull tracks.
- [Saved scene audit](V2_SAVED_PATROL_AUDIT.json) and [C++ build log](BUILD_LAST.txt).
- Native planner Geometry and RealOSM tests passed, including exclusion-circle edge clearance. These unit tests are separate from the real PIE loop evidence.

## Manual check

Open MainMapLevel and Play. Inspect EnemyNavigation on each living AI: Current Waypoint Index, Desired Throttle/Rudder, Navigation State, Patrol Legs Completed and the actual Patrol Actor A/B references. Combat can temporarily replace the patrol goal with a retreat destination; the original references remain available for return. For an isolated patrol check, run verify_patrol_loops.py through the existing MCP client after stopping PIE.

## Remaining scope

The expanded V2 tactical, reverse-retreat, difficulty, positional damage, ship-profile, destruction, spectator, turn-based and menu phases are tracked separately in [NavalBattleV2_Progress.md](../NavalBattleV2_Progress.md). This report does not claim those features are complete.

## Saved-state regression

Combat-enabled real PIE: 25.10 seconds, result PASS. All five ships moved with their saved navigation settings. See [regression measurements](COMBAT_NAVIGATION_REGRESSION_SUMMARY.json).
Seven affected Blueprints were explicitly compiled and their native compile status checked: [compiler results](BLUEPRINT_COMPILE_RESULTS.txt). Fresh initialization confirmed six ships, five AIControllers, one player, Blue 3 / Red 3 and 100 HP per ship.
