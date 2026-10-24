# CRITICAL TASK: FIX NAVAL SHIPS ON NEW QUANGNINH CESIUM TERRAIN AND CONTINUE NAVAL BATTLE V2

## PROJECT CONTEXT

Project: VDTPhase2Project

Project directory:
E:\LeVietThanh1412-Product\VDT-Phase2-Project

Terrain dataset:
E:\LeVietThanh1412-Product\QuangNinh

Engine: Unreal Engine 5.6

Main level: MainMapLevel

Available tools:
- SpecialAgent MCP
- Unreal Editor automation
- Unreal Python
- C++
- Blueprint editing
- Existing test scripts
- Git

Official Cesium documentation:

https://cesium.com/learn/unreal/unreal-placing-objects/

**Read and apply the relevant official Cesium documentation before changing actor georeferencing, physics, or terrain collision.**

## PRIMARY MISSION

The project was recently merged with my teammate's new QuangNinh terrain map.

The original ship navigation worked in the previous map.

After transitioning to the new terrain, multiple ships stopped moving.

Your highest priority is to restore correct movement, georeferencing, buoyancy, water interaction, and geographic A* navigation for all five AI-controlled ships.

Then continue implementing and validating the remaining Naval Battle v2 features.

Do not rebuild the working A* algorithm unnecessarily.

Do not recreate my naval ship Blueprints.

Do not replace my teammate's terrain map.

Do not ask routine questions.

Work autonomously, inspect the actual implementation, fix confirmed problems, compile, run real PIE tests, save changes, and continue through the remaining phases.

---

# PHASE 0 — PROTECT THE CURRENT PROJECT

The previous merge Agent reported that main and Thanh were successfully merged at commit:

28b7f7b

Development may now be taking place on:

feature/quangninh-battle-integration

Do not assume the current working branch or HEAD.

Inspect Git first.

Run the equivalent of:

- git status
- git branch -vv
- git log --oneline -10
- git diff
- git diff --cached

Preserve all uncommitted work.

Inspect previous Agent progress and reports, particularly:

Docs/QuangNinhBattle_Continuation.md

Docs/QuangNinhIntegration/GROUNDING_DIAGNOSTIC.json

Docs/NavalBattleV2_Progress.md

Docs/MergeIntegration/REPORT.md

Inspect the existing navigation and PIE test scripts.

Do not restart the previous Git merge.

Do not reset, revert, or discard gameplay changes.

Check for another Agent actively writing files or controlling Unreal Editor.

Use the existing integration branch if appropriate.

Do not modify main directly.

## PHASE 0 GATE

Understand and preserve the actual saved project state before making corrections.

---

# PHASE 1 — INVESTIGATE THE ROOT CAUSE OF SHIPS NOT MOVING

Previous diagnostic evidence:

- The new map contains a QuangNinh terrain tileset.
- Six ships and their patrol targets were repositioned near the new world origin.
- The planned routes were reported to clear the coastline by at least 250 meters.
- All five AI ships generated valid A* routes.
- However, all five ships remained physically stationary.
- A previous diagnostic suggested submerged terrain collision at sea level was grounding the ships' deep collision hulls.

Do not assume that this is the only problem.

Investigate two independent categories:

A. Cesium georeferencing and component configuration.

B. Terrain collision and naval buoyancy/physics.

## 1.1 Reproduce the current failure

Start a controlled PIE test with combat disabled.

For every AI ship record:

- Actor label
- Blueprint class
- AIController
- Geographic coordinates
- Unreal world coordinates
- Physics body position
- Current A* destination
- Waypoint count
- Navigation state
- Throttle
- Rudder
- Linear velocity
- Angular velocity
- Collision contacts
- Grounding status
- Water immersion and buoyancy status

Observe whether the AI issues nonzero propulsion commands.

Determine if the ship fails to move because of missing commands, collision resistance, physics configuration, or coordinate errors.

Do not modify pathfinding before identifying the actual cause.

---

# PHASE 2 — AUDIT CESIUM COMPONENTS AND GEOREFERENCING

