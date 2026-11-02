# Authored map A/B QA — 2026-11-02

> Historical layout/build record. The owner subsequently rearranged the saved map. This D0FE049B result does not certify that new layout; see `USER_LAYOUT_SHORELINE_QA.md` for its separate fixes and runtime evidence. Do not restore the earlier checkpoint over the owner's map.

**PASSED** for the authored A/B and handling scope: 11 completed serial PIE scenarios, four native automation tests and all 39 Blueprint compile requests. Compact evidence: [MANUAL_MAP_PIE.json](MANUAL_MAP_PIE.json).

Native DLL SHA-256: `d0fe049b5166c9fe04ac54b7fda7eb3393570b261e2f33b963f9c88907263f4e`.

## Corrections

- Explicit **USE MY MAP (A-B)** and **GENERATE FROM SEED** actions replace the old scenario dropdown. A fresh match defaults to Medium; Small remains selectable.
- Manual mode preserves placed ship transforms and A/B references, starts at A, removes runtime circuit/formation offsets, and prevents pursuit/retreat from replacing the authored route. Targeting and shared weapons remain active.
- Startup validates references/spacing first, streams the authored area, builds the existing per-hull 25 m collision chart over dynamic bounds, then validates start→A, A→B and B→A. A coarse-grid rejection no longer prevents the physical survey.
- Near-coast avoidance used to alternate its rudder against the required route turn. The existing checked stationary-rotation query is reused earlier: a preferred 4° step rotates the actual hull OBB about its COM, checks collision and chart drift, and retains intentional-turn timing. It does not teleport/rotate the body directly or bypass terrain.
- Passive lateral water resistance of 4/s reduces rudder-induced side slip. Original forward propulsion and rudder force paths remain authoritative; no yaw lock or hull velocity assignment was added.
- Full AI requires exactly one A tag and one B tag on distinct TargetPoints. Invalid roster configuration stops active drive commands before an extra hull can be spawned.

## Persistent map changes

The five placed ship poses and the main map/georeference were preserved. Friendly_AI_1's A/B markers were moved to physically checked chart water. Existing `TP_ShipDestination` was tagged `Naval.PlayerPatrol.A`; `TP_Player_Patrol_B` was added with `Naval.PlayerPatrol.B` for the sixth AI at PlayerStart. Other patrol links remain unchanged.

The pre-edit map/external-actor checkpoint is under `Saved/AuthoredMapCheckpoints/20261102_154058/`. Edit/save assets through Unreal Editor.

## Verified evidence

- Win64 Development Editor C++ build succeeded.
- All 39 Blueprint compile requests completed with generated classes present and no Blueprint compiler errors.
- Native automation: IsolatedWorld, PublicState, Geometry, RealOSM — all passed.
- Three Full AI tag faults (missing, duplicate, same actor) each produced an actionable setup error, five stopped original ships, no sixth spawn and no propulsion/rudder force.
- Same-actor A/B route preparation was rejected with the ship named and zero commands; restoring the pair successfully restored the route.
- Full AI manual endurance ran **795.9 game seconds**, with damage disabled only. Every hull remained physically simulated, its AI controller and marker references were retained, and no tactical destination overrode A/B. No recovery cooldown occurred.

| Ship | Completed legs |
|---|---:|
| BP_ShipEnemy2 | 6 |
| Friendly_AI_1 | 4 |
| Friendly_AI_2 | 5 |
| BP_ShipEnemy3 | 7 |
| BP_ShipEnemy | 6 |
| Blue Flagship | 6 |

Actual sampled arrivals satisfied the configured radius: 80 m for the five placed instances, 60 m for the spawned flagship. Markers retained their authored locations within 1 cm; placed horizontal starts remained within 1 m after physical settling. This is acceptance within arrival zones, not exact center-to-center contact. The low-speed, zero-throttle p95 absolute side slip was 0.1145 m/s.

Red player + Small on My Map also passed: dedicated Small Blueprint, correct selected faction, preserved PlayerStart/placed AI poses, all five first-A arrivals, negative seed rejected, zero accepted, and positive seed retained across travel.

## Handling and generated-layout regressions

All three profiles passed real Enhanced Input forward travel, opposite rudder directions, mouse look, aim/release and one original Blueprint projectile. These use the established mixed-fleet Red staging start, keeping the scripted open-water drive away from the defended island. Camera alignment uses the real mouse action; no camera/hull transform or velocity is assigned by the fixture.

| Profile | Right / left yaw (deg) | Worst p95 roll (deg) | Worst p95 pitch oscillation (deg) | Worst p99 vertical speed (m/s) |
|---|---:|---:|---:|---:|
| Small | 35.825 / -32.966 | 0.1611 | 0.5171 | 0.6573 |
| Medium | 23.437 / -20.789 | 0.1346 | 0.8308 | 0.6252 |
| Large | 9.512 / -8.782 | 0.1520 | 0.6545 | 0.4245 |

Idle and firing phases each lasted 40 game seconds after settling. All six hulls retained 100% sampled water contact and physical simulation, normalized pontoon weights, and zero commanded propulsion/rudder; each fired ten original shared rounds. Pitch oscillation is measured around mean trim, rather than treating authored steady trim as oscillation.

The generated seed `20261028` contact regression passed with the established Blue Small/Small/Small and Red Small/Large/Medium fleet. All six acquired a valid firing opportunity and fired within 15.6 game seconds, preserving their controllers. Damage was disabled; this is not a natural elimination result.

The controlled result/RETURN TO SETUP regression passed: current outcome/counters, clickable result HUD, preserved selections, paused setup, idempotent return, back-to-result cursor and a clean six-ship restart. An offscreen viewport paint is requested before inspecting HUD text because an occluded Editor can defer Slate widget ticks. The earlier unpainted-HUD diagnostic is not counted as a pass.

Earlier Blue fixed-view input diagnostics correctly could not fire with the crosshair hidden while looking through the ship; the native weapon rejection string stayed empty because the preserved Blueprint gate stopped the click upstream. The final tests aim forward/above the bow through actual mouse input and retain this guard. A separate experimental basic-layout fleet was rejected by native keel/turn clearance; no safety checks were bypassed. These diagnostic cases are preserved and excluded from the passing matrix.

## Scope and reproducibility

Run `Tools/Testing/verify_authored_patrol.py` through the Editor Python bridge with no existing PIE world. `FULL_AI=True, REQUIRED_LEGS=4` checks the six-ship loop. `TAG_ERROR` accepts `missing`, `duplicate`, or `same_actor` for controlled PIE-only faults; it never saves the injected tags. `CHECK_INVALID_CONFIG=True` exercises rejection/restoration of an invalid pair.

Raw current traces: `Saved/AUTHORED_FINAL_*.json`; route/chart illustration data: `Saved/AUTHORED_FINAL_LAYOUT.json`. Earlier `INCOMPLETE` and fixture-diagnostic traces are preserved separately and are not counted as passes. Fixtures are serialized; never start a second PIE world while one is active.

These controlled patrol/handling tests are not a fresh natural elimination match, packaged-build certification, or a GPU performance benchmark. A 30 FPS cap was used; it is a cap, not a minimum frame-rate claim. See [MANUAL_MAP_GUIDE.md](MANUAL_MAP_GUIDE.md) for setup instructions.
