# Teammate Match Settings integration

Baseline: latest remote main `9c74630c7a72716678aff3eecef0e4e0eeea8157` (PR18), isolated branch `feature/integrate-ui-buoyancy`. The original main worktree was clean and remains untouched.

## Source and ownership

The actual teammate UI already belongs to main: PR16 (`feature/ui-3`, merge `3e18479`) introduced the team/deployment layout and kill feed; PR17 (`feat/ui-4`, merge `d7f1846`) added the scoreboard, match summary, loading screen and font changes; PR18 (`small-fix`, merge `9c74630`) updated cameras, naming and cleanup. No historical UI branch was cherry-picked and no replacement menu was created.

`MainMapLevel` uses `/Game/Ship/BluePrint/GMBP`, inheriting the existing native game-mode path. `ACustomPlayerController::BeginPlay` schedules `ToggleMatchSetup` for a fresh game instance. That creates exactly one native `UShipSetupWidget`. Its teammate `RebuildWidget` supplies MATCH SETTINGS / GENERAL / DEPLOY TROOPS / TEAM SETUP, fonts/colors, class fields and +/- controls. `ApplySelection` writes `UNavalMatchSettings`; `StartMatch` closes the menu and calls `StartNewMatch`, followed by level travel. The new controller presents `ULoadingScreenWidget` until `UNavalBattleSubsystem::ConfigureRoster` and `UNavalBattlefieldSubsystem` finish. The existing game-mode selector creates the selected player hull or an independent Full AI camera.

The older MATCH SETUP appearance is not the current source implementation. A fresh native build of main displays the teammate design; stale local binaries must not be mistaken for missing UI commits.

## Correctness fixes

- Hidden invalid seed text no longer blocks manual A/B deployment. Generated mode still rejects negative/non-integer seeds; valid values and zero retain their existing behavior.
- A failed battlefield/roster setup returns to this same menu with its authoritative error, paused world and usable cursor. An unresolved preparation has a bounded 90-second wall-clock watchdog instead of an indefinite loading overlay.
- Repeated start requests during pending travel are ignored. Restart explicitly unpauses before scheduling travel, so a paused result/setup cannot strand the travel timer.
- Extended regression found a result/loading race: the loading watcher's final input reset could hide the result cursor. Showing the summary now cancels the loading transition first and ignores stale results while travel is pending. An early-result fault also exposed a preparation Tick dereferencing a cleaned-up ship; preparation/mission ticks now respect the authoritative finished outcome, and lost preparation actors return a named setup error. A 35-second early-result hold through wreck cleanup, return and restart passed after these small lifecycle fixes.

The initial UI-binding commit modified `ShipSetupWidget.cpp` and `CustomPlayerController.h/.cpp`, preserving the teammate layout and widget assets. The later preparation-lifecycle guard and stationary-traffic correction are documented separately in the regression report.

## Bindings and verification

The final 10-case real PIE menu/travel matrix passed: manual Player+AI, manual Full AI, generated Full AI, generated Red player, manual1v1 Player+AI turns/Easy, manual2v2 Full AI turns/Hard, generated2v3, manual3v1 Red Small player/Hard, generated1v1 with zero seed, and mixed-hull class-based balance. Checks cover +/- limits, deletion preserving the next class choice, negative-seed behavior, actual per-slot Movement hull classes, counters, faction, controller counts, mode, deployed seed, released input locks and removed loading overlay. Each ship's health and projectile damage were compared with the selected standard or class profile. A separate all-Medium generated 3v3 completed 60s of physical movement with preserved controllers. Missing Full AI A-tag returned to the menu with a named error and a stopped fleet; no sixth hull was spawned.

These are handler-driven integration checks, distinct from desktop interaction and full-match outcome tests. Native C++ compilation, 39 Blueprint compile requests (generated classes present, no Blueprint compiler errors) and 4 navigation automation tests passed before the matrix. Screenshots and compact evidence are retained with the final regression report. Timeout/desktop checks and the later physics/post-merge matrix are recorded separately as they complete.