Read:

https://cesium.com/learn/unreal/unreal-placing-objects/

The documentation explains:

- CesiumGeoreference controls the relationship between world coordinates and the globe.
- CesiumGlobeAnchor makes movable actors geographically anchored.
- CesiumOriginShift can rebase the georeference.
- Non-georeferenced actors may move relative to the globe during georeference changes.
- Multiple simultaneously active origin-shift controllers can cause problems.
- Unreal physics and local gravity require additional consideration.

## 2.1 Inspect the level

Find all relevant actors and components:

- CesiumGeoreference
- CesiumOriginShift
- Cesium3DTileset
- QuangNinh terrain tileset
- CesiumGlobeAnchor
- WaterBodyOcean
- WaterZone
- BP_Ship
- BP_ShipEnemy
- All friendly AI ships
- All enemy AI ships
- Patrol Target Points
- AI navigation components
- FreeCameraPawn
- PlayerStart

Identify:

- Which georeference each geographic actor uses.
- Whether multiple georeference actors exist.
- Whether multiple origin-shift components are active.
- Whether target actors preserve geographic positions.
- Whether runtime Blueprint instances inherit the expected components.
- Whether ship transform updates conflict with physics simulation.

## 2.2 Correct ship anchoring

For georeferenced movable ship actors, inspect whether a correctly configured CesiumGlobeAnchor is present.

If missing and required:

- Add or restore CesiumGlobeAnchor to the appropriate shared ship class.
- Confirm Mobility is compatible.
- Connect it to the correct CesiumGeoreference.
- Preserve existing physics simulation.
- Preserve skeletal mesh component hierarchy.
- Preserve buoyancy and force-based movement.

Do not create duplicate CesiumGlobeAnchor components on inherited ship instances.

Do not recreate existing components that are already configured correctly.

Inspect Cesium's transform-update options and determine which settings are appropriate for the current physics-driven implementation.

Avoid continuous teleport-like transform corrections that fight Chaos physics.

Adding CesiumGlobeAnchor alone does not solve ocean buoyancy, land collision, or wrong navigation coordinates.

## 2.3 Check Target Points

For every AI patrol endpoint:

- Inspect its current saved world transform.
- Verify its geographic position.
- Verify its current World Partition loading state.
- Check whether it moves incorrectly after georeference changes.
- Ensure the navigation system reads current actor locations.
- Avoid stale cached world coordinates.

If geographic anchoring is required for a Target Point, configure it safely as a movable georeferenced actor.

Do not move Target Points unnecessarily.

Do not add an anchor blindly if the target is already correctly maintained in the fixed local coordinate frame.

## 2.4 Determine the correct origin-shifting policy

The merged map reportedly changed the previous ship origin-shifting configuration.

Inspect the currently active policy.

For this relatively compact battlefield, consider whether a fixed CesiumGeoreference is the simplest stable solution.

However, do not change the policy merely because origin shifting was problematic before.

Validate the need for shifting using actual distance, coordinate precision, and gameplay behavior.

If origin shifting is enabled:

- Ensure one authoritative shift controller.
- Ensure required actors are georeferenced consistently.
- Preserve WaterBodyOcean collision alignment.
- Recompute world-space navigation coordinates after shifts.
- Preserve ship velocity and movement continuity.
- Keep target positions geographically consistent.

If origin shifting is disabled:

- Verify all ships operate safely within the chosen fixed coordinate frame.
- Ensure the current geographic A* conversions remain correct.
- Document why this policy was selected.

## 2.5 Coordinate conversion tests

Test:

- Longitude/latitude/height to Unreal world.
- Unreal world to geographic coordinates.
- Round-trip consistency.
- Ship world location versus geographic location.
- Target Point world location versus A* destination.
- A* waypoint conversion.
- Active georeference identity.

Use correct units:

Unreal positions: centimeters.

Geographic heights/distances: meters where applicable.

Do not mix longitude/latitude with Unreal XY.

## PHASE 2 GATE

All ships and Target Points use a coherent coordinate frame.

