# MASTER DEVELOPMENT TASK — NAVAL BATTLE 3v3: COMPLETE GAMEPLAY, TACTICAL AI, SHIP DESTRUCTION VFX, AND AUTOMATED VERIFICATION

## 0. MISSION AND OPERATING INSTRUCTIONS

You are the lead autonomous Unreal Engine development Agent working directly on my existing project.

**Project:** VDTPhase2Project  
**Engine:** Unreal Engine 5  
**Main Level:** MainMapLevel  
**Available Integration:** SpecialAgent MCP localhost:8767, Unreal Editor, C++, Blueprint editing, Python or editor automation where available.

Your mission is to transform the current naval combat prototype into a polished, configurable, playable 3v3 naval battle while **preserving its existing working systems**.

This is an implementation task, not a request for an architecture proposal.

You are authorized to inspect project files, modify relevant source code and Blueprint assets, configure level actors, compile, save, execute automated tests, start and stop Play In Editor (PIE), collect runtime telemetry, diagnose failures, and perform corrective iterations.

### AUTONOMY REQUIREMENTS

- Work autonomously without asking routine questions.
- Inspect the project to resolve implementation details.
- Choose reasonable defaults where unspecified.
- Reuse working systems rather than recreating them.
- Complete implementation phases sequentially.
- Validate each critical phase before moving forward.
- Automatically fix detected failures and retest.
- Preserve user modifications and existing assets.
- Do not terminate other active Agent processes without permission.
- Do not perform destructive source-control operations.
- Do not invent test results.
- Report genuine tool limitations accurately.

If a feature already exists and works correctly, verify and extend it rather than replacing it.

**Never treat compilation success as proof that the gameplay works.**

---

# PHASE 1 — FULL PROJECT AUDIT AND BASELINE STABILIZATION

## 1.1 Inspect the existing architecture

Before modifying any files, inspect the current implementation of:

### Ship systems

- BP_Ship
- BP_ShipEnemy
- ABP_Ship
- Existing ship C++ classes
- Ship movement and physics components
- Buoyancy
- GunYaw / GunPitch
- Projectile and firing functions
- Health and damage
- Ship destruction
- Ship camera components

### AI systems

- Existing AIController
- EnemyNavigation
- SeaNavigationMap
- Geographic A* pathfinding
- Existing AI tactical states
- Hit-and-run implementation
- Patrol route generation
- Obstacle avoidance
- AI target acquisition
- AI combat logic

### Battle systems

- Team affiliation
- GameMode and GameState
- PlayerController
- Spawn management
- HUD and health bars
- Team counters
- Match state
- Player death handling
- Spectator behavior

### World systems

- CesiumGeoreference
- CesiumGlobeAnchor
- CesiumOriginShift
- Unreal Water
- WaterBodyOcean
- World Partition
- Island geographic polygon data
- Existing Target Points

Determine which features are fully working, partially implemented, missing, or broken.

Create an implementation checklist based on the ACTUAL project state.

## 1.2 Preserve existing work

Inspect Git status and editor dirty assets.

Preserve all unrelated user modifications.

Do not reset, revert, force checkout, or overwrite existing changes.

Check for another active Agent controlling SpecialAgent MCP before writing.

Use one exclusive writer for Unreal Editor modifications.

## 1.3 Establish the baseline

Run the current game in PIE.

Record:

- Number of ships
- Team affiliation
- AI possession
- Navigation initialization
- Actual ship velocities
- Current health
- Combat activity
- Existing runtime errors
- Current spectator behavior

Save these findings in:

`Docs/NavalBattleV2_Progress.md`

Use this document as a persistent progress ledger for the entire task.

### PHASE 1 GATE

Do not proceed until the existing gameplay systems and major current failures are documented.

---

# PHASE 2 — FIX AI MOVEMENT AND PRESERVE EXISTING A* NAVIGATION

**This phase has the highest priority.**

The game must not have AI ships remaining motionless for no valid reason.

## 2.1 Fix stationary AI ships

Every living AI ship assigned to patrol must be capable of moving automatically.

For every AI instance, verify:

- AIController possession
- Auto Possess AI configuration
- Correct AI-controlled state
- Navigation component activation
- Valid A/B patrol references
- Geographic coordinate conversion
- A* route generation
- Path waypoint count
- Current waypoint index
- Tactical-state authorization to move
- Desired throttle
- Desired rudder
- SetDriveCommand execution
- Actual AddForce-based propulsion
- Physics simulation
- Ship collision
- Buoyancy interaction
- World Partition loading
- Cesium origin-shift correctness

Trace the full chain:

Navigation Goal
→ A* Waypoints
→ Steering Controller
→ SetDriveCommand
→ Physics Propulsion
→ Actual Ship Movement

Determine whether any stationary ship suffers from:

- Missing route
- Invalid Target Point
- Shared navigation state
- Premature arrival detection
- Stale coordinate transforms
- Permanent braking
- Zero throttle
- Conflicting navigation commands
- Incorrect tactical transitions
- Physical obstruction
- Incorrect buoyancy or collision

Correct the root cause.

Do not use teleportation to hide navigation failures.

## 2.2 Independent patrol routes

Default match:

BLUE:
- One player-controlled ship
- Two AI-controlled ships

RED:
- Three AI-controlled ships

Each AI must have its own two-point patrol assignment.

Suggested endpoint naming:

BLUE_AI_01:
- TP_Blue01_A
- TP_Blue01_B

BLUE_AI_02:
- TP_Blue02_A
- TP_Blue02_B

RED_AI_01:
- TP_Red01_A
- TP_Red01_B

RED_AI_02:
- TP_Red02_A
- TP_Red02_B

RED_AI_03:
- TP_Red03_A
- TP_Red03_B

Reuse appropriate existing Target Points instead of duplicating them unnecessarily.

Each ship independently follows:

A → B → A → B → ...

Every change of destination must use the existing geographic A* system.

No shared mutable waypoint indexes or route arrays between AI instances.

## 2.3 Battlefield region

Use the following four Unreal Editor world-space reference coordinates, in centimeters, to define the intended compact battle region.

