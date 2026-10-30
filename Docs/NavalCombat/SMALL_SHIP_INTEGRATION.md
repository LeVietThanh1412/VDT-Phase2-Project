# Small ship integration

Work in progress, 26 October 2026. This document records saved implementation;
the complete randomized battlefield and final demo acceptance remain pending.

## Source and licence

“Sweeden Military Boat”, creator **YourAverageGamer**, Fab listing
[b6d0889d-e855-4b3f-bf48-5ec7f9f4a4a7](https://www.fab.com/listings/b6d0889d-e855-4b3f-bf48-5ec7f9f4a4a7).
The user confirmed **Fab Standard**. The public listing returned HTTP 403;
licence identification is user-confirmed, not inferred from the local metadata.
See the [Fab Standard licence summary](https://www.fab.com/eula).
The repository is private; these assets are included for project collaborators,
not distributed as a standalone model. Entitlement metadata and redundant
thumbnails are excluded from the intended commit.

The supplied FBX is unrigged: one mesh, no skin, joints, deformers, animations,
or Unreal static sockets. `SMALL_FBX_SKELETON_AUDIT.json` and
`SMALL_NATIVE_SOURCE_AUDIT.json` record the actual inspection. The source atlas
is **4096 x 4096**, despite the listing metadata advertising 8K. Nanite was
already enabled; its fallback LOD0 has 9,735 vertices.

## Assets and Blueprint setup

| Asset | Purpose |
| --- | --- |
| `/Game/ShipSmall/BluePrint/BP_ShipSmall` | Dedicated player child of the existing BP_Ship |
| `/Game/ShipSmall/BluePrint/BP_ShipSmallAI` | Dedicated AI child of BP_ShipEnemy; retains its controller and navigation |
| `/Game/ShipSmall/Asset/sweedenmilitaryboat` | Original imported static visual |
| `/Game/ShipSmall/Asset/model` | Repaired source material |
| `/Game/NavalBattle/Profiles/DA_SmallShip` | Art, physical hull, buoyancy and Small balance |
| `/Game/NavalBattle/Profiles/DA_MediumShip` | Preserved current original-hull balance |
| `/Game/NavalBattle/Profiles/DA_LargeShip` | Existing Large profile |
| `/Game/NavalBattle/Equipment/SM_SmallPhysicsHull` | Compact convex physical hull |
| `/Game/NavalBattle/Equipment/SM_SmallGunYaw` | Extracted original front-gun yaw geometry |
| `/Game/NavalBattle/Equipment/SM_SmallGunPitch` | Extracted original front-gun pitch geometry |

GMBP now inherits the small native `NavalGameMode` extension. It preserves its
original default pawn and CustomPlayerController; selecting Small for Blue
slot 1 chooses BP_ShipSmall on the next match travel. The existing five placed
AI retain their identities/controllers and receive the selected data profile.
BP_ShipSmallAI is also available as an explicitly authored Small-only AI child.
Neither child copies the movement, A*, combat, projectile or damage graphs.

The inherited `ShipSM` is the only simulated body. `SmallBoatVisual`,
`SmallGunYaw` and `SmallGunPitch` never simulate or collide. The original
skeletal `Ship` is hidden and noncolliding for Small, but still evaluates the
original ABP aiming pose as the gun driver. Existing authored parts are reused
at runtime instead of creating duplicate components.

## Geometry and weapons

Source bounds: 6.804 m across X, 24.358 m along Y, 8 m high. Source cross-sections
identify **+Y as the bow**. The visual uses local yaw -90 degrees and uniform
scale **2.75**, giving approximately **67.0 x 18.7 x 22.0 m**. The existing hull
is 157.59 m long; Small is 42.5% of that length. Actor/root scale stays 1.
The visual offset is Z=-400 cm. The physical wedge is 66 x 17 x 8 m and tapers
towards the bow. Its collision does not retain the original invisible 158 m hull.

**The imported model's baked gun is aft of the cabin and points astern.** It is
part of the one unrigged mesh and remains static source decoration. The working
weapon is a separate **bow-mounted** assembly, with its yaw pivot at local
X=2000, Y=0, Z=420 cm. This distinction must be explained during the demo;
the baked aft gun must not be described as animated or firing.

The original skeleton has 20 bones, including front yaw/pitch and muzzle socket.
The extracted parts contain 266 yaw and 90 pitch triangles, with original UVs
and materials. They follow the actual `front_gun_h_jnt` / `front_gun_v_jnt`
transforms after animation. Muzzle socket `front_gun_v_socket` stays on that same
driver, preserving one aiming/firing pipeline. Measured visible-gun position
error in the underway run was below 0.001 cm and axis dot exceeded 0.999.

The source material had a confirmed disconnected diffuse atlas: Base Color
instead used the imported Phong conversion and constants, producing a black
silhouette. The repair connects the original `shaded` RGB atlas directly to
Base Color and uses roughness 0.65 / metallic 0.1 because no separate roughness
or metallic textures were supplied. Source art was not recolored. No normal
map is claimed. `SMALL_FIXED_NORMAL00000.png` and
`SMALL_BLUEPRINT_SHOWCASE00000.png` show the repaired actual material.

## Balance and physical setup

| Parameter | Small | Medium | Large |
| --- | ---: | ---: | ---: |
| Max HP | 38 | 100 | 100 |
| Attacker base damage | 6 | 10 | 10 |
| Bow / middle / stern | 5 / 6 / 5 | 8 / 10 / 8 | 8 / 10 / 8 |
| Game mass, kg | 90 | 150 | 240 |
| Cruise target, m/s | 24 | 20 | 20 |
| Forward cap, m/s | 29 | 26 | 26 |
| Reverse cap, m/s | 7 | 7 | 7 |
| Propulsion multiplier | 0.7 | 1 | 1.1 |
| Rudder multiplier | 0.5 | 1 | 0.8 |

Damage rounds to the nearest integer **after** the positional multiplier.
Projectile metadata captures the attacker profile once; the regression test
changes the attacker class while a round is in flight to verify this.

Small uses four radius-100 cm pontoons at X=+/-2100, Y=+/-550, Z=-100 cm,
COM offset Z=-150 cm, linear damping 0.5 and angular damping 0.65.
Displacement is 0.6x and vertical damping 0.8x the original data, matching the
smaller game mass. The compact hull preserves original sockets 1-5 at 0.425x their source locations.
Rudder socket 5 is [1942.25, -323, 0] cm; its application filter is 1x, so the
point is not scaled twice;
the shared Blueprint AddForce/AddForceAtLocationLocal remain the force authority.
No simulated velocity override or movement teleport is introduced.

The third-person arm is set to 90 m at a 22 m local height for Small. Its gun
camera attaches to the actual front pitch bone instead of the old ship's
Camera socket. Wreck billboard offsets/sizes, explosion and burn effects use
0.425x presentation scale while the original death/sinking state machine remains.

## Verification status

- Native source build and affected Blueprint compilation passed.
- Dedicated Small selection, six ships, one player, controller preservation,
  38 HP / 6 damage, compact hull and no duplicate parts passed actual menu travel.
- A 38-second underway fixture passed original Enhanced Input movement/reverse/
  firing, single physical body and gun alignment. Peak speed about 28.9 m/s;
  maximum roll 9.47 degrees; measured contact gap after settling at most 0.407 s.
- Early oversized-pontoon and low-rudder results are retained separately; they
  are not acceptance evidence for the final tuning.
- The first damage matrix exposed impact-time reinitialization of the attacker
  profile (Small round incorrectly became 10 after changing its owner to Medium).
  That failure is preserved in `SMALL_DAMAGE_SNAPSHOT_FAILURE.json`; a once-only
  capture fix has been compiled. The isolated matrix rerun passed all 16 actual
  projectile cases across Small/Small, Small/Medium, Medium/Small and Small/Large,
  including friendly-fire guards, once-only death, full 65 m sinking and cleanup.
  A separate target-interception fixture failure is retained and was corrected
  by moving each completed test target away before introducing the next one.
- Settled comparison: Small acceleration 51.40 vs Medium 44.03 m/s²; speed
  28.76 vs 25.73 m/s; mean absolute turning 5.084 vs 3.458 deg/s (1.470x).
  Maximum observed roll was 7.87 / 8.34 degrees. These are disposable controlled
  input comparisons, not steady turning-circle or six-ship combat claims.
- New-field Small AI combat, new-field seeds,
  full patrol soak, restart persistence and final Git delivery remain pending.

Final socket-aware response: Small mean turning 5.255 vs Medium 3.467 deg/s (1.516x). Actual socket locations were checked in PIE. A native repeat mesh-builder crash exposed replacement of initialized render resources; the idempotent builder now preserves existing geometry and updates the socket contract. The final underway rerun also checks immediate gun placement, including its first frame. See SMALL_MILESTONE_QA.json for passing evidence hashes. All five physical AI moved over 900 m in the 75 s old-layout fixture, including Small Blue and Red units; this is not a full new-layout loop soak.
