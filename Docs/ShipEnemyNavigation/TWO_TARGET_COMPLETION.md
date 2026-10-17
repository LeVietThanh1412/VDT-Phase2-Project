# Two-target ship navigation — 18 October 2026

## Recorded outcome

Real full-run status: **COMPLETE_A_B_A**. Completed physical legs: **3**. A reached: **True**. B reached: **True**. Complete startup → A → B → A observed: **True**.

| Requested test | Recorded result |
| --- | --- |
| Automatic startup | A-first route and physical progress after a fresh Editor restart; final audit PASS |
| Reach A | First settled arrival at 59.404 m |
| Switch automatically | A → B, then B → A, then A → B observed |
| Return to B | Settled physical arrival at 59.852 m |
| Complete cycle | Three physical legs, startup → A → B → A |
| Saved target relocation | PASS; original full transform restored |
| Editor persistence | PASS; on-disk module, saved references, normal-speed fresh PIE |
| Cesium/geographic origins | True; 30 sampled frames across two real runtime transitions |
| Live target movement | True; new exact goal and native route after moving A by 2 m in disposable PIE |

The full-run probe does not relocate the ship or either target, set their velocities, interpolate their positions, or apply forces. It samples the native controller and existing Blueprint physics. Global time dilation is 2.0 during the bounded test and is restored when it stops. Simulation time is reported below, not wall-clock time.

| Completed leg | Target switch | Simulation seconds | Distance A (m) | Distance B (m) |
| --- | --- | ---: | ---: | ---: |
| 1 | A → B | 424.24 | 59.404 | 1241.949 |
| 2 | B → A | 988.40 | 1211.493 | 59.852 |
| 3 | A → B | 1481.34 | 59.692 | 1240.423 |

Minimum sampled distances: A: 59.145 m, B: 59.626 m. Arrival radius remains **60 m**. Arrival additionally requires a clear segment to the live actor, horizontal speed below 0.5 m/s, and 2 seconds settled before one switch.

![Recorded physical track, coast exclusion and arrival radii](TWO_TARGET_TRACK.png)

## Existing architecture and integration

Gameplay retains the single FSeaNavigationMap A* implementation: bounded eight-neighbor search on a fixed 400×400 grid, 100 m cells, Euclidean edge costs, diagonal corner checks, endpoint connectors, bounded line-of-sight smoothing and validation of every selected segment. Temporary exclusions apply to edges and smoothing as well as nodes. A* minimizes grid travel cost; this is not a claim of globally optimal continuous ship dynamics. No NavMesh or second gameplay planner was added. The Python reference search is an independent test oracle.

Routes store fixed WGS84 ECEF-derived East/North **meters**. Current Unreal world positions are **centimeters**. Conversion uses the ship's resolved Cesium georeference and current Unreal origin/parent frame. The exact requested LLH, including actor height, is retained separately from the snapped grid node. The final waypoint and marker convert that exact destination into the current world frame.

The placed enemy saves actor references A and B on EnemyNavigation in Patrol mode. BeginPlay resets per-run tracking and starts A. Actual settled arrival alternates the actor, clears the old route and runs A* from the current physical hull position. Active actor motion greater than 1 ECEF meter also invalidates the route, including height changes. Searches are event-driven and rate-limited (3 seconds), not performed every Tick. Target placement in accepted connected water requires no map regeneration; changed coastline/exclusion data does require regeneration.

Only SetDriveCommand writes AIThrottle/AIRudder. The existing exclusive Blueprint branch still executes the original AddForce propulsion and rudder functions. Buoyancy, ship collision, player input and BP_Ship are preserved. Required enemy/A/B actors are configured as non-spatially-loaded to survive World Partition origin transitions.

## Problems found and repairs

