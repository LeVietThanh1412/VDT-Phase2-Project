# UI/buoyancy release integration

Initial main: `9c74630c7a72716678aff3eecef0e4e0eeea8157`, verified by fetch on2026-11-06. Feature: `feature/integrate-ui-buoyancy`, created exactly at that origin/main in `E:/LeVietThanh1412-Product/VDT-Phase2-UI-Buoyancy`.

The original `VDT-Phase2-Project` worktree was initially clean on main. At the latest UI-repair inspection, it contains one user modification: `Content/Fonts/BlackOpsOne-Regular.uasset`. It is backed up in the original Saved/UserAssetBackup and preserved; SHA256 `d0e98d5ef17ca931dec15b04d0ba547446ebb25a00e5a8913ef0bf91de3f6764`. The detached `VDT-QuangNinh-Release` worktree and three existing stashes are preserved. No Unreal Editor was running at inspection; builds/tests use only the isolated project. The isolated project is open in VS Code. No merge/rebase was in progress. Git LFS3.6 is configured; `git lfs fsck --objects` passed. Some pre-existing Git blobs trigger non-pointer warnings during checkout, but the worktree starts clean and actual asset bytes match the baseline. No unrelated LFS migration is attempted.

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

2026-11-07: after implementation/report commit `b307f09`, fetched origin and ran `git merge origin/main` on the feature branch. Remote main remained `9c74630c7a72716678aff3eecef0e4e0eeea8157`; Git returned **Already up to date**, with no conflicts. No conflict-resolution commit is invented for this case. Post-integration verification and the final natural-result run precede feature push/PR/normal main merge.

The old MATCH SETUP screenshot was traced to the original project running an outdated DLL. Both current source trees contain the teammate MATCH SETTINGS. The old editor had no dirty packages or active PIE and was closed cleanly; only the feature editor was launched for the fresh uninterrupted battle. Rebuilding and verifying the original project after main integration is necessary to fix the actual user launch path.

Earlier natural runs are preserved separately as diagnostics or external interruption. Their results are not relabeled as successful. The fresh final candidate is running on DLL `bf8b893a3b5ccc88e02bc9744982d81b2f723ec12683e27115a1c38a228519b6`. No direct main development, reset, clean, force-push or branch deletion has been performed.

The fresh uninterrupted natural 3v3 passed at 802.999 simulation seconds: BLUE WINS, all six fired, three Red HEALTH_DEPLETED deaths, correct one-time native summary/cursor, and actual desktop mouse Return opened the same MATCH SETTINGS. Its source digest is `eb51849f8fb31cd8dfb44a457e1dd7248f26c681556533ef524dd46061783f03`. Post-main-to-feature native rebuild succeeded in 35.48 seconds. The rebuilt DLL digest is `df386d3acb19576261c761e2578c2234d60e884a1809613b2f8276ea2a2ef2e9`; production source/assets are unchanged from the natural test. All 39 Blueprint compile requests and four native navigation tests passed again before post-integration PIE.

Post-main-to-feature serialized PIE completed **PASSED**: ten menu cases, Small physical/player controls, all-six firing opportunities, and result/return/restart. [Post-integration verification](../Testing/POST_INTEGRATION_VERIFICATION.json) retains the rebuilt DLL/source digests and native test evidence.

## Normal PR merge and original-project rebuild

PR [#19](https://github.com/Mr-Gwenchana/VDT-Phase2-Project/pull/19), head `c46eef14b6ba36f90d93997245e0d50ac7fa3ade`, was merged through GitHub's ordinary merge endpoint to `9b915b0f1421627e033c3cb8defce13d1174a34a`. Before merging, main matched the fetched baseline; mergeability was clean, no requested reviewers or changes-requested review existed, and no hosted check runs/status contexts were configured. Main reported unprotected. No bypass, force push or direct main commit was used. Copilot's COMMENTED response said its review quota was exhausted; this is **not** a completed code review or an approval. Local tests are the stated evidence, not hosted CI.

The original `VDT-Phase2-Project` main worktree fast-forwarded normally. All34 protected map/actor/profile assets now match the verified feature, and the user's modified font retains its exact pre-update SHA. A native original-project build succeeded in38.87seconds, producing DLL `702e9465b2c4d8a03a001fca8d5618106d58ba636ff309fbc4af57d777bb3a03`. It has the same production source digest as the final natural-result candidate. All39 Blueprint requests and all four isolated navigation cases passed on that project.

Actual desktop observation confirmed the teammate MATCH SETTINGS with a single visible menu and all six default hulls Medium. Real mouse input changed the player to Small and clicked Start in manual mode: six ships/five AI controllers initialized without error and the loading overlay released. The local font renders correctly and was not replaced. [Restored original-project UI](images/merged-main-settings.png), [Small manual startup](images/merged-main-small-manual.png), and [normal merge proof](PR_19_MERGE_PROOF.json) preserve evidence.

## Final functional verdict

**MERGED_AND_VERIFIED** for the documented Editor demo scope. The original merged-main project passed authored Full AI, Medium and Small input/steering/aim/fire, all-six firing contact, and native summary/return/restart. No new critical native/Blueprint/Accessed None error was found. [Original-main verification](../Testing/MERGED_MAIN_VERIFICATION.json) records exact project path, DLL/source digests, protected asset matches and the preserved font hash.

The final follow-up PR contains only this verification record, compact JSON and screenshots; Source/Content/Config/Plugins remain identical to functional main `9b915b0`. No cooked build, multiplayer, arbitrary seed/placement/sea-state certification or hosted CI result is claimed. Raw failures and interrupted attempts remain in Saved. Three existing stashes and both independent worktrees remain preserved. UE is closed without saving automatic actor-construction dirtiness from Blueprint compilation; no authored placement or user font change is discarded.