Georeference changes, if enabled, do not misplace actors.

The system has no competing origin-shift controllers.

No unwanted teleporting or physics-transform fighting occurs.

---

# PHASE 3 — FIX QUANGNINH TERRAIN COLLISION UNDER THE OCEAN

THIS IS THE MOST IMPORTANT PHYSICS FIX.

The previous Agent discovered a likely cause:

The QuangNinh terrain tileset contains flat or nearly flat surfaces around sea level.

These surfaces may collide with the submerged portions of ship physics hulls.

Because ships extend below the visible water surface, collision with this terrain can cause them to become grounded despite having perfectly valid water routes.

## 3.1 Verify collision geometry

Inspect the actual QuangNinh terrain tileset.

Identify:

- The tileset actor.
- Terrain collision configuration.
- Collision mesh generation.
- Collision channels.
- Physical material.
- Loaded collision components.
- Sea-level terrain triangles.
- Actual water surface elevation.
- Ship hull collision geometry.
- Ship physics body bounds.

At a grounded ship, gather actual collision evidence:

- Ship hull collision shape.
- Contact point.
- Contact normal.
- Colliding component name.
- Terrain tile identity where possible.
- Hull bottom position.
- Local sea surface position.
- Terrain collision height.
- Physics linear velocity.
- Applied propulsion force.

Use Unreal traces and physics diagnostics as appropriate.

Do not diagnose grounding from screenshots alone.

## 3.2 Controlled collision isolation

In a temporary test fixture only:

1. Place one ship in confirmed open water.
2. Run with existing terrain collision.
3. Measure propulsion and forward movement.
4. Temporarily disable or isolate the suspected terrain collision interaction.
5. Repeat the identical test.
6. Compare actual physical movement.

If movement is restored only when that collision interaction is disabled, record this as evidence.

Restore the original test state before implementing the final solution.

Do not leave the entire map collision disabled.

## 3.3 Implement a selective, persistent correction

The final goal:

**The ship should interact physically with the ocean and buoyancy system, without being blocked by artificial submerged terrain surfaces, while still respecting real islands and coastlines.**

Evaluate the available solutions based on the actual Cesium tileset and project architecture.

Potential approaches, in preferred order when feasible:

A. Correct collision generation for terrain geometry that should not exist beneath open water.

B. Filter invalid submerged terrain collision while preserving actual land geometry and legitimate shallow-water obstacles.

C. Use well-defined collision channels/profiles with a separate reliable physical shoreline/land blocking representation.

D. Build a maintained, geographically aligned coastal collision proxy only where necessary.

Do not assume per-triangle runtime collision filtering is supported by the existing Cesium integration.

Inspect the actual plugin APIs before choosing that implementation.

If streamed terrain tiles are involved, ensure the solution survives tile loading, unloading, and LOD changes.

Do not patch Cesium plugin source unnecessarily.

## 3.4 Never disable important collision globally

The following are NOT acceptable final fixes:

- Disabling all ship collision.
- Disabling all terrain collision permanently without replacement.
- Making ships pass through islands.
- Disabling all world collision on the skeletal mesh.
- Teleporting ships above the terrain every Tick.
- Forcing velocity directly to overcome a blocked collision.
- Removing geographic polygons.
- Moving every route far offshore to hide the issue.

A collision-channel workaround is only acceptable if a separate reliable mechanism prevents the ships from passing through land.

## 3.5 Preserve island collision

Verify that:

- Real islands remain physically obstructive.
- Shorelines are not passable by ships.
- A* polygons agree with collision boundaries.
- Ship-to-ship collision continues working.
- Projectiles still interact with intended terrain obstacles.
- Retreat behavior cannot pass through coastline geometry.

The final solution must preserve naval gameplay collision safety.

## 3.6 WaterBodyOcean and buoyancy integration

Inspect:

- WaterBodyOcean bounds.
- WaterZone configuration.
- Water mesh collision extents.
- Buoyancy pontoons.
- Buoyancy collision channels.
- Physics body mass.
- Center of mass.
- Water-contact detection.
- Physics simulation state.
- Ocean surface height.