* Earlier mode/reference configuration did not represent an actor-backed two-target loop. Editing an SCS component reconstructs it; the editor configuration script now reacquires and verifies the component after every property edit, then verifies both serialized references before saving.
* The prior 200 m coast exclusion rejected the authoritative A position, which is 186.074 m from the supplied coast. The inspected hull radius is 80.661 m and local margin is 30 m. The rebuilt 125 m exclusion retains more than their 110.661 m sum, keeps whole-cell conservative rejection, and accepts the actual target without moving it. The source polygons and map extent are unchanged.
* A wide spherical collision query falsely treated deep seabed as an obstacle. Hull sweeps retain the actual vertical hull bounds while checking horizontal clearance. Real above-keel obstacles still block.
* EN projection alone lost exact final actor height; the final approach now uses the original geographic destination. Geographic target tracking avoids stale local-world coordinates after origin changes.
* Streaming teardown cleared the shared map while leaving initialization cached. Teardown now invalidates bindings, and every update validates map/components before use.
* The first physical return failure blacklisted a point only 50 m ahead with a 150 m exclusion, enclosing its own start. Exclusions now protect start/goal connectors. A second run exposed rudder-induced lateral drift cancelling stationary turns. The controller reserves the drift stopping envelope inside the chart margin, validates the full rotation area against collision, caps alignment rudder at 0.6, and retains emergency forward braking. Recovery/retry budgets remain bounded.
* A later return approached the player ship in accepted chart water. Avoidance validated 30°/60° trajectories but executed a different 45° trajectory. It now retains the selected angle and validates that same trajectory with a hull sweep. Detected physical pawn hulls feed endpoint-safe temporary exclusions to the existing A*, and avoidance only consumes the elapsed-time recovery budget when route progress has also stalled. The expanded sweep is level to the geographic sea plane while its vertical extent follows the actual pitched/rolled hull, preventing clearance width from becoming false draft.
* Adding fields to the runtime tracking type through Live Coding exposed retained TMap storage with the old element size and crashed a regression run. The Editor was rebuilt normally and restarted. Subsequent native validation uses the on-disk module; future changes to that tracking type require a normal rebuild/restart.

Failed physical runs are preserved in TWO_TARGET_FIRST_RETURN_FAILURE.json, TWO_TARGET_SLOW_TURN_FAILURE.json and TWO_TARGET_RETURN_APPROACH_FAILURE.json. The focused relocation-of-ship diagnostic is separately marked INCONCLUSIVE_FIXTURE_SETTLING; its original Python Rotator fixture used positional arguments in the wrong order, so it is discarded as acceptance evidence. That helper now uses an explicit direction-derived rotation. TWO_TARGET_OBSTACLE_DIAGNOSTIC.json is a separate disposable approach replay, also excluded from full-loop acceptance. The full accepted loop never repositions the ship.

## Independent geometry and physical telemetry

TWO_TARGET_GEOMETRY.json checks all **48,894** accepted cells against the 125 m buffered land polygons: **0 intersections**. Both A/B are accepted water, with coast distances 186.074 m and 793.710 m respectively. Direct A–B travel intersects the island exclusion; the reference grid routes detour around it. The native route chooses its own intermediate points on every search.

| Physical-run measurement | Recorded value |
| --- | --- |
| Simulation duration (s) | 1473.721 |
| Samples | 2930 |
| Observed route generations | 10 |
| Physical track length (m) | 8015.842 |
| Minimum track coast clearance (m) | 185.482 |
| Track intersects land | False |
| Track intersects coast exclusion | False |
| Physical samples in blocked cells | 0 |
| Generated routes intersect land/buffer | 0 / 0 |
| Commands equal Blueprint inputs | True |
| Highest recovery-attempt counter | 1 |
| Failed navigation samples | 0 |

Telemetry records simulation time, physical hull world/ECEF/LLH positions, velocity, current target, waypoint, route generations, state, recovery count, distance and throttle/rudder every ~0.5 simulation seconds. The independent validator compares the sampled physical polyline and all recorded native routes against the actual GIS data. These checks certify the recorded samples and connecting segments, not unseen depth or every point on a rotating hull mesh.

One bounded avoidance/replan attempt was recorded on the B leg (counter active approximately 829.65–834.17 simulation seconds). Navigation resumed and completed the full cycle. This corrects the earlier progress-update statement that there were no recovery attempts; there were no terminal failed navigation states.

## Relocation, origins and saved-state checks

