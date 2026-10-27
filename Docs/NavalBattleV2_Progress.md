# Naval Battle V2 — progress ledger

## Small milestone gate PASSED — 2026-10-27

SMALL_MILESTONE_QA.json hashes six passing actual PIE evidence files: settled
Small/Medium response, real P-menu Small Blueprint spawn, 16-projectile damage
matrix plus 65m sink/cleanup (94 checks), five physical AI with original
controllers on an old runtime-only certified patrol fixture (23 checks), and
underway player movement/reverse/firing/gun alignment (11 checks).

Final compact hull preserves original sockets1-5 scaled .425; socket5 local
[1942.25,-323,0]cm, no double scaling, rudder multiplier.5. Measured response:
Small acceleration51.21 vs44.03, peak28.76 vs25.73m/s; mean yaw5.255 vs3.467deg/s
(1.516x). Old rudder3.0 numbers below are superseded. All5AI traveled>900m.
Fixed first-frame visible gun initialization; final rerun max error<.001cm.
Retain the original failed pipeline JSON, snapshot failure and fixture failures.
A repeat mesh-build render-resource crash was fixed by preserving existing
geometry and updating sockets; no map/source model files were lost.

Small assets saved/reopened and menu spawn retested. GMBP preserves its original
controller/default pawn and selects BP_ShipSmall for a Small player. Base BP_Ship
now defaults to Medium so existing original hull instances retain their identity.
Raw Fab entitlement metadata, redundant thumbnails/FBX/PNG are local-only;
necessary UAssets are intended for this private repository. Small milestone
commit/push preparation is in progress. Main and the5pre-existing dirty actor
packages remain untouched. Full demo NOT READY: randomized island field,
>=5seeds, zero-stall diagnosis/fix, new full loop soak, mixed combat/turns,
performance and final reopen/push/PR remain required.

## Small milestone verification — 2026-10-27

Saved the repaired source atlas material, compact Small hull, visible gun parts,
Small/Medium profiles, dedicated BP_ShipSmall and BP_ShipSmallAI. GMBP inherits
NavalGameMode so selecting Small for the player actually spawns BP_ShipSmall;
original controller/default original pawn retained. Actual menu travel passed.
Source boat has no rig/sockets. Its built-in gun is aft and static; the separate
working gun is bow-mounted and follows the original yaw/pitch/muzzle pipeline.
No duplicate simulated body or actor-wide scale; Small HP38/base6; Medium/Large
100/base10. Original parent Blueprint force calls remain authoritative.

Physical comparison after water settling: Small 51.40 vs Medium 44.03 m/s²,
peak speed 28.76 vs25.73, mean absolute yaw5.084 vs3.458 deg/s (1.470x).
Maximum roll7.87/8.34deg. Small pontoon radius100cm, not the rejected400cm;
rudder3.0 and application-point scale.425. Source/profile build succeeded.
38s underway passed movement/reverse/firing/alignment; actual texture now visible.

The first matrix caught a real impact-time damage snapshot bug: a Small round
became10 when the attacker changed profile in flight. Fixed InitializeSource to
capture once, including after source destruction. New actual 16-shot matrix
PASSED Small/Small, Small/Medium, Medium/Small, Small/Large; bow/mid/stern,
friendly guards, once-only death,65m sink and cleanup. Retained the failure and
a separate fixture-target interception failure. Do not erase these records.

Small AI on both teams is being checked in a runtime-only old certified-layout
fixture. This is NOT the required new randomized-field soak. The five pre-existing
user-modified actor packages remain preserved and are not reset. Current saved
positions include blocked cells, so the final demo is not ready until the new
validated field, seed tests, stationary-AI diagnosis, full soak and reopen pass.
No Small-phase commit/push/PR has been made yet. Source art licence Fab Standard
is user-confirmed; private repository collaborator use only. Exclude entitlement
metadata, redundant FBX/thumbnail source files from the intended milestone commit.

## Small warship / randomized battlefield checkpoint — 2026-10-26

**Controlling request:** attachment `29d40d15-cfb1-4f2b-9e19-c58867938252/Pasted text.txt`,
plus the user's explicit requirement to inspect the Small ship skeleton itself.
This extends the previous goal. Do not reuse the old 402.63 s patrol as proof
for the future randomized island-rich battlefield.

- Git audit: user/another session committed final-QA preparation as `2e51f06`
  and merged PR #7 into main at `25baf4ca855fce6115a4a264f36d5c663996124d`.
  This agent did not merge it. PR #7 is closed/merged; the old remote integration
  branch is absent. Repository is authenticated as PRIVATE. Created local
  `backup/quangninh-before-small-20261026` at 25baf4c, then
  `feature/small-warship-seeded-battlefield`; no pushes/main changes in this phase.
- Recovery PASSED: `Saved/active_small_ship_recovery.txt` points to
  `SmallWarshipRecovery_20261026_202209`, 98 files / 170,772,980 bytes with
  verified SHA-256 copies, unstaged/staged patches and original status/HEAD.
  Includes source/config, original BPs/profiles, MainMap/ExternalActors and the
  new Small model. Preserve the five pre-existing modified ship actor packages.
- Editor was closed on arrival. Opened one unattended instance using the existing
  filesystem DDC fallback, audited outside PIE, then closed cleanly for C++.
  Dirty map/content lists were empty. No asset/level mutation in this new phase yet.
- Phase 1 asset/skeleton audit IN_PROGRESS: `SMALL_FBX_SKELETON_AUDIT.json`
  proves FBX 7400 contains one Mesh model/geometry, NO joint models, NO skin or
  cluster deformers, NO animation stacks/layers/curves. Actual source geometry:
  999,713 vertices and 5,999,970 polygon indices. The imported asset is StaticMesh,
  not SkeletalMesh; 1 LOD/section, Nanite already enabled. Bounds are 680.375 ×
  2435.821 × 800 cm (long axis Y), not a usable skeleton. Native static sockets
  remain to be checked because the Python property is protected.
- Source PNG is 4096×4096, not the listing's advertised 8K. Unreal's Blueprint
  size initially returned resident 32×32; do not confuse that with source size.
  Material `model` is opaque/default-lit with a material-function BaseColor input;
  no material repair has been justified yet. `SMALL_UNREAL_ASSET_AUDIT.json`
  records the actual objects, attachments and original ship rig.
- Existing DDG102 skeleton has 20 bones. Front yaw `front_gun_h_jnt` (parent
  cog_jnt), front pitch `front_gun_v_jnt` (child yaw), and muzzle socket exist.
  A native read-only inspector now checks real skin weights/sections, sockets,
  collision and texture source dimensions before choosing weapon reuse.
  New `ShipArchetypeEditor.cpp` / `InspectArchetypeSources` are audit preparation,
  not Small integration. No Small HP/damage/physics/gameplay changes yet.
- The baseline clean-source Unity build exposed duplicate anonymous IgnoreWater
  helpers in combat/navigation; renamed only the navigation helper/calls to
  IgnoreNavigationWater. First audit build also needed explicit pointer type for
  TObjectPtr socket iteration. Rebuild is in progress; do not claim compilation
  until its terminal result is checked.
- License: user explicitly confirmed **Fab Standard**. Source metadata identifies
  “Sweeden Military Boat”, YourAverageGamer, Fab listing b6d0889d-e855-4b3f-bf48-5ec7f9f4a4a7.
  Public listing fetch returned 403; exact licence is user-confirmed. Official
  https://www.fab.com/eula permits sharing with project collaborators via a
  private repository. Do not commit entitlement metadata or unnecessary thumbnails.