Preserve the existing BP_Ship propulsion and buoyancy logic.

Do not replace the current force-based ship movement.

Check whether the new map requires different WaterBodyOcean bounds to cover all patrol routes.

Do not expand bounds arbitrarily without measuring the actual region.

## PHASE 3 GATE

In real PIE:

- A ship in open water is not grounded on artificial terrain.
- The ship floats correctly.
- The ship accelerates using AddForce.
- The ship turns using existing rudder physics.
- The ship cannot pass through an island.
- Collision behavior remains valid after terrain tile streaming.
- There is no persistent collision-induced stationary state.

DO NOT PROCEED TO OTHER GAMEPLAY FEATURES BEFORE THIS PHASE IS VERIFIED.

---

# PHASE 4 — VERIFY ALL FIVE AI SHIPS MOVE ON THE NEW TERRAIN

Once collision and Cesium issues are resolved, test all five AI ships.

The existing setup contains:

BLUE:
- One player.
- Two friendly AI.

RED:
- Three enemy AI.

Each AI already has a two-point A/B patrol assignment or an equivalent existing configuration.

Reuse the saved setup.

## 4.1 Verify independent navigation

For each AI:

- Read PatrolPointA.
- Read PatrolPointB.
- Generate geographic A*.
- Follow waypoints.
- Apply throttle/rudder.
- Physically move.
- Reach the current patrol destination.
- Switch to the opposite endpoint.
- Recalculate a new route.
- Continue the loop.

Do not share mutable route arrays or waypoint indexes across different ships.

Do not replace A* with straight-line movement.

## 4.2 Verify physical speeds

Target gameplay tuning:

Normal cruise: approximately 18–22 m/s.

Maximum speed: approximately 26 m/s.

Sharp turn: approximately 7–12 m/s.

Reverse retreat should be slower than forward movement.

These are measured velocity targets, not raw force multipliers.

Do not blindly increase AddForce to hide ground collision.

## 4.3 Mandatory runtime telemetry

For every ship, sample:

- Simulation time.
- Actor location.
- Geographic coordinates.
- Current speed.
- Forward speed.
- Current throttle.
- Current rudder.
- Current path.
- Current waypoint.
- Distance traveled.
- Current target.
- Collision status.
- Buoyancy status.
- Navigation state.

Check progress at multiple simulated times, for example:

0 seconds.

5 seconds.

15 seconds.

30 seconds.

60 seconds.

120 seconds, when practical.

The tests must allow actual Unreal simulation time to advance.

No claim of movement success may be based solely on nonzero throttle or a valid A* route.

## 4.4 Full patrol soak

Run the full six-ship level long enough to verify:

- All five AI ships physically move.
- Their routes remain valid.
- No repeated grounding.
- No permanently stationary living ship.
- A/B destination switching occurs.
- Island avoidance works.
- Combat does not permanently block navigation.
- Player control remains functional.

If an endpoint cycle cannot be completed in the available runtime, report the actual progress and do not claim the cycle completed.

## PHASE 4 GATE

All five AI ships demonstrate sustained real physical movement.

At least representative A/B switches are verified, with remaining routes checked through actual runtime progress.

The ship movement remains correct after a fresh PIE restart.

---

# PHASE 5 — REGRESSION TEST THE PREVIOUS GAMEPLAY

Preserve and verify:

- Player ship movement.
- Gun aiming.
- Front turret animation.
- Projectile spawning.
- Friendly-fire restrictions.
- Automatic AI targeting.
- AI projectile dispersion.
- Easy/Normal/Hard difficulty.
- Bow/midship/stern damage.
- Hit-and-run behavior.
- Reverse retreat.
- Low-health retreat.
- Passive/Fighter/Hunter role transitions.
- Large/Small ship physics profiles.
- Capsize detection.
- Health/death handling.
- Minimap.
- CustomPlayerController.
- FreeCameraPawn.
- Spectator functionality already present.

Damage defaults:

MaxHealth = 100.

Bow damage = 8.

