# Player death spectator

The existing CustomPlayerController keeps Q/F behavior while the player is alive.
It caches the original player ship even during free flight. On that ship's death,
the next controller tick resets blocked input, possesses its own FreeCameraPawn,
and views a surviving Blue teammate. It never possesses a teammate or enemy.

- Tab cycles living Blue teammates. Q does the same after death.
- M opens a battlefield overview. F also opens overview after death.
- A followed teammate's death selects another; no teammates selects overview.
- The dead player pawn is never restored, and its normal sinking continues.
- The free camera has a Cesium GlobeAnchor and geographic height limits, so a
  coordinate-frame change does not clamp it back to an obsolete world Z range.
- HUD shows spectator controls without covering the battle with a player-death
  overlay. Victory/defeat still comes from team elimination.

Native Development Editor build succeeded (7 actions, 35.70 seconds), followed
by a fresh editor and seven explicit Blueprint compiles. The original ship force
graph validator passed. Test: file-based execution of
`Tools/NavalCombat/test_spectator.py` through `Tools/SeaNavigation/mcp_client.py`,
with CASE selecting `health`, `projectile`, `capsize`, or `free`.

Health-death test passed all assertions, including 5 AI controller identities,
automatic follow, both camera handlers, followed-target death, no teammates,
georeference change, submerged fallback hiding and final wreck cleanup.
The first original-projectile test observed death in a Slate callback before
the same frame's PlayerTick and asserted too early. This fixture failure is
preserved in `SPECTATOR_PROJECTILE_SAME_FRAME_FAILURE.json`. The fixture now
allows 0.2 seconds for a controller tick; no gameplay delay or fallback damage
was added. The repeat passed with the original BP projectile delivering the
lethal 10 HP midship hit. Final case results: health 22/22, projectile 24/24,
confirmed physical player capsize 22/22, death while F free camera 23/23.
Capsize is detected by the real sustained-roll threshold, not by calling Die
with a fabricated reason. Each case ran through final wreck cleanup at 40 s.

These are live PIE functional tests that invoke the bound handlers directly;
they do not by themselves prove physical keyboard routing. Final integrated
input/visual QA remains required.