Corner 1:

X = 8339.935681  
Y = 305820.231920  
Z = 626.169385

Corner 2:

X = 138490.097508  
Y = 231430.109534  
Z = 651.177124

Corner 3:

X = -34766.365371  
Y = 116668.053293  
Z = 611.803141

Corner 4:

X = 72699.096226  
Y = 64133.062018  
Z = 634.787785

Treat them as a reference quadrilateral, not mandatory waypoints.

Order the polygon vertices geometrically and verify its validity.

Inspect actual coastline, island polygons, water depth, and navigable areas.

The default battlefield layout should resemble a MOBA-style naval encounter:

- Three BLUE ships begin near one another on the friendly/bay side.
- Three RED ships begin near one another on the opposite side.
- AI routes travel toward the opposing side of the map.
- Opponents naturally meet in the central combat area.
- Routes have lateral separation to reduce collisions.

Automatically choose safe spawn and patrol locations using the existing geographic A* and polygon constraints.

Do not place ships on land.

Do not create unnecessarily long ocean-crossing patrol routes.

Persist level placements and patrol references.

## 2.4 Speed tuning

The current BP_Ship uses physics-based propulsion, including AddForce.

An existing propulsion multiplier was previously changed from approximately 320000 to 700000.

Inspect the actual current code.

This is a force multiplier, NOT a speed in meters per second.

Target arcade gameplay speeds:

- Normal cruise: approximately 20 m/s
- Cruise range: 18–22 m/s
- Maximum forward speed: approximately 26 m/s
- Sharp turns: 7–12 m/s
- Fast tactical disengagement: up to approximately 26 m/s

Tune throttle, acceleration, damping, turning response, and propulsion according to actual ship mass and physics performance.

Do not continuously override physics linear velocity.

Preserve buoyancy and player movement.

### PHASE 2 TESTS

Run combat-disabled PIE tests.

For every AI:

1. Confirm successful initialization.
2. Confirm a valid A* route.
3. Record initial position.
4. Record throttle/rudder.
5. Record actual physical speed.
6. Verify meaningful movement.
7. Verify waypoint progress.
8. Verify correct A/B switching.
9. Verify island avoidance.
10. Verify the ship is still moving after a new PIE session.

Then reenable combat and verify navigation continues.

**GATE: Every active AI ship must demonstrate actual physical movement before advancing.**

---

# PHASE 3 — UNIFIED TACTICAL AI STATE MACHINE

Reuse the existing AI state machine wherever possible.

Do not create separate competing tactical systems.

Implement or consolidate the following states:

- PASSIVE
- PATROL
- FIGHTER
- HUNTER
- ENGAGE
- BACKWARD_RETREAT
- ESCAPE
- RETURN_TO_PATROL
- DEAD

If the existing project uses a separate role enum and state enum, preserve that architecture.

Roles define intended long-term behavior.

States define current tactical actions.

## 3.1 Passive behavior

Passive AI:

- Navigates normally.
- Does not initiate combat without permission.
- Does not fire automatically.
- Can change to Fighter when a configured activation condition occurs.

Supported triggers:

- Receiving hostile damage
- Explicit tactical command
- A configurable threat alert

Do not automatically promote every passive unit just because any enemy exists anywhere in the map.

## 3.2 Fighter behavior

Fighter AI:

- Continues normal A/B navigation.
- Detects hostile ships.
- Tracks targets with the existing turret system.
- Fires within effective engagement range.
- Keeps moving while attacking.
- Avoids unnecessarily stopping for combat.
- Retreats under appropriate danger conditions.

This is the default combat role.

## 3.3 Hunter behavior

Hunter AI:

- Prioritizes hostile target acquisition.
- Can use geographic A* for pursuit.
- Must still avoid islands.
- Must respect engagement and disengagement ranges.
- Must not pursue indefinitely.
- Can transition to Fighter when pursuit becomes inappropriate.

Examples:

- Target becomes too distant.
- Target is lost.
- Hunter health is low.
- The ship returns to a normal combat patrol area.

## 3.4 Dynamic role transitions

Implement configurable role transitions:

Passive → Fighter

Hunter → Fighter

Fighter → Retreat state

Retreat → Fighter or Patrol

Do not delete or overwrite patrol endpoint references when changing roles.

Use explicit transition criteria, hysteresis, and minimum state durations.

Avoid state flickering.

## 3.5 One movement authority

Only one active navigation/tactical controller may issue final throttle and rudder commands per AI ship.

Combat aiming must operate independently.

Preserve:

SetDriveCommand(Throttle, Rudder)

Do not allow navigation, combat, and retreat components to overwrite one another unpredictably.

### PHASE 3 TESTS

Verify all roles independently.

Verify role transitions using actual runtime events.

Verify transitions do not reset the A/B route.

Verify that AI resumes movement after losing a target.

Verify that no AI becomes permanently motionless after a tactical state change.

---

# PHASE 4 — LOW-HEALTH RETREAT AND REVERSE-AND-FIRE

Implement a tactical retreat system suitable for a naval combat game.

The AI should not always turn 180 degrees and flee.

It must support retreating backward while keeping its gun oriented toward the enemy.

## 4.1 Low-health trigger

Default parameters:

MaxHealth = 100

LowHealthRetreatThreshold = 30 HP

CriticalHealthThreshold = 15 HP

These values must be configurable.

At health <= 30, a Fighter AI should prefer retreating when an appropriate safe escape is available.

Do not automatically retreat if the ship is dead or the tactical mode forbids movement.

Prioritize survival at critical health.

## 4.2 Backward retreat

Implement a dedicated BACKWARD_RETREAT behavior.

The intended result:

1. Detect dangerous enemy proximity or low health.
2. Determine a navigable safe retreat direction.
3. Keep the bow generally facing the hostile threat when feasible.
4. Apply reverse throttle.
5. Move backward physically.
6. Continue aiming and firing when the weapon has a valid firing solution.
7. Maintain safe separation.
8. Stop retreating when safe conditions are satisfied.
9. Rejoin an appropriate tactical state.

The ship must actually move backward through the water, not simply turn around and accelerate forward.

