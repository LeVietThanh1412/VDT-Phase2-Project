# Enemy ship navigation architecture

The implementation adds one coordinating `UShipEnemyNavigationComponent`, a plain immutable `FSeaNavigationMap` with a small weak shared cache, and an enemy-only `AShipEnemyProjectile`. No new AIController class, Behavior Tree, manager actor, subsystem, or Blueprint parent is introduced.

## Existing systems preserved

`BP_ShipEnemy` still derives from `/Game/Ship/BluePrint/BP_Ship.BP_Ship_C`. `ApplyAIForward`, `ApplyAIRudder`, `UpdateMoveToDestination`, `UpdateObstacleAvoidance`, `ScanOneDirection` and `ScanSurroundings` remain. The existing 12-ray/0.2-second timer remains the sensor. The native component reads its `ScanAngles` and `ScanDistances`; it does not duplicate the 360-degree scan.

Only the enemy EventGraph command-selection path changes:

```
Tick -> Parent Tick -> existing bAIEnabled gate
  -> EnemyNavigation.UpdateNavigation(DeltaSeconds)
       false -> original destination calculation -> original avoidance --+
       true  -> commands already committed -----------------------------+
                              ApplyAIForward -> ApplyAIRudder
```

The new native call is synchronous. It returns true even on a new-pipeline failure, committing zero commands rather than accidentally falling through to legacy steering. The native component never applies force or changes an actor transform. Its own component Tick is disabled. Existing force functions execute once, with no duplicate AddForce calls. Command reflection validates the actual numeric Blueprint fields before use. No nonexistent SetDriveCommand legacy function is assumed: the component provides the new callable command boundary.

## Global map and A*

The immutable 400×400 grid covers a 40×40 km fixed chart, at 100 m resolution. Whole cells intersecting land buffered by 200 m are blocked. The total exclusion includes a validated maximum vessel radius of 125 m and an additional shoreline/calibration margin. Larger hulls fail explicitly. Out-of-coverage queries fail, including the outer grid boundary.

A* uses eight neighbors with no corner cutting, nonnegative shoreline-clearance cost, admissible Euclidean heuristic and turn-count tie breaking. Search is capped by elapsed time, expansions, grid allocation and open-set size. Endpoints require safe cell-center connectors. Every line-of-sight smoothing segment is checked with a conservative grid traversal. Up to eight failed local corridors can be temporarily excluded. Multiple components retain the same immutable map allocation.

## Coordinates

Persisted waypoints and route progress are fixed East/North **meters** relative to a recorded WGS84 ECEF origin, not mutable Unreal positions. Coastline coordinates are longitude then latitude in degrees. ECEF coordinates are meters; Unreal positions/forces use their existing centimeter conventions. Routing is horizontal; coastline height is not water depth or MSL.

The installed Cesium 2.30 APIs convert ECEF/LLH to the current georeference-local coordinates. The component also applies the anchored root's attachment-parent transform when needed. `OnGeoreferenceUpdated` and `UWorld::OriginLocation` changes invalidate local sensor/progress caches. The stable route is retained. No existing Cesium/Water settings are changed or rebasing enabled.

## Following, avoidance and recovery

Heading error uses a signed projected angle with wrap-safe atan2. Positive rudder matches the inspected live yaw/45 convention. Rudder/throttle rates, yaw damping, braking deceleration, practical turn radius, lateral acceleration, cruise speed and arrival radius are editable rather than claimed as measured physics constants.

Speed is limited by destination stopping distance, upcoming turns, heading error, nearby clearance and collision uncertainty. Existing rays are supplemented by one hull-width predictive sweep. The same predicted forward trajectory is checked against the offline chart, so a collision miss cannot allow movement across a known island.

Avoidance selects chart-safe candidate trajectories scored for route alignment and clearance, latches a side, bounds avoidance duration and replans after repeated reversals. Emergency braking removes positive thrust immediately. Recovery brakes, checks rear chart/sensor/sweep clearance, performs bounded reverse, stabilizes and replans. Recovery attempts reset only after substantial useful route progress, not merely because a new path was created. Exhausted attempts stop in Failed. Explicit Stop remains latched until another navigation command.

## Patrol and roles

Patrol uses editable geographic A/B fields or optional actor references, arrival speed threshold, wait duration and automatic direction switching. The original TargetPoint is untouched; it lies in the buffered exclusion region. The new component's tested B default is `(107.29882028905828,20.984919743714805,0)`; A is `(107.29702857184641,20.975025161990075,0)`.

Passive ignores engagement and continues patrol. Coward selects covered escape candidates away from a visible player and uses A*. Fighter selects a covered standoff position; Hunter selects a closer pursuit position. Player checks run every 0.5 seconds with distance/visibility/loss hysteresis. Route requests are throttled by the configurable interval. The component exposes selected state, target, route, waypoint index, desired commands, timing, uncertainty, failure, health and recovery diagnostics.

For Fighter/Hunter, a standard engine AAIController is created only if needed because the inspected inherited ABP_Ship explicitly aims from GetControlRotation. No controller Blueprint/class/default is replaced. The inherited animation's elevation limits are respected. The enemy-only projectile preserves inspected speed/gravity/sphere/damage constants while excluding owner and friendly enemies from collision and radial damage. The player projectile and animation assets remain unchanged. Firing requires muzzle alignment, cooldown, a predicted ballistic first hit on the target, loaded nearby collision and safe blast clearance. Collision uncertainty withholds fire.

The minimal enemy-only health handler stops commands/firing and clears navigation on death. No new health UI or player damage changes are introduced.

## Limitations requiring gameplay acceptance

The map has unknown bathymetry. Streamed collision coverage cannot be certified globally; the observed alignment samples do not establish a regional error bound. Current turn/braking parameters are starting values, not empirically calibrated. Rate-limited following and speed planning do not prove every geometric route dynamically feasible for the actual hull. Ballistic firing, health/death, possession, multi-ship interaction, recovery and continuous patrol require the documented final PIE acceptance tests. They must not be reported as runtime-tested solely because the module compiles.