- Previous final QA is NOT fully passed: default five-Small-placeholder patrol
  completed 402.635 s / 15 arrivals / no nav errors, but strict summary 8/9
  (Friendly_AI_2 sampled water contact 98.72% <99%). Frame test 4/4, max loss
  0.3733 s, no floor hits. Mixed 3-Large/3-placeholder-Small real-time battle
  completed 300 s but FAILED: Enemy3 Large exhausted recovery at t120.33,
  Enemy2 Small at t218.99. Root cause not established; no fix applied. Preserve
  `FINAL_INTEGRATED_BATTLE.json`, `FINAL_BATTLE_QA.json`, screenshots and memory
  trace now committed in 2e51f06. Earlier process memory monitor was late/short;
  no memory-leak-free claim. New task phase 7 must diagnose these stalls.
- Original `promt.txt` / original “FINAL DEMO” task was not found in the bounded
  project/parent/attachment search; c73b619d attachment is a duplicate of the new
  Small task. Read existing progress/continuation; FINAL_DEMO_REPORT.md is absent.

**Next:** finish native source audit; inspect the actual model appearance and bow
axis. Build a coherent Small hull/weapon archetype on shared gameplay (one body,
no oversized hidden hull, visible muzzle/aiming). Preserve original hull as Medium
and Large profile; Small must become 38 HP / attacker damage 6 (5/6/5 rounded),
faster/more agile. Verify independently, commit/push verified milestone. Then
map-derived island-rich square, opposite team halves, one seeded spawn authority,
class-safe routes, at least five real seeds and a NEW saved-layout soak; finish
mixed-fleet stalls, all regressions, restart persistence, new PR. Do not stop at audit.

Everything below this section is historical and does not override the new task.

## Active checkpoint — 2026-10-26

Branch `feature/quangninh-battle-integration`, HEAD `faa966c` (pushed). Earlier
verified commits `d4a8b38` / `84a6210` / `aefaea6` are in open, unmerged PR #7.
Main and Thanh were not changed by this agent. The dated records below are
historical; use this active checkpoint for the next action.

- Phase 6 destruction: PASSED. Standalone native billboard fallback built
  successfully (21.69 s / 5 actions). Two materials and five hidden shared
  BP_Ship components saved with recovery copies and exact original force-graph
  validation. First low-emission visual attempt failed despite activation flags;
  daylight emissive correction shows actual fire on a rolled wreck. Warm tint
  and radial smoke opacity are saved; final screenshot A00002 visibly shows
  orange flames and dark smoke. Existing Niagara and staged sinking are preserved.
  Final five-wreck health/capsize/rebase/sinking/cleanup regression PASSED 66/66;
  short-roll fixture missed a narrow reset window once, failure preserved and
  deterministic one-time reset fixed; gameplay capsize unchanged. Saved
  runtime health/capsize spectator tests also pass effect activation, hiding
  at Submerged and actor cleanup. See WRECK_BILLBOARD_REPORT.md and JSON files.
- Phase 7 spectator: PASSED focused functional gate, source compiled (35.70 s / 7 actions), editor
  restarted, seven BPs compiled, original force graph validated. Current-map
  health, original-projectile and confirmed-player-capsize cases PASSED. Each
  checks automatic Blue follow, Tab/M handlers, preserved five AI controllers,
  follow-target death, no-teammate overview, explicit geographic rebase and
  cleanup. Death while F free camera PASSED too. Results: 22/24/22/23 checks.
  Physical keyboard
  routing and final visual integration are still pending. Initial projectile
  fixture checked before PlayerTick; failed raw result preserved, bounded
  0.2 s next-tick observation added, actual projectile repeat passed.
- Phase 8 turns: PASSED focused gate. One timer in the existing battle subsystem;
  10-second alternating living-ship windows, action gates in the original force
  filters, shared weapon and aim selector, inactive AI decisions/nav held while
  physics/projectiles/damage/sinking continue. Match settings backend retains
  direct PIE defaults and will reuse the six existing ships for selected counts.
  Normal native build PASSED (13 actions / 33.07 s). Three full six-ship
  rotations, 18 original projectiles crossing turn boundaries, inactive gates,
  continued physics, active-death skip, victory and sinking/cleanup PASSED.
  All 2932 assertions passed (includes repeated per-frame/ship invariants).
  Window durations 10.001-10.190 s. Separate 90 s real-time integration PASSED
  20/20. Evidence TURN_MODE_REPORT.md and TURN_* JSON/gzip. Fresh PIE initially entered
  built-in spectator mode: standard PlayerController + SpectatorPawn, five AI,
  no player ship. Fixture correctly failed the six-ship assertion. Engine
  PlayLevel.cpp:2867 also starts spectators when Ctrl is held; do not patch
  gameplay to hide an editor-start condition. Fresh normal play configured six
  ships. Two other fixture failures (player AI-only probe and unbounded upward
  test rounds) are preserved; the final bounded fixture passed without native
  physics/capsize changes. Zen DDC startup crashed once before gameplay; current
  editor uses built-in -DDC=InstalledNoZenLocalFallback after cold shaders.
- Phase 9 prematch: PASSED functional/visual gates. ShipSetupWidget.h/.cpp and PC P/menu integration
  compiled after two C++ API fixes (explicit graph-node pointer type; renamed
  locals that hid UWidget::Slot). Build PASSED 5 actions / 4.32 s. Actual menu
  pause/cancel/level-travel matches 1v1, 2v2, 3v3, 2v3, 3v1 PASSED **127/127**:
  per-slot classes, skill, selected mode, exact pawn/controller counts, one Blue
  player, original patrol references and fresh-PIE defaults. Screenshot inspection
  caught clipped bottom buttons; height 490→560 and shorter turn label built
  successfully (4 actions / 4.18 s), repeated 127/127 and corrected screenshot
  inspected: both buttons fully visible. Physical key/mouse injection unavailable
  because hidden editor window is not targetable by computer-use; native
  bindings/delegates source-inspected, their handlers live-tested. Standalone
  auto-open branch source-inspected only; PIE menu/travel behavior executed.
  New ShipDebugEditor.cpp helper compiled too, but has NOT been invoked or
  verified; excludes it and its header declaration from the eventual menu commit.
- Phase 10 final QA: NOT_STARTED.
- Modified source: ShipWreckBillboardComponent.h/.cpp, CustomPlayerController.h/.cpp,
  Actor/Pawn/FreeCameraPawn.h/.cpp, NavalBattleUI.cpp (preserves pre-existing
  formatting). Assets: BP_Ship and two M_Wreck*Billboard materials. Tools:
  setup_wreck_billboards.py, probe_wreck_billboards.py, test_spectator.py.
- Reproduction: execute each file through Tools/SeaNavigation/mcp_client.py using
  file-based exec; never start an editor-only operation while PIE is active.
- VFX and spectator delivered: `9b0a350` and `49d874d`, normal push succeeded
  (18 LFS objects / 16 MB). Main unchanged; PR #7 remains open/unmerged.
- Saving inherited wreck defaults also resaved the five existing external ship
  actor packages. Saved-state audit PASSED 7/7, no actor/target/controller/patrol
  setting differences. Persist these reviewed packages in a focused follow-up.
- Turns delivered: `faa966c`, normally pushed; PR #7 body includes its evidence.
- Next: save/commit/push verified menu, apply and
  verify the narrow sensor-debug default cleanup, then final
  end-to-end/input/debug/performance/soak QA. Do not stop at this checkpoint.

Current checkpoint (2026-10-24): integration branch and origin/main both contain `920936b`, merged PR #6 (terrain implementation `f16b5bb`). This merge was already present when the agent resumed; no merge was performed in this session. Clean starting tree, one worktree, three existing feature/map stashes preserved, Thanh retained. Current task explicitly authorizes incremental feature-branch commits, ordinary pushes and PR creation; no automatic main merge.