## 4.3 Reverse propulsion must be weaker

The reverse propulsion system must have lower acceleration than forward propulsion.

Initial suggested values:

ReverseForceRatio = 0.45

ReverseAccelerationRatio = 0.45

ReverseMaxSpeed = 7 m/s

ForwardMaxSpeed = 26 m/s

These values are tuning targets.

Inspect the existing AddForce implementation before applying them.

Do not multiply acceleration reductions twice accidentally.

Tune using actual physics measurements.

The desired relationship is:

Reverse acceleration < Forward acceleration

The ship must reverse smoothly without suddenly teleporting or snapping backward.

## 4.4 Reverse steering

When reversing, conventional steering response may behave differently.

Inspect the existing rudder and force-at-location logic.

Ensure yaw control is correct under negative forward velocity.

Avoid reversed steering signs that make the AI turn toward land or become unstable.

Use measured signed forward speed and actual desired reverse direction.

## 4.5 Reverse-and-fire behavior

During retreat:

- Keep tracking the selected hostile ship.
- Maintain turret aim independently.
- Fire only when aligned and within valid range.
- Respect line of sight.
- Respect projectile cooldown.
- Respect friendly-fire rules.
- Stop firing if the muzzle is obstructed.
- Do not override retreat steering with combat aiming.

The AI must be capable of shooting while physically moving backward.

## 4.6 Geographic retreat safety

Before reverse movement, validate the intended path behind the ship.

Use hull-aware clearance checks.

Do not assume the water behind the ship is safe simply because the bow has a clear path.

Check:

- Nearby islands
- Coastal clearance
- Other ships
- Hull collision
- Water navigation constraints

If backward retreat is unsafe:

1. Attempt a short bounded maneuver.
2. Consider a safe turn.
3. Use geographic A* to reach a safe escape route.
4. Fall back to forward escape if necessary.

Do not force backward movement through an island.

Do not repeatedly reverse into a collision.

## 4.7 Retreat target selection

Use the actual map rather than a hardcoded retreat direction.

Choose temporary safe-water destinations that:

- Increase distance from the threat.
- Have reachable geographic A* paths.
- Avoid land.
- Maintain physical maneuvering room.
- Do not create unnecessary cross-map escapes.

Do not regenerate retreat destinations every frame.

Apply a minimum commitment duration and configurable timeout.

## 4.8 Recovery

When safely disengaged:

- Return to Fighter or Patrol according to the current role.
- Recalculate the original A/B route from the CURRENT location.
- Resume movement.
- Preserve the original patrol references.
- Do not teleport.

### PHASE 4 TESTS

Test:

- Retreat at 30 HP.
- No premature retreat above threshold without another threat trigger.
- Reverse speed and acceleration.
- Turret tracking during reverse movement.
- Actual projectile firing while reversing.
- Retreat near an island.
- Retreat with another ship behind.
- Unsafe reverse path fallback.
- Reentry into normal patrol.
- Prevention of state oscillation.

**GATE: At least one real PIE test must demonstrate reverse physical movement while the turret remains capable of engaging the target.**

---

# PHASE 5 — REALISTIC AI AIMING AND PROJECTILE ACCURACY

The current AI appears too accurate.

Make AI shooting imperfect but mechanically correct.

Reuse the existing:

- BP_Ship firing functions
- ABP_Ship
- front_gun_h_jnt
- front_gun_v_jnt
- front_gun_v_socket
- Projectile Blueprints
- Existing projectile speed and gravity
- Firing cooldown
- Niagara muzzle effects
- Recoil animation

Do not create a duplicate weapon system.

## 5.1 AI aiming

Compute a valid target direction or ballistic interception solution using:

- Current target position
- Target velocity
- Firing ship velocity
- Muzzle position
- Projectile speed
- Projectile gravity behavior

The turret must aim through its actual skeletal joints.

Respect mechanical yaw and pitch limits.

## 5.2 Introduce configurable accuracy

Expose:

AIAccuracy

AimErrorDegrees

TargetTrackingDelay

AimUpdateInterval

DistanceAccuracyPenalty

MovementAccuracyPenalty

OptionalSuppressionPenalty

Default Fighter AI should have moderate accuracy, not perfect precision.

Use a combination of:

- Angular aiming error
- Target motion estimation error
- Limited reaction time
- Tracking delay
- Range-dependent dispersion

Prefer physically plausible, bounded angular error applied to the projectile firing solution.

Do not randomly alter the projectile's hit result after impact.

Do not teleport bullets toward enemies.

Do not artificially guarantee hits.

## 5.3 Skill presets

Create three configurable AI shooting skill presets:

Easy:
- Slower target acquisition
- More dispersion
- Lower effective accuracy

Normal:
- Moderate reaction time
- Moderate aiming error

Hard:
- Faster acquisition
- Smaller aiming error
- Better target leading

Use Normal as the default.

Do not interpret skill presets as guaranteed hit probabilities.

Actual hit rates should emerge from physical aiming and projectile simulation.

## 5.4 Combat range

Inspect the size of the current map.

Use appropriate detection and engagement ranges.

Check:

- Target distance
- Line of sight
- Turret firing arc
- Barrel alignment
- Projectile range
- Cooldown
- Team affiliation
- Projectile collision

Use bounded traces and do not allow projectiles to pass through islands.

### PHASE 5 TESTS

Use controlled test scenarios with stationary and moving targets.

Measure:

- Total shots fired
- Valid shots
- Hits
- Misses
- Hit ratio
- Distance to target
- Target speed
- AI difficulty

Run repeatable seeded tests when practical.

Verify that AI can genuinely miss a target.

Verify that the projectile still follows the actual muzzle direction and physics.

Verify Hard is generally more accurate than Easy over a meaningful repeated sample, without relying on fabricated percentages.

---

# PHASE 6 — POSITION-BASED SHIP DAMAGE

Implement positional damage based on the projectile's actual hull impact location.

Every ship has:

MaxHealth = 100 HP

BaseProjectileDamage = 10 HP

**These values are the final requested defaults and supersede the older 2-damage gameplay preset.**

## 6.1 Damage regions

