# UI/buoyancy release integration

Initial main: `9c74630c7a72716678aff3eecef0e4e0eeea8157`, verified by fetch on2026-11-06. Feature: `feature/integrate-ui-buoyancy`, created exactly at that origin/main in `E:/LeVietThanh1412-Product/VDT-Phase2-UI-Buoyancy`.

The original `VDT-Phase2-Project` worktree is clean on main. The detached `VDT-QuangNinh-Release` worktree and three existing stashes are preserved. No Unreal Editor was running at inspection; builds/tests use only the isolated project. The isolated project is open in VS Code. No merge/rebase was in progress. Git LFS3.6 is configured; `git lfs fsck --objects` passed. Some pre-existing Git blobs trigger non-pointer warnings during checkout, but the worktree starts clean and actual asset bytes match the baseline. No unrelated LFS migration is attempted.

Protected original map/external-actor/profile hashes were captured for34 assets. The main map and teammate UI are inherited from the current main, not selected blindly from a binary conflict. No assets were deleted or text-merged.

The GitHub connector returned404 for this repository. Its normal existing Git credential successfully fetched GitHub metadata: push permission exists, main reports unprotected, normal merge methods are allowed. No token is saved in project artifacts or logs. Required checks/reviews will be inspected on the final PR, and normal server-enforced merge is required.

## Verified feature commits

| Commit | Change and evidence |
|---|---|
| `bd4a2fd` | Reuse the teammate MATCH SETTINGS; manual seed validation, bounded setup failure and restart/input fixes. Actual Player+AI and Full AI startup passed before physics tuning. |
| `686049f` | Tune passive roll/pitch water resistance using three-hull/five-state physical measurements; remove only linked impact debug-sphere execution from two existing projectile Blueprints. |
| `564b0b9` | Cancel loading before result input and guard preparation after an authoritative outcome or lost actor. Early-result 35-second cleanup and missing-actor retry passed. |
| `6e04558` | Requested Small-only 2–3 degree response: roll/pitch rates 1.5/s and 0.8/s, synchronized with saved profile; idle/reverse/player/shoreline evidence. |
| `e6acd7c` | A moving-following exemption no longer defeats traffic priority behind a stopped same-heading AI. Physical queue reproduction, build, Blueprint compilation and four navigation automation tests passed. |
| `9772c55` | Update integrated-menu, capsize, dynamic-relocation, snapshot-write and serialized regression fixtures. Preserve failed diagnostics and capture gameplay evidence in Saved. |
| `9023ac2` | Save the single open-water waypoint correction after a cold reload and successful 1350.93-second, six-ship/four-arrival physical endurance run. |
| `7690037` | Reopen exhausted escort route retries after the existing cooldown; preserve combat/search ownership. Native build, 39 Blueprint requests, four navigation automation cases and a real exhausted-budget recovery fault passed. |
| `ce25120` | Stop validation safely if the observed PIE world is ended/replaced. Two isolated fixture-contract checks passed; a replacement user session is never closed by the observer. |

The verified waypoint correction is confined to one World Partition target actor package. A first 150 m candidate was insufficiently separated from an opposing endpoint. The final candidate is approximately 645 m from the original position and passed twelve-heading placement, connected-route checks, cold reload and fresh four-arrival endurance for all six ships. No ship start, other target, terrain, ocean or level package is changed. [Asset preservation evidence](ASSET_PRESERVATION.json) records all 34 protected original assets before integration.

## Release integration status

Implementation and extended saved-map testing remain on the feature branch. The final main-to-feature merge, PR and post-merge main verification will be appended only after they occur. No direct main development, reset, clean, force-push or branch deletion has been performed.

The final candidate passed the ten-case UI rerun and Small player controls. The next autonomous run was stopped/replaced from the Editor outside its fixture; its partial evidence is retained explicitly as interrupted. Editor availability is being coordinated before restarting that dependent run. No feature push, PR creation or main merge has occurred; main remains at the protected baseline `9c74630`. Small's saved 1.5/s roll and 0.8/s pitch rates and its measured 2–3 degree response are already committed on the feature worktree.
