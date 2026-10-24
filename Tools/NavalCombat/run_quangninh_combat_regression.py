"""Run an existing real-PIE combat fixture without overwriting historical proof.

Set FIXTURE to a key in options before executing this file. Each invocation
requires PIE to have fully stopped.
"""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
fixture = globals().get('FIXTURE', 'integration')
options = {
    'integration': ('verify_merge_integration.py','MERGE_INTEGRATION_PIE.json','QUANGNINH_FINAL_INTEGRATION_PIE.json'),
    'roles': ('test_roles.py','ROLES_PIE.json','QUANGNINH_ROLES_PIE.json'),
    'reverse': ('test_backward_retreat.py','BACKWARD_RETREAT_PIE.json','QUANGNINH_BACKWARD_RETREAT_PIE.json'),
    'island_reverse': ('test_island_reverse.py','ISLAND_REVERSE_PIE.json','QUANGNINH_ISLAND_REVERSE_PIE.json'),
    'accuracy': ('test_accuracy.py','ACCURACY_PIE.json','QUANGNINH_ACCURACY_PIE.json'),
    'destruction': ('test_destruction_v2.py','DESTRUCTION_V2_PIE.json','QUANGNINH_DESTRUCTION_PIE.json'),
    'profile_small': ('test_profile_physics.py','PROFILE_SMALL_PHYSICS.json','QUANGNINH_FINAL_PROFILE_SMALL_PHYSICS.json'),
    'profile_large': ('test_profile_physics.py','PROFILE_LARGE_PHYSICS.json','QUANGNINH_FINAL_PROFILE_LARGE_PHYSICS.json'),
    'damage_small': ('test_hull_damage.py','HULL_DAMAGE_PIE.json','QUANGNINH_FINAL_SMALL_DAMAGE.json'),
    'damage_large': ('test_hull_damage.py','HULL_DAMAGE_PIE.json','QUANGNINH_FINAL_LARGE_DAMAGE.json'),
}
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None, 'Stop the previous PIE first'
filename, old_output, output = options[fixture]
path = root/'Tools/NavalCombat'/filename
code = path.read_text().replace(old_output, output)
if fixture == 'roles':
    code = code.replace('if s!=subject:', 'if s!=subject:\n                    s.set_actor_tick_enabled(False)')
elif fixture in ['reverse','island_reverse']:
    code = code.replace('if other!=s:', 'if other!=s:\n                    other.set_actor_tick_enabled(False)')
if fixture == 'integration':
    scope = dict(OUTPUT=output)
else:
    scope = {}
if fixture == 'island_reverse':
    # The accepted Friendly1 start is deep open water. Its eastward rear
    # crosses the chart's island exclusion; the hostile remains in valid water.
    scope['ISLAND_FIXTURE']=dict(p=[10550,-4050],target=[9900,-3900],heading_en_deg=180)
    scope['DURATION']=90
    scope['CHECK_COMMITMENT']=True
if fixture.startswith('profile_'):
    scope['CLASS_NAME']='Small' if fixture.endswith('small') else 'Large'
    code=code.replace('Docs/NavalCombat/PROFILE_', 'Docs/NavalCombat/QUANGNINH_FINAL_PROFILE_')
if fixture.startswith('damage_'):
    scope.update(PROFILE_CLASS='Small' if fixture.endswith('small') else 'Large',CLEARANCE_CM=100000,OUTPUT=output)
    code=code.replace('hull(s).set_simulate_physics(False)', 'hull(s).set_simulate_physics(False);s.set_actor_tick_enabled(False)')
if fixture == 'accuracy':
    # Same 900 m / 400 m geometry and seeded 144 shots, translated into the
    # actual operating bay. All four positions clear the supplied coast by
    # >=404 m. No ship is moved in the saved level.
    for old, new in [('[750,-2150]','[11650,-4350]'),('[750,-1250]','[11650,-3450]'),('[650,-1250]','[11550,-3450]'),('[1050,-1250]','[11950,-3450]')]:
        code = code.replace(old, new)
    code = code.replace('hull(other).set_simulate_physics(False)', 'hull(other).set_simulate_physics(False);other.set_actor_tick_enabled(False)')
    code = code.replace('hull(v).set_simulate_physics(True)', 'hull(v).set_simulate_physics(True);v.set_actor_tick_enabled(True)')
if fixture == 'destruction':
    code = code.replace('V2_WRECK_', 'QUANGNINH_WRECK_')
scope.update(__name__='quangninh_'+fixture, __file__=str(path))
exec(compile(code, str(path), 'exec'), scope)
