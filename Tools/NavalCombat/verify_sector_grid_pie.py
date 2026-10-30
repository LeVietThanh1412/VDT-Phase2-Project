"""Verify the saved candidate-1 spatial data through the native PIE subsystem."""
import builtins,json,traceback
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir());ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert ed.get_game_world() is None
st=dict(status='RUNNING',error=None)
builtins.sector_grid_probe=st

def finish(status):
    st['status']=status
    (root/'Docs/QuangNinhIntegration/SECTOR_GRID_PIE.json').write_text(json.dumps({k:v for k,v in st.items() if k!='handle'},indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(st['handle']);unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

def tick(dt):
    w=ed.get_game_world()
    if not w or unreal.GameplayStatics.get_time_seconds(w)<2:return
    try:
        lib=unreal.get_default_object(unreal.load_class(None,'/Script/Engine.SubsystemBlueprintLibrary'))
        grid=lib.call_method('GetWorldSubsystem',args=(w,unreal.NavalSectorSubsystem.static_class()))
        assert grid.grid_ready,grid.grid_error
        data=json.loads((root/'Content/SeaNavigation/quangninh_sectors.json').read_text())
        max_error=0;wrong=[]
        for i,s in enumerate(grid.sectors):
            roundtrip=grid.world_to_chart(grid.chart_to_world(s.anchor_en))
            max_error=max(max_error,(roundtrip-s.anchor_en).length())
            if grid.sector_at_chart_position(s.anchor_en)!=i:wrong.append(i)
        paths=json.loads(grid.validate_required_connections())
        targets=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.TargetPoint)
        fleet=[a for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Pawn) if a.get_component_by_class(unreal.ShipWeaponComponent)]
        st.update(sector_count=len(grid.sectors),columns=grid.columns,rows=grid.rows,
            chart_roundtrip_max_error_m=max_error,wrong_sector_indices=wrong,connections=paths,
            target_point_count=len(targets),combat_pawns=len(fleet),grid_error=grid.grid_error)
        st['checks']=dict(saved_grid_loaded=len(grid.sectors)==625 and grid.columns==25 and grid.rows==25,
            correct_region=grid.center_en==unreal.Vector2D(14500,3500) and grid.width_m==2500 and grid.height_m==2500,
            geographic_mapping=max_error<.001 and not wrong,native_astar_connectivity=paths['passed'],
            no_sector_target_actor_flood=len(targets)<30,existing_six_ship_roster=len(fleet)==6,
            candidates_match_saved_data=sum(s.spawn_eligible for s in grid.sectors)==sum(s['spawn_eligible'] for s in data['sectors']))
        finish('PASSED' if all(st['checks'].values()) else 'FAILED')
    except Exception:st['error']=traceback.format_exc();finish('FAILED')

st['handle']=unreal.register_slate_post_tick_callback(tick)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