| Master phase | Status | Current evidence / next action |
|---|---|---|
| 0 Recovery | PASSED | Source/assets/evidence already preserved in f16b5bb and PR #6; fetched refs and audited clean tree |
| 1 Final narrowed collision | PASSED | Rebuilt cache fix: 415.22 s saved-level soak, all five B/A/B, 15 arrivals, independent 9/9 assertions passed including strict contact metric. Forced eviction 10/10, source records/proxies stable at 221, 12 supported probes. Shore obstruction 4/4, frame flotation 4/4, cleanup passed; 2031 external source hashes unchanged |
| 2 Core Git delivery | PASSED | Already delivered by f16b5bb / PR #6 / 920936b. Follow-up validation evidence will be committed and pushed before new gameplay work |
| 3 Current-map 3v3 | PASSED | Final native cache/filter saved-level 90 s integration 20/20: input, actual projectile, GunCamera, Q/F, five moving AI, exact endpoints, minimap counts/colors/death removal |
| 4 Combat/tactics | PASSED | Focused escape commitment fix: 7/7 critical escape, 12/12 reverse/fire, 20/20 roles, 3/3 physical island occlusion, 20/20 battle and 144 real accuracy shots. Fresh 402.77 s patrol completes all loops/15 arrivals; strict contact statistic fails narrowly (98.97%), retained; frame flotation 4/4 |
| 5 Profiles | PASSED | Large-only vertical damping 1.2→1.8, native fallback matched; mixed frame flotation 4/4, mixed AI navigation 7/7, Small/Large input and 33/33 damage each passed |
| 6 Destruction | IN_PROGRESS | Existing staged sinking/capsize preserved; current-map lifecycle and visible VFX pending |
| 7 Spectator | NOT_STARTED | Existing Q/F only; automatic teammate follow required |
| 8 Optional turns | NOT_STARTED | No turn scheduler yet |
| 9 Prematch | NOT_STARTED | No configuration UI yet |
| 10 Final QA | NOT_STARTED | Depends on remaining features |
| 11 Delivery | IN_PROGRESS | Existing integration branch retained for focused follow-up commits/PR |
| 12 Thanh cleanup | SKIPPED_WITH_REASON | Optional; retained safely while active development continues |

Current files: `SeaTerrainCollisionComponent.cpp`, `PATROL_SUMMARY.json`, regression/flotation/shore fixtures and final-filter evidence. Commands: `python Tools/NavalCombat/summarize_quangninh_patrol.py`; run test files through `Tools/SeaNavigation/mcp_client.py` using file-based `exec(compile(...))` (the installed bridge rewrites literal multiline strings). Blueprint compile: seven assets explicitly OK. The original strict 99% summary assertion remains false; it has not been silently relaxed. Frame-resolution flotation passed 4/4, maximum contact loss 0.404 s. Extended seaward shore approach passed 4/4 at 320 s, after shorter fixtures failed due to preserved shallow collision/insufficient approach time. These raw failures are preserved with compressed telemetry.

**Immediate next step (2026-10-24):** deliberate runtime `MaximumCachedBytes=1` eviction test confirmed stale-source metadata growth: 669 records versus 230 reused proxies after 125 s; physical island checks remained passed (9/10 overall). Evidence `LAND_COLLISION_EVICTION_BEFORE_FIX.json[.gz]`. Implemented a small expired-source-slot rebind in SeaTerrainCollisionComponent.cpp; geometry/filter/forces unchanged. Normal native build PASSED (4 actions / 45.67 s), editor fully restarted, seven Blueprint compiles PASSED. Saved-state comparison PASSED 7/7 (numeric tolerance 1e-6; world transforms exact, sub-nanometre EN conversion rounding only). Repeated forced eviction PASSED 10/10: both metadata and geometry settled at 221, all 12 island probes supported. `LAND_COLLISION_FORCED_EVICTION.json[.gz]`, `COLLISION_CACHE_BUILD.json`, `SAVED_STATE_VERIFICATION.json`.

The rebuilt cache fix's full saved-level patrol completed naturally at **415.21699 s**, 411 samples, **15 arrivals**, **all five three legs**, **0 errors**, **9/9 independent assertions PASSED** (`PATROL_CACHE_SUMMARY.json`). Each travelled 2971–3677 m; cruise medians 18.34–18.91 m/s; no chart-land/Failed samples; exact-target errors below 4e-9 m. All five physics/controllers and six-pawn/one-player counts persisted. Post-match GC left only eight class-default procedural component templates, no PIE proxies (`COLLISION_CACHE_CLEANUP.json`). Existing `PATROL_SUMMARY.json` remains the earlier strict 99% failure and is preserved. Reproduction: `summarize_quangninh_patrol.py --input Docs/NavalCombat/QUANGNINH_CACHE_PATROL_SOAK.json --output Docs/QuangNinhIntegration/PATROL_CACHE_SUMMARY.json`; the reader transparently opens the adjacent lossless raw gzip when input is a compact summary.