Midship damage = 10.

Stern damage = 8.

If a feature exists and already passes tests, do not recreate it.

If a feature is missing from the current branch, inspect available progress and previous commits before implementing it again.

## PHASE 5 GATE

No major regression is introduced by the Cesium and terrain collision fixes.

---

# PHASE 6 — FINISH FIRE, SMOKE, AND SINKING VFX

The previous Agent reported:

- Sinking works.
- Cleanup works.
- Capsized wrecks preserve their roll.
- Wrecks maintain geographic positioning.
- Fire and smoke Niagara simulation runs.
- However, fire and smoke are barely visible in screenshots.

Inspect the current saved implementation.

Preserve working sinking.

Fix or replace only the problematic visual part.

Required destruction sequence:

ALIVE
→ DESTROYED
→ BURNING AND SMOKING
→ GRADUAL SINKING
→ SUBMERGED
→ CLEANUP

The ship must not disappear immediately.

Verify the Niagara effect in real PIE.

Inspect:

- Sprite size.
- Material opacity.
- Sprite facing.
- Emitter bounds.
- Renderer configuration.
- Particle color.
- Local/world space.
- Attachment transforms.
- Camera distance.

If the existing Niagara asset is unsuitable, use a lightweight compatible fallback.

Avoid repeated unlimited diagnostics on the same VFX issue.

## PHASE 6 GATE

Either:

A. Fire/smoke are visibly working in real PIE and sinking is verified.

Or:

B. The remaining visual blocker is documented precisely while the already-working sinking system remains intact.

Do not claim a successful VFX visual test unless the effect is actually visible.

---

# PHASE 7 — FINISH PLAYER DEATH AND SPECTATOR

When the player dies:

- Stop movement.
- Stop firing.
- Start destruction/sinking.
- Automatically switch to spectator.
- Follow a living BLUE teammate.
- Allow teammate switching.
- Allow whole-map overview.
- Keep all AI ships operating.
- Do not steal possession from AIControllers.

Verify integration with CustomPlayerController and FreeCameraPawn.

Remember that F8 may be used by Unreal Editor for PIE Eject.

Use reliable game input mappings for spectator controls.

Suggested:

Tab: Next teammate.

M: Battlefield overview.

## PHASE 7 GATE

The player can die, watch teammates, switch camera views, and observe the ongoing battle without stopping AI.

---

# PHASE 8 — FINISH OPTIONAL TURN-BASED MODE

Inspect whether the system is already implemented.

Default gameplay remains REAL_TIME.

Optional TURN_BASED mode:

- BLUE and RED alternate.
- Each living ship receives a 10-second action window.
- Active ship may move and fire.
- Inactive ships cannot voluntarily move or fire.
- Physics continues.
- Existing projectiles continue.
- Sinking VFX continue.
- Dead ships are skipped.
- Team elimination ends the battle.

Reuse existing GameMode/GameState systems.

Do not replace real-time gameplay.

Test both modes.

---

# PHASE 9 — FINISH PRE-MATCH SETTINGS

Implement or complete pre-match configuration:

BLUE ships: 1–3.

RED ships: 1–3.

Exactly one BLUE ship is player-controlled.

Remaining ships are AI-controlled.

Ship classes:

- Large.
- Small.

AI difficulty:

- Easy.
- Normal.
- Hard.

Game mode:

- Real-Time.
- Turn-Based.

Default:

3 BLUE vs 3 RED.

Real-Time.

Normal difficulty.

100 HP.

10 base projectile damage.

Use the already-existing ship assets as placeholders for Large and Small.

Ensure the match starts with the correct number of ships and valid A/B navigation routes.

Preserve direct PIE startup with development defaults.

---

# PHASE 10 — FINAL FULL-BATTLE TEST

Run an integrated real PIE test on the actual restored QuangNinh terrain.

Verify:

