# CONTINUE DEVELOPMENT — Restore QuangNinh Terrain, Verify Merged Naval Battle, and Complete Remaining Gameplay Features

## PROJECT CONTEXT

You are continuing development of my Unreal Engine 5 naval battle project.

Project name:
`VDTPhase2Project`

Project directory:
`E:\LeVietThanh1412-Product\VDT-Phase2-Project`

Main level:
`MainMapLevel`

Git repository:
`https://github.com/Mr-Gwenchana/VDT-Phase2-Project`

The previous merge Agent reported that `Thanh` was successfully merged into `main`, and both branches were pushed to commit:

`28b7f7b`

The merged implementation should preserve:

- Main's geographic map, coastlines, terrain and water environment.
- Thanh's naval ship Blueprints and gameplay systems.
- Geographic A* navigation.
- Independent AI ship patrol.
- Naval combat and hit-and-run.
- Physics-based movement.
- Buoyancy.
- Cesium integration.
- Team affiliations and health.
- Minimap and team markers.
- FreeCameraPawn and CustomPlayerController.

The previous Agent also reported:

- Successful C++ compilation.
- Successful Blueprint compilation.
- 135 passing PIE checks.
- Minimap and camera integration verified.
- All five AI controllers preserved during camera switching.

However, real terrain rendering was NOT verified because the external QuangNinh dataset could not be found.

The full multi-ship patrol soak test was also NOT repeated after the merge.

I am now providing the missing terrain directory.

---

# IMPORTANT NEW INFORMATION — TERRAIN DATA LOCATION

The original map references terrain data under:

`E:\UnrealTerrainSet\QuangNinh`

The actual dataset available on my machine is located at:

`E:\LeVietThanh1412-Product\QuangNinh`

**Use this actual local directory as the candidate source for the missing QuangNinh terrain and imagery data.**

This is a local Windows filesystem path, NOT an HTTP URL.

Do not attempt to download it from the internet.

Do not assume its directory structure matches the old location until you inspect it.

You are authorized to inspect and configure access to this folder, but must preserve its original contents.

---

# GENERAL AUTONOMY REQUIREMENTS

Work autonomously using the available Unreal Editor, SpecialAgent MCP, Git, C++, Blueprint, and scripting tools.

Do not ask unnecessary confirmation questions.

Inspect the actual current project state before implementing anything.

Do not recreate features that already exist and work.

Do not start another Git merge or rebase.

Do not force-push.

Do not reset or discard working changes.

Do not overwrite the teammate's environment assets.

Do not damage the existing naval gameplay.

Use targeted tests and real PIE verification.

If a task encounters a genuine technical blocker, document it and continue only with independent work that can be performed safely.

Keep a persistent progress report so interrupted Agent sessions can resume without repeating completed work.

---

# PHASE 0 — VERIFY THE MERGED REPOSITORY AND PROTECT CURRENT WORK

Before modifying any project assets:

1. Run git status.
2. Inspect the current branch and HEAD commit.
3. Fetch remote references without changing working files.
4. Verify whether main and Thanh both reference commit 28b7f7b.
5. Check for pending Git merges or rebase operations.
6. Inspect uncommitted and untracked files.
7. Inspect current Git LFS configuration.
8. Check for concurrently active Agents.
9. Inspect Unreal Editor dirty assets.
10. Preserve any existing unfinished work.

Do not assume that the last reported commit contains all Naval Battle v2 changes.

Inspect recent Git history and the current codebase.

Specifically look for the features previously reported by the Naval Battle v2 Agent:

- Easy/Normal/Hard AI accuracy.
- Angular shooting dispersion.
- Island occlusion checks.
- Bow/midship/stern positional damage.
- Large/Small ship physics profiles.
- Buoyancy class tuning.
- Staged destruction and sinking.
- Fire and smoke Niagara integration.

The previous development Agent reported testing these features, but some modifications may have existed only in an uncommitted worktree.

Check whether they are actually present in the current merged project.

If any completed feature is missing, investigate available local worktrees, branches, commits, and preserved files before rebuilding it.

Do not cherry-pick or restore changes blindly.

Record the baseline in:

`Docs/NavalBattleV2_Progress.md`

Maintain a separate integration record if necessary.

## PHASE 0 ACCEPTANCE

- Repository state is understood.
- Existing gameplay modifications are protected.
- No active merge conflict remains.
- The actual status of Naval Battle v2 features is documented.

