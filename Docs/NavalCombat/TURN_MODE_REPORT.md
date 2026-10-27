# Optional turn-based naval battle

One 0.1-second timer in the existing NavalBattleSubsystem owns scheduling.
Default direct PIE remains real time. Turn mode alternates Blue/Red; within
each team, stable roster cursors choose the next living ship for a 10-second
window. Dead entries are skipped and an active death advances immediately.
Team elimination clears the active ship and prevents further actions.

The existing original Blueprint propulsion/rudder force filters and shared
weapon enforce permission for player and AI. The aim selector holds inactive
manual aim; inactive AI skip aiming/tactical decisions and output zero drive
commands. Active-time tactical timers do not consume the inactive window.
The world is never paused for turns: buoyancy, inertia, existing projectiles,
damage, capsize detection and geographic wreck sinking/cleanup continue.

GameInstance match settings preserve selections through level travel. The
roster configurator reuses the existing player and five placed AI; it can remove
unselected AI and their controllers and apply existing class/skill presets.
The untouched direct PIE path preserves saved settings. Selected-count/class
configuration remains a separate prematch UI acceptance gate.

Native build: 13 actions / 33.07 seconds, succeeded; seven battle Blueprints
compiled after a full editor restart. `TURN_MODE_PIE.json[.gz]` records an actual
saved-level run with 18 completed windows (three full six-ship rotations), a
19th observed active turn, 18 real original-BP projectiles observed continuing
after the shooter's window closed, all five AI physically moving, preserved
controller identities, active-death skip, victory and final wreck cleanup.
Observed window durations were 10.001–10.190 seconds, including frame/timer
scheduling delay. All 2,932 assertions passed; this count includes repeated
per-frame/per-ship invariants, not 2,932 independent scenarios.

Diagnostic failures are retained:

- `TURN_ROSTER_INITIAL_FAILURE.json`: editor started as a built-in spectator,
  with a standard PlayerController/SpectatorPawn and five AI. The six-ship check
  failed correctly. A fresh normal PIE had the proper player and configured
  roster. UE PlayLevel.cpp also selects spectator startup when Ctrl is held.
- `TURN_PLAYER_SENSOR_FIXTURE_FAILURE.json`: the fixture tried to read AIThrottle
  from the player Blueprint. The probe now reads AI fields only on AI actors.
- `TURN_RETURNING_TEST_ROUND_CAPSIZE_FAILURE.json[.gz]`: upward fixture rounds
  were left alive after observation; a stationary player later capsized and the
  scheduler correctly skipped it. A returning round collision was a suspected
  contaminant. The fixture now destroys each upward test round after its
  boundary observation. The full repeat had no unplanned deaths; no native
  physics/capsize threshold was changed to obtain that pass.

The separate 90-second real-time integration passed **20/20** checks, covering
Enhanced Input movement and original manual projectile fire, GunCamera, Q/F,
five moving AI/controllers/exact target endpoints, minimap counts/colors/motion
and death removal. Evidence: `TURN_REALTIME_INTEGRATION_PIE.json[.gz]`.
Final integrated/menu/input/performance/full-patrol QA remains a separate gate.

One subsequent editor startup crashed before gameplay in Zen/DevHttp's local
DDC HTTP reset. Retrying with the engine's existing
`-DDC=InstalledNoZenLocalFallback` profile succeeded after cold shader compilation;
the real-time regression ran in that session. This is a session-only launch
option; no engine/plugin/DDC graph config was edited or cache deleted.