Core follow-up delivery is complete: **`d4a8b38`** committed and normally pushed to `origin/feature/quangninh-battle-integration`; **[PR #7](https://github.com/Mr-Gwenchana/VDT-Phase2-Project/pull/7)** targets main and remains open/unmerged. Main was not changed by this agent. The connected GitHub app could not access the repo (404); delivery used existing local Git credentials without exposing them. PR creation initially rejected a Windows-encoded JSON body (400); rewritten ASCII/UTF-8 JSON succeeded. No duplicate PR was created.

Final-filter full saved 3v3 integration PASSED 20/20 (`QUANGNINH_FINAL_INTEGRATION_PIE.json[.gz]`, 90 s). Current-map roles PASSED 19/19 (`QUANGNINH_ROLES_PIE.json[.gz]`): physical Passive/Hunter movement, friendly/hostile activation guards, Fighter acquisition, threat/command promotion, Hunter target-loss/timeout/range/health exits, and original patrol restoration. The deliberate turn interruption occurred while this fixture ran; it completed naturally and was inspected before starting another PIE.

Backward-retreat regression PASSED 12/12 (`QUANGNINH_BACKWARD_RETREAT_PIE.json[.gz]`): real backward movement and projectile fire at 30 HP, bounded reverse speed, patrol restoration, rear-hull rejection and forward-escape availability. The new critical island fixture correctly blocks reversing and plans a safe escape, but FAILED physical departure in both 25 s and 90 s windows. Preserved `ISLAND_ESCAPE_SHORT_WINDOW.json[.gz]` and `ISLAND_ESCAPE_BEFORE_COMMITMENT_FIX.json[.gz]`. At 90 s maximum displacement was 24.85 m, hull remained afloat at ~3–5 m with physics on. Steering reversed each time temporary gun-visibility loss caused Escape → ReturnToPatrol → Patrol → Escape, preventing departure. This is a tactical commitment bug, not terrain grounding.

Implemented a focused `ShipCombatComponent.h/.cpp` fix: keep a weak navigation threat independently of the gun target; occluded gun targets still clear immediately. Forward escape requires safe additional separation (100 m, matching the existing escape planner's criterion), while preserving minimum/maximum duration, dead/team guards and backward-retreat behavior. Normal native build PASSED (12 actions / 20.86 s), editor fully restarted (PID 28604), seven affected Blueprints explicitly compiled. The 90 s critical-island fixture PASSED **7/7**: unsafe reverse rejected, safe forward escape, **190.67 m actual displacement during Escape**, continued commitment before departure, cleared occluded gun target, no blocked-cell entries. Before fix: 24.85 m, repeated premature patrol returns. `QUANGNINH_ISLAND_REVERSE_PIE.json[.gz]`, `RETREAT_COMMITMENT_BUILD.json`.

Post-fix regressions completed: reverse/fire **12/12** (three original shots at -4.24 to -4.29 m/s, minimum -4.327 m/s), roles **20/20**, physical island occlusion **3/3**, normal 3v3 **20/20**. Accuracy **144 actual shots / six checks passed**: stationary Easy/Normal/Hard hits 2/12/24 of 24 each; moving 8/11/18 of 24 each; all launches track the animated muzzle. A role fixture failed once because it assumed target visibility at the range-bound stage; it now asserts the visible hostile beyond 500 m before testing the bound. Failed fixture preserved; no gameplay LOS constraint weakened.

Fresh patrol after tactical change completed **402.7678 s / 401 samples / 15 arrivals / all five B/A/B / 0 errors, land entries or Failed states**. The strict summary is **8/9, overall passed=false**: Enemy2's one-second contact ratio 98.9717% misses the unchanged 99% threshold by 0.0283 percentage points, four isolated samples. A separate current-build frame flotation test PASSED **4/4**, maximum continuous contact loss 0.384 s and root heights 1.72–5.36 m. No repeated soak was used to hide that statistical failure. See `QUANGNINH_COMBAT_REGRESSION.md`, `PATROL_TACTICAL_SUMMARY.json`, `FLOTATION_TACTICAL_FIX.json[.gz]`.

Tactical fix delivery complete: **84a6210**, normally pushed to the existing integration branch and included in open/unmerged PR #7; its title/body now cover both verified fixes and the failed strict contact statistic. Main remains unchanged by this agent.

Phase 5 current results: Small/Large input physics PASSED (initial acceleration 41.99 / 29.90 m/s²; turn peaks 2.466 / 0.287 deg/s); actual original-projectile damage PASSED **33/33 each**, applying the class to both shooter and victim. Mixed two-Large/three-Small AI run completed 60.44 s: all five moved 909–1039 m, two Large 920/980 m, intact physics/controllers, valid water tracks, no Failed states or persistent errors. One first-frame Blueprint sensor warmup is preserved separately. The initial blanket-error summary was corrected to distinguish that startup notice from a Failed navigation state; raw data is unchanged.

Mixed-class frame flotation exposed extra heave on Large Enemy2: 0.656 s contact loss, world root peak 7.079 m, **zero terrain-floor hits during losses**. This is buoyancy damping rather than grounding. Large mass increases 1.6x but damping only increased 1.2x, reducing damping per kg. Changed **only Large VerticalDampingMultiplier 1.2→1.8**, retaining all other profile fields, and matched the native fallback default. Original data asset backed up at `Saved/QuangNinhProfileDampingRecovery/DA_LargeShip_before.uasset`. Normal build PASSED (4 actions / 10.16 s); restarting editor. REQUIRED next: compile BPs, repeat mixed frame flotation, Large input and mixed navigation, then complete destruction. Preserve `FLOTATION_MIXED_BEFORE_DAMPING.json[.gz]`; do not claim the new damping passed until tested.

Destruction's first current-map run reached native cleanup then failed in the Python fixture (29 checks passed): UE cannot marshal a stale Pawn wrapper into SystemLibrary.IsValid. Added a narrow TypeError guard that treats already-destroyed wrappers as invalid; no native lifecycle change. Failure preserved in `DESTRUCTION_STALE_WRAPPER_FAILURE.json[.gz]`. Rerun the full lifecycle/georeference/capsize fixture after profile validation.

Large damping retest PASSED **4/4**, maximum contact loss **0.467 s**, peak root height **6.458 m**, zero terrain floor hits. Large input repeat passed (27.49 m/s² initial acceleration / 0.247 deg/s yaw peak). Repeated 60.50 s mixed navigation PASSED **7/7**: all five travelled 895–1040 m, Large 920/980 m, no land entries, Failed states or persistent errors. Destruction fixture with safe stale-wrapper handling PASSED **51/51**, including actual health/capsize deaths, preserved roll/geography, explicit georeference change, 65 m progressive sinking and cleanup. Report: `QUANGNINH_PROFILE_DESTRUCTION_REPORT.md`.

Next: commit/push the verified Large-only damping and regression evidence. `ShipWreckBillboardComponent.h/.cpp`, `setup_wreck_billboards.py` and `probe_wreck_billboards.py` are **preparation only, uncompiled/unexecuted**, excluded from the profile commit. No VFX assets changed. Hull scale is 1 and bounds include the ~43 m mast; fallback uses a configured ~5 m deck height and geographic up. After profile publication: build the standalone VFX component, create the two materials outside PIE, run bounded health/capsize prototypes and inspect actual screenshots before applying Blueprint components. Preserve original Niagara/sinking. Spectator, optional turn scheduler and prematch UI remain NOT_STARTED. Final integrated QA remains required after those features.

Then complete current-map profiles/damage and destruction regressions, destruction visual work, spectator, turns and setup in priority order. Prepared fixture changes remain uncommitted until tested. A concurrent external save reformatted `NavalBattleUI.cpp`; lexical tokens were compared with HEAD and match exactly. Preserve that unrelated formatting and incorporate it when editing UI later.

The sections below are historical phase records; their passes do not replace final QuangNinh acceptance.

## Phase 1 — PASSED (audit/baseline)

- Existing shared BP_Ship/BP_ShipEnemy physics, buoyancy, ABP gun animation, original projectile Blueprints, geographic A*, ShipCombatComponent and event-driven UMG health/counters are present.
- Older real PIE evidence is under Docs/NavalCombat. Initialization previously showed five AIController pawns and one PlayerController pawn. Health/team guards, controls and bounded gun checks were exercised. Older patrol runs did **not** prove two completed legs for all five ships.
- Known causes found: waypoint skipping ignored temporary ship exclusions; separation steering counteracted A* steering beside a stationary ship; blocked visibility retained an old target. Fixes compiled, but the latest full patrol verification was interrupted.
- Last saved balance from this agent: 100 HP, 10 projectile damage, AI cooldown 8 seconds. This is not yet the V2 positional 8/10/8 damage system.
- User changes occurred after the previous session: BP_Ship and external actors changed; two old red actor packages are deleted and new packages exist. Preserve these edits and inspect the current editor scene before configuring anything. Do not rerun the old battlefield placement script blindly.
- No Unreal Editor or Python writer was running at the start of this resumed audit. Started one editor for MainMapLevel; waiting for live connection before baseline sampling.
- Recovery snapshot: Saved/NavalBattleV2Recovery_20261019 (current source, ship Blueprints, level actor packages and binary git diff). No source-control reset/revert performed.
- Source inspection shows per-instance PatrolActorA/B, independent route/index, automatic A/B switching and current-transform sampling. Current formation offsets can intentionally place the effective goal away from the named Target Point; this must be made explicit or replaced with actual per-ship endpoints for location verification.
- Fresh audit: CURRENT_PATROL_AUDIT.json records five BP_ShipEnemy instances (including the user's BP_ShipEnemy2/3), all AI enabled, AutoPossessAI PlacedInWorldOrSpawned, AIController, physics active and mass 150 kg. Red cruise is 20 m/s; Friendly cruise still 8 m/s. Existing force graph gate validates successfully.
- **Confirmed root causes:** both Friendly spawn cells are blocked (coast distances 130.2 m and 81.6 m). The moved red B anchor at world (29050.006, 312849.984, -9.221) is also in a blocked navigation cell. A 35-second combat-disabled PIE baseline reproduces `Ship start is blocked or outside navigation coverage` on both Friendly ships. Controller settings are not the failure.
- Baseline evidence: Docs/NavalCombat/V2_BASELINE_PATROL.json. Red ships obtain routes and physical velocity; both Friendly remain in Failed with zero throttle. New actor placements, not missing possession, explain the reported stationary behavior.
- Phase 1 movement baseline is documented; proceed with Phase 2. Inspect current force graphs, validate revised positions against chart/land, use explicit independent Target Points with zero hidden offsets, then measure every A/B arrival.

## Phase status

| Phase | Status | Evidence / next action |
|---|---|---|
| 1 Audit/baseline | PASSED | Actual source, scene, controllers, physics and failure causes documented |
| 2 Physical movement and independent A/B loops | PASSED | All five B/A/B loops; saved startup and combat-on movement regression passed. Shared player speed cap/reverse-force tuning will accompany phase 4 |
| 3 Unified roles/transitions | PASSED | ROLES_PIE.json: 19 runtime checks, physical Passive/Hunter movement, all activation/exit conditions and original endpoint preservation |
| 4 Low-health backward retreat | PASSED | Actual reverse fire at 30 HP, patrol resumption, rear-ship block, island/15 HP escape, measured W/S acceleration/speed |
| 5 Imperfect aiming / presets | PASSED | 144 original-projectile shots, real stationary/moving hits/misses, animated muzzle alignment and island occlusion; ACCURACY_REPORT.md |
| 6 Positional 8/10/8 damage | PASSED | HULL_DAMAGE_PIE.json: 31 checks; actual 8/10/8 collisions, rotated hull, team/self/duplicate guards and lethal collision |
| 7 Large/small profiles | PASSED | Real W/S/turn measurements, stable mixed-class A* movement, Large physical projectile/damage/death compatibility; PROFILE_SUMMARY.json |
| 8 Capsize, fire, gradual sinking | IN PROGRESS | Replace old short wreck cleanup with unified capsize/health death and visible geographic sinking |
| 9 Automatic spectator | NOT STARTED | Existing player-wreck camera/PLAYER DESTROYED overlay, no teammate follow |
| 10 Turn-based mode | NOT STARTED | Real-time only |
| 11 Prematch menu | NOT STARTED | Saved direct PIE configuration only |
| 12 Performance/debug polish | NOT STARTED | Existing debug flags and scan interval can be reused |
| 13 Integrated verification | NOT STARTED | Older tests are baseline evidence, not V2 acceptance |
| 14 Save/restart regression | NOT STARTED | Must repeat on final saved configuration |
| 15 Handoff | IN PROGRESS | This ledger records actual progress and outstanding work |

Compile success is not gameplay success. Never present temporary fixture movement or earlier actor layouts as proof for the current saved level.

## Phase 2 implementation and current verification

- Native movement graph audit: Docs/NavalCombat/MOVEMENT_GRAPH_AUDIT.txt. Player and AI forward force are 700000, with AddForce `bAccelChange=false`; all current hull masses are 150 kg. AI rudder remains the original AddForceAtLocationLocal at socket 5, force multiplier 12000. No continuous velocity override was introduced.
- A water-valid compact layout was selected from the supplied quadrilateral and real coastline chart. Saved original five ship instances, retained valid Enemy A and Friendly A/B actors, moved invalid Enemy B to accepted water, and created only six missing per-ship endpoint actors. Blue starts on the bay side and Red on the opposite side. Minimum initial ship separation: 400 m.
- All five now use cruise 20 m/s, arrival radius 80 m, wait 3 s, zero formation offsets, unique explicit A/B assignments, and staggered startup. Player remains manually possessed. Plan: Docs/NavalCombat/PATROL_V2_PLAN.json; applied with Tools/NavalCombat/configure_patrol_v2.py.
- Preserved actual actor ellipsoid height when forming the patrol goal. Runtime A* endpoint and displayed world goal now match the real Target Point instead of a height-losing ENU projection.
- First revised PIE: all five physically moved; peak speeds 18.6–20.0 m/s, no blocked grid positions or land crossings in sampled tracks. Friendly_AI_1 completed B then A; Friendly_AI_2 and Red3 reached B. This run was stopped before full acceptance because a stale temporary traffic exclusion delayed a later detour around the stationary player. Diagnostic: PATROL_V2_TRAFFIC_COMMITMENT_DIAGNOSTIC.json.
- Fixed exclusion lifetime: release its commitment after physically passing the old obstacle, allowing the next obstacle to be planned around early. Normal editor build succeeded and editor was fully restarted.
- Reloaded saved scene audit: Docs/NavalCombat/V2_SAVED_PATROL_AUDIT.json confirms all five 20 m/s settings, actual references and zero offsets persist. Fresh, unmodified-physics PIE loop verification is RUNNING. Only combat/damage is disabled in this disposable patrol fixture; starts, speed settings, physics and navigation remain the saved configuration.
- Acceptance fixture: Tools/NavalCombat/verify_patrol_loops.py records actual controller, hull position/speed, route endpoint errors and every arrival. It requires three alternating arrivals on each of five AI (B → A → B), within the actual 80 m radius, and independently audits geography with summarize_patrol_loops.py. Do not mark passed before its result.
- User-facing settings reference: Docs/NavalCombat/PATROL_SETTINGS.vi.md. Visual track map: PATROL_V2_MAP.png (regenerate after final run).

Next: finish all five loops, investigate any remaining stop/failure, rerun fresh PIE and combat-on movement regression, update this ledger and report. V2 phases 3–14 remain outstanding; do not claim the expanded master task complete.

### Full-run result and next corrective iteration

- The next real PIE ran 1501 seconds. Four ships proved repeated exact-actor loops: Friendly_AI_1 9 legs, Friendly_AI_2 4, BP_ShipEnemy2 5, BP_ShipEnemy3 7. Every recorded arrival was within 80 m, endpoint/world-goal errors were below 1 mm, no blocked-water samples or land-crossing segments occurred, and peak physical speeds were 19.4–20.0 m/s.
- Original BP_ShipEnemy completed zero legs: it stopped near the stationary player. Overall navigation acceptance therefore FAILED despite the four passing ships. Evidence preserved as PATROL_V2_PLAYER_PRIORITY_DIAGNOSTIC.json.
- Further cause: a moving-AI traffic commitment could prevent planning around the player until already inside the soft braking circle. New fix lets a newly encountered player obstruction replace that commitment. Safe tangential passage can request up to 3 m/s after existing chart and hull-width sweeps pass, while retaining global rudder authority.
- Added native A* safety tests for 80, 262.5 and 350 m exclusion radii. C++ build passed after the fix. Fresh editor / planner tests / full-scene patrol retest are next. No passing claim yet for original BP_ShipEnemy.

### Traffic starvation iteration

- User requested English responses. Continue in English.
- The next full run proved original Enemy reaches B (800.07 s, distance 79.78 m, speed 0.016 m/s), then physically returns north toward A. Other AI continued repeated loops. No endpoint assertion or blocked-water/land-crossing error appeared.
- However, return congestion delayed original Enemy excessively while higher-priority ships repeatedly reused the narrow channel. Stopped the run for correction; evidence: PATROL_V2_STATIC_PRIORITY_DIAGNOSTIC.json. Do NOT claim its return to A completed.
- New code adds local waiting-age right of way: after 15 s yielding to AI, the oldest eligible nearby waiter can receive 40 s temporary priority, with cooldown. Player priority, hull sweeps, island checks and emergency brakes always remain active. Dead ships/player-yielding waiters cannot monopolize admission. No central fleet framework added.
- Moving AI are no longer proactively treated as long-lived circular islands. Stationary AI detours remain, respecting current right of way; player obstacles can replace a previous AI commitment. Detour release now checks fixed route progress direction, rather than a bow rotation incorrectly implying the obstacle has been passed.
- Added editable TrafficPriorityWaitSeconds / TrafficPriorityHoldSeconds and read-only TrafficWaitSeconds / TemporaryTrafficPriority for diagnostics. verify_patrol_loops.py records these fields and now stores its configurable time limit in probe state.
- Build pending final small eligibility correction. After success, restart editor (native static tracking struct changed), run planner tests, then fresh full-scene 3-arrival-per-AI patrol test. No gameplay success claim yet for this iteration.
- Existing damaged-ship Niagara asset found: /Game/Ship/Assets/VigilanteContent/Vehicles/West_Ship_DDG102/FX/NS_West_Ship_DDG102_Damaged. V2_AVAILABLE_VFX.json lists 14 available effects for later phase 8 inspection.
- Exact full requested specification preserved in Docs/NavalBattleV2_Specification.md. V2_RUNTIME_ARCHITECTURE.json confirms GMBP GameMode, native Engine PlayerController, one BP_Ship player, two Blue AI and three Red AI.
- Legacy configure_battlefield.py now refuses a V2 scene, preventing accidental recreation of deleted legacy Enemy_AI_2/3 alongside the user's BP_ShipEnemy2/3.

### Current observation checkpoint

- Waiting-priority build and both native planner tests passed. Fresh full-scene PIE is running, normal physics/time, with five distinct possessed AIController instances independently confirmed in V2_CONTROLLER_POSSESSION.json.
- Original BP_ShipEnemy reached B at 459.616 s (79.762 m, 0.049 m/s), returned to A at 1000.365 s (79.606 m, 0.163 m/s), and automatically selected B again. Four other ships already have at least three alternating arrivals. At 1191 s original Enemy was physically moving on its next B leg. Three-arrival acceptance for original Enemy remains pending; do not claim the final loop certificate passed yet.
- No endpoint assertions, blocked-water samples, land-crossing segments or navigation failures have appeared. Peak AI speeds approximately 18.8–20.1 m/s. The running fixture limit was extended to 1800 s via builtins.patrol_loop_probe['limit_seconds']; this changes observation duration only.
- IMPORTANT PENDING SOURCE BUILD: added editor-only CompileBattleBlueprints() to the navigation header/editor bridge AFTER starting this runtime test. It compiles seven affected assets and explicitly checks native Blueprint status, since Python does not expose that status. Do not compile/restart until the current loop test completes. Runtime navigation code is unchanged by this editor-only addition.
- After loop completion: summarize/render/write_patrol_report.py; stop PIE; save; close editor; normal C++ build for editor bridge; restart; call CompileBattleBlueprints(), assert OK and save; then fresh initialization and combat-on navigation regression. No need to repeat the entire long loop solely for this editor-only helper if runtime navigation remains unchanged.
- Report generator writes factual English PATROL_LOOP_REPORT.md; it must be regenerated from final summary. Track map must also be regenerated.

### Navigation loop acceptance PASSED

- The waiting-priority run ended naturally at **1595.84 seconds** with `ALL_FIVE_LOOPS_VERIFIED`; independent summary `passed=true`.
- Actual completed arrivals: BP_ShipEnemy 3, BP_ShipEnemy2 5, BP_ShipEnemy3 9, Friendly_AI_1 10, Friendly_AI_2 6. Every ship demonstrated B → A → B with independent route state and possessed AIController.
- Original Enemy arrivals: B at 459.616 s / 79.762 m; A at 1000.365 s / 79.606 m; B at 1595.836 s / 79.685 m. Speeds at arrival <0.5 m/s; normal configured wait 3 seconds.
- Peak physical speeds 19.5–20.1 m/s. No blocked-water samples, land-crossing segments, navigation failures, endpoint assertions or probe exceptions. All sampled hulls simulated physics and remained AI controlled. Endpoint error below 1 mm (actual maximum ~2.6e-9 m).
- Longest traffic waits include player-yield periods (Friendly_AI_2 109.3 s, original Enemy 78.7 s). Priority eligibility is not a guarantee of passage in 15 seconds; physical obstructions/player right of way still apply.
- Final map visually inspected. Artifacts regenerated: PATROL_LOOP_SUMMARY.json, PATROL_V2_MAP.png, PATROL_LOOP_REPORT.md. Report clearly states the 80 m physical arrival tolerance and separates it from exact planned endpoint matching.
- PIE ended, modified packages saved, editor closed. Building the editor-only compile-status bridge now. Next: reopen, explicitly compile/check seven battle Blueprints, save, run fresh initialization and ~25 s combat-enabled movement regression, and record results. Runtime navigation body did not change after this successful run.

### Saved startup and combat regression PASSED

- Editor-only bridge build succeeded. Seven Blueprints explicitly compiled with native status checks; all `compiled` without warnings. BLUEPRINT_COMPILE_RESULTS.txt records each asset. Saved dirty packages successfully, validated original exclusive forward/rudder branch, and verified both original projectile damage templates remain 10.
- V2_FINAL_SAVED_AUDIT.json confirms five existing AI instances, correct individual target references, zero offsets, cruise 20 and combat enabled after compilation.
- Fresh initialization: exactly six pawns, five AIController and one PlayerController, Blue 3 / Red 3, all 100 HP, physics/buoyancy active, HUD in viewport.
- Fresh combat-enabled run: 25.096 seconds, all five AI physically moved 126.9–386.9 m; three Red ships fired 1–3 shots while moving. No navigation failures; one first-frame sensor warmup on Friendly1 is normal. COMBAT_NAVIGATION_REGRESSION_SUMMARY.json passed.
- Navigation request is now supported by real saved-state evidence. Continue broader authorized master task sequentially; phase 3 begins next. Player maximum-speed cap / weaker reverse propulsion remain to be implemented with phase 4's shared propulsion filter, not claimed complete from the AI cruise measurements.

### Roles and backward retreat PASSED

- Phase 3: ROLES_PIE.json passes 19 runtime assertions. Passive moves without automatic fire; friendly damage is rejected; hostile damage, explicit command and configured nearby alert activate Fighter. Hunter obtains geographic pursuit routes and moves physically, exits for target loss, timeout, range or low HP, and resumes its unchanged original patrol references.
- Phase 4: BACKWARD_RETREAT_PIE.json passes 12 assertions at 30 HP. Three original Blueprint shots fired at signed speeds -4.28, -4.27 and -4.27 m/s; minimum speed -4.32 m/s. Independent turret tracking remains active. It resumes its original A/B patrol without teleporting. A parked hull behind prevents reverse commitment; forward A* remains available.
- PROPULSION_PIE.json uses actual Enhanced Input W/S and original AddForce. Forward peak 25.724 m/s, reverse peak -6.846 m/s. Initial measured accelerations 44.13 versus 18.27 m/s², ratio 0.414. Shared filter applies reverse reduction once, preserves strong braking while moving forward, and smoothly caps propulsion rather than setting velocity.
- ISLAND_REVERSE_PIE.json passes: independently selected accepted-water fixture has blocked chart cells behind. At 15 HP reverse is rejected, forward geographic escape begins, and the ship moves 42 m over 25 s without entering blocked cells. This is a deliberately near-coast maneuver, not a new saved spawn layout.
- Original player/AI force graphs retain their AddForce nodes and one command authority. New ShipMovement component is inherited by both classes. Explicit Blueprint compilation and integration validation succeeded after full native build/restart. Fixture mutations remain disposable PIE only.

### Accuracy implementation and pending final gate

- Added Easy/Normal/Hard/Custom presets to the existing combat component. Bounded disk error affects the actual joint aim and original projectile launch; delayed observations, acquisition time and limited updates add reaction latency. CRC-based per-named-ship streams use editable AccuracySeed. No random modification of collision damage/hit outcomes.
- Conservative geographic chart checks supplement targeting/arc traces when Cesium terrain collision is not streamed. Existing traces, friendly corridor checks, mechanical arc limits, animation alignment and cooldown remain.
- First 112-shot run passed Easy/Hard comparisons, but Normal hit 15/16 stationary shots. Preserved as ACCURACY_PRIOR_NORMAL_TUNING.json. Normal tuned to accuracy 0.6 / error 2.2 degrees / delay 0.35 s / update 0.4 s / acquisition 0.6 s. A six-case 144-shot retest is now running; do not claim phase 5 passed until it completes.
- Editor restart stalled before level load, with its last active startup log in optional NNERuntimeORT hardware initialization. Computer-use native pipe is unavailable in this session. Stopped only the editor process launched by this task, then relaunched with temporary -DisablePlugins=NNERuntimeORT. It opened successfully; no project plugin configuration was changed. Seven battle Blueprints explicitly compiled again.
- English current settings guide: Docs/NavalCombat/V2_SETTINGS.md. Phase 6 positional damage and all later master-task phases are still outstanding.

### Accuracy gate PASSED

- Final 144-shot seeded original-projectile fixture passed. Stationary Easy/Normal/Hard hull hits: 1/24, 12/24, 24/24. Moving: 5/24, 9/24, 19/24. All observed physical launch vectors align with actual animated muzzle (dot >0.999). Moving target peak speeds 14.55–14.80 m/s; these are measured fixture speeds, not a promise of identical velocity at each shot.
- ACCURACY_REPORT.md records methodology and limits. No hit/damage outcomes are modified; misses are actual projectile outcomes. Fixtures temporarily accelerate shot sampling with 1 s AI cooldown and invulnerable hulls; saved gameplay cooldown stays 8 s.
- Actual prominent-island LOS test passed again: target on accepted water, 848.9 m away, remained unseen and unfired upon for 15 s. Independent metric coastline geometry confirms the segment crosses the island. The fixture now uses explicit geographic placement rather than the moved B marker.
- Physical hull audit: ShipSM StaticMesh local bounds X [-7775.93,7982.82] cm, Y [-1156.28,1144.40] cm. Its local +X matches Arrow forward (dot 1.0). Positional damage will use this verified physical-hull coordinate frame.
- PIE ended and dirty packages saved. Phase 6 local-hull classification patch is applied; build and actual collision tests pending. Later phases remain outstanding.

### Positional damage and class profiles PASSED

- HULL_DAMAGE_PIE.json passes 31 checks. Real original projectile collisions reduce HP 100→92 (bow), 92→82 (midship), 82→74 (stern), then 74→64 on an arbitrarily rotated midship hit. Friendly collision causes zero damage. A real final 8 HP bow hit kills once. Supplemental explicit pipeline probes reject same-projectile repeated victim, self, friendly and unknown-source hits.
- DA_SmallShip and DA_LargeShip data assets created. Shared existing BP_Ship/BP_ShipEnemy retained; original forward and manual/AI rudder force nodes are now filtered once. Profile swapping preserves original COM/inertia base and buoyancy configuration rather than multiplying changes repeatedly.
- Real possessed-player input comparison: Small initial acceleration 40.887 m/s² / turn peak 1.993 deg/s; Large 28.069 m/s² / 0.481 deg/s. Both physically float, stay upright and respond to controls. This is an arcade placeholder differentiation using one mesh, not real hydrodynamic calibration.
- Mixed-class 60-second saved-layout fixture: all five AI obtain A* routes and physically move 130.8–787.6 m, remain upright/in water with no navigation failures. Two Large ships moved 130.8 and 160.2 m but did not fire in that encounter; do not claim they did.
- Separate PROFILE_LARGE_COMBAT_PIE.json passes 33 assertions with Large source and victim: original shots, local damage regions, team/duplicate guards and one health-depletion death all work. PROFILE_SUMMARY.json records independent physics/navigation measurements. Staged sinking compatibility is tested next, after replacing the old destruction path.
- Future profile fields cover visual/animation/physics assets, static hull collision, pontoon setup, movement, camera and weapon settings. /Game/NavalBattle is explicitly included in cook settings because native code loads its assets. New art still requires compatible rig/socket/collision configuration and water validation.
- Default saved ships remain Small until the later prematch class selector chooses otherwise; no original level placements/target references were changed by class fixtures.
- Phase 8 unified death/capsize/burning/geographic-sinking source patch is applied. Native build and real visual/runtime validation are pending. Do not claim the new destruction sequence passed yet.


## 2026-10-22 continuation — Phase 0 / terrain restoration

- Fetched origin; main and Thanh remain 28b7f7b, clean tree, no merge/rebase or unmerged paths. Created feature/quangninh-battle-integration; main will not be pushed automatically.
- Source/assets actually contain the completed accuracy presets, angular error, chart occlusion, 8/10/8 damage, Small/Large profiles, role/retreat logic, capsize/staged sinking and copied NS_ShipBurn/M_ShipBurnFire. They are committed through cd50c54 and the merge. The old top-level phase table is historical; visual VFX remains partial, automatic spectator/turn mode/setup menu are absent.
- No Unreal Editor was running at audit start. Two Codex front ends are present; no other editor writer identified. Do not stop unrelated app servers solely from process names.
- Recovery: path in Saved/active_quangninh_recovery.txt contains source/config/Blueprint/level/actor copies and hashes of all 2031 source dataset files.
- Dataset exists at E:/LeVietThanh1412-Product/QuangNinh: 919 quantized-mesh terrain tiles, 1110 PNG imagery tiles, layer.json and tilemapresource.xml. EPSG:4326/TMS; matching terrain/imagery bounds. All advertised terrain tiles exist. Source files were not modified.
- Created an initially absent compatibility junction E:/UnrealTerrainSet/QuangNinh to the actual folder. Map URLs remain authored by main, avoiding personal-path edits to shared assets. Repeatable setup: Tools/SeaNavigation/setup_quangninh.ps1. This data is a runtime file dependency, not just import metadata.
- Baseline C++ Development Editor build PASSED. Starting one editor; terrain visual/collision acceptance and real-map gameplay are not yet claimed passed.

| Continuation phase | Status | Next gate |
|---|---|---|
| 0 Baseline/protection | PASSED | Snapshot and source audit recorded |
| 1 Terrain | IN PROGRESS | Live visual, collision and geographic alignment checks |
| 2 Real-map regression | PENDING | Genuine full five-AI A/B soak and controls/camera tests |
| 3 Existing V2 verification | PENDING | Repeat meaningful current-map fixtures |
| 4 Destruction visuals | PARTIAL | Focused material/effect inspection, practical-camera screenshots |
| 5 Automatic spectator | MISSING | Follow teammate / Tab / M without stealing possession |
| 6 Optional turns | MISSING | Alternating teams/10 s action windows; physics continues |
| 7 Prematch setup | MISSING | Counts, profiles, difficulty, mode; direct PIE defaults |
| 8 Integrated test | PENDING | Saved real level end-to-end |
| 9 Handoff/commits | IN PROGRESS | Focused commits; no main push |


## 2026-10-22 critical continuation — grounding and Cesium audit

- Latest controlling task: `Docs/QuangNinhIntegration/CRITICAL_TASK.md`. Restore physical movement before additional gameplay. Reviewed the full chat and official Cesium placing-objects documentation. No merge or reset; all existing changes protected.
- Saved layout uses the existing five AI and target identities, within 3.3 km of main georeference. All routes clear original coast polygons by at least 250 m. World origin itself is land; ships use the adjacent bay.
- `COLLISION_BASELINE.json`: five valid A* paths and intact controllers, but final speed 0.000–0.004 m/s, no water immersion; downward traces hit actual Cesium sea-level triangles.
- `COLLISION_ISOLATED.json`: identical fresh PIE with only terrain interaction temporarily removed; all five enter water and move physically. This is diagnosis, NOT an acceptable final collision solution or patrol pass. Both fixtures ran at editor background throttling; displacement is limited by physics maximum delta time. Subsequent acceptance runs use unattended editor automation without focus throttling.
- Coordinate round trips are below 0.000001 cm; final geographic destination versus live Target Point below 0.000001 m. One shared inherited GlobeAnchor per ship; no duplicate required. Audit file: `CESIUM_CONFIGURATION_AUDIT.json`.
- Audit found origin-shift components enabled on every ship and on the georeference after the earlier snapshot. No shift observed in the compact bay, but these compete if their distance limits are crossed and water is unanchored. Preserve components/anchors and restore fixed-frame mode for the local battlefield.
- Native selective DEM filter compiled. It reads current Cesium CPU physics geometry (TriMeshGeometries), creates invisible runtime collision from retained land triangles, and removes only submerged triangles whose entire conservative bounding cell region is chart water. Ambiguous coastline/land stays solid. No plugin or dataset edits. Runtime and island/streaming acceptance are PENDING.
- New game features remain gated until terrain collision and five-AI movement pass.


## 2026-10-23 — physical repair and current-map evidence

- Read the full conversation and latest critical task; its three repeated attachments have identical SHA-256. The latest no-main/new-feature-branch instruction supersedes older merge requests. Official Cesium placing-objects guidance was read and applied.
- The 0.5 m collision candidate passed 12 physical island probes through view/LOD changes (5 assertions), actual player-hull island obstruction (4), 405.9-second five-AI B/A/B soak (15 arrivals, 3193–3662 m each), 20 integration checks, 66 real projectile/profile damage checks and two profile input/flotation comparisons. Planner Geometry/RealOSM automation both passed.
- The control fixture initially selected a stale input subsystem retained from a prior PIE. All input fixtures now resolve the subsystem from the active PlayerController; no gameplay input graph was changed.
- Full header inventory found negative elevations in 149 terrain tiles. The FINAL filter was narrowed to +/-0.05 m zero-level surfaces plus identified zero-level tile skirts, preserving other negative geometry. Native build and seven Blueprint compiles passed. The actor's tolerance is saved as 0.05 m. A fresh full patrol recheck is running; initial telemetry: five ships near 19 m/s, 1104 submerged triangles retained.
- Core source: SeaTerrainCollisionComponent; no source dataset or Cesium plugin edit. Persistent report: Docs/QuangNinhIntegration/CESIUM_TERRAIN_COLLISION_FIX.md.
- Pending: finish final 0.05 m repeat, record final collision evidence, focused core commit; then complete wider accuracy/tactical/VFX/spectator/turn/setup work. No new feature is marked complete prematurely.


### 2026-10-27 sector defense/assault master - phase 0 and navigation reproduction

- New authoritative task: attachment 8e30490c-8b1f-4503-9ad0-3a6b303b4980. Preserve selected candidate 1 at chart EN (14500,3500), 2500 m square; do not relocate terrain/georeference. New sectors/tactics/tags/buoyancy/magazine work remains pending until navigation passes.
- Current branch feature/small-warship-seeded-battlefield, user setup commit 94f47aa after Small milestone 9f2bb3f. Seven binary assets already modified at task start; no reset. Verified SHA256 backup of 86 source/config/Blueprint/map actor files in Saved/active_navigation_recovery.txt. One authoritative editor launched; no competing editor found.
- The saved DEMO_SAVED_PATROL_LOOPS.json is FAILED: TIME_LIMIT_REACHED at 600.75 s, all five AI only one arrival, then bounded recovery exhausted. Controller/physics retained; route endpoint assertions passed. This is current failure evidence, not a successful loop certificate.
- Fresh NAVIGATION_REGRESSION_REPRO.json started against the current saved actor/target configuration with combat disabled only in disposable PIE. Trace the actual post-arrival steering, sensor and sweep data before changing owning logic.
- Required phase order: repair/reposition navigation and focused verified commit; fixed sectors and deterministic safe spawning; Blue defense and Red scout assault; synchronized tags; measured buoyancy tuning; shared 10-round/30-second reload; integrated multi-seed/restart QA; reports/PR. No later phase is marked passed.

### Geographic return-loop collision repair - fresh physical B/A/B PASS

- Identified the actual return-route blocker: generated Cesium zero-surface skirts shifted laterally by 0.228-0.489 m, defeating the former 0.05 m coincidence heuristic. Public glTF skirt metadata now identifies the synthetic vertices; cooked-particle positions are matched without relying on Chaos index order. Only chart-water portions are removed. Land, seabed, source rendering and geographic A* remain intact.
- NAVIGATION_FIXED_SAVED_PATROL.json PASSED at 393.57 actual game seconds after restart: all five completed at least B/A/B, 16 measured arrivals, exact live target endpoints, original controllers and continuous physics. Camera cycled six times. No automatic reposition resets during normal motion.
- Structured diagnostics and explicit/automatic development-reposition invalidation added. Actual reposition, genuine selected-field island traversal, retained physical island support and combat-return gates are still pending; phase 1 is not yet complete. Buoyancy contacts are measured separately and are not misrepresented as a full stability pass.

### Phase 1 VERIFIED - navigation, saved/runtime relocation and island traversal

- NAVIGATION_PHASE1_VERIFIED.json aggregates eleven passing current reports. Native suite 4/4; saved fleet B/A/B 392.8 s; corrected runtime relocation B/A 242.15 s; saved editor relocation plus restart; candidate-1 island B/A 239.35 s with 2159 m physical travel and four native waypoints. Actual shoreline clearance >=176.60 m, planned >=226.01 m; all samples inside the selected field and no land intersections.
- Physical island-support, repeated LOD and cache regression passed with real land/submerged geometry retained. Shared role/target-loss and target-health-death patrol return passed. Temporary excluded marker was rejected explicitly and recovered automatically when restored, followed by 374.34 m physical travel.
- The failed runtime-relocation sampler ordering case remains preserved, with its actual fixture error documented; the rerun passed. The additional retreat fixture used a deliberately moved stationary threat and an actual hostile health death, and returned to original patrol. This does not claim natural full 3v3 combat or later sector/ammunition completion.
- Only TP_Friendly_Patrol_A and the deliberately tested BP_ShipEnemy2 saved placement changed in this focused navigation milestone. Initial unrelated pending binary edits are retained. No terrain/georeference movement, graph rewrite, duplicate drive authority or normal-game teleport was introduced.
- Next: fixed sector grid for the exact selected candidate, then seeded safe spawns, Blue defense/Red assault, tags, measured stability, magazines and combined QA in the user-specified order.

### Phase 2 VERIFIED - exact candidate-1 sector grid

- Navigation milestone 68be150 was normally pushed and remote SHA verified. Draft PR #11 targets main; no automatic merge.
- Created the fixed 2500 x 2500 m, 25 x 25 logical sector grid from the original candidate script/metadata and actual coastline. No terrain/georeference changes or 625 TargetPoint actors. Source chart and data hashes are recorded.
- 337 chart-navigable sectors; main field-connected component 332, two disconnected groups (4 and 1) excluded from spawn/patrol flags; 295 spatial spawn candidates at >=200 m anchor shore clearance. Six tactical regions are saved.
- Native UNavalSectorSubsystem loads bounded, hash-validated data and exposes geographic queries and existing native A* routes. Fresh PIE passed all 625 indices, EN/world round-trip <=1.21e-9 m, five required tactical connections (3/2/3/2/5 waypoints), 11 original TargetPoints and six original combat pawns.
- SECTOR_BATTLEFIELD_GUIDE.md, SECTOR_GRID_QA.json, SECTOR_GRID_PIE.json and cropped preview describe actual results. This milestone does not yet reposition the fleet or claim safe physical spawns. Next: deterministic seeded layout authority and runtime water/terrain/hull validation.
