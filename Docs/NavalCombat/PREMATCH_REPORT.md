# Match setup

P opens the native setup panel. Choose 1–3 Blue and Red ships, six individual
Small/Large slots, Easy/Normal/Hard AI shooting, and Real-Time or Turn-Based
(10 seconds). Blue slot 1 is always the player. Unselected slots are disabled.
The menu pauses the match; Return/P cancels without changing selections and
resumes it. Start New Match stores selections in GameInstance and reloads the
saved QuangNinh MainMapLevel. The original five placed AI, player spawn and
patrol actors are reused; no replacement fleet or duplicate controllers spawn.
Small/Large use existing physics profiles and placeholder art.

Direct PIE starts 3v3, Small, Normal, Real-Time with 100 HP / 10 base damage.
The standalone BeginPlay path opens setup automatically unless a selection is
already stored. That standalone-specific startup branch is source-inspected;
the executed tests below use PIE. Selection persistence is through level travel,
not an external save file; a fresh PIE gets fresh development defaults.

The first native compile exposed three locals hiding UWidget::Slot and a
TObjectPtr iteration deduction in the separate debug helper. Both were fixed;
normal native build succeeded (5 actions, 4.32 s). Seven battle BPs compiled.
Actual menu/pause/cancel/travel tests passed **127/127** for 1v1, 2v2, 3v3, 2v3
and 3v1. They verify exact pawn/AI-controller counts, one Blue player, per-slot
physics class, difficulty, mode, original patrol references, persisted selections
and fresh-PIE defaults. The turn-selected matches begin with only the player active.

Visual inspection caught clipped buttons at the original 490 design height.
The panel now uses height 560 inside ScaleToFit and a shorter turn label. The
layout rebuild passed (4 actions, 4.18 s), all 127 tests passed again, and the
corrected `PREMATCH_DEFAULT00001.png` shows both buttons fully in a 974×509
editor viewport. `PREMATCH_DEFAULT00000.png` preserves the initial clipping.
Both screenshots show the temporary 1v3 cancel-preview edit; the initial 3v3
defaults are asserted before that edit.

`Tools/NavalCombat/test_prematch_menu.py` executes the handlers bound to the
buttons. `PREMATCH_MENU_PIE.json` records five actual level travels and a fresh
PIE restart. Physical keyboard/mouse injection was not verified: the hidden
automation editor window was not targetable by the Windows computer-use helper.
Bindings/delegates are source-inspected and handler behavior is live-tested.