Divide the physical hull into three configurable longitudinal regions:

BOW / FRONT:
80% base damage

MIDSHIP / CENTER:
100% base damage

STERN / REAR:
80% base damage

Therefore:

| Impact Region | Multiplier | Damage |
|---|---:|---:|
| Bow | 0.8 | 8 HP |
| Midship | 1.0 | 10 HP |
| Stern | 0.8 | 8 HP |

## 6.2 Determine impact position correctly

Do not hardcode global-world axis thresholds.

Use the actual hit location and ship hull transform.

Convert the projectile impact point into the physical hull's local coordinate space.

Determine the ship's verified local longitudinal axis.

If the hull's forward axis is local X, use normalized local X position relative to the hull bounds.

Verify the actual mesh orientation before implementing this assumption.

Suggested initial normalized boundaries:

Bow:
Front 25% of hull length

Midship:
Central 50%

Stern:
Rear 25%

Make proportions configurable.

Avoid classifying hits based on unrelated bounding boxes, deck decoration, or arbitrary world coordinates.

## 6.3 Centralized damage processing

Use one authoritative damage pipeline.

Suggested conceptual flow:

Projectile collision
→ Identify hit ship
→ Validate teams
→ Validate hit
→ Determine local hull impact region
→ Resolve base damage
→ Apply region multiplier
→ Apply health reduction
→ Update UI
→ Check death

Ensure each physical projectile impact applies damage only once.

Prevent friendly fire.

Prevent self-damage.

Prevent damage to already destroyed ships.

Keep damage logic compatible with future ship classes.

## 6.4 Debug feedback

When damage debugging is enabled, log:

- Shooter
- Victim
- Projectile identity
- Impact world location
- Impact local location
- Region
- Damage multiplier
- Final damage
- Remaining health

Optionally display a brief floating damage number.

Avoid excessive debug rendering in normal gameplay.

### PHASE 6 TESTS

Test controlled hull hits:

- Bow hit → 8 damage.
- Midship hit → 10 damage.
- Stern hit → 8 damage.
- Team-friendly collision → no friendly damage.
- Self-collision → no accidental damage.
- Repeated collision callbacks → only one applied hit.
- Health reaching zero → one death event.

Verify using actual health values, not only visual effects.

---

# PHASE 7 — TWO SHIP CLASSES WITH REUSABLE PHYSICS PROFILES

I do NOT currently have separate 3D assets for large and small ships.

Implement the gameplay architecture now, but do not create new 3D ship models.

Reuse the existing ship model temporarily as a placeholder for both types.

The ship class system must support future asset replacement.

## 7.1 Ship classes

Create or reuse a ship-class configuration structure.

Required types:

LARGE_SHIP

SMALL_SHIP

Use Data Assets, data-driven configuration structures, or the current project architecture.

Avoid duplicating entire ship Blueprints unnecessarily.

## 7.2 Large ship profile

Intended characteristics:

- Greater physical mass.
- Greater rotational inertia.
- Slower acceleration.
- Lower turning responsiveness.
- More stable roll and pitch behavior.
- Slower response to wave disturbances.
- More deliberate, heavy movement.
- Appropriate buoyancy displacement.

Large ships should feel heavy and stable.

Do not simulate this simply by freezing rotation or adding excessive damping.

## 7.3 Small ship profile

Intended characteristics:

- Lower mass.
- Faster acceleration.
- Faster turning response.
- Lower rotational inertia.
- More responsive to waves.
- More visible roll and pitch.
- Better maneuverability.

Small ships should feel agile, but must not randomly flip over during ordinary navigation.

## 7.4 Buoyancy integration

Inspect the existing Buoyancy component and physics mesh.

Configure class-specific physical parameters such as:

- Mass
- Center of mass
- Pontoon configuration
- Pontoon radius
- Buoyant displacement
- Linear damping
- Angular damping
- Roll stability
- Pitch stability
- Propulsion
- Rudder response

Do not use arbitrary scaling of the skeletal mesh to simulate different hull classes.

Because the current mesh is shared, treat the initial profiles as approximate placeholders.

Avoid claiming realistic large/small hydrodynamics without actual appropriately shaped collision hulls.

Verify compatibility with Unreal Water and Cesium.

Keep actual water collision and origin-shift behavior functional.

## 7.5 Data-driven future asset support

Each class should be able to define:

- Skeletal mesh
- Animation Blueprint
- Physics asset
- Hull collision configuration
- Buoyancy setup
- Movement parameters
- Camera offset
- Weapon parameters

When I later import new ship assets, I should be able to assign them to the ship-class configuration without rewriting AI navigation or combat behavior.

## 7.6 Default gameplay balance

For now:

- Keep MaxHealth = 100 for both classes.
- Keep BaseProjectileDamage = 10.
- Keep the same positional damage multipliers.
- Differentiate classes primarily through movement and buoyancy.

Do not introduce unnecessary armor or health differences until those systems are explicitly requested.

### PHASE 7 TESTS

Spawn both classes with the current placeholder mesh.

Verify:

- Both initialize correctly.
- Both float.
- Both can navigate through A*.
- Both can fire.
- Both can take damage.
- Large class has measurably lower angular responsiveness.
- Small class accelerates and turns more readily.
- Neither class is unstable during normal cruise.
- Both remain compatible with death and sinking systems.

If the existing hull/collision asset prevents meaningful buoyancy differentiation, preserve a safe baseline, report the constraint, and complete the reusable class architecture.

---

# PHASE 8 — CAPSIZING, SHIP DEATH, FIRE VFX, AND REALISTIC SINKING

**This phase must replace the current behavior where destroyed ships immediately disappear.**

When a ship dies, it should visibly burn, smoke, and gradually sink into the sea.

The process must work for:

- Player ships
- Friendly AI
- Enemy AI
- Large ship class
- Small ship class

Reuse existing Niagara, particle, sound, and mesh assets where possible.

Do not depend on paid plugins or unavailable external assets.

## 8.1 Unified death system

Inspect the existing health/death logic.

Implement one authoritative death function or reuse the current one.

Suggested API:

Die(DeathReason)

