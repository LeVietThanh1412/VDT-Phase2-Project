# Minimap 9x interaction fix

Implemented on the user's `refactor/code`, starting from `6531a0c`. That branch already includes `origin/main` at `4c37eda`, including the original `f7a820f` 9x maximum and `d014f88` tutorial change. The temporary `codex/minimap-9x` branch received no commits and is not the integration source.

## Behavior

- M enters interaction and immediately sets map-content zoom to 9x. M again restores the 1x overview and game input.
- The zoom setter now accepts intermediate values, clamps to 1–9, rejects nonfinite values to the overview and preserves the point at the viewport center.
- Opening match setup exits minimap interaction first, preventing stale zoom/mode state when the menu closes.
- Existing wheel 20% steps, cursor-centered wheel zoom, dragging, double-click 1x/9x and marker compensation remain. The minimap panel dimensions are unchanged.
- Tutorial text describes the controls. No map, ship Blueprint, physics, navigation or combat asset is changed by this fix.

## Verification

Evidence: `Saved/MinimapZoom/20261114/`.

| Check | Actual result |
|---|---|
| Regression before fix | RED: entering interaction stayed at 1x; setting 3x returned 1x |
| Same native regression after fix | GREEN: 1 succeeded, 0 failed, 0 not run |
| Editor Development compile/link | PASS |
| Game Development compile/link | PASS |
| Existing reflected types / Blueprints | 41 identities resolved / 39 compiled; commandlet summary 0 errors, 0 warnings; no saves |
| Actual PIE controller/widget API | PASS, all six checks below |

PIE verified initial 1x, controller activation at 9x with interaction/cursor enabled, intermediate 3x, setup-menu reset to 1x, reentry at 9x after closing setup, and final 1x/game-input restoration. The test owned and ended its PIE session. No ship positions, damage gates, HP, combat settings or assets were modified. These are runtime API checks, not claims of physical keyboard/mouse injection.

The first test-only build on the abandoned flat-layout scratch branch used the wrong header include path and failed. The test was moved to `Private/Tests` on `refactor/code`, its include corrected, and both the meaningful RED and GREEN runs then executed there. This harness error is separate from the two reproduced behavior failures.

## Integration and branch cleanup

Merge the verified `refactor/code` into `main` with normal ancestry, push both refs, and verify the remote hashes. Branch cleanup is limited to obsolete feature/test/codex refs whose exact tips are ancestors of the new main; remote deletion must check the recorded tip. Keep `refactor/code`, all backup refs, and branches with unmerged commits such as `package` and `Thanh`. No history rewrite or broad filesystem cleanup is needed.

## Dynamic ship focus follow-up

M now centers the 9x view on `GetSelectedShip()` and follows its existing Blueprint-projected marker position as it moves. Q/Tab camera selection therefore changes the focus. Without a matching live selected marker, the view follows the centroid of the remaining live markers; invalid/dead markers are ignored. Map-edge clamping remains, so a ship at the edge cannot be centered by exposing blank map space.

The native tick runs after Blueprint tick and reads the paired `ShipMarkerWidgets` / `ShipMarkerSlots` canvas positions. It does not reproject longitude/latitude, retain actor references or change combat/navigation. Focus uses the arranged, unscaled `Overlay_Minimap` viewport, excluding border padding. `GetMapViewportSize()` exposes that size as a vector.

Effective drag, wheel zoom or double-click/manual zoom suspends following. Wheel input already at its limit does not suspend it. M off/on resumes following the currently viewed ship; exit returns to 1x. The tutorial describes ship following.

Verification under `Saved/DynamicMinimap/20261114/`:

- Editor and Game Development compile/link PASS.
- Native minimap regression PASS, including effective drag/wheel suspension, wheel-at-limit and resume.
- Final reflected identity / Blueprint check PASS: 41 native identities, 39 Blueprints compiled after the final native API change, commandlet summary zero errors/warnings; no asset saves.
- Real PIE follow regression: five checks PASS; 91 moving-ship samples; maximum measured map-space centering error 0. Captured `FOLLOW_PIE.png` was visually inspected, showing the player marker at the minimap center.
- PIE checked initial player centering, camera switch and moving-ship following, manual-view persistence, reopening follow and exit to overview. It changes only camera/minimap input and ends its owned PIE; no actor positions, HP or combat settings are overridden.
- `Tools/Testing/verify_minimap_follow.py` reproduces this check through the Editor Python API. Native tests inject pointer events; the PIE manual-view phase uses the public zoom action. No physical mouse/keyboard test is claimed here.

Initial Python probes could not read private widget properties and returned unusable `FGeometry` bounds; unconstrained desired sizes also differed from the arranged viewport. Those attempts are harness failures, not a valid RED centering result. The final probe reads authored marker slots, actual rendered overlay translation and native arranged viewport dimensions. An initial C++ local named `Slot` shadowed `UWidget::Slot`; it was renamed `MarkerSlot`, with no compiler suppression or weakened test.
