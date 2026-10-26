"""Repeat physical island support with the final filter and repeated LOD views.

Uses the saved map. The reused fixture only adds disposable physical probes.
Counts collision proxies to distinguish intentional land caching from repeated
allocation on revisiting the same views; process RSS is not a leak assertion.
"""
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir())
cache_bytes = globals().get('CACHE_BYTES')
code = (root / 'Tools/NavalCombat/verify_quangninh_land_collision.py').read_text()
code = code.replace('LAND_COLLISION_PIE.json', 'LAND_COLLISION_FINAL_FILTER.json')
if cache_bytes is not None:
    code = code.replace('LAND_COLLISION_FINAL_FILTER.json', 'LAND_COLLISION_FORCED_EVICTION.json')
    code = code.replace("geo=next(iter(", "terrain.maximum_cached_bytes="+str(int(cache_bytes))+"\n        geo=next(iter(")
code = code.replace("if t>=42:", "if t>=125:")
code = code.replace("if t-st['last']>=1:", """if st['stage']>=4 and t>=40:
            # Repeat exactly the original views; allow streaming to settle.
            view=int((t-40)//15)%3
            eye=[unreal.Vector(-150000,-150000,240000),unreal.Vector(-150000,-150000,240000),unreal.Vector(-170000,-100000,8000)][view]
            rotation=[unreal.Rotator(pitch=-90),unreal.Rotator(pitch=10,yaw=90),unreal.Rotator(pitch=-15,yaw=-90)][view]
            st['camera'].set_actor_location(eye,False,True)
            st['camera'].set_actor_rotation(rotation,False)
        if t-st['last']>=1:""")
code = code.replace("land_triangles=f.retained_land_triangles,water_triangles=f.removed_water_triangles", "land_triangles=f.retained_land_triangles,water_triangles=f.removed_water_triangles,submerged_triangles=f.retained_submerged_triangles,tolerance_m=f.sea_level_tolerance_m,last_error=f.last_error,proxy_count=len(terrain.get_components_by_class(unreal.ProceduralMeshComponent))")
code = code.replace("final=st['rows'][-1]['probes']", """final=st['rows'][-1]['probes']
            settled=[r for r in st['rows'] if r['t']>95]
            check('Saved final filter uses 0.05 m tolerance',all(abs(r['tolerance_m']-.05)<1e-6 for r in st['rows']))
            check('Nonzero submerged collision geometry retained',max(r['submerged_triangles'] for r in st['rows'])>0)
            check('No collision build error',all(not r['last_error'] for r in st['rows']))
            check('Repeated settled views do not allocate duplicate collision proxies',bool(settled) and max(r['proxy_count'] for r in settled)==min(r['proxy_count'] for r in settled))""")
if cache_bytes is not None:
    code = code.replace("final=st['rows'][-1]['probes']", """final=st['rows'][-1]['probes']
            st['forced_cache_bytes']="""+str(int(cache_bytes))+"""
            recent=[r for r in st['rows'] if r['t']>95]
            check('Revisited collision source bookkeeping remains bounded',bool(recent) and max(r['tiles'] for r in recent)==min(r['tiles'] for r in recent))""")
exec(compile(code, str(root / 'Tools/NavalCombat/verify_quangninh_collision_regression.py'), 'exec'))
