# Target Point / A* destination fix — 17 October 2026

**Historical single-destination configuration.** The current saved A-first two-target patrol and real A → B → A results are documented in [TWO_TARGET_COMPLETION.md](TWO_TARGET_COMPLETION.md).

## Outcome

The placed enemy now follows its live Blueprint `DestinationActor` in Destination mode. Its saved active target is the additional `TP_ShipEnemy_OpenSeaDestination` requested during this task. Existing markers were preserved. The Blueprint component template also uses Destination mode, so reconstruction does not restore the earlier Patrol default.

Two fresh PIE runs produced direct routes containing **only start and exact destination**, with no fixed right-side detour waypoints. The second run used the subsequently edited target position, performed 9 A* expansions, and took 0.246 ms. A later bounded physical test reached a temporary valid target, settled for three seconds below 0.5 m/s, and finished at **59.7968 m** distance with **0 throttle / 0 rudder**. The existing arrival tolerance remains **60 m**.

## Causes, established before changes

1. The actual placed component was in **Patrol** mode. The assigned Blueprint `DestinationActor` was ignored; the geographic PatrolPointB was the active goal. In PIE the target was at `(-40770.000243, 117569.999170, -6.184448)` cm, while the requested destination was `(24999.999758, 84999.999171, -6.184451)` cm: **733.927639 m apart**. Evidence: `TARGET_MISMATCH_BEFORE.json`.
2. Destination mode sampled the actor only when `bHasGoal == false`. Moving an assigned actor after initial planning therefore retained the old geographic destination. It now samples the placed reference and live transform each navigation update, comparing persistent ECEF meters and replanning for changes over 1 m or actor replacement. The existing 3 s replan interval remains; stale routes clear and commands stay neutral while waiting. An unchanged actor reuses its route.
3. The chart is a horizontal projection. Its generic inverse (`OriginECEF + East*E + North*N`) reconstructs the tangent plane, not the original three-dimensional actor position. Final waypoint steering and drawing now use the exact requested geographic position, including height. Intermediate chart waypoints retain the existing chart convention. The final marker and steering use the same native conversion.
4. World Partition originally spatially loaded the placed enemy and target; a runtime world-origin rebase unloaded the pair and invalidated the navigation session. Relevant navigation actors now remain loaded. The successful final run retained its route and target alignment through a Cesium georeference change and Unreal origin rebase.

The earlier target was **on water**, about **189.63 m from the OSM shoreline**, but inside the configured **200 m coastline exclusion**. The 100 m raster conservatively rejects entire intersecting cells. Its direct line from the ship crossed land. Being across an island is supported by A*; the endpoint's exclusion status is the separate reason it cannot be accepted with the current safety settings. No safety margin or ArrivalRadius was enlarged or reduced to hide the error.

## Coordinates and route validation

`DestinationActor` is the existing field on the placed BP_ShipEnemy instance; no conflicting component destination actor property was introduced. The pipeline is:

1. Live actor transform: Unreal world centimeters.
2. Account for the ship's anchor parent frame / Unreal world-origin offset; use the resolved CesiumGeoreference to obtain ECEF meters.
3. Convert to longitude / latitude degrees and ellipsoid height meters for `CurrentFinalDestination`.
4. Project ECEF onto the immutable chart's fixed East/North axes, in meters. CurrentRoute stores these chart coordinates, never Unreal locations.
5. `CellAt` selects the containing water cell; A* searches accepted cells and prevents diagonal corner cuts.
6. The existing planner already rejects blocked endpoints, validates start/node and node/exact-target connectors, appends the exact requested chart point, and validates every smoothing shortcut against the same blocked cells and temporary exclusions. This was preserved and regression-tested. A smoothing shortcut may eliminate the visible goal-node waypoint only if its direct final connection is also validated.
7. Waypoints are transformed through the current georeference on use. The final point uses the exact geographic destination. Origin changes do not require moving cached world waypoints because the route is stored in fixed chart meters.