---

# PHASE 1 — RESTORE THE QUANGNINH TERRAIN DATA

This is the highest-priority task.

## 1.1 Inspect the provided directory

Inspect:

`E:\LeVietThanh1412-Product\QuangNinh`

Recursively inventory relevant files and directories.

Identify:

- Terrain heightmaps.
- Imagery and textures.
- Cesium-compatible terrain assets.
- Raster datasets.
- Landscape source files.
- Material dependencies.
- Metadata and configuration files.
- Any existing Unreal asset packages.

Determine the actual dataset structure rather than relying on filenames alone.

Do not delete, move, overwrite, or reimport these files unnecessarily.

## 1.2 Identify the original references

Search the merged project for references to:

`E:\UnrealTerrainSet\QuangNinh`

Inspect:

- MainMapLevel.
- Terrain actors.
- Cesium actors.
- Landscape components.
- Material assets.
- Asset import metadata.
- Configuration files.
- Project scripts.
- Relevant plugins.

Determine whether each reference is:

A. A runtime file dependency.

B. Editor import-source metadata.

C. A terrain-generation source.

D. A cached path that is no longer required.

Do not assume every old path must be rewritten.

## 1.3 Restore correct data access

If the provided directory contains the required assets, use the least invasive method to restore the project.

Prefer:

1. Configurable external data-root mapping when the existing implementation supports it.
2. A local filesystem junction or directory link when exact old-path compatibility is required.
3. Updating relevant configurable source paths when necessary.
4. Reimporting individual assets only if their original imported data is actually missing.

If using a directory junction, verify the old destination does not already contain unique data before creating it.

Never delete or overwrite an existing directory just to establish a junction.

Do not create a recursive junction.

Do not change the supplied source dataset.

Do not hardcode my personal E: drive path into gameplay C++ or shared Blueprint logic.

Use a project-local development configuration or documented setup mechanism where appropriate.

## 1.4 Validate Cesium and geographic terrain

After restoring data access:

- Open MainMapLevel.
- Load necessary World Partition cells.
- Inspect the geographic terrain.
- Verify the islands.
- Verify coastline alignment.
- Verify water alignment.
- Verify imagery placement.
- Verify terrain and water collision.
- Verify CesiumGeoreference.
- Verify CesiumGlobeAnchor.
- Verify origin-shift behavior where applicable.

Do not shift the whole map just to hide a coordinate mismatch.

Do not move ship patrol Target Points unless a confirmed integration error requires it.

Do not replace the teammate's original terrain with generated placeholder terrain.

## 1.5 Verify geographic A* against real terrain

The existing A* uses geographic island polygons and water navigation constraints.

Compare the real visible coastline with the navigation polygon data.

Verify:

- Valid-water classification.
- Island exclusion.
- Coastal clearance.
- Path segments.
- Waypoint placement.
- Final destination approaches.
- Existing ship patrol routes.

The navigation polygons must remain geographically aligned with the restored terrain.

If a mismatch is found, determine whether the cause is terrain alignment, polygon data, or coordinate conversion before modifying anything.

## PHASE 1 ACCEPTANCE

The actual QuangNinh terrain and imagery load correctly in MainMapLevel.

The ocean and islands visually align.

The naval ships remain correctly positioned on navigable water.

The geographic A* route visualization aligns with the terrain.

Record screenshots and runtime evidence.

Do not report this phase as passed based on asset existence alone.

---

# PHASE 2 — FULL MERGED MAP AND NAVAL GAMEPLAY REGRESSION

The previous Agent passed 135 PIE checks, but the terrain was unavailable.

Repeat meaningful integration tests using the restored REAL map.

## 2.1 Verify the full 3v3 scene

Default match:

BLUE:
- One player-controlled ship.
- Two friendly AI ships.

RED:
- Three enemy AI ships.

Verify:

- Exactly six active combat ships.
- Exactly one human-controlled pawn.
- Exactly five AI-controlled ships.
- Correct team affiliations.
- Correct GameMode initialization.
- Correct PlayerController possession.
- No duplicate player spawn.

## 2.2 Verify naval navigation

All five AI ships must independently navigate between their own two Target Points.

Test:

- Valid A* path generation.
- Actual physical ship movement.
- Correct A/B destination switching.
- Valid waypoint progression.
- Island avoidance.
- Coastal clearance.
- No permanent stationary AI.
- No repeated pathfinding failure.
- No accidental navigation reset during combat.

