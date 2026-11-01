# Return to Setup after match results

Verified2026-11-01 against baseline7743ec0. Development Editor C++ build and39 Blueprint compiles passed. Source/DLL hashes are in RETURN_TO_SETUP_PIE.json.

Finished matches now show a top-center result card with RETURN TO SETUP. The first authoritative result reveals the card and cursor, including when ship details were hidden. The button opens the existing match setup with current selections preserved. Repeated return requests keep the same menu open. BACK TO GAME returns to a clickable result screen; START NEW MATCH travels through the existing path and resets the result, alive counts and six-ship roster.

Global result/alive-counter text is refreshed before resolving the selected ship, so it remains current after a ship or wreck disappears. Result visibility reads authoritative match state independently of a selected pawn.

| Case | Input | Result |
|---|---|---|
| Full AI BLUE WINS | Actual desktop mouse click | PASS |
| Full AI RED WINS | Actual desktop mouse click | PASS |
| Blue player VICTORY, HUD initially hidden | Same controller action used by button | PASS |
| Blue player DEFEAT, original ship cleaned up | Same controller action used by button | PASS |
| Red player VICTORY | Same controller action used by button | PASS |
| Red player DEFEAT, original ship cleaned up | Same controller action used by button | PASS |

Every case checks inactive return before a result, interactive result/cursor, correct counters/result, preserved setup selections, idempotence, Back to Game cursor restoration and a fresh unpaused3v3 after Start New Match. Defeat waits for actual original-ship removal rather than assuming an exact timer boundary or that every heavy wreck has disappeared. Initial fixture lifetime assumptions were corrected; the final six cases all pass on one frozen native DLL.

Scope: CustomPlayerController.h/.cpp, NavalBattleUI.cpp and UI/ShipDetailsUI.h/.cpp. Existing setup, navigation, physical profiles and asset paths are retained. The two pre-existing external-map-actor edits are byte-for-byte preserved against Saved/ReturnToSetupCheckpoints/20261101_114845 and excluded from this change.

Run Tools/Testing/verify_return_to_setup.py through the existing Editor bridge. FULL_AI,LOSING_TEAM,PLAYER_TEAM and WAIT_FOR_CLICK select cases. WAIT_FOR_CLICK pauses the fixture at the result card for a real mouse click; fixtures use guarded lethal damage, not fake outcome assignment. Evidence: RETURN_TO_SETUP_PIE.json and RETURN_BLUE_WIN.png / RETURN_RED_WIN.png / RETURN_PLAYER_RED_DEFEAT.png.