Explicit `SetGeographicDestination` commands retain their existing override semantics; normal placed-target operation does not invoke that setter to bootstrap actor tracking. Invalid/destroyed actors clear the route and produce neutral commands. Moving an unreachable target back into accepted water automatically requests a new route. Arrival acceptance checks the live actor even for motion below the 1 m replan threshold.

## Before / after measurements

| Case | Actor to requested / final endpoint |
| --- | ---: |
| Original placed Patrol configuration | 733.927639 m |
| Original target in corrected Destination mode | 0.000000153 m; rejected as excluded water, no route |
| Added open-sea target, first fresh PIE | 0.000000000298 m; 2 route points |
| Edited open-sea target, second fresh PIE | 0.000001092 m; 2 route points |
| Physical approach test | Arrived at 59.7968 m from live target; tolerance 60 m |

The saved open-sea actor's final world position is `(-5999.996148, 235069.985780, 0)` cm (the subsequently edited position was retained and saved). Its accepted goal-node center is about **10.0244 m** from the exact requested chart point. This is deliberate snapping; the actual final waypoint remains the target. See `OPEN_SEA_FRESH_PIE_1.json`, `OPEN_SEA_FRESH_PIE_2.json`, and `TARGET_MISMATCH_FINAL_CONFIG.json`.

Debug is toggleable through EnemyNavigation.DrawDebug: **red** requested actor, **green** accepted goal node and connector, **cyan** route, **yellow** active waypoint, **magenta** actual final waypoint. Text reports positions, snap/error distances, ship distance, state, waypoint, and path success. Failed requests show no valid endpoint. Destination logs emit on planning/failure events rather than each frame; existing Blueprint sensor prints were not changed.

## Exact implementation / asset changes in this task

- `Source/VDTPhase2Project/ShipEnemyNavigationComponent.cpp`: live actor tracking; safe invalidation/replanning; exact final world position; live arrival acceptance; georeference rebinding; debug markers and event logs. Runtime tracking uses weak per-component entries, cleaned on EndPlay, to avoid a native UObject layout change during Live Coding.
- `Source/VDTPhase2Project/SeaNavigationMap.cpp`: distinguish a blocked start from a rejected destination and report the existing buffer/grid settings. Existing A*, exclusions and connector checks are retained.
- `Source/VDTPhase2Project/ShipEnemyNavigationTests.cpp`: off-center exact endpoint, moving/stationary/invalid actor, recovery, final coordinate round-trip, georeference/rebase, direct open-sea route and fresh alternate-target regressions.
- `/Game/ShipEnemy/BluePrint/BP_ShipEnemy`: existing EnemyNavigation template's NavigationMode set to Destination; compiled and saved. Force/control graphs were not edited.
- Placed enemy package `/Game/__ExternalActors__/Levels/MainMapLevel/B/8F/CHJKNRGBDUPAW44FPQ44PZ`: Destination mode, assigned new open-sea actor, debug enabled, spatial loading disabled. Ship transform, role and movement settings preserved.
- Existing marker package `/Game/__ExternalActors__/Levels/MainMapLevel/1/MS/LC9BZZ0JPJBJN9U5MJNJTN`: spatial loading disabled for origin-shift continuity. Subsequent unsaved user transform edits on this older target were left intact and were not saved over.
- New target package `/Game/__ExternalActors__/Levels/MainMapLevel/6/KT/OKTK4YNX6YEY0QR4PIW9N0`: `TP_ShipEnemy_OpenSeaDestination`, movable root, CesiumGlobeAnchor, spatial loading disabled; final edited transform saved.
- Added tools: `Tools/SeaNavigation/configure_open_sea_target.py` and `verify_target_mismatch.py`. Updated this report and `PIE_TEST_PLAN.md`; JSON/log evidence is alongside this report.