Supported death reasons:

- HealthDepleted
- Capsized
- Other existing valid death reasons

Ensure death is processed exactly once.

When death starts:

- Mark the ship dead immediately.
- Stop its AI navigation.
- Stop movement commands.
- Disable weapon firing.
- Remove it from target acquisition.
- Apply the team counter update once.
- Notify GameMode/GameState.
- Trigger spectator transition if it was the player ship.
- Start the destruction visual sequence.
- Preserve the hull actor temporarily for the sinking animation.

Do not immediately destroy the Actor.

## 8.2 Capsize detection

Use the actual physical hull orientation.

Compare ship local up with the correct Cesium geographic up direction.

Do not rely blindly on global Unreal Z.

Suggested initial threshold:

Capsize angle >= 105 degrees.

Confirmation duration:

0.5–0.8 seconds.

Make both parameters configurable.

Ordinary wave movement and brief extreme roll should not immediately kill a ship.

A confirmed capsize must trigger the same death sequence as health depletion.

## 8.3 Destruction visual state machine

Implement a simple destruction presentation state machine:

ALIVE
→ DYING
→ BURNING
→ SINKING
→ SUBMERGED
→ CLEANUP

Gameplay death must occur at the beginning of DYING.

The remaining states are visual presentation stages.

A burning or sinking ship is already dead and must not continue fighting.

## 8.4 Initial impact and fire effects

On destruction:

- Trigger an appropriate impact/explosion effect.
- Spawn fire near suitable hull or superstructure locations.
- Spawn rising smoke.
- Optionally spawn sparks or small secondary explosions.
- Play destruction audio if existing suitable assets are available.
- Optionally add localized damage material effects when feasible.

Use Niagara where appropriate.

Attach effects to suitable mesh components or sockets so the effects follow the sinking ship.

Avoid putting the entire fire effect at world origin.

Do not spawn hundreds of expensive particle emitters.

Do not use permanently persistent debug VFX.

If dedicated high-quality fire or smoke assets are unavailable, implement a lightweight functional effect using existing engine/project content and report any visual limitations.

Do not delay the entire task waiting for unavailable art assets.

## 8.5 Burning stage

Suggested default duration:

3–6 seconds.

During this period:

- The ship remains visibly present.
- Flames are active.
- Smoke rises.
- The ship loses its combat capability.
- The hull may begin to list or settle.
- The camera can still observe the wreck.
- The battle continues around it.

Expose the burning duration in configuration.

The ship should not instantly vanish.

## 8.6 Gradual sinking

After the burning stage begins, transition into gradual sinking, allowing visual stages to overlap where appropriate.

Suggested sinking duration:

8–15 seconds.

The ship must move progressively beneath the water surface.

Requirements:

- No instant disappearance.
- No abrupt teleport below the map.
- No extreme physics explosion.
- No infinite falling caused by unrelated Cesium transform errors.
- No accidental reactivation of AI movement.
- No repeated death callbacks.

Choose a stable implementation compatible with the existing buoyancy and physics architecture.

Possible approaches include:

- Gradually reducing or disabling relevant buoyant support.
- Applying controlled sinking forces.
- Adding carefully constrained angular listing behavior.
- Transitioning into a dedicated physics-compatible wreck state.

Inspect the project before selecting the method.

Do not directly fight an active buoyancy solver with conflicting transform assignments every Tick.

If the sinking sequence uses kinematic animation, explicitly transition the hull into a nonconflicting physics state before using it.

## 8.7 Cesium and water correctness

The project uses Cesium.

Determine the correct geographic down direction.

Do not assume that lowering world Z always represents physically correct sinking under all origin-shift conditions.

Water level and geographic orientation may vary.

Ensure the wreck remains visually aligned with the ocean during Cesium origin shifts.

Avoid world-origin-related sinking bugs.

## 8.8 Capsized ship sinking

When death occurs due to capsizing:

- Preserve the visible rolled-over orientation.
- Start fire/smoke where appropriate.
- Allow the overturned hull to sink.
- Avoid artificially rotating it upright before the sinking sequence.

The destruction must look visually consistent with the original accident.

## 8.9 Submerged wreck and cleanup

Once the wreck is sufficiently submerged:

- Gradually stop fire effects.
- Allow smoke to fade.
- Stop unnecessary particle systems.
- Disable expensive collision when no longer relevant.
- Hide, pool, or destroy the wreck actor only after presentation is complete.
- Release timers and component references safely.

Use a configurable cleanup delay.

Prevent accumulation of invisible wreck actors and particle components across many battles.

Do not destroy a wreck while the spectator camera still directly references it without first switching the camera.

## 8.10 Optional finishing polish

If suitable existing assets are available, also add:

- A short explosion flash.
- Secondary sparks.
- Small surface splash effects.
- A water disturbance around the sinking hull.
- A brief destruction sound.
- Gradual disappearance beneath the waves.

These are secondary to producing a stable functional burning-and-sinking sequence.

Do not introduce external assets that cannot be licensed or accessed.

### PHASE 8 TESTS

Run real PIE tests for:

1. Health depletion death.
2. Capsize death.
3. Fire activation.
4. Smoke activation.
5. Burning duration.
6. Visible gradual sinking.
7. Proper geographic down direction.
8. Buoyancy/physics stability.
9. Cleanup.
10. Multiple ships dying simultaneously.
11. Team counter correctness.
12. No repeated death events.
13. No AI action after death.
14. Spectator camera safety while a wreck sinks.
15. Restarting PIE and repeating the same destruction sequence.

**GATE: A destroyed ship must remain visible long enough to burn and then gradually sink, rather than disappearing immediately.**

---

# PHASE 9 — FIX PLAYER SPECTATOR AND MATCH-END LOGIC

When the player dies, the game must not simply eject the editor camera or become stuck.

Inspect existing PlayerController, GameMode, spectator pawn, and camera logic.

## 9.1 Automatic spectator mode

When the player's ship dies:

