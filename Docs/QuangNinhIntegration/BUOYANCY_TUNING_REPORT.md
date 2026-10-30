# Buoyancy tuning report - in progress

Fixed seed20261028; mixed Small/Large/Medium roster. Each fixture measures120 actual game seconds after readiness and five seconds settling. Original BP propulsion/rudder, continuous rigid-body physics, waves, terrain and class clearance remain active. No teleport, rotation snap, velocity clamp, added force controller or freeze. Native DLL hash and applied profile values are recorded in JSON.

## Acceptance fixed before tuning

Physical simulation throughout; water contact >=98 percent and no continuous loss>=0.5s; roll max<35deg/pitch max<25deg, p95 both<15deg; vertical speed max<6m/s/p99<3.5m/s, heave range<6m; each AI travels>100m, exceeds5m/s and changes yaw>15deg. Idle player remains a wave-response observation. These operational demo limits are not naval certification.

| Ship | Hull | p99 vertical m/s | Max contact loss s | p95 roll deg | Baseline |
|---|---|---:|---:|---:|---|
| BP_ShipEnemy2 | <ShipHullClass.SMALL: 0> | 2.881 | 0.221 | 10.74 | PASS |
| BP_ShipEnemy | <ShipHullClass.MEDIUM: 2> | 3.104 | 0.349 | 7.04 | PASS |
| Friendly_AI_1 | <ShipHullClass.LARGE: 1> | 4.205 | 0.501 | 11.83 | FAIL |
| Friendly_AI_2 | <ShipHullClass.MEDIUM: 2> | 2.796 | 0.128 | 9.26 | PASS |
| BP_ShipEnemy3 | <ShipHullClass.SMALL: 0> | 2.520 | 0.000 | 9.07 | PASS |
| BP_ShipSmall0 | <ShipHullClass.SMALL: 0> | 2.771 | 0.000 | 12.48 | PASS |

## Measurement-led changes

Historical aefaea6 Large vertical multiplier1.8 is retained in the baseline. Small (90kg, linear0.5/angular0.65, buoyancy0.6/vertical0.8) and Medium (150kg, linear0.5/angular0.5, buoyancy1/vertical1) pass and are unchanged. Large remains240kg, inertia1.8/1.8/2, buoyant lift1.6, rudder0.8, propulsion1.1 and angular damping0.65.

- Baseline vertical1.8/body0.55: FAIL; Large p99 vertical4.205m/s, contact loss0.501s.
- Vertical2.8/body0.55: FAIL; Large p99 3.637m/s, loss0.431s; responsiveness PASS.
- Vertical3.6/body0.55: FAIL; Large p99 3.775m/s, loss0.459s; responsiveness PASS. Ocean phase/timing varies across runs; increasing upward damping alone did not robustly pass.
- Vertical3.6/body0.9 with four end supports: FAIL; vertical speed passes but contact loss0.572s remains. The100m unsupported gap between original pontoon pairs was inspected directly. Added a central pair at(-445,+/-790,-85)cm, retaining all four original locations,100cm radii, lift/mass/inertia/drive/collision hull. Final six-support configuration: PASS120.053s for all three hull classes; Large p99 vertical1.584m/s and contact100 percent with zero loss.

Root cause inspected in UE5.6 BuoyancyComponent.cpp ComputeBuoyantForce: damping subtracts Max(first-order + signed second-order drag,0), and final buoyant force is clamped>=0. It damps upward velocity, leaving downward heave less controlled. Native water linear drag explicitly zeros Z and therefore cannot address downward heave. Moderate existing body linear damping is being tested for both directions; horizontal acceleration/turning must still pass. No engine/plugin code changed.

Evidence: HULL_STABILITY_BEFORE.json, HULL_STABILITY_DAMPING_2_8.json, HULL_STABILITY_DAMPING_3_6.json; full recorded rows in matching Saved files. Final evidence: HULL_STABILITY_AFTER.json, PHYSICS_PONTOON_CONFIGURATION.json and HULL_STABILITY_COMPARISON.png. The latter is rendered only from real before/after sample rows. The rejected body-only run is HULL_STABILITY_BODY_0_9.json. No separate controlled terrain-impact recovery test has been completed in this phase. Natural collision avoidance remains the original navigator.

## Final measured response

| Ship | p99 vertical m/s | Contact percent | Max lost contact s | Max speed m/s | Max yaw change deg | Actual patrol arrivals | Result |
|---|---:|---:|---:|---:|---:|---:|---|
| BP_ShipEnemy2 | 2.792 | 99.71 | 0.239 | 18.55 | 179.95 | 2 | PASS |
| BP_ShipEnemy | 3.207 | 99.71 | 0.344 | 18.80 | 135.74 | 1 | PASS |
| Friendly_AI_1 | 1.584 | 100.00 | 0.000 | 14.04 | 19.02 | 1 | PASS |
| Friendly_AI_2 | 3.277 | 99.90 | 0.116 | 18.86 | 179.89 | 1 | PASS |
| BP_ShipEnemy3 | 2.625 | 100.00 | 0.000 | 22.34 | 179.86 | 2 | PASS |
| BP_ShipSmall0 | 2.678 | 100.00 | 0.000 | 3.87 | 5.08 | 0 | PASS |

Final configuration is in DA_LargeShip; native fallback matches the asset and compiled successfully in UE5.6 Development Editor after the measured run. Small and Medium profile assets are unchanged. Acceleration/deceleration and turning occur through native patrol commands; speed and angular-rate time series are recorded. No separate collision-impact fixture has been accepted; this does not claim impact or bathymetric certification.