Run a genuine patrol soak test on the restored map.

Record each AI ship's:

- Initial position.
- Actual speed.
- Current destination.
- Waypoint index.
- Distance traveled.
- Destination switches.
- Path failures.
- Stuck events.

Do not claim all ships work based on a single moving enemy.

## 2.3 Verify map integration

Check:

- Main's environment actors remain present.
- Landscape and imagery render.
- WaterBodyOcean works.
- Buoyancy works.
- World Partition loads required actors.
- Cesium origin shifting is compatible with the current map configuration.

The previous merge reportedly adopted main's ship origin-shift configuration.

Verify that this configuration is actually compatible with naval navigation and gameplay.

Do not enable or disable origin shifting globally without first demonstrating the cause of any issue.

## 2.4 Verify teammate features

Test:

- Minimap.
- Ship markers.
- Team marker colors.
- FreeCameraPawn.
- CustomPlayerController.
- Camera switching.
- Player input mappings.

Important:

Camera switching must not steal AIController possession from friendly or enemy AI ships.

Verify all five AI controllers remain active after switching cameras.

## PHASE 2 ACCEPTANCE

The merged environment and naval gameplay function together in real PIE.

No critical player-control, camera, buoyancy, or navigation regression remains.

---

# PHASE 3 — VERIFY EXISTING NAVAL BATTLE V2 FEATURES

Inspect the actual merged implementation.

Do not assume the previous Agent's uncommitted changes were merged.

For every feature below, classify its current state as:

- Present and verified.
- Present but broken.
- Partially implemented.
- Missing.
- Not testable because of a specific blocker.

## 3.1 AI shooting accuracy

The previous Agent reported a 144-shot test.

Reported results for moving targets:

Easy: 5/24.

Normal: 9/24.

Hard: 19/24.

Verify the current project actually contains the tuned accuracy implementation.

Test real projectile collisions.

Confirm AI can miss.

Confirm projectile trajectories agree with muzzle direction.

Preserve existing difficulty presets.

## 3.2 Positional damage

Required final damage model:

MaxHealth = 100.

BaseProjectileDamage = 10.

Bow = 80% = 8 HP.

Midship = 100% = 10 HP.

Stern = 80% = 8 HP.

Verify:

- Actual local hull impact classification.
- Rotated hull correctness.
- Friendly-fire prevention.
- Self-hit prevention.
- Duplicate hit prevention.
- Exactly one death event.

## 3.3 Large and Small ship profiles

Verify both profile implementations exist.

Use the current existing ship mesh as a placeholder.

Confirm:

- Separate mass/inertia settings.
- Propulsion tuning.
- Rudder response.
- Buoyancy configuration.
- Large ships behave more heavily.
- Small ships are more responsive.
- Both remain stable on the real map.

Do not claim realistic hydrodynamics based on placeholder geometry alone.

## 3.4 Tactical retreat

Verify:

- Low-health retreat.
- Reverse propulsion weaker than forward propulsion.
- Reverse-and-fire.
- Safe geographic retreat paths.
- Return to A/B patrol.
- Passive to Fighter transition.
- Hunter to Fighter transition.

Do not rebuild working tactical systems.

## PHASE 3 ACCEPTANCE

Every previously completed feature is found in the current source, tested, and classified accurately.

Only missing or failing functionality should be implemented again.

---

# PHASE 4 — FINISH SHIP DESTRUCTION FIRE AND SMOKE VFX

The previous Agent reported:

- Gradual sinking works.
- Wreck cleanup works.
- Capsized hulls retain their roll.
- Cesium origin-shift position preservation works.
- Niagara particles simulate.
- Fire/smoke remain difficult to see in screenshots.

Do not discard the existing sinking implementation.

## 4.1 Inspect the current effect

Inspect the actual assets and source code for:

- Fire emitters.
- Smoke emitters.
- Niagara sprite renderers.
- Sprite facing mode.
- Particle size.
- Opacity.
- Material blend mode.
- Cutout geometry.
- Particle color.
- Emitter bounds.
- Local versus world space.
- Attachment transforms.
- Camera visibility.

Check whether these changes survived the branch merge.

## 4.2 Repair the visual presentation

Expected ship destruction:

HEALTH ZERO OR CAPSIZED

→ Gameplay death event.

→ Fire and smoke activate.

→ Ship remains visible as a burning wreck.