Saved-target relocation check: **PASS**; restoration: **ORIGINAL_TRANSFORM_RESTORED_AND_SAVED**. The test position is checked against accepted water before saving. Its separate fresh-PIE telemetry is TWO_TARGET_RELOCATION_PIE.json (bounded run status: BOUNDED_DEADLINE). This test checks a changed goal and physical progress, not another full cycle. Recorded progress was 535.537 m; the new route had 2 points, demonstrating that the right-side waypoints are not forced. The full original transform is restored and saved afterward; B is checked unchanged. The authoritative original snapshot is TWO_TARGET_EDITOR_ORIGINAL.json.

Real disposable PIE Cesium georeference setter plus Unreal world-origin rebase: **True**. TWO_TARGET_ORIGIN.json contains frame-level routes, geographic target positions, endpoint errors, physical displacement and transition commands. Editor-world Cesium/Water settings are not saved by this test.

Live PIE target movement: **True**, with final endpoint error 1.5378518193516783e-07 m. TWO_TARGET_LIVE_TARGET_MOVE.json records both the changed geographic goal and the recalculated route; stopping PIE discards this test-only movement.

Final saved-state/Blueprint/persistence evidence: TWO_TARGET_FINAL_STATE.json. Status: **PASS**. Native test and build logs are kept separately. Native automation uses disposable worlds and simulated arrivals; it does not establish physical travel.

Clean Editor/Game build results: **{'EDITOR': 'PASS', 'GAME': 'PASS'}**. Native suite totals: **{'succeeded': 4, 'failed': 0, 'notRun': 0}**. Blueprint validator: OK: component attached; parent and six legacy functions preserved; exclusive command branch; one forward/rudder call pair; Blueprint compiled. The final startup probe uses a newly launched Editor and normal 1× simulation time, with 71 recorded samples. The final Editor is left open on MainMapLevel with PIE stopped and required navigation assets saved.

## Changed files and assets

* Source/VDTPhase2Project/ShipEnemyNavigationComponent.cpp — startup, actor-backed patrol/tracking, coordinate/lifecycle repairs, safe command/recovery and debug telemetry.
* Source/VDTPhase2Project/SeaNavigationMap.cpp — segment and temporary-exclusion validation repairs.
* Source/VDTPhase2Project/ShipEnemyNavigationTests.cpp — planner, actor/mode/arrival, coordinates, lifecycle and turnaround regressions.
* Content/ShipEnemy/BluePrint/BP_ShipEnemy.uasset — existing navigation integration/defaults, compiled and saved.
* Existing enemy external actor: Content/__ExternalActors__/Levels/MainMapLevel/B/8F/CHJKNRGBDUPAW44FPQ44PZ.uasset — saved navigation settings and references.
* Existing A external actor: Content/__ExternalActors__/Levels/MainMapLevel/1/MS/LC9BZZ0JPJBJN9U5MJNJTN.uasset.
* Existing B external actor: Content/__ExternalActors__/Levels/MainMapLevel/6/KT/OKTK4YNX6YEY0QR4PIW9N0.uasset. Target packages retain their original full transforms after testing; persistent loading/globe-anchor setup is retained. No new targets were created for this task.
* Content/SeaNavigation/halong_navigation.json, Data/SeaNavigation/halong_buffered_enu.geojson and navigation_grid.png — 125 m exclusion rebuild from the existing coastline source.
* Tools/SeaNavigation/configure_two_target_patrol.py and verify/validate/relocation/plot/report scripts — reproducible configuration and separate physical/static test evidence. PIE_TEST_PLAN.md and the tooling README describe the current actor-backed patrol; TARGETPOINT_NAVIGATION.md and TARGET_MISMATCH_FIX.md are explicitly marked historical.

Preexisting user changes were retained. Scoped backups are under Saved/TwoTargetBackup. The protected asset audit is protection_check.json; only the explicitly authorized enemy/navigation actor assets are allowed to differ from that baseline.

## Limits

The supplied OSM data contains coastlines, not a bathymetric survey. The existing Cesium alignment samples cover streamed locations only; they do not establish region-wide agreement with every visible shoreline. Invalid/unloaded references, overlapping arrival zones, blocked/out-of-coverage targets and exhausted planning/recovery budgets produce a visible safe failure. Infinite future repetitions cannot be proven by a finite run; the exact number of completed physical legs is reported above.
