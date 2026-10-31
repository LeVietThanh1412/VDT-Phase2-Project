"""Keep existing GMBP defaults/graphs, add class-sensitive player spawning."""
import unreal,json
from pathlib import Path
ed=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem);assert ed.get_game_world() is None
bp=unreal.load_asset('/Game/Ship/BluePrint/GMBP');assert bp
c=unreal.get_default_object(bp.generated_class())
before={k:c.get_editor_property(k).get_path_name() for k in ['default_pawn_class','player_controller_class']}
unreal.BlueprintEditorLibrary.reparent_blueprint(bp,unreal.NavalGameMode.static_class())
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
c=unreal.get_default_object(bp.generated_class())
after={k:c.get_editor_property(k).get_path_name() for k in ['default_pawn_class','player_controller_class']}
assert before==after,(before,after)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)
Path(unreal.Paths.project_dir(),'Docs/NavalCombat/SMALL_GAME_MODE.json').write_text(json.dumps(dict(before=before,after=after,parent='NavalGameMode',small_player_class='/Game/ShipSmall/BluePrint/BP_ShipSmall'),indent=2),encoding='utf-8')
print('OK: GMBP reparented and saved; controller and default original pawn preserved')
