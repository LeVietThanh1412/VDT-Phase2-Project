# Ship buoyancy and steering QA — 2026-11-01

Status: PASSED for this handling scope. Both visual hull families and all three selectable physics profiles were tested after a cold Editor restart, with a 30 FPS cap. The saved profiles and C++ defaults agree.

## Cause and correction

The initial stabilization incorrectly treated UE `InertiaTensorScale` as independent roll/pitch/yaw multipliers. Chaos scales equivalent body dimensions: increasing X/Y also increases yaw inertia, even with Z=1. Actual Enhanced Input reproduced the loss of steering. The corrected profiles keep X/Y=1; only Z is raised (Small=20, Medium/Large=6). Hull mass, collision geometry, propulsion and rudder scales remain as authored. No body rotation or velocity is overwritten.

Passive water resistance damps vertical motion in both directions and local roll/pitch, with no local yaw damping. It runs before physics and stops on destruction/dry hull/disabled simulation. Match-selected buoyancy scalars use Water's supported synchronous path. The engine sprung-mass helper refreshes normalized support weights after profile application; every measured fleet member reported weight sum 1. Small/Medium support radius doubles with an eightfold coefficient reduction to compensate spherical volume.

## Actual PIE measurements

Steering uses real Enhanced Input, original Blueprint forces, aim cameras and projectiles. Rudder command windows last eight seconds; the left measurement excludes the first second of reversal. Large/Medium player tests use the Red team's open-water start to avoid driving an uncontrolled straight line into the defended island. No fixture teleports, freezes physics or assigns hull velocity.

Idle and firing each last 40 game seconds after 15 seconds of settling. All six ships fire ten original Blueprint rounds, with damage disabled only for this controlled measurement. Pitch oscillation is measured around mean trim: the original hull's authored bow/stern support heights naturally produce about one degree of steady trim. Contact is sampled pontoon/water contact.

| Profile | Right / left yaw (deg) | Worst p95 roll (deg) | Worst p95 pitch oscillation (deg) | Worst p99 vertical speed (m/s) | Minimum contact |
|---|---:|---:|---:|---:|---:|
| Small | 35.75 / -33.11 | 0.155 | 0.517 | 0.663 | 100.000% |
| Medium | 23.56 / -20.82 | 0.137 | 0.833 | 0.625 | 100.000% |
| Large | 9.51 / -8.91 | 0.158 | 0.669 | 0.431 | 100.000% |

The 120-second mixed fleet test passed all six stability/responsiveness checks, including physical travel and AI heading changes. The six-AI encounter passed with original shared weapons and preserved controllers; damage was disabled, so this is not a natural elimination-match claim.

C++ compilation succeeded. All 39 Blueprints compiled with generated classes present and no Blueprint compiler errors. No new critical runtime/assertion errors were found; the existing Cesium enum initialization startup diagnostic remains unrelated. The 30 FPS cap checks low-frame-rate handling; it is not a packaged GPU performance benchmark. A fresh natural full-match soak was outside this focused handling regression.

Native DLL SHA-256: `f0b9bacca378f37ddfe585a07f6cc9a969bc4ea27f68064e05399e35eea3a6cf`.

Evidence: [BUOYANCY_STEERING_PIE.json](BUOYANCY_STEERING_PIE.json). Raw traces remain in `Saved/BUOYANCY_*.json`. The two pre-existing external-actor edits were preserved and excluded from this change.