1. Mark the player dead.
2. Disable player propulsion and firing.
3. Preserve the ship as a burning/sinking wreck.
4. Transition the PlayerController to spectator state.
5. Automatically follow a surviving BLUE teammate.
6. Continue the battle normally.
7. Support switching between living BLUE AI ships.
8. Support switching to a battlefield overview camera.
9. If no BLUE teammates remain, use the overview camera.

Do not require pressing a key merely to begin spectating.

## 9.2 Input controls

Unreal Editor may intercept F8 as PIE Eject.

Do not confuse editor Eject with a real gameplay spectator mode.

Use reliable gameplay controls.

Suggested defaults:

Tab:
Next living teammate.

M:
Toggle overview mode.

Optional:
Previous teammate using Shift+Tab if compatible with existing input mappings.

Preserve F8 functionality only if it can be safely bound without conflicting with the editor.

Implement controls through the actual game PlayerController/Input Mapping system.

## 9.3 Spectator HUD

Display:

- SPECTATING
- Current teammate
- Remaining BLUE ships
- Remaining RED ships
- Current viewed ship health
- Camera-switch instructions

Do not show the destroyed player's health as if the player were alive.

## 9.4 Match victory and defeat

Maintain accurate team counts.

When every RED ship has been destroyed:

BLUE VICTORY.

When every BLUE ship has been destroyed:

RED VICTORY.

Do not count a sinking wreck as alive.

Do not register death twice.

Keep the final match result stable.

Stop turn-based scheduling or active gameplay control as appropriate after the match ends.

Preserve the camera long enough to observe the final battle result.

### PHASE 9 TESTS

Verify:

- Player health death.
- Player capsize death.
- Automatic spectator activation.
- Teammate camera follow.
- Camera switching.
- Overview mode.
- Spectated teammate dying.
- No teammates remaining.
- Correct team counters.
- Correct match-end result.

---

# PHASE 10 — OPTIONAL 10-SECOND TURN-BASED GAME MODE

Add a selectable alternate game mode.

The existing real-time battle must remain the default and fully functional.

Supported modes:

REAL_TIME

TURN_BASED

## 10.1 Real-time mode

All ships may:

- Move simultaneously.
- Aim simultaneously.
- Fire simultaneously.
- Run AI behavior simultaneously.
- Execute hit-and-run tactics simultaneously.

This is the existing gameplay style.

Preserve it.

## 10.2 Turn-based mode

Implement alternating BLUE and RED turns.

Turn duration:

10 seconds.

The turn scheduler must select one eligible living ship at a time.

Suggested order:

BLUE ship
→ RED ship
→ BLUE ship
→ RED ship

Cycle through eligible ships fairly until the battle ends.

Each active ship may move, aim, and fire freely during its 10-second window.

Inactive ships cannot issue voluntary movement or fire commands.

However:

- Buoyancy must continue operating.
- Physics must remain stable.
- Existing projectiles must complete their physical trajectories.
- Impact/damage processing must continue.
- Destruction VFX must continue.
- Sinking sequences must continue.

Do not pause the entire Unreal physics world between turns.

## 10.3 Player-controlled turns

When the player ship becomes active:

- Enable manual movement.
- Enable aiming.
- Enable firing.
- Show a turn timer.
- Allow the player to act freely for 10 seconds.

When its turn ends:

- Disable new movement input.
- Disable new firing commands.
- Transition safely to the next ship.

Do not create a sudden physics impulse merely because the turn ends.

## 10.4 AI turns

When an AI ship becomes active:

- Select a tactical action.
- Continue valid A* navigation.
- Engage enemies when appropriate.
- Retreat if threatened.
- Use only its allocated turn duration.

Inactive AI ships must not continue issuing propulsion or weapon commands.

Their physics and destruction presentation may continue normally.

## 10.5 Fairness and death handling

Skip destroyed ships.

If the active ship dies:

- End its turn.
- Select the next eligible ship.
- Preserve the destruction VFX.

If the player is dead, do not assign further controllable turns to the player ship.

Avoid duplicate turns and scheduler deadlocks.

## 10.6 Turn UI

Display:

- Active team
- Active ship
- Remaining turn time
- Current turn number
- Game mode

Provide a clear visual indication of whose turn is active.

### PHASE 10 TESTS

Verify:

- Real-time mode is unaffected.
- Alternating team turns.
- Exactly one active voluntary movement/firing authority.
- 10-second turn duration.
- Correct player controls during player turn.
- AI actions during AI turn.
- No input when inactive.
- Projectiles still travel between turns.
- Sinking still operates.
- Destroyed ships are skipped.
- Match end terminates the scheduler safely.

---

# PHASE 11 — PRE-MATCH SHIP AND TEAM CONFIGURATION MENU

Implement a configurable pre-match setup screen.

The current default match remains 3v3.

Do not remove the ability to start the existing map directly for developer testing.

## 11.1 Team size selection

Allow configuration of:

BLUE total ships: 1–3

RED total ships: 1–3

For the current single-player mode:

BLUE always contains exactly one player-controlled ship.

Remaining BLUE slots are AI ships.

All RED ships are AI-controlled.

Examples:

1v1:
1 BLUE player vs 1 RED AI.

2v2:
1 BLUE player + 1 BLUE AI vs 2 RED AI.

3v3:
1 BLUE player + 2 BLUE AI vs 3 RED AI.

Allow asymmetric counts within the supported range, such as 2v3, when selected.

Do not spawn extra ships beyond the chosen counts.

## 11.2 Ship class selection

For each selected ship slot, allow selection of:

LARGE_SHIP

SMALL_SHIP

Use the placeholder existing mesh for both until real assets become available.

Apply the correct gameplay/physics profile.

Do not require external models.

## 11.3 Combat AI difficulty

Expose:

Easy

Normal

Hard

Use Normal as default.

Apply difficulty through aiming and decision parameters rather than arbitrary guaranteed hit outcomes.

## 11.4 Game mode selection

Expose:

Real-Time Battle

Turn-Based Battle — 10 seconds per ship

Default:

Real-Time Battle.

## 11.5 Suggested settings

Show configurable options for:

- Team size
- Ship class
- AI difficulty
- Game mode
- Navigation debugging
- Combat debugging
- VFX quality, if performance tuning is implemented

