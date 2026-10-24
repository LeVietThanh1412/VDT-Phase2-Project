# Naval Battle V2 — progress ledger

Current work (2026-10-22): QuangNinh integration on `feature/quangninh-battle-integration`, based on merged `28b7f7b`. Terrain source access is restored; the near-origin layout is saved. Actual five-AI patrol on the restored terrain FAILED because of sea-level DEM collision. Selective collision restored all five ships and passed B/A/B loops; a narrower final filter preserving nonzero submerged geometry is undergoing the final recheck. The previous map's results below are historical, not a current-map pass. Spectator, optional turns and pre-match settings remain outstanding.

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