Manual deployment uses saved actors and A/B references; generated mode uses seed/sector placement. Counts are 1–3 per team. In Player+AI the first selected-team slot is the player; Full AI keeps all ships AI-possessed and changes only the independent camera. Difficulty goes to the existing shooting skill; ship mode chooses the existing standard/class balance. A fresh PIE rebuilds the menu from the selected native version.

The missing-roster watchdog passed in 90.063 real seconds, returning the paused, usable error menu and removing the loading overlay. The first fault fixture incorrectly changed only Weapon.Faction, while Team() reads Combat.Faction; that diagnostic is preserved in Saved and excluded from passing results. All4 teammate UI screenshots are in `images/`.

## Using the integrated menu

1. Start fresh PIE on MainMapLevel. MATCH SETTINGS appears automatically. The default player hull is Medium; choose Small or Large in that team’s first slot to change it for the next match.
2. Use each team’s + and X buttons for 1–3 ships. X removes that row and preserves the following class selections. Select difficulty, Real-Time/Turn-Based, standard/class balance, and Player+AI/Full AI. Player+AI uses the first slot of the selected player team; Full AI keeps all hulls AI-possessed.
3. For generated deployment, click GENERATE FROM SEED, enter a non-negative integer, then START NEW MATCH. Zero chooses a new seed. For manual deployment, click GENERATE MANUALLY (A-B); the saved map and referenced target actors are used. Seed input is hidden and does not block manual startup.

Before running manual mode, place the five AI hulls and set each Navigation component’s Patrol Actor A/B to two different valid-water TargetPoints. Position PlayerStart for the player. Full AI also needs exactly one target tagged Naval.PlayerPatrol.A and one different target tagged Naval.PlayerPatrol.B for the sixth ship. Keep ship starts and complete routes clear of islands and other hulls; invalid water or disconnected routes are diagnosed rather than silently teleported. AI physically travels from its placed start to A, then B, then A repeatedly. Extended endurance required moving only Friendly_AI_2's TP_Blue02_B approximately 645 m into open water, giving 481 m clearance from the opposing A marker; the saved ship starts and all other markers remain unchanged. See the regression report for exact coordinates and validation.

P reopens settings during play; BACK TO GAME resumes that match. START NEW MATCH performs level travel using the current selections. A fresh initial menu has QUIT, which was tested by a real mouse click and ends PIE normally. The teammate’s result screen supplies RETURN TO MAP SETTING, RESTART and QUIT.

Real desktop interaction additionally verified remove/add, selecting Full AI, clicking Start, six AI controllers plus the independent spectator, released loading/input locks, reopening/resuming, fresh PIE restart and Quit. The automated post-physics matrix also verifies actual per-slot Movement profiles, not only the stored settings.

During the final natural battle, real I/Q/Tab input opened the live details, followed an opposing-team ship and cycled teammates. The details' displayed ship matched the controller's selected ship, the minimap remained visible, and every surviving ship retained its original AIController while the player stayed in FreeCameraPawn. Native kill-feed messages and decreasing live counters were captured. [Desktop HUD evidence](../Testing/PIE_EVIDENCE_SUMMARY.json) separates these checks from handler-driven fixtures.

## Original launch-path diagnosis, 2026-11-07

The user screenshot with MATCH SETUP came from an outdated original-worktree DLL. Source in both worktrees already contains MATCH SETTINGS; the teammate implementation was not lost. That old editor had no active PIE or dirty packages and was closed cleanly. The original local BlackOpsOne font package is separately backed up and preserved. A final rebuild of the original main project and actual menu verification are required after the PR, rather than replacing UI source or discarding the font. The fresh feature battle reached BLUE WINS naturally at 803 seconds; an actual mouse click on Return reopened MATCH SETTINGS.