→ Ship gradually sinks.

→ Fire and smoke fade.

→ Wreck is cleaned up.

Fire and smoke must be clearly visible from practical gameplay camera distances.

Avoid extreme particle counts.

Do not unnecessarily change the original shared Niagara asset.

Prefer a separate naval destruction effect or material instance when suitable.

## 4.3 Avoid endless Niagara debugging

Perform focused diagnostics with measurable screenshot comparisons.

If the current effect remains unsuitable after a small number of focused correction attempts, use a simpler existing compatible fire/smoke fallback.

Do not sacrifice the working destruction system for cosmetic polish.

If a limitation remains, document it and proceed to the next phase.

## PHASE 4 ACCEPTANCE

Real PIE visibly demonstrates fire, smoke, gradual sinking, and cleanup.

If the available VFX cannot be made visibly functional, report the visual limitation separately without falsely claiming a complete VFX pass.

---

# PHASE 5 — COMPLETE PLAYER DEATH AND SPECTATOR CAMERA

Inspect the current spectator implementation first.

The game must continue running after the player's ship dies.

When the player dies:

1. Disable player ship movement and firing.
2. Keep the destroyed ship visible during sinking.
3. Transition to spectator automatically.
4. Follow an alive friendly AI ship.
5. Allow switching to another friendly AI ship.
6. Allow switching to a battlefield overview.
7. Preserve all existing AIController possession.
8. Display appropriate spectator HUD information.

Unreal Editor may reserve F8 for PIE Eject.

Do not confuse editor Eject with gameplay spectating.

Use reliable gameplay bindings, preserving the current input architecture.

Suggested:

Tab = next teammate.

M = battlefield overview.

Verify compatibility with CustomPlayerController and FreeCameraPawn.

Do not create multiple competing PlayerControllers.

## PHASE 5 ACCEPTANCE

Kill the player ship in an actual PIE test.

Verify automatic spectator transition.

Verify teammate switching and overview.

Verify no friendly AI loses its controller.

---

# PHASE 6 — COMPLETE OPTIONAL TURN-BASED MODE

The default battle must remain real-time.

If the existing turn-based feature is incomplete, implement it using the current GameMode/GameState architecture.

Supported modes:

REAL_TIME.

TURN_BASED.

Turn-based requirements:

- BLUE and RED alternate turns.
- Each eligible living ship receives a 10-second action window.
- The active ship may move, aim, and fire.
- Inactive ships cannot voluntarily move or initiate new shots.
- Buoyancy and physics continue.
- Already-fired projectiles continue traveling.
- Damage and destruction continue.
- Burning and sinking VFX continue.
- Dead ships are skipped.
- Team elimination ends the match.

Keep the existing real-time AI behavior unchanged.

## PHASE 6 TESTS

Verify both modes.

Test a full turn rotation.

Test player controls.

Test AI turns.

Test death during a turn.

Test sinking across turn boundaries.

Verify no duplicate turn execution.

---

# PHASE 7 — COMPLETE PRE-MATCH CONFIGURATION

Create or finish the existing pre-match setup feature.

The player should be able to select:

- BLUE team size.
- RED team size.
- Large or Small ship profile for each selected slot.
- AI difficulty.
- Real-Time or Turn-Based mode.

Current limits:

BLUE: 1–3 total ships.

RED: 1–3 total ships.

Exactly one BLUE ship is player-controlled.

All remaining ships are AI-controlled.

Default configuration:

BLUE = 3.

RED = 3.

Game mode = Real-Time.

AI difficulty = Normal.

MaxHealth = 100.

BaseProjectileDamage = 10.

Do not create duplicate spawns.

Use existing spawn and patrol infrastructure.

Do not replace the geographic A* system.

Preserve the ability to start MainMapLevel directly in PIE using default development settings.

## PHASE 7 TESTS

Verify:

- 1v1.
- 2v2.
- 3v3.
- Asymmetric team sizes.
- Large/Small selection.
- Difficulty selection.
- Real-Time selection.
- Turn-Based selection.
- Player possession.
- Correct AI counts.
- Persistence after a fresh PIE start.

---

# PHASE 8 — FINAL END-TO-END GAMEPLAY VALIDATION

Perform one integrated 3v3 validation on the restored QuangNinh map.

The test must use the ACTUAL saved level and terrain.

Verify:

