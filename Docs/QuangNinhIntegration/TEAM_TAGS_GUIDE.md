# Standard ship identity tags

Implementation: `UShipWeaponComponent::RefreshShipTags` in ShipCombatComponent.cpp. This project uses Unreal **Actor Tags**, the same architecture already used by wreck/dead filtering; these names are not FGameplayTag values and need no GameplayTags module or .ini registration. The managed namespaces are reserved for this synchronizer.

Registered managed names:
- Team.Blue, Team.Red
- Ship.Class.Small, Ship.Class.Medium, Ship.Class.Large
- AI.Role.Scout, AI.Role.Defender, AI.Role.Attacker, AI.Role.Follower
- AI.State.Patrol, AI.State.Approach, AI.State.Attack, AI.State.Retreat, AI.State.Recovery

`Weapon.Team()` remains the authoritative faction read: combat component Faction on AI, otherwise weapon Faction on the player. Actor tags are a projection of this value, never an additional team counter. Unknown factions receive no team tag and cannot be hostile. Final damage still rejects allies and invalid faction values using Team(), including captured projectile source team. Changing a target never changes faction.

C++: set the existing authoritative Faction during roster initialization, apply the selected ShipPhysicsProfile, then call `Weapon->RefreshShipTags()`. The existing presentation timer maintains AI tags every 0.5 seconds. ApplyProfile refreshes class tags immediately. Ship initialization refreshes them and death removes live AI role/state tags while preserving team/class and unrelated actor tags.

Blueprint: obtain ShipWeaponComponent, use Team for authoritative ally/enemy decisions and Refresh Ship Tags after an explicit setup change. Inspect the actor Tags array or Actor Has Tag for presentation/debugging. Do not manually add managed identity tags or maintain a second faction variable. Class application and mission assignment remain on their original shared interfaces.

Roles derive from actual battlefield assignments: Blue AI Defender; current Red leader Scout; Red convoy members Follower before contact; independent contact members Attacker. Player ships carry team/class, with no artificial AI role. State precedence is Recovery, Retreat, Attack, Approach, Patrol. Mission changes replace only managed namespaces, preserving e.g. Dead and all unrelated actor tags.

Examples: an initial Red Small leader has Team.Red, Ship.Class.Small, AI.Role.Scout and AI.State.Approach after departure. A Blue Medium defender retains Team.Blue and Ship.Class.Medium when its AI.State changes from Patrol to Attack, Retreat and back to Patrol. A replacement Red leader acquires Scout on the next synchronization tick.

Adding a hull: extend EShipHullClass, its profile/equipment mapping and the class-name mapping here together. Adding an AI role: extend the mission policy and its named mapping; do not add faction logic to the new role. Adding a state: map the actual tactical/navigation condition at an appropriate precedence.

Initial runtime identity PASS: IDENTITY_TAGS_MIXED_PIE.json records all six ships and all three classes. Use SetHostileTarget for an explicit runtime focus change: it rejects allies and invalid/dead targets through IsHostile. Never use the editor property setter during gameplay: it can rerun construction scripts and replace components. Combined runtime PASS: INTEGRATED_SECTOR_BATTLE.json; all four required roles and all five required states were observed in real combat rows, with native hostile filtering and friendly-damage rejection on all six ships.
