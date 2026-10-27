# Geographic A* patrol recovery

Status: PHASE 1 VERIFIED. The new sector battlefield and complete combat integration are pending.

## Reproduced failures

The previous 600.75-second saved-position run produced only one arrival for each of the five AI. All eventually exhausted recovery despite intact AIControllers and simulated hulls. A new 367.37-second camera/LOD reproduction again left all five at one arrival. See NAVIGATION_RETURN_LOD_REPRO.json. These runs are failure evidence.

A separate current-state audit found Friendly_AI_1's authored A marker in the chart exclusion buffer. Its first B arrival succeeded, then the return target was correctly rejected. Only that A marker was restored to its previously validated water coordinate. Existing actor identities, references and controllers are preserved. The 86-file SHA256 recovery manifest is recorded by Saved/active_navigation_recovery.txt.

## Owning collision defect

A* correctly switched endpoints and built return routes. Cesium camera changes loaded tile skirts across those routes. The old collision filter tried to identify skirts through coincident longitude/latitude within 0.05 m. Captured actual generated skirt pairs shift horizontally by 0.228-0.489 m while extending 47-94 m downward. This left artificial vertical walls in accepted water, repeatedly triggering hull-sweep braking, avoidance and bounded recovery.

Cesium's loader deliberately offsets skirts outward. Its public ICesiumLoadedTilePrimitive interface exposes the primitive, glTF model and coordinate scale. The correction reads SkirtMeshMetadata and matches generated local vertex positions to cooked Chaos particles; it does not assume Chaos preserves vertex indices. Only zero-surface generated skirt portions in accepted chart water are filtered. Nonzero seabed, islands, chart exclusion zones and geometry outside chart coverage remain collidable. The original dataset, terrain rendering, georeference and A* graph are unchanged.

Reference: [Cesium terrain loader](https://github.com/CesiumGS/cesium-native/blob/main/CesiumQuantizedMeshTerrain/src/QuantizedMeshLoader.cpp), checked alongside installed UE plugin headers.

## Patrol lifecycle

The current target is sampled in the fixed ECEF-derived East/North chart. PlanRoute uses the existing FSeaNavigationMap::FindPath. Intermediate route points steer the original Blueprint's sole propulsion/rudder pair. A confirmed final arrival requires the actual live target within ArrivalRadiusM, a clear final chart segment, horizontal speed below 0.5 m/s and the configured wait. The component then switches A/B once, invalidates the route and requests the next A* path. Invalid/overlapping endpoints are rejected explicitly.

## Development repositioning

NotifyDevelopmentReposition invalidates derived route, sensor, arrival, progress, avoidance, traffic and recovery state while retaining patrol endpoint selection, team, controller, geographic command and explicit stop intent. RequestReplan uses that reset and clears an explicit stop. The component detects large EN position changes beyond the measured physical velocity envelope. Normal movement and Cesium/world-origin rebasing do not continually replan. A valid placement replans from the new physical start; a land placement is rejected. This never teleports a ship or overrides velocity.

Before PIE, authored placements initialize from the saved anchor. Runtime tooling should move the actor or anchor consistently and synchronize its anchor. A caller may explicitly invoke NotifyDevelopmentReposition after a small intentional placement.

## Diagnostics and actual acceptance

GetNavigationDiagnostics returns JSON including controller, team/class, tactical/navigation state, goal, A/B, index/count, route result, distance, throttle/rudder, forces supplied through the Blueprint filters, speed, physical/chart-water status, no-progress time and recovery/reposition counts. Forces are the Blueprint filter outputs, not a second force authority. Full time series remain in Saved; committed compact reports contain measured arrivals and acceptance checks.

Tools/NavalCombat/verify_navigation_repair.py runs five independent physical patrols with combat disabled only in disposable PIE and camera cycles every 60 seconds. MODE=runtime_reposition additionally performs one explicit development relocation and checks automatic replanning and continued arrivals. It does not override drive, velocity, physics or arrival tolerances.

Compilation succeeded. All four native navigation automation tests passed after making their authored anchors explicit and isolating legacy fallback cases from the newer shared combat components. These are state/planning tests, not physical-motion evidence.

The fresh saved-fleet B/A/B run passed at 392.8 game seconds, with 16 actual arrivals, exact endpoints, preserved controllers and continuous physics. NAVIGATION_FIXED_SAVED_PATROL.json records the result. The separate saved editor relocation passed after restart with over 100 m actual displacement and no spurious runtime resets.

NAVIGATION_REPOSITION_FIXTURE_FAILURE.json deliberately preserves a failed instrumented run: an arrival sample was taken after the development relocation, attributing its new position to the previous B arrival. The corrected fixture waits for the arrival sampler to record B before relocating. Its mode requires two arrivals per AI (B then A); the independent saved-fleet mode requires three per AI (B/A/B). It never labels the two-arrival case as a complete three-arrival soak.

NAVIGATION_PHASE1_VERIFIED.json aggregates eleven passing reports and hashes their contents. The corrected runtime placement test passed at 242.15 s, including physical return and a replan observed within the next 1-second diagnostic sample. The 100 m saved editor placement survived a clean restart and moved normally.

The selected-field Small traversed 2159.11 m around a real island and back: B at 90.03 s and A at 239.35 s. Native A* used four waypoints and 14-29 expansions. Independent coastline geometry confirms every trajectory sample stayed inside candidate 1, with no land intersection; planned clearance >=226.01 m and actual trajectory clearance >=176.60 m.

The updated terrain filter passed retained physical island-support/LOD/cache checks. The shared-combat role fixture passed all checks, including target-loss return and real displacement. A separate hostile health-death pursuit fixture passed patrol resumption, endpoint/controller preservation and continued movement. An additional staged proximity-retreat fixture returned after its threat was destroyed through the health pipeline at 71.85 s; that case deliberately moved the stationary threat during retreat and is not evidence of unmodified natural fleet combat.

The temporary excluded marker inside candidate 1 was explicitly rejected with neutral thrust, then restoring its valid live position automatically replanned; the ship physically traveled another 374.34 m. No ship teleport was used in that recovery.

Full sector spawning, Blue/Red tactics, synchronized tags, buoyancy tuning and magazine/reload acceptance remain separate outstanding phases.