Default settings:

BLUE = 3 ships

RED = 3 ships

Mode = Real-Time

AI Difficulty = Normal

MaxHealth = 100

BaseProjectileDamage = 10

Bow Damage Multiplier = 0.8

Midship Damage Multiplier = 1.0

Stern Damage Multiplier = 0.8

## 11.6 Match startup

When START BATTLE is selected:

1. Validate setup selections.
2. Assign team configurations.
3. Choose the correct number of ships.
4. Select the requested ship class profiles.
5. Assign player control to exactly one BLUE ship.
6. Create/activate AIControllers for remaining ships.
7. Assign appropriate A/B patrol routes.
8. Initialize navigation and combat.
9. Start the selected game mode.
10. Initialize the HUD.
11. Begin the match.

Do not accidentally spawn an extra DefaultPawn through GameMode.

Do not create duplicate player ships.

## 11.7 Persistent configuration

Store the selected match configuration in an appropriate GameInstance or other existing configuration object.

Do not rely on transient actor references from a previous PIE session.

Make it possible to return to the pre-match menu after a match ends.

Keep a developer-friendly direct PIE startup path using default settings.

### PHASE 11 TESTS

Verify:

- 1v1
- 2v2
- 3v3
- Asymmetric team sizes
- Different ship-class selections
- Different AI difficulties
- Real-time selection
- Turn-based selection
- Correct number of ships
- Correct player possession
- Correct spawn formations
- Correct HUD team counts
- No duplicate actors
- Consistent state after restarting PIE

---

# PHASE 12 — PERFORMANCE, VISUAL DEBUGGING, AND GAMEPLAY POLISH

## 12.1 Debug drawing

The previous project displayed excessive red debug lines.

Inspect their origin.

Make debugging configurable:

bDebugNavigation

bDebugCombat

bDebugPhysics

bDebugAIState

bDebugDamage

bDebugSinking

Disable excessive persistent traces in normal gameplay.

Keep useful diagnostics available for testing.

## 12.2 Performance requirements

The scenario contains a maximum of six simultaneously active combat ships in the current menu design.

Keep AI efficient.

Avoid:

- Rebuilding A* every Tick.
- Searching every actor every frame.
- Unbounded particle spawning.
- Permanent debug lines.
- Excessive Output Log messages.
- Repeated unnecessary physics calculations.
- Multiple conflicting movement controllers.

Use suitable update intervals for target acquisition and tactical planning.

Keep movement and animation smooth.

## 12.3 UI polish

Preserve existing:

- Team counters
- Health bars
- Player HUD

Integrate:

- Pre-match configuration
- Current game mode
- Spectator status
- Turn timer
- Victory/Defeat screen

Do not redesign unrelated UI unnecessarily.

---

# PHASE 13 — COMPREHENSIVE AUTOMATED VERIFICATION

This phase is mandatory.

You must validate the actual integrated game, not just individual components.

## 13.1 Automated testing rules

For each critical feature:

- Run static validation.
- Run unit/functional tests where suitable.
- Compile modified C++.
- Compile modified Blueprint assets.
- Run real PIE when possible.
- Collect runtime measurements.
- Compare actual results to acceptance criteria.
- Fix failures.
- Retest.

Do not claim a real PIE success if only static tests were performed.

## 13.2 Navigation tests

Verify:

- All configured AI ships physically move.
- A* routes are valid.
- Each AI maintains independent patrol state.
- Every AI can travel between A and B.
- Ships do not intentionally cross islands.
- Speed is within an appropriate tuned range.
- Tactical transitions do not permanently stop navigation.
- Cesium origin shifting does not invalidate routes.

## 13.3 Tactical tests

Verify:

- Passive behavior.
- Passive → Fighter.
- Hunter → Fighter.
- Fighter engagement.
- Low-health retreat.
- Reverse movement.
- Reverse acceleration lower than forward acceleration.
- Turret tracking during reverse.
- Firing during reverse.
- Safe fallback when reverse path is blocked.
- Return to original patrol.

## 13.4 Weapon tests

Verify:

- AI can miss.
- AI can hit.
- Aim error responds to difficulty.
- Projectile muzzle orientation is correct.
- Projectile collision is correct.
- Firing cooldown works.
- Line-of-sight restrictions work.
- Team-friendly fire is rejected.

## 13.5 Damage tests

Use 100 HP ships and base 10 damage.

Verify:

- Bow hit = 8 HP.
- Midship hit = 10 HP.
- Stern hit = 8 HP.
- No duplicate hit application.
- No unintended self-damage.
- No friendly damage.
- Health UI updates correctly.
- Death triggers once.

## 13.6 Ship-class tests

Verify both placeholder classes:

- Initialize correctly.
- Float correctly.
- Accelerate correctly.
- Steer correctly.
- Navigate A*.
- Fire.
- Receive damage.
- Die.
- Burn and sink.

Compare measured large/small physical response.

## 13.7 Destruction VFX tests

Verify:

- Explosion or destruction initiation.
- Visible fire.
- Visible smoke.
- Fire attached to wreck.
- Gradual sinking.
- Stable physics.
- Correct geographic down direction.
- No immediate disappearance.
- Correct cleanup.
- No lingering expensive particle systems.
- Multiple simultaneous destruction events.
- Player spectator continuity.

## 13.8 Game mode tests

Real-time:

- All living ships act simultaneously.

Turn-based:

- Exactly one active voluntary-action ship.
- Alternating teams.
- 10-second turns.
- Correct input control.
- Dead ships skipped.
- Physical projectiles finish traveling.
- Destruction visuals continue.

## 13.9 Pre-match menu tests

Verify all supported team sizes, ship classes, and game modes.

Start a match from actual saved configuration.

Ensure the correct ships appear.

Verify match restart behavior.

## 13.10 Full battle simulation

Run an actual 3v3 PIE scenario.

Verify:

