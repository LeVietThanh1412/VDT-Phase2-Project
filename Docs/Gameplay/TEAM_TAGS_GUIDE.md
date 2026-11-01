# Teams, classes and mission tags

The authoritative team is `ShipWeaponComponent.Team()`: it reads `ShipCombatComponent.Faction` when that component exists, otherwise the weapon faction.0 is Blue,1 is Red. Identity names and Actor.Tags are presentation/inspection values. Changing a tag or TeamName alone does not change combat allegiance.

`RefreshShipTags` maintains these standard Actor.Tags:

| Family | Values |
|---|---|
| Team | `Team.Blue`, `Team.Red` |
| Hull | `Ship.Class.Small`, `Ship.Class.Medium`, `Ship.Class.Large` |
| Mission role | `AI.Role.Defender`, `AI.Role.Scout`, `AI.Role.Follower`, `AI.Role.Attacker` |
| Live state | `AI.State.Patrol`, `AI.State.Approach`, `AI.State.Attack`, `AI.State.Retreat`, `AI.State.Recovery` |

These mission labels differ from the behavioral role enum: Passive, Coward, Fighter and Hunter. The latter controls combat response and bounded pursuit/retreat. A Red scout is still an actual hostile ship using the shared gun; its tag does not bypass faction, range, LOS, aim alignment or ballistic checks.

The projectile captures source ship/team/damage once. The final damage authority rejects self, friendly, unknown-team and duplicate impacts, including after source-ship cleanup. Turret targeting also applies hostility checks. Defensive movement remains bounded; a defender may fire at a real nearby attacker across the sector boundary when the existing gun's effective range permits it. The old sector boundary must not make a physically valid in-range attacker cease to be hostile.

The setup menu's Standard demo balance sets each active ship to100 HP and base damage10 without changing its mesh, hull physics or profile asset. Bow/midship/stern multipliers remain0.8/1/0.8 (8/10/8). Class balance remains selectable; the existing Small profile retains38 HP/base6. The shared magazine remains10 rounds and30 game seconds to reload.
