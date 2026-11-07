# UI and buoyancy integration regression

Status: native gameplay candidate passed its cumulative 24 checks, including a fresh natural 3v3 result. Post-main-to-feature checks passed before remote integration; a remote merge is certified only in the Git integration report.

## Evidence boundaries

Native build, Blueprint compilation, isolated navigation automation and real saved-map PIE are separate checks. No compilation-only result is counted as gameplay proof. `Tools/Testing/ui_buoyancy_suite.json` specifies the serialized PIE matrix; `run_final_regressions.py` records the tested DLL hash and stops at failures. Raw evidence stays in Saved; compact results and screenshots are retained in Docs.

The physical wave protocol is described in [SHIP_BUOYANCY_TUNING.md](../Physics/SHIP_BUOYANCY_TUNING.md), with absolute/centered metrics in [BUOYANCY_METRICS.json](BUOYANCY_METRICS.json). It uses a transient probe's initial placement and original physical actuators thereafter. No wave animation or angle clamp supplies compliance.

Authored patrol disables damage only, preserving the saved ship and target placements. Player-input checks inject the actual Enhanced Input actions. The shoreline fixture uses explicit initial probe placement and physical forward/reverse thrust. Tactical retreat parks non-subject fixture ships and supplies a legal hostile/low-health condition; the subject continues using native physics and navigation. Core/result fixtures deliberately apply damage and one capsize pose; they are **not natural matches**. Dynamic-placement checks explicitly relocate fixture ships/targets to validated water, then observe native reinitialization/replanning and physical continuation. Only the separately identified lethal seeded run is counted as an autonomous natural result.

## Completed checks before extended regression

- Latest teammate MATCH SETTINGS rendered in actual PIE. Real mouse input tested add/remove, Full AI selection, Start, menu reopen/resume, fresh PIE and Quit. Six controllers plus an independent spectator were verified; no loading/input locks remained.
- UI startup/travel nine-case matrix passed before physics tuning. The later ten-case matrix additionally checks class-based balance and actual per-slot hull components, counters, team selection, difficulty and mode.
- Missing authored player target returned a named setup error; a deliberately invalid roster exercised the 90.063-second real-time timeout and restored the same menu. The first timeout fault changed only the Weapon faction; that diagnostic was corrected to include Combat faction, which is authoritative for Team().
- The full three-hull/five-state physics protocol passed. Medium/Small shoreline checks retained water contact and reversed approximately 99 metres away from the coast. Player checks passed for all three classes, including both rudder directions, mouse aim, firing and camera release.
- Real-Time and both Full AI/Player+AI Turn-Based startup were exercised. The extended turn checks passed 120 simulation seconds each: twelve turns with alternating teams and approximately ten-second periods; inactive ships issued no propulsion/rudder force.
- Native C++ builds succeeded, all 39 Blueprint compile requests produced generated classes with no Blueprint compiler errors, and all four navigation automation tests succeeded. Unreal/Cesium warnings are separated from new runtime failures in the final log review.

## Investigated fixture failure

The first extended suite stopped at the old core fixture's capsize assertion. The native implementation correctly saw only 96–100° against its unchanged 105° threshold. `unreal.Rotator(0, yaw, 179)` uses Python's positional roll/pitch/yaw order, so it had not applied the intended inversion. The fixture now uses `Rotator(pitch=0, yaw=..., roll=179)`. Captured telemetry confirmed approximately 179° and the native CAPSIZED death path; the corrected core fixture passed, including shooter cleanup. No gameplay threshold, collision policy or capsize algorithm was changed to satisfy the test. Failed diagnostics are preserved separately in Saved.

## Lifecycle and fixture corrections

An additional early-outcome fault found a production lifecycle failure: a summary could be shown while its loading watcher remained active, and continued preparation later dereferenced a cleaned-up assignment actor (`NavalBattlefieldSubsystem.cpp`, original line 655). The pre-fix crash/log remains in Saved/EARLY_RESULT_CRASH.log. `ShowMatchResult` now cancels loading first and ignores pending-travel results. Battlefield Tick returns after the authoritative outcome; missing/destroyed actors during unfinished preparation produce a named error. The corrected fault passed a 35-second hold, preserved summary/cursor, return/back/idempotence and six-ship restart. No A* or propulsion algorithm was rewritten.

Reverse-fire, result/restart and live target/ship relocation now pass. The dynamic fixture initially confused identical actor labels (the spawned flagship and a placed opponent), and its five-second polling delay could miss a moved active goal before arrival. It now identifies actors by unique object name, moves the target farther along a validated water route and polls rapidly until the new route endpoint is observed. All six ship relocations reinitialized, all six moved targets appeared within 1 m of the physical route endpoint, and every ship physically continued. Native A*, throttle and rudder algorithms were not changed.