1. Exactly three BLUE ships.
2. Exactly three RED ships.
3. Exactly five AI-controlled ships.
4. Exactly one player-controlled ship.
5. Correct team spawn formations.
6. All living AI ships navigate.
7. A* routes avoid islands.
8. Opposing teams encounter each other.
9. AI fires with realistic accuracy.
10. Positional damage is correct.
11. Low-health ships retreat appropriately.
12. Reverse-and-fire works.
13. Destroyed ships burn and sink.
14. Team counters update correctly.
15. Player death transitions into spectator mode.
16. Victory and Defeat conditions work.
17. No major runtime exceptions occur.
18. All important game state remains consistent.

If a full battle cannot finish within the available automated test time, provide the exact duration observed and which behaviors were actually verified.

Never invent match results.

---

# PHASE 14 — PERSISTENCE AND FINAL REGRESSION

Stop PIE.

Save all relevant modified assets.

Compile all affected Blueprints.

Confirm the project contains no newly introduced compile errors.

Verify persistent level references.

Start a fresh PIE session.

Test with the SAVED configuration, not temporary test state.

Confirm that:

- The configured number of ships appears.
- Navigation initializes.
- Combat initializes.
- Tactical AI initializes.
- Damage works.
- Death VFX works.
- Spectator works.
- Menu settings persist appropriately.
- Game mode selection works.

Restore any temporary test-only actor relocation or parameter overrides.

Do not leave the main level in an artificial debugging configuration unless it is the documented developer default.

Keep the Unreal Editor in a safe usable state.

---

# PHASE 15 — CONTINUOUS PROGRESS AND HANDOFF

Maintain:

`Docs/NavalBattleV2_Progress.md`

For every phase record:

- Status: NOT STARTED / IN PROGRESS / PASSED / BLOCKED
- Existing implementation discovered
- Root cause or design decision
- Files modified
- Automated tests run
- Actual PIE tests run
- Pass/fail results
- Remaining issues
- Next action

If the session is interrupted, another Agent should be able to resume from this file without duplicating completed work.

Do not ask for confirmation between ordinary implementation phases.

If context limits prevent completing the entire task in one session, leave an accurate checkpoint rather than claiming the task is done.

---

# REQUIRED FINAL REPORT

When implementation and testing are complete, produce a technical report covering:

## A. Implementation

- Existing systems reused.
- New systems implemented.
- Tactical state machine.
- Low-health retreat.
- Reverse-and-fire.
- AI accuracy and difficulty.
- Positional damage.
- Ship-class configuration.
- Buoyancy profiles.
- Destruction VFX.
- Sinking implementation.
- Spectator behavior.
- Turn-based mode.
- Pre-match selection.

## B. Exact project changes

- Modified C++ source files.
- Modified Blueprint assets.
- Modified Animation Blueprint assets.
- New or modified VFX assets.
- Modified GameMode/GameState.
- Modified UI widgets.
- Modified level actors.
- Created configuration assets.

## C. Runtime measurements

- AI movement speed.
- Number of AI ships moving.
- Reverse acceleration.
- Forward acceleration.
- Projectile hit/miss statistics.
- Bow/midship/stern damage values.
- Burning and sinking durations.
- Successful tactical transitions.
- Successful match configurations.

## D. Test evidence

Provide a table containing:

Test Name | Test Type | Result | Evidence | Notes

Distinguish:

- Source inspection
- Unit tests
- Blueprint compilation
- Functional tests
- Real PIE runtime tests

Do not merge these categories into a generic "tests passed" claim.

## E. Remaining limitations

Clearly identify:

- Unavailable VFX assets.
- Placeholder ship visuals.
- Physics approximations.
- Tool limitations.
- Unverified gameplay scenarios.
- Known remaining bugs.

Provide instructions for one final manual gameplay test.

---

# FINAL ACCEPTANCE CRITERIA

The project is successful when I can open Unreal Engine 5, start the battle, and experience the following:

**1. CONFIGURATION**

I can choose how many BLUE and RED ships participate, select Large or Small ship profiles, choose AI difficulty, and select Real-Time or Turn-Based gameplay.

**2. NAVIGATION**

All AI ships automatically travel between their own two patrol endpoints using the existing geographic A* system.

They do not remain stationary without a valid reason.

**3. COMBAT**

AI ships detect and attack only opposing ships.

Their shooting is imperfect and physically simulated.

**4. TACTICAL AI**

AI ships can change roles and tactical states.

Low-health ships retreat.

Some ships can reverse away from enemies while keeping their turrets aimed and firing.

Backward propulsion has lower acceleration than forward propulsion.

**5. DAMAGE**

Every ship has 100 HP by default.

Bow hits inflict 8 HP damage.

Midship hits inflict 10 HP damage.

Stern hits inflict 8 HP damage.

**6. SHIP CLASSES**

Both Large and Small ship physics profiles function with the existing placeholder ship mesh.

Large ships behave more heavily and stably.

Small ships behave more responsively.

Future ship assets can be assigned without rewriting AI systems.

**7. DESTRUCTION**

When a ship reaches zero health or capsizes, it does NOT instantly disappear.

Instead, it becomes a destroyed wreck, displays fire and smoke, gradually sinks beneath the sea, and is cleaned up only after the visual sequence is complete.

**8. SPECTATOR**

When my player-controlled ship dies, the camera automatically follows a living friendly teammate or switches to a battlefield overview.

**9. TURN-BASED MODE**

When selected, BLUE and RED ships alternate individual 10-second action windows.

**10. PERSISTENCE**

All required changes are saved.

Restarting PIE reproduces the configured gameplay correctly.

---

# EXECUTION COMMAND

Begin by inspecting the actual existing implementation.

Do not create a new project.

Do not rebuild working systems.

Proceed through each development phase sequentially.

For each phase:

INSPECT
→ BASELINE
→ IMPLEMENT
→ COMPILE
→ AUTOMATED TEST
→ REAL PIE TEST
→ FIX
→ RETEST
→ SAVE
→ RECORD RESULTS
→ NEXT PHASE

Prioritize correctness and stable gameplay over unnecessary architectural complexity.

You have permission to complete routine implementation and verification steps autonomously without asking me questions.

If a hard technical blocker exists, report it precisely and preserve all working progress.

**Your completion claim must be supported by actual saved project changes and test evidence.**

Start Phase 1 now.