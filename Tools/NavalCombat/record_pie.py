"""Observe real PIE; no transforms, input, commands, or time dilation are changed."""
import builtins
import json
from pathlib import Path
import unreal

w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert w
e=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,"/Game/ShipEnemy/BluePrint/BP_ShipEnemy.BP_ShipEnemy_C"))[0]
n=e.get_component_by_class(unreal.ShipEnemyNavigationComponent)
c=e.get_component_by_class(unreal.ShipCombatComponent)
weapon=e.get_component_by_class(unreal.ShipWeaponComponent)
m=e.get_component_by_class(unreal.SkeletalMeshComponent)
h=next(x for x in e.get_components_by_class(unreal.PrimitiveComponent) if x.get_name()=="ShipSM")
v=lambda x:[x.x,x.y,x.z]
r=lambda x:[x.pitch,x.yaw,x.roll]
path=Path(unreal.Paths.project_dir())/"Docs/NavalCombat/INITIAL_PIE.json"
old=getattr(builtins,"combat_probe",None)
if old and old.get("handle"):unreal.unregister_slate_post_tick_callback(old["handle"])
s=dict(rows=[],handle=None,last=-1,path=path)
builtins.combat_probe=s
def tick(dt):
    t=unreal.GameplayStatics.get_time_seconds(w)
    if t-s["last"]<.2:return
    s["last"]=t
    try:
        shells=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.load_class(None,"/Game/Ship/BluePrint/BP_ShipProjectileWithSpeed.BP_ShipProjectileWithSpeed_C"))
        player=unreal.GameplayStatics.get_player_pawn(w,0)
        row=dict(t=t,target=c.target.get_name() if c.target else None,range=c.distance_to_target_m,
            status=c.firing_status,solution=c.has_firing_solution,visible=c.target_visible,
            yaw=c.turret_yaw,desired_yaw=c.desired_turret_yaw,pitch=c.barrel_pitch,desired_pitch=c.desired_barrel_pitch,
            muzzle=v(m.get_socket_location("front_gun_v_socket")),muzzle_rot=r(m.get_socket_rotation("front_gun_v_socket")),
            bone_h=r(m.get_socket_rotation("front_gun_h_jnt")),bone_v=r(m.get_socket_rotation("front_gun_v_jnt")),
            component_rotation=r(m.get_world_rotation()),hull=v(h.get_world_location()),velocity=v(h.get_physics_linear_velocity()),
            anim_yaw=m.get_anim_instance().get_editor_property("GunYaw"),anim_pitch=m.get_anim_instance().get_editor_property("GunPitch"),
            recoil=e.get_editor_property("Recoil Value"),can_shoot=e.get_editor_property("CanShoot"),shots=weapon.shots_fired,hits=weapon.hits,
            cooldown=c.cooldown_seconds_remaining,launch=v(c.launch_velocity),intercept=v(c.predicted_intercept),
            nav=str(n.navigation_state),legs=n.patrol_legs_completed,waypoint=n.current_waypoint_index,path=n.pathfinding_succeeded,
            throttle=n.desired_throttle,rudder=n.desired_rudder,bp_throttle=e.get_editor_property("AIThrottle"),bp_rudder=e.get_editor_property("AIRudder"),
            active=e.get_editor_property("DestinationActor").get_actor_label(),player=player.get_class().get_name() if player else None,
            gun_camera=next(x for x in e.get_components_by_class(unreal.CameraComponent) if x.get_name()=="GunCamera").is_active(),
            projectiles=[dict(name=x.get_name(),position=v(x.get_actor_location()),velocity=v(x.get_component_by_class(unreal.ProjectileMovementComponent).velocity),owner=x.get_owner().get_name() if x.get_owner() else None,instigator=x.get_instigator().get_name() if x.get_instigator() else None) for x in shells])
        s["rows"].append(row)
        if len(s["rows"])%10==0:s["path"].write_text(json.dumps(dict(rows=s["rows"]),indent=2),encoding="utf8")
    except Exception as error:
        s["error"]=str(error);unreal.unregister_slate_post_tick_callback(s["handle"]);s["handle"]=None
        print("COMBAT PROBE ERROR",error)
s["handle"]=unreal.register_slate_post_tick_callback(tick)
print("Read-only combat telemetry recording")