An earlier mixed-hull diagnostic's recorder encountered a transient Windows rename denial while an external reader held its JSON snapshot. The native battle remained running; its observer was reattached to the same world without damage/pose/drive edits. The raw diagnostic and the 136-second observation gap are retained explicitly. Snapshot writes now tolerate a temporary reader lock and retry at the next sample. That diagnostic's observation bound was extended from 900 to 1800 simulation seconds after native reacquisition and a third sinking around 850 seconds. The fresh final-build rerun starts with the 1800-second bound and retains the original natural-winner criterion.

That mixed-hull run was subsequently retained as a navigation diagnostic rather than a passing natural match. A stationary same-heading queue defeated the existing bounded traffic-priority lease: `Following` depended only on heading, so a stopped leader could suppress the priority ship indefinitely. The smallest correction requires the lead hull to be moving above the existing 1 m/s stopped-hull threshold before treating it as moving-following traffic. The normal priority policy, player yielding, A*, swept collision, emergency brake, throttle and rudder pipeline remain authoritative. Native build and all 39 Blueprint compile requests succeeded without Blueprint compiler errors; all four navigation automation tests passed. An explicit physical reproduction passed in 39.13 seconds, including the priority lease, more than 100 m of travel, a real endpoint arrival, water contact and 141.9 m minimum hull-center separation. See [STATIONARY_TRAFFIC_PIE.json](STATIONARY_TRAFFIC_PIE.json). Fresh full patrol and natural-battle checks use the corrected DLL `9eb9db985a56281a8fc7a13a9f03a77386a4e4c9917deb7ca3bf42ab530f7845`.

That build passed the complete manual endurance but its seeded natural run reached the 1800-second observation limit with the last Red escort failed and its strategic retry count fixed at three. Two Blue ships remained active. This run is preserved as `Saved/MISSION_RETRY_NATURAL_DIAGNOSTIC.json`; it is not a natural-result pass. A further small correction in the existing assault coordinator reopens a failed/idle/arrived escort's retry window after the existing search cooldown and requests a fresh goal through the existing validated planner. Active combat destinations and scout-search missions keep their own authority. No forces, poses, damage or winning rules are changed.

The targeted exhausted-budget fault passed on DLL `bf8b893a3b5ccc88e02bc9744982d81b2f723ec12683e27115a1c38a228519b6`: the mission held through its cooldown, selected a fresh goal and physically moved over 50 m with its original controller and water contact. The injected budget uses the untouched initial retry timestamp of zero; native resumption occurred at world time 45.22 s. See [ASSAULT_RETRY_WINDOW_PIE.json](ASSAULT_RETRY_WINDOW_PIE.json). Native compilation, 39 Blueprint requests without compiler errors and all four navigation automation cases passed again. The subsequent UI/player/natural-result matrix uses this final candidate. The earlier full manual endurance remains applicable because authored mode returns before the generated-assault coordinator and its source path/assets are unchanged.

The final-candidate rerun passed all ten UI configurations and the Small player-input case. Its natural battle was then stopped/replaced from the Editor outside the fixture after 349.05 observed simulation seconds, with two ships sunk. That attempt is explicitly `INTERRUPTED_EDITOR_SESSION_CHANGED`, not a completed result or a new gameplay failure. Own callbacks were removed without changing the user's replacement session or map assets. The fixture now checks the observed world before touching gameplay objects and interrupts without closing a replacement world. Both ended-world and replaced-world contract tests passed: [PIE_WORLD_IDENTITY_GUARD.json](PIE_WORLD_IDENTITY_GUARD.json). At that checkpoint an uninterrupted natural-result run and integration checks were still required. The fresh completed result is recorded below; the interrupted attempt remains separate.

## Current regression matrix

Compact results, per-fixture DLL/source hashes and raw evidence fingerprints are in [PIE_EVIDENCE_SUMMARY.json](PIE_EVIDENCE_SUMMARY.json). Earlier failed suite attempts retain their original status in Saved; this cumulative table does not relabel them as passing. The final Small-only parameter change was rebuilt as DLL `6f9e282849a16cedc493156ab2df5612b0bd95c3647bcce387f4b4b3b00c28fd`.

