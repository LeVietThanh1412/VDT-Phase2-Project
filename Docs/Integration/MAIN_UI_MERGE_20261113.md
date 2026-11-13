# Main UI integration — 2026-11-13

Destination: `refactor/code`, starting at `3c7af1a`. Source: fetched `origin/main` at `4c37eda`. Local `main` remains at `a1ea046`. Clean starting worktree; recoverable reference: `backup/refactor-before-main-ui-20261113`. No stash, history rewrite, force push or main modification.

## Resolution and preserved behavior

A regular `--no-ff` merge produced five modify/delete conflicts because main changed the old flat paths. The upstream UI changes were incorporated into these existing destination files:

- `Private/Gameplay/CustomPlayerController.cpp`, `Public/Gameplay/CustomPlayerController.h`.
- `Private/UI/NavalBattleUI.cpp`, `Public/UI/NavalBattleUI.h`.
- `Private/UI/ButtonTutorialsUI.cpp`.

All paths are under `Source/VDTPhase2Project/`. The temporary upstream copies at the old paths were removed only after transferring their behavior; no duplicate classes or old root implementations remain. No binary asset, map, gameplay component, configuration, module dependency or reflected class identity was replaced.

Retained main features: M interaction toggle, cursor/game-and-UI input mode transitions, zoom up to 9×, wheel multiplication/division by 1.2, cursor-centered zoom, left/right drag and clamping, double-click zoom/reset, marker scale compensation with up to 30% visual growth, authored border visibility/clipping, and the M tutorial entry. Existing destination marker cleanup includes component-backed actor resolution and destroyed/zero-health checks, and was preserved rather than replaced with main's older cleanup body.

Independent static review found no new merge blocker. Imported minimap methods and the controller toggle match main after whitespace normalization. Existing upstream limitations remain: mouse handlers do not consult the interaction flag, drag uses screen-pixel deltas, and menu reset does not reset the widget's flag. These were not rewritten as part of integration.

## Actual verification

Evidence directory: `Saved/UIIntegration/20261113/`.

| Check | Baseline | Merged result |
|---|---|---|
| Editor Development | PASS | PASS, UHT/compile/link |
| Game Development | PASS | Pending independent package-branch build after integration |
| Native reflected types / Blueprint compilation | 41 / 39, PASS | 41 / 39, PASS; no asset saves |
| Navigation automation | 3 PASS, 1 FAIL | Same 3 PASS and same pre-existing IsolatedWorld obstruction assertion |
| Representative 3v3 PIE | Not rerun as baseline | Six ships, 3 Blue / 3 Red; 126 observation samples and physical travel of all five AI |
| Native controller interaction API | Absent before upstream feature | PASS: enter/exit mode with cursor state, 9× zoom, toggle/reset to 1× |
| Physical M key / wheel / drag injection | Not tested | Deferred at user's request while they used the Editor; source parity reviewed, API test passed |
| Gameplay damage after returning to normal Editor | Not changed in source | Six ships have `can_be_damaged=True`, each 100 HP in fresh PIE |

The isolated-world assertion and Cesium enum-metadata initialization diagnostic existed before merging; they are not new UI regressions. Their raw logs and both automation reports remain available. No assertion or test was weakened to obtain green output.

The long UI observation used a **transient nonlethal fixture** that disabled damage/combat in that PIE world. This caused the user's observation that shots did not reduce HP. The callback was unregistered, PIE ended, and Unreal restarted without the fixture. A separate short native API check changed no damage/combat settings and confirmed all six damage gates were enabled. This observation is not evidence of a natural battle result.

Initial observer attempts had Python harness errors (`get_url` getter export and `WidgetBlueprintLibrary` script-name alias); a simulation-only API attempt obtained a base controller. These harness attempts are preserved separately. The final observer and actual-PIE API receipts contain no harness errors and the API checks pass.

The earlier source-organization tool pins the original pre-feature function bodies and remains a historical refactor guard; intentionally adding main's UI behavior changes those bodies. It is not relabeled as passing this feature update. Architecture preservation here is established by the unchanged source path set, semantic diff/review, native registration, UHT and successful Editor compilation.

## Handoff to packaging

Commit this regular merge only after the above verification, then create `package` from that exact validated merge HEAD. Record the resulting hash in the packaging report. Runtime-data copying, path resolution, cooking rules and release changes belong exclusively on `package`. No remote push or merge into main is authorized.
