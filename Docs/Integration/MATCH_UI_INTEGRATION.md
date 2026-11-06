# Teammate Match Settings integration

Baseline: latest remote main `9c74630c7a72716678aff3eecef0e4e0eeea8157` (PR18), isolated branch `feature/integrate-ui-buoyancy`. The original main worktree was clean and remains untouched.

## Source and ownership

The actual teammate UI already belongs to main: PR16 (`feature/ui-3`, merge `3e18479`) introduced the team/deployment layout and kill feed; PR17 (`feat/ui-4`, merge `d7f1846`) added the scoreboard, match summary, loading screen and font changes; PR18 (`small-fix`, merge `9c74630`) updated cameras, naming and cleanup. No historical UI branch was cherry-picked and no replacement menu was created.

`MainMapLevel` uses `/Game/Ship/BluePrint/GMBP`, inheriting the existing native game-mode path. `ACustomPlayerController::BeginPlay` schedules `ToggleMatchSetup` for a fresh game instance. That creates exactly one native `UShipSetupWidget`. Its teammate `RebuildWidget` supplies MATCH SETTINGS / GENERAL / DEPLOY TROOPS / TEAM SETUP, fonts/colors, class fields and +/- controls. `ApplySelection` writes `UNavalMatchSettings`; `StartMatch` closes the menu and calls `StartNewMatch`, followed by level travel. The new controller presents `ULoadingScreenWidget` until `UNavalBattleSubsystem::ConfigureRoster` and `UNavalBattlefieldSubsystem` finish. The existing game-mode selector creates the selected player hull or an independent Full AI camera.

The older MATCH SETUP appearance is not the current source implementation. A fresh native build of main displays the teammate design; stale local binaries must not be mistaken for missing UI commits.

## Correctness fixes

- Hidden invalid seed text no longer blocks manual A/B deployment. Generated mode still rejects negative/non-integer seeds; valid values and zero retain their existing behavior.
- A failed battlefield/roster setup returns to this same menu with its authoritative error, paused world and usable cursor. An unresolved preparation has a bounded90-second wall-clock watchdog instead of an indefinite loading overlay.
- Repeated start requests during pending travel are ignored. Restart explicitly unpauses before scheduling travel, so a paused result/setup cannot strand the travel timer.

Production changes are limited to `ShipSetupWidget.cpp` and `CustomPlayerController.h/.cpp`. The layout, Blueprint assets, combat and navigation implementation are preserved in this phase.

## Bindings and verification

The9-case real PIE menu/travel matrix passed: manual Player+AI, manual Full AI, generated Full AI, generated Red player, manual1v1 Player+AI turns/Easy, manual2v2 Full AI turns/Hard, generated2v3, manual3v1 Red Small player/Hard, and generated1v1 with zero seed. Checks cover +/- limits, deletion preserving the next class choice, negative-seed behavior, actual counters, faction, selected classes/settings, controller counts, mode, deployed seed, released input locks and removed loading overlay. A separate all-Medium generated3v3 completed60s of physical movement with preserved controllers. Missing Full AI A-tag returned to the menu with a named error and a stopped fleet; no sixth hull was spawned.

These are handler-driven integration checks, distinct from desktop interaction and full-match outcome tests. Native C++ compilation,39 Blueprint compile requests (generated classes present, no Blueprint compiler errors) and4 navigation automation tests passed before the matrix. Screenshots and compact evidence are retained with the final regression report. Timeout/desktop checks and the later physics/post-merge matrix are recorded separately as they complete.

Manual deployment uses saved actors and A/B references; generated mode uses seed/sector placement. Counts are1â€“3 per team. In Player+AI the first selected-team slot is the player; Full AI keeps all ships AI-possessed and changes only the independent camera. Difficulty goes to the existing shooting skill; ship mode chooses the existing standard/class balance. A fresh PIE rebuilds the menu from the selected native version.

The missing-roster watchdog passed in90.063 real seconds, returning the paused, usable error menu and removing the loading overlay. The first fault fixture incorrectly changed only Weapon.Faction, while Team() reads Combat.Faction; that diagnostic is preserved in Saved and excluded from passing results. All4 teammate UI screenshots are in `images/`.