| Feature | Test type | Candidate result |
|---|---|---|
| Teammate UI, counts, classes, seed/manual, balance, difficulty | Real PIE, 10 cases | Pass |
| Desktop add/remove, Full AI, Start, reopen/resume, Quit | Actual mouse input | Pass |
| Live HUD, minimap, opposing-team/teammate camera | Actual I/Q/Tab input during battle | Pass |
| Small, Medium, Large player controls and original shots | Enhanced Input in real PIE | Pass |
| Small final 2–3 degree wave response | Five-state physical measurement | Pass |
| Three-hull initial response | 15 hull/state physical measurements | Pass; initial Small superseded |
| Final Small/Medium shoreline and physical reverse | Real ocean/collision PIE | Pass |
| Player+AI and Full AI Turn-Based | 120 seconds each, 12 alternating turns | Pass |
| Regional damage, team/self/duplicate guards, capsize, cleanup | Explicit fault fixtures | Pass |
| Native retreat, reverse fire and resume | Controlled tactical PIE fixture | Pass |
| Full AI result, player victory and defeat, return/restart | Explicit lethal fixtures | Pass |
| Early result through wreck cleanup | 35-second fault hold, return/restart | Pass |
| Actor lost during preparation | Named failure, menu/cursor, fresh retry | Pass |
| Live ship and target relocations | 12 validated relocations, physical continuation | Pass |
| Six authored ships, four physical A/B arrivals each | Fresh saved-map endurance | Pass; 1350.93 simulation seconds |
| Natural seeded 3v3 winner and native summary | Autonomous lethal battle | Pass; fresh uninterrupted BLUE WINS at 803.00 simulation seconds |
| Main-to-feature and merged-main verification | Build, Blueprint and gameplay checks | Pending |

## Authored waypoint clearance correction

The unmodified six-ship endurance exposed a saved-layout bottleneck at Friendly_AI_2’s B target (TP_Blue02_B). A logical route existed, but final local maneuvering repeatedly entered recovery near shore; the stalled hull also delayed a crossing ship. This diagnostic run is retained separately and is not counted as a passing endurance test.

A first candidate 150 m from the original target passed the existing individual full-hull placement and connected-route checks. Its fresh six-ship run nevertheless delayed arrival for several minutes: the marker was only 118 m from the opposing A marker, so one hull's turn obstructed the other's approach. It eventually arrived after that observation was stopped; this attempt remains diagnostic evidence and is not certification.

The final candidate at chart EN (12000, -3000) is approximately 645 m from the original B marker and 481 m from the opposing A marker. All twelve tested headings (30-degree increments) passed full-hull placement and a connected route was found. Only that external target actor package was saved through Unreal Editor; ship starts, all other target positions, terrain, ocean and MainMapLevel.umap are unchanged. The waypoint correction does not alter A*, arrival tolerance or collision filters. Coordinates, package hashes and the original backup reference are in [WAYPOINT_CLEARANCE_CORRECTION.json](WAYPOINT_CLEARANCE_CORRECTION.json).

Cold reload reproduced the saved position exactly. The subsequent untouched six-Medium patrol passed all checks in 1350.93 simulation seconds: every ship physically reached at least four alternating A/B endpoints; the fleet recorded 4–8 arrivals per ship. All original controllers, marker references and physical simulation were preserved, with no tactical route replacement. Minimum water-contact fraction was 99.974%. Maximum recorded pitch/roll was 3.05/4.49 degrees during the extended maneuvering/close-quarters run, distinct from the representative idle wave measurements. The test imposed no angle clamp or ship drive/pose writes. See [AUTHORED_SIX_SHIP_PIE.json](AUTHORED_SIX_SHIP_PIE.json).

## Final uninterrupted natural result

The fresh final-candidate seeded mixed-hull 3v3 match reached native **BLUE WINS** after **802.999 simulation seconds**. All six ships fired (23, 20, 29, 8, 17, 10 shots); all three Red deaths were HEALTH_DEPLETED, with no capsize or injected damage. Every ship preserved physical movement and its AIController through its live lifetime. The native summary appeared exactly once with a usable cursor and correct Blue 3 / Red 0 counters. This is separate from prior diagnostics and the externally interrupted attempt. See [NATURAL_MATCH_PIE.json](NATURAL_MATCH_PIE.json) and [actual native summary](../Integration/images/natural-match-result.png).

## Post-main-to-feature regression

After fetch/merge reported Already up to date, a fresh native build, 39 Blueprint requests and all four navigation automation cases passed. The rebuilt DLL then passed the ten-case menu matrix, final Small physical/player input, all-six nonlethal firing opportunities and result/return/restart. Production source digest and assets are unchanged from the 803-second natural result. See [POST_INTEGRATION_VERIFICATION.json](POST_INTEGRATION_VERIFICATION.json). The remote/main verification is recorded separately in the integration report.
