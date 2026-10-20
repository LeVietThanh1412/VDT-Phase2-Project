# Current naval AI settings

All five AI ships inherit BP_ShipEnemy and use native AIController, Auto Possess AI = Placed in World or Spawned, Auto Possess Player = Disabled. The player remains manually possessed. Each AI has its own EnemyNavigation component, route array, waypoint index and two explicit Target Point references.

## Patrol

Enable New Navigation, select Patrol, assign both Patrol Actor A/B in the same level, and use zero Patrol Offset A/B when the destination must match the actual marker. Keep both markers always loaded. Each pair must be farther apart than twice the arrival radius.

Current saved defaults: cruise 20 m/s, arrival radius 80 m, wait 3 s, separate startup delays and 350 m soft ship separation. A leg ends only within 80 m at physical speed below 0.5 m/s; the next route starts automatically. Path endpoint matching is more precise than physical arrival tolerance.

Keep the existing physical ShipSM hull, buoyancy, Arrow heading, original ApplyAIForward / ApplyAIRudder and the exclusive legacy/new navigation branch. These functions apply force once; combat changes objectives and aims the gun independently.

## Roles

Fighter follows A/B while aiming and firing. Passive follows A/B without automatic fire. ShipCombat.SetCombatRole provides an explicit command. Passive can promote on hostile damage (default enabled) or RaiseThreatAlert (default disabled, radius 800 m). Friendly damage cannot activate it.

Hunter may temporarily pursue through geographic A*. Defaults: maximum pursuit 30 s / 1700 m, lost-target grace 3 s, replanning no faster than 3 s, promotion to Fighter at 40% HP. Role changes preserve the original A/B actor references and selected leg.

## Propulsion and retreat

Inherited ShipMovement filters the force going into the original Blueprint AddForce for both player and AI. Forward Max Speed = 26 m/s, Reverse Max Speed = 7 m/s, Reverse Force Ratio = 0.45, blend band = 1 m/s. Reverse force is reduced once; reverse input initially brakes forward motion at full strength. No continuous velocity override is used.

ShipCombat defaults: low HP = 30, critical HP = 15, prefer backward retreat enabled, reverse commitment distance 300 m. Existing proximity retreat remains another trigger. BackwardRetreatSpeedMps = 6 is a controller target; measured AI reverse speed is about 4.3 m/s under drag and limited throttle. The player can reach about 6.85 m/s using full reverse input.

Rear chart checks cover the hull width, with collision sweeps and other-ship clearance. Unsafe reverse may use a forward A* escape. Retreat duration and cooldown prevent repeated rapid transitions. Turret aiming/firing continues independently while retreating.

## Shooting accuracy

ShipCombat.ShootingSkill selects Easy, Normal, Hard or Custom. Built-in presets apply when play starts and when SetShootingSkill is called. Select Custom to retain individual editable accuracy values. AIAccuracy is a dispersion parameter, not a promised hit probability.

AimErrorDegrees, TargetTrackingDelay, TargetAcquisitionSeconds, AimUpdateInterval, DistanceAccuracyPenalty, MovementAccuracyPenalty and OptionalSuppressionPenalty alter the real aiming solution. AccuracySeed makes each named ship's random stream repeatable. Error affects both actual joint aiming and the projectile launch vector; collision outcomes remain physical.

Normal defaults: AIAccuracy 0.6, AimErrorDegrees 2.2, tracking delay 0.35 s, update interval 0.4 s, acquisition 0.6 s. The 144-shot stationary/moving test passed; measured results are in ACCURACY_REPORT.md. See NavalBattleV2_Progress.md for later master-task features.


## Hull impact damage

Base projectile damage is 10 HP. ShipWeapon classifies actual impacts in physical ShipSM local X, normalized across its bounds. Default front/rear quarters multiply by 0.8 (8 HP); central half multiplies by 1.0 (10 HP). BowLengthFraction, SternLengthFraction and the three multipliers are editable. The existing final faction/self/dead/duplicate guard still controls all health changes. Splash remains the base damage with its existing distance/visibility checks; region modifiers apply to direct hull impacts.

Enable ShipWeapon.DebugDamage for world/local impact position, normalized X, region, multiplier and final HP logs. HULL_DAMAGE_PIE.json records real rotated-hull collisions and guard probes.

## Placeholder hull classes (validation in progress)

/Game/NavalBattle/Profiles/DA_SmallShip and DA_LargeShip configure shared ShipMovement. Small preserves 150 kg, baseline inertia, 0.5 damping, and original force/rudder scales. Large uses 240 kg, higher inertia, slightly increased damping, propulsion scale 1.1, rudder scale 0.8, and buoyancy displacement multiplier 1.6. Meshes are not scaled. These are arcade placeholder profiles, not calibrated real-world hydrodynamics.

Use ApplyShipClass(Small/Large) or assign a Profile asset before play. ApplyProfile supports future visual mesh/animation/physics asset, static physical hull collision, buoyancy/pontoon setup, camera offset and weapon tuning. New ship art must supply compatible weapon bones/sockets and collision dimensions; test its water behavior before use. Default assets preserve the verified four-pontoon arrangement.
