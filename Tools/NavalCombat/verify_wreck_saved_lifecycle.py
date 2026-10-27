"""Reuse real five-wreck health/capsize/rebase fixture with saved fallback checks."""
from pathlib import Path
import unreal
root=Path(unreal.Paths.project_dir())
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None
p=root/'Tools/NavalCombat/test_destruction_v2.py';code=p.read_text()
code=code.replace('DESTRUCTION_V2_PIE.json','WRECK_SAVED_DESTRUCTION.json').replace("('V2_'+name+'.png')","('WRECK_SAVED_'+name+'.png')")
old="check('burning FX active',hp(s).destruction_fx is not None and hp(s).destruction_fx.is_active())"
assert old in code
code=code.replace(old,old+"\n                    effects=s.get_components_by_class(unreal.ShipWreckBillboardComponent)\n                    check('five saved effects active on wreck',len(effects)==5 and all(e.wreck_visual_active and e.is_visible() for e in effects))\n                    check('fallback no collision',all(e.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION for e in effects))")
old="hidden=s.get_editor_property('hidden')))";assert old in code
code=code.replace(old,"hidden=s.get_editor_property('hidden'),fallback_visible=sum(e.is_visible() for e in s.get_components_by_class(unreal.ShipWreckBillboardComponent))))")
old="check(name+' progressive sinking',any(5<r['sunk']<55 for r in rows) and max(r['sunk'] for r in rows)>=64.9)";assert old in code
code=code.replace(old,old+"\n                    check(name+' fallback hidden after submersion',all(r['fallback_visible']==0 for r in rows if 'SUBMERGED' in r['stage']) and any('SUBMERGED' in r['stage'] for r in rows))")
exec(compile(code,str(p),'exec'),{'__name__':'saved_wreck_lifecycle'})
