# Target Point A* integration — 17 October 2026

**Historical integration notes below.** The current saved A-first two-target patrol and real physical loop results are in [TWO_TARGET_COMPLETION.md](TWO_TARGET_COMPLETION.md). They supersede the one-time sampling and single-destination configurations described here.

## Saved behavior

`BP_ShipEnemy` and its placed MainMapLevel instance now enable EnemyNavigation in Destination mode. The existing Blueprint `DestinationActor` field references `TP_ShipEnemy_AStarDestination`, a new TargetPoint at longitude 107.29882028905828, latitude 20.984919743714805, ellipsoid height 0 m. The original `TP_ShipDestination` remains at its original location with its original file hash.

On the first valid navigation update, the component resolves the existing Blueprint field, converts the actor position through the ship's Cesium georeference into a persistent geographic destination, and requests A*. DestinationActor is sampled once; assign it before Play. Explicit SetGeographicDestination commands take precedence. Destination arrival does not alternate back into patrol. A missing or blocked target produces neutral commands and an explicit failure reason. RequestReplan deliberately retries a failed route.

The existing bounded A* planner and conservative smoothing remain unchanged. All route edges and shortcuts must stay in accepted water cells, including diagonal corner checks. The map uses a 200 m coastline safety margin and supports the inspected hull radius up to 125 m. Bathymetry is unknown: this dataset cannot certify depths or exclude unidentified shallow water. Any known additional shallow-water exclusions must be represented as blocked cells when generating navigation data.

Commands pass through the existing SetDriveCommand implementation to AIThrottle/AIRudder. The Blueprint's exclusive branch then calls the preserved ApplyAIForward and ApplyAIRudder functions once. Navigation does not teleport or set actor transforms. Test-fixture repositioning is used only to simulate arrivals; actual vessel handling/arrival must be checked visually in PIE.

## Changes and compilation

- ShipEnemyNavigationComponent.cpp: bootstrap Destination mode from the existing Blueprint DestinationActor after georeference initialization, without changing reflected APIs or component layout.
- ShipEnemyNavigationTests.cpp: add real TargetPoint integration, A* route safety, arrival, explicit-command precedence, missing-target and original-blocked-target checks. Existing patrol, obstacle, recovery, perception, coordinate and command checks remain.
- BP_ShipEnemy: enable navigation and select Destination mode on the existing component template; compile and validate the existing graph.
- Existing enemy instance: enable destination navigation/debug drawing and assign the new target. Its user-selected position and physical components are preserved.
- New TargetPoint external actor package: `/Game/__ExternalActors__/Levels/MainMapLevel/1/MS/LC9BZZ0JPJBJN9U5MJNJTN`.
- configure_target_destination.py: repeatable editor setup, saving only the enemy Blueprint and the two relevant actor packages.

Both changed C++ files compiled through Live Coding successfully at 12:08:08 UTC / 19:08:08 Asia/Saigon. No object-layout changes were detected. A regular on-disk Editor DLL rebuild was not performed because the Editor must remain running. After a future manual Editor closure, build the Editor target normally to persist the native change in the base DLL. The currently running Editor has the patch applied for the user's visual test.

## Executed validation

All four suites passed initially at 12:08:51 UTC / 19:08:51 Asia/Saigon and again against the saved Blueprint at 12:15:38 UTC / 19:15:38 Asia/Saigon. The final queue is empty with four tests performed:

| Suite | Result |
| --- | --- |
| ShipEnemyNavigation.Component.IsolatedWorld | PASS |
| ShipEnemyNavigation.Component.PublicState | PASS |
| ShipEnemyNavigation.Planner.Geometry | PASS |
| ShipEnemyNavigation.Planner.RealOSM | PASS |

New checks run in the disposable native test world with the real BP_ShipEnemy class and packaged OSM map. Successful TargetPoint routing has intermediate waypoints; every smoothed segment clears the blocked grid. The original target coordinates are rejected with no forward thrust. Existing tests also exercise a sudden blocked forward sensor, latched avoidance side, blocked reverse/recovery, and delivery of bounded commands to Blueprint fields. These checks validate logic and geometry; they do not prove real-time physical obstacle avoidance or hydrodynamic arrival.

A separate reference A* check using the placed enemy's current geographic position found accepted water at the start and a connected route to the new target (approximately 2.19 km of unsmoothed grid edges). No ship relocation was needed.

The Blueprint integration validator reports the original parent and six legacy functions preserved, an exclusive command branch, one forward/rudder call pair, and successful compilation. BP_Ship SHA-256 remains `28e992c9b100958388f584931b69f08d733b492fa33653ec5c165b2d3990b64a`. Original target SHA-256 remains `70b1e8842846c14639d7a238ceb654c2d0d0ccc161372a415562de49aae8cfe3`.

Backups of the pre-task enemy Blueprint, enemy actor and changed source files are in `Saved/ShipEnemyTargetBackup/20261017`. Pre-existing generated reports/images were preserved. The Unreal Editor remains running; no unattended shutdown or restart was performed. Final visual PIE testing is assigned to the user, as requested.