1. QuangNinh terrain loads.
2. Imagery loads.
3. Islands and water align.
4. The correct CesiumGeoreference is active.
5. Ship anchors are valid.
6. No conflicting origin shifting.
7. Ship hulls do not ground on artificial underwater terrain.
8. Islands remain physically obstructive.
9. Exactly six default combat ships initialize.
10. All five AI ships move physically.
11. All AI ships use geographic A*.
12. AI A/B patrol switching works.
13. AI attack behavior works.
14. Projectile firing works.
15. AI shots can miss.
16. Correct positional damage is applied.
17. Reverse retreat functions.
18. Large/Small profiles function.
19. Capsized ships die.
20. Destroyed ships sink.
21. Fire/smoke display when verified.
22. Player spectator mode functions.
23. Minimap functions.
24. Camera switching preserves AI possession.
25. Both game modes work if implemented.
26. Pre-match settings correctly configure ships if implemented.
27. No major new runtime errors occur.

Do not substitute static inspection for real PIE validation.

Distinguish actual tested features from unverified ones.

---

# PHASE 11 — SAVE, DOCUMENT AND COMMIT

Update:

Docs/NavalBattleV2_Progress.md

Docs/QuangNinhBattle_Continuation.md

Create a concise technical report for the terrain and Cesium repair.

Suggested:

Docs/QuangNinhIntegration/CESIUM_TERRAIN_COLLISION_FIX.md

Document:

- Root cause of stationary ships.
- Cesium components before and after.
- Georeference configuration.
- Origin-shifting policy.
- Terrain collision findings.
- Final collision approach.
- Why island collision remains functional.
- Water and buoyancy configuration.
- A* regression results.
- Five-AI movement telemetry.
- Full PIE results.
- Remaining issues.

Compile C++.

Compile affected Blueprints.

Save modified assets.

Restart PIE using the saved state.

Verify the fixes persist.

Create a focused Git commit for the completed Cesium/terrain correction.

Create additional commits for independently completed gameplay features where appropriate.

Do not force push.

Do not modify main directly.

Do not automatically merge a new PR into main.

Preserve the QuangNinh source dataset outside Git unless repository policy explicitly requires otherwise.

---

# AUTONOMOUS WORKFLOW

For EVERY phase:

INSPECT
→ REPRODUCE
→ IDENTIFY ROOT CAUSE
→ IMPLEMENT
→ COMPILE
→ AUTOMATED TEST
→ REAL PIE TEST
→ FIX
→ RETEST
→ SAVE
→ UPDATE PROGRESS
→ NEXT PHASE

Do not ask routine implementation questions.

Do not restart the project from scratch.

Do not create duplicate navigation or combat systems.

Do not rewrite the teammate's terrain.

Do not hide physics errors by teleporting ships.

Do not declare movement successful based only on A* path generation.

Do not declare fire/smoke successful based only on Niagara particle counts.

If a critical test fails, diagnose and repair it before marking the phase passed.

If a genuine blocker remains, document it and preserve completed work.

---

# FINAL SUCCESS CRITERIA

When I press Play in MainMapLevel:

- The new QuangNinh terrain displays correctly.
- Six combat ships appear in the configured formation.
- Ships have correct geographic positions.
- All five AI ships physically move.
- They follow valid A* routes through the bay.
- They do not pass through islands.
- They do not become grounded on artificial sea-level terrain.
- They preserve water physics and buoyancy.
- They automatically patrol between A and B.
- They engage enemy ships while moving.
- They execute hit-and-run or retreat when appropriate.
- Projectiles apply correct damage.
- Capsized or destroyed ships enter the sinking sequence.
- The player can spectate surviving teammates.
- Existing minimap and camera functionality remain working.
- Optional game modes and pre-match configuration function when completed.
- All changes survive stopping and restarting PIE.

**TOP PRIORITY: FIX THE ACTUAL STATIONARY SHIP PROBLEM ON QUANGNINH TERRAIN BEFORE IMPLEMENTING ANY ADDITIONAL GAMEPLAY FEATURES.**

Begin with the existing grounding diagnostic and official Cesium documentation.

Then inspect the project's current collision configuration, restore reliable physical naval movement, and continue the remaining Naval Battle v2 phases autonomously.