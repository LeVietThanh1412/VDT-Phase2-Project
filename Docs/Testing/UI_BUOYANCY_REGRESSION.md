# UI and buoyancy integration regression

Status: final regression in progress on `feature/integrate-ui-buoyancy`; this document does not yet certify a release or remote merge.

## Evidence boundaries

Native build, Blueprint compilation, isolated navigation automation and real saved-map PIE are separate checks. No compilation-only result is counted as gameplay proof. `Tools/Testing/ui_buoyancy_suite.json` specifies the serialized PIE matrix; `run_final_regressions.py` records the tested DLL hash and stops at failures. Raw evidence stays in Saved; compact results and screenshots are retained in Docs.

The physical wave protocol is described in [SHIP_BUOYANCY_TUNING.md](../Physics/SHIP_BUOYANCY_TUNING.md), with absolute/centered metrics in [BUOYANCY_METRICS.json](BUOYANCY_METRICS.json). It uses a transient probe's initial placement and original physical actuators thereafter. No wave animation or angle clamp supplies compliance.

Authored patrol disables damage only, preserving the saved ship and target placements. Player-input checks inject the actual Enhanced Input actions. The shoreline fixture uses explicit initial probe placement and physical forward/reverse thrust. Tactical retreat parks non-subject fixture ships and supplies a legal hostile/low-health condition; the subject continues using native physics and navigation. Core/result fixtures deliberately apply damage and one capsize pose; they are **not natural matches**. Dynamic-placement checks explicitly relocate fixture ships/targets to validated water, then observe native reinitialization/replanning and physical continuation. Only the separately identified lethal seeded run is counted as an autonomous natural result.

## Completed checks before extended regression

- Latest teammate MATCH SETTINGS rendered in actual PIE. Real mouse input tested add/remove, Full AI selection, Start, menu reopen/resume, fresh PIE and Quit. Six controllers plus an independent spectator were verified; no loading/input locks remained.
- UI startup/travel nine-case matrix passed before physics tuning and again with the final profiles. Actual per-slot hull components, counters, team selection, difficulty and mode were checked. Class-based balance is additionally checked in the final post-merge matrix.
- Missing authored player target returned a named setup error; a deliberately invalid roster exercised the 90.063-second real-time timeout and restored the same menu. The first timeout fault changed only the Weapon faction; that diagnostic was corrected to include Combat faction, which is authoritative for Team().
- The full three-hull/five-state physics protocol passed. Medium/Small shoreline checks retained water contact and reversed approximately 99 metres away from the coast. Player checks passed for all three classes, including both rudder directions, mouse aim, firing and camera release.
- Real-Time and both Full AI/Player+AI Turn-Based startup were exercised. The extended turn checks passed 120 simulation seconds each: twelve turns with alternating teams and approximately ten-second periods; inactive ships issued no propulsion/rudder force.
- Native C++ builds succeeded, all 39 Blueprint compile requests produced generated classes with no Blueprint compiler errors, and all four navigation automation tests succeeded. Unreal/Cesium warnings are separated from new runtime failures in the final log review.

## Investigated fixture failure

The first extended suite stopped at the old core fixture's capsize assertion. The native implementation correctly saw only 96–100° against its unchanged 105° threshold. `unreal.Rotator(0, yaw, 179)` uses Python's positional roll/pitch/yaw order, so it had not applied the intended inversion. The fixture now uses `Rotator(pitch=0, yaw=..., roll=179)`. Captured telemetry confirmed approximately 179° and the native CAPSIZED death path; the corrected core fixture passed, including shooter cleanup. No gameplay threshold, collision policy or capsize algorithm was changed to satisfy the test. Failed diagnostics are preserved separately in Saved.

## Pending release gates

An additional early-outcome fault found a production lifecycle failure: a summary could be shown while its loading watcher remained active, and continued preparation later dereferenced a cleaned-up assignment actor (`NavalBattlefieldSubsystem.cpp`, original line 655). The pre-fix crash/log remains in Saved/EARLY_RESULT_CRASH.log. `ShowMatchResult` now cancels loading first and ignores pending-travel results. Battlefield Tick returns after the authoritative outcome; missing/destroyed actors during unfinished preparation produce a named error. The corrected fault passed a 35-second hold, preserved summary/cursor, return/back/idempotence and six-ship restart. No A* or propulsion algorithm was rewritten.

Complete reverse-fire, result/restart, live target/ship relocation, six-ship A/B endurance, natural battle result, main-to-feature integration and post-merge checks before certification. Final compact evidence and the completed acceptance table will replace this progress status.