`BP_Ship` remains byte-for-byte unchanged: SHA-256 `28e992c9b100958388f584931b69f08d733b492fa33653ec5c165b2d3990b64a`. No player controls, buoyancy, Water assets, Cesium settings, or ship force functions were changed. Navigation continues to issue SetDriveCommand values into AIThrottle/AIRudder; the existing exclusive Blueprint branch calls the preserved force/rudder functions. Production navigation contains no actor teleport or waypoint transform setter.

## Validation

| Test | Result / evidence |
| --- | --- |
| A — live actor equals requested destination | PASS, PIE world round trips and isolated native fixture |
| B — accepted A* goal node | PASS, all six unique runtime routes have valid water goal cells; independent GIS verification |
| C — exact final point after safe connector | PASS, off-center native tests, PIE endpoint projection and native world-coordinate logs |
| D — moving target replans; unchanged target reuses route | PASS, PIE and native fixture; two fresh PIE starts also performed new A* searches |
| E — unreachable/destroyed target safely rejected | PASS, PIE exclusion/recovery and native destroyed-actor checks; neutral commands and no fake route |
| F — no prohibited chart land crossings | PASS for supplied data: 41,161 accepted cells and 20 segments across 6 runtime routes have zero buffered-land intersections. Unknown shallows cannot be verified without bathymetry/exclusion data. |
| G — Cesium conversion consistency | PASS, native world/geographic round trips, fixed-chart endpoint equality, current-origin runtime alignment |
| H — origin changes | PASS, native and final live PIE georeference setter + Unreal rebase. Automatic CesiumOriginShift distance-trigger behavior was not separately exercised in the final run. |
| I — physical movement / commands | PASS, live hull moved 66.759 m without navigation teleporting, entered Arrived within 60 m, settled below 0.5 m/s for 3 s, commands matched Blueprint fields and ended at 0/0. Long island-detour physical completion remains a manual check. |
| J — protected player unchanged | PASS, SHA-256 comparison |

All four final Editor automation suites passed: Component.IsolatedWorld, Component.PublicState, Planner.Geometry, Planner.RealOSM. The added tests use the real enemy Blueprint in a disposable native world, distinct from the live physics test. Game Development build passed. Editor Live Coding patch creation succeeded; native changes are active in the running Editor. **The base Editor DLL has not been replaced while loaded: after closing Editor in the future, perform a normal VDTPhase2ProjectEditor build before relaunching.**

Earlier diagnostic attempts are retained: the first world-rebase run exposed streaming removal; another was interrupted by PIE restart; the first expanded arrival fixture failed because it retained synthetic throttle before teleporting its test actor. The fixture now explicitly neutralizes that synthetic command. Final suites have zero failures. Physical telemetry uses the simulated ShipSM hull's ECEF position, not a possibly offset actor-root anchor.

Read-only process/session checks found no other active navigation writer before source/asset writes. Existing worktree changes were retained, not reset. Recovery copies and hashes are under `Saved/TargetMismatchBackup/20261017_2052`. The older target's later user edits were preserved. Unreal Editor remains running; PIE was ended after verification.

## Final manual PIE check

1. In MainMapLevel select BP_ShipEnemy. Confirm DestinationActor = `TP_ShipEnemy_OpenSeaDestination`, EnemyNavigation mode = Destination, DrawDebug enabled. No Blueprint wiring is required.
2. Press Play. With the saved open-sea position, the cyan path should go directly to the target, without the former right-side detour. Red and magenta share the destination center; green may be offset because it marks the actual grid node.
3. Move the target in PIE to another accepted-water location. The route should clear, then update within the configured 3 s planning interval. Move it behind an island but outside the 200 m buffer to test a real detour. Putting the endpoint in blocked cells should instead show the explicit failure reason and neutral commands.
4. Stop and Play again. Check a fresh destination log / LastSearchExpansions and verify that the route uses the current saved actor location. PIE-only marker moves are temporary; make changes outside PIE and save the target to retain them.
5. Let a longer island route finish and inspect actual hull handling, buoyancy and arrival. Toggle DrawDebug off when finished. Full regional Cesium/OSM alignment and unprovided shallow-water data remain unverified.
