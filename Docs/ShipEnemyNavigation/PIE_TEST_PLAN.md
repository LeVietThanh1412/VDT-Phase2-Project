# Final Play-In-Editor acceptance

The component is installed and saved; new navigation defaults **off**, preserving legacy gameplay. The tests below are the user's final gameplay acceptance, not claims of already-observed physical behavior.

1. Open MainMapLevel. Load/select the existing BP_ShipEnemy actor. Select its EnemyNavigation component.
2. Confirm bAIEnabled is true on the enemy, enable New Navigation Enabled, enable Draw Debug, and leave Role=Passive initially. No hundreds of nodes or BP_SeaGraph setup is required.
3. Use the supplied geographic Patrol A/B defaults first. They are longitude/latitude/ellipsoid-height vectors. B differs from the original TargetPoint because that point lies inside buffered land. Alternatively assign valid covered geographic points or PatrolActorA/B. Do not use the blocked original target without checking it.
4. Play. Observe an A* detour with cyan route segments, smooth rudder/throttle changes, no teleportation and existing buoyancy. Inspect CurrentRoute, CurrentWaypointIndex, DesiredSpeed/Throttle/Rudder, NavigationState, LastSearchMilliseconds and LastNavigationFailureReason.
5. Let four patrol legs finish. Verify B→A→B→A with stable arrival and optional waiting. Confirm passive ships keep patrolling when the player approaches.
6. Test a sharp turn and a destination behind the hull. Tune CruiseSpeedMps, MinimumTurnRadiusM, MaxLateralAccelerationMps2, BrakingDecelerationMps2, RudderSign, YawRateDamping and command rates from observed behavior. Initial values are not measured handling guarantees. Preserve force calculations and buoyancy.
7. Test one island, a large detour, two-island channel and U-shaped inlet. A narrow passage removed by the exclusion buffer must fail safely. Inspect DrawLandGrid around questionable passages.
8. Request land and out-of-coverage destinations through SetGeographicDestination. Expect Failed/neutral commands with an explicit reason. RequestReplan is the deliberate retry; StopNavigation remains latched until StartPatrol or a new command.
9. Introduce a temporary nearby obstacle in a separate test context. Verify existing 12-ray sensor changes, immediate forward-thrust suppression, bounded side choice and return to the route. Remove the obstacle afterward.
10. Trap the enemy in that test context. Verify rear clearance before any negative thrust, bounded reverse distance/time, finite recovery attempts and terminal safe failure. Confirm slowing intentionally for a turn does not immediately count as stuck.
11. Select Coward and approach in BP_Ship. Check detection/loss hysteresis, a routed escape to connected covered water, and patrol resumption.
    Also issue a valid SetGeographicDestination command before engagement. After losing the player, verify the ship resumes that requested destination and replans instead of retaining its temporary combat route.
12. Select Fighter. Check standoff routing, independent inherited gun animation, muzzle alignment, cooldown and the WeaponStatus reason. Test island/friendly occlusion and blast proximity; firing must be withheld. Collision uncertainty also withholds fire. Hunter should close range and pursue through the same planner.
13. Observe an actual enemy projectile: correct Owner/Instigator, ballistic movement, impact/radial damage on the target, no self/friendly damage. The isolated tests cover ownership/ignore configuration, not a full live shot.
14. Apply enemy damage and verify terminal death stops movement/firing. Verify player cannon, projectile, aiming, animation and buoyancy still behave as before.
15. In a disposable test map/context, run multiple enemies and exercise the project's origin-shift mechanism. Verify routes remain geographic, no sudden steering flip, no false progress/stuck event, and no actor teleport caused by navigation. Do not change the original Cesium or Water configuration for this test.
16. Compare cyan navigation/OSM data with visible Cesium island shorelines across the intended region. The current streamed alignment sample is partial. Increase/rebuild the safety margin if observed offsets require it; never treat a collision miss as safe water or claim known depth.

Saved project/level configuration is unchanged except the authorized active enemy Blueprint. Per-instance values changed for your gameplay trial should be saved only when you choose to retain them. The Git checkpoint is recorded in protected_baseline.json.