1. Terrain and imagery display correctly.
2. Ocean and coastline align.
3. Exactly six ships initialize.
4. Player controls work.
5. All five AI ships move physically.
6. Independent A/B patrol routes work.
7. A* paths avoid islands.
8. Ships encounter opposing teams.
9. AI aiming and firing work.
10. AI can genuinely miss shots.
11. Positional damage is 8/10/8 HP.
12. Low-health retreat works.
13. Reverse-and-fire works where implemented.
14. Large/Small profiles remain stable.
15. Destroyed ships burn and sink.
16. Spectator follows teammates.
17. Minimap and camera controls work.
18. Team counts remain accurate.
19. Match victory/defeat works.
20. No major new runtime errors occur.

Use real runtime telemetry.

For every critical assertion, distinguish:

- Static inspection.
- Unit test.
- Blueprint compilation.
- PIE functional test.
- PIE visual verification.

Do not report unperformed scenarios as passed.

---

# PHASE 9 — DOCUMENTATION, CLEANUP AND COMMIT

Update:

`Docs/NavalBattleV2_Progress.md`

Also document the QuangNinh local terrain setup.

Include:

- Original expected terrain path.
- Actual available terrain path.
- Chosen mapping or resolution method.
- Whether the source folder is required at runtime.
- How another developer can configure their own local dataset.
- Required project setup steps.
- Remaining data limitations.

Do not commit large external raw terrain datasets automatically.

Respect Git LFS rules for actual Unreal asset packages.

Remove only temporary test artifacts created by this development session.

Do not delete existing user data.

Compile and save modified project assets.

Run a final regression test after saving.

Create focused commits for completed changes.

Use clear commit messages.

Do not amend or rewrite the previous merge commit.

Do not merge into or push to main automatically.

Prefer a NEW feature branch based on the verified merged main commit for further development.

Suggested branch:

`feature/quangninh-battle-integration`

Push this feature branch when verified and authorized by the existing repository workflow.

Do not force-push.

If no remote publish action is required or authorized, leave the tested commits locally and report the commands needed to publish them.

---

# REQUIRED FINAL REPORT

Provide a complete summary covering:

## Git status

- Current branch.
- Current HEAD.
- Confirmation of previous merge state.
- Whether Naval Battle v2 changes existed in the merged code.
- New commits created.

## Terrain restoration

- Actual files discovered.
- Missing or present dependencies.
- Selected path-mapping method.
- Changes to terrain references.
- Screenshot verification.
- Cesium and water compatibility.

## Naval gameplay

- Number of moving AI ships.
- A* navigation results.
- Combat results.
- Accuracy verification.
- Damage tests.
- Large/Small profile results.
- Retreat behavior.
- Sinking and VFX results.

## Additional features

- Spectator implementation status.
- Turn-based implementation status.
- Pre-match menu status.
- Minimap and camera compatibility.

## Testing

Report:

Test | Test Type | Result | Evidence | Remaining Issues

Do not claim success based only on compilation.

Include actual PIE runtime evidence.

## Remaining limitations

Clearly distinguish:

- Missing data.
- Tool limitations.
- Unfinished features.
- Unverified real-time gameplay.
- Remaining visual defects.

---

# FINAL SUCCESS CONDITION

When I open MainMapLevel and press Play:

The REAL QuangNinh terrain loads correctly.

The combined map from main and naval gameplay from Thanh work together.

Ships remain physically stable on the water.

All configured AI ships navigate using the existing geographic A* system.

The game supports real-time 3v3 naval combat.

The previously implemented AI accuracy, positional damage, retreat, buoyancy, and destruction functionality remain intact.

Destroyed ships visibly burn and sink instead of instantly disappearing.

The player can spectate teammates after death.

Optional turn-based and pre-match configuration features work if implemented and verified.

All important settings and actor configurations survive a new PIE session.

---

# EXECUTION DIRECTIVE

Do not start over from scratch.

Do not repeat the Git merge.

Begin by verifying the merged source state and inspecting:

`E:\LeVietThanh1412-Product\QuangNinh`

Then restore terrain access, validate the real map, verify previous Naval Battle v2 progress, and complete the remaining phases sequentially.

For each phase follow:

INSPECT → BASELINE → IMPLEMENT → COMPILE → TEST → PIE → FIX → RETEST → SAVE → RECORD.

Work autonomously, avoid unnecessary questions, and preserve both the teammate's map and my naval gameplay.

Begin Phase 0 and Phase 1 immediately.