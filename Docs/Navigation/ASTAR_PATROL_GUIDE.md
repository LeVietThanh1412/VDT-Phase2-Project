# Geographic patrols

The original Blueprint propulsion/rudder force calls remain the only normal movement actuators. `ShipEnemyNavigationComponent` calculates throttle/rudder; mission logic supplies goals. Buoyancy and rigid-body simulation continue during ordinary navigation. Prematch placement and explicit development relocation fixtures are the only placement operations; wreck presentation has its separate kinematic lifecycle.

## Coordinates and clearance

`CurrentRoute` and the route JSON use fixed-chart East/North **meters**, derived from WGS84 ECEF. `CurrentFinalDestination` and fallback `PatrolPointA/B` use longitude/latitude degrees and ellipsoid height meters. A TargetPoint's actor transform uses Unreal centimeters. Do not paste chart coordinates into an actor's Location fields. Use `NavalSectorSubsystem.ChartToWorld` / `WorldToChart`, or move the actual actor in the editor.

The original Candidate1 logical field remains 2500 m square, centered at EN(14500,3500), with 625 sectors of100 m. Its original source chart retains its125 m shoreline buffer. After Cesium collision is loaded and stable, the battlefield surveys a25 m operational grid for each selected hull class. Full hull/turn areas and initial route segments receive actual collision/keel checks with2 m clearance. This is coastline/collision evidence, not certified bathymetry.

The island demo extends only the operational survey600 m west of the spawn box. The drawing's west passage was blocked after real hull clearance inside the old box. No terrain, georeference, logical sector data or shore buffer is moved or reduced. Native A* verified a closed Small-ship loop with winding number1 around the left island group. Medium/Large west/north passages are disconnected after their hull survey; those classes use the eastern defensive arc.

## Two-point patrol

For a placed AI, enable New Navigation, select Patrol mode, assign distinct same-world `PatrolActorA` and `PatrolActorB`, and choose Start At Patrol B if the ship begins near A. Keep endpoint arrival circles separate (more than twice Arrival Radius). Empty `PatrolCircuitActors` selects this original A/B behavior. Set the setup menu to **Placed ships and targets / Player + AI** to preserve manual placements instead of generating a scenario.

Both targets are live references. Position changes invalidate cached goals and routes. A development ship jump beyond its physical velocity envelope invalidates old progress, sensor, arrival, recovery and traffic samples. Normal world-origin shifts retain fixed geographic coordinates. A completed arrival requires the actual hull to reach the live target's horizontal arrival circle and slow down before the wait timer switches the objective. Current production arrival speed already projects velocity onto the hull plane; total wave heave was not the cause of the tested patrol timing.

The current baseline completed at least B/A/B/A on all five AI, using original physics/controllers. The slowest route needed about731 game seconds. A720-second budget was insufficient, not a permanent stationary failure. Five100 m ship relocations and five live target moves were followed by repeated physical arrivals. See `Docs/Testing/BASELINE_FIVE_AI_PATROLS.json` and `DYNAMIC_FIVE_AI_PATROLS.json` for exact test scope.

## Island circuit

An optional ordered `PatrolCircuitActors` array adds a closed circuit to the same navigator; it does not add a force controller. The island generator creates live TargetPoints from `Content/SeaNavigation/quangninh_demo_routes.json`. Index, circuit-leg count and completed-circuit count are exposed for inspection. Empty the array to use A/B. Combat/retreat temporarily overrides the objective and then resumes the retained circuit index.

Every match validates all circuit edges with the native per-hull A*, actual stop/turn clearance and route collision checks. A winding check rejects an apparent loop whose shortest paths actually backtrack around the island. The generator retains bounded attempts and reports a setup failure rather than moving through blocked water.

## Diagnosing a stopped ship

Inspect `GetNavigationDiagnostics()` and the actual hull. Legitimate waits include arrival/braking, staggered convoy departure, traffic yielding, reload/retreat behavior and an inactive turn. Inspect current target, route waypoint/count, path success, explicit-stop flag, recovery count, controller, chart-water flag and the failure reason before changing anything.

Missing/overlapping targets, disconnected water, unstreamed collision and exhausted bounded recovery produce explicit failures. Correct the target or placement and replan; do not add Tick teleports, unconditional recovery loops or a second AddForce path. Use `Tools/Testing/verify_dynamic_patrol.py` for the explicit development relocation regression. Its placement operations are fixture actions, not normal gameplay recovery.

Use `InspectNavigationChart`, `InspectNavigationRoute` and `ValidateDevelopmentPlacement` only for development inspection; they expose the real surveyed planner and collision checks. Do not treat a chart-only route or successful build as proof of physical travel.

## Final verified behavior

The exact original five-AI fleet completed B/A/B/A in667.86s after the source refactors and navigation correction. All three Blue Small hulls physically completed a ten-point island circuit by749.64s. Following ships now retain soft separation even when they have crossing priority; hull sweeps, emergency braking, original Blueprint forces and one native SetDriveCommand remain authoritative. Three named local-recovery exhaustion conditions may be reassessed after at least30s on a still-valid water goal; hard configuration/land failures remain terminal and planner-error retries keep their existing bound. This backoff is not proof that every heavy-hull turn is navigable. The alternative legacy new-fleet and assault-disabled heavy reverse-flow stress failed; reports retain those restrictions.
