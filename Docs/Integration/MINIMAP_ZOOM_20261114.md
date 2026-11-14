# Minimap 9x interaction fix

## Current controls

- **M:** toggle 1x overview / 9x ship-follow view. Gameplay retains mouse look, aim and fire; the cursor stays hidden.
- **Hold Left Alt while zoomed:** show the cursor to drag, wheel-zoom or double-click the minimap. Release Alt to restore gameplay, keeping the current map view. Actual manual map movement suspends following; M off/on resumes following.
- Setup, loading and match-result menus retain their own cursor/input behavior. M cannot replace a modal menu's input mode.

The sections below record the earlier implementation and its follow-ups; the current controls above supersede the original M-to-cursor behavior.

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

## Gameplay mouse ownership follow-up

Problem: M used GameAndUI, displayed the cursor and left the minimap subtree hit-testable. Its mouse handlers could capture the same buttons used for aiming and firing.

The controller now separates visual zoom from explicit mouse interaction. M uses GameOnly with `SetConsumeCaptureMouseDown(false)`, so a capture click is forwarded to gameplay. The minimap and all children are HitTestInvisible until Left Alt is held; all native mouse handlers also reject passive input. Releasing Alt clears dragging and existing minimap capture. PlayerTick recovers a missed Alt release using current key state. Setup/loading/result transitions reset map interaction without allowing a late release to replace modal input. The existing Enhanced Input mappings, aiming gate and projectile dispatch are preserved.

Evidence: `Saved/MinimapInput/20261114/` and the refreshed `Saved/DynamicMinimap/20261114/FOLLOW_PIE.json`.

| Check | Result |
|---|---|
| Native reproduction before fix | FAIL: zoom handled wheel input without explicit map control |
| Same native regression after fix | PASS: 1 succeeded, 0 failed; includes passive fire-button rejection, manual interaction and follow resume |
| Editor / Game Development compile and link | PASS / PASS |
| Reflection and Blueprint commandlet | 41 native identities resolved, 39 Blueprints compiled; 0 errors, 0 warnings; no asset saves |
| Owned PIE input regression | Eight checks PASS: passive zoom, explicit cursor mode/release, setup transition/late release, modal guard, missed-release recovery, original Enhanced Input aim/fire path, overview restoration |
| Actual Blueprint firing via Enhanced Input at 9x | Shots 0 to 1; rounds 10 to 9; damage remained enabled |
| Refreshed dynamic-follow PIE regression | All five checks PASS; 93 samples, maximum map-space centering error 0; selected-ship/camera-switch follow, manual-view persistence, follow resume and overview restoration |
| Desktop keyboard M | Observed entering 9x while native cursor state stayed false |

Desktop-tool click/drag attempts did not produce a projectile; a diagnostic callback sampled zero LeftMouseButton-down frames in Unreal. These attempts are inconclusive for physical mouse firing and are retained separately as `PHYSICAL_*` evidence, not counted as successful gameplay tests. The final firing check injects the existing IA_Aim/IA_Fire Enhanced Input actions through the original Blueprint path; it does not call FirePlayer directly. Temporary aim callbacks were stopped and owned PIE sessions ended. Real held-Alt dragging/focus-loss and physical RMB+LMB remain manual smoke checks.

`Tools/Testing/verify_minimap_gameplay_input.py` reproduces the eight PIE checks. The follow test now waits until the minimap is visible after loading before toggling M. No ship positions, health, damage settings, collision, buoyancy, navigation, input assets or runtime JSONs changed. Packaging was not rerun.
