"""Prepare a lightweight, camera-facing wreck fire/smoke fallback.

Editor only, outside PIE. Creates two ordinary surface materials using existing
textures. Set APPLY_BLUEPRINT=True only after the disposable visual prototype
passes; that adds hidden, non-colliding components without changing force graphs.
"""
import datetime,hashlib,json,shutil
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir())
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,'Stop PIE before editing assets'
if not globals().get('REPAIR_BUILDING',False):
    assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True,True)
backup=root/'Saved'/('WreckBillboardRecovery_'+datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d_%H%M%S'))
backup.mkdir(parents=True,exist_ok=True)
ship_file=root/'Content/Ship/BluePrint/BP_Ship.uasset'
shutil.copy2(ship_file,backup/'BP_Ship.uasset')
report=dict(before_ship_sha256=hashlib.sha256(ship_file.read_bytes()).hexdigest(),backup=str(backup),materials=[],components=[])
api=unreal.MaterialEditingLibrary
folder='/Game/NavalBattle/FX'
textures='/Game/Ship/Assets/VigilanteContent/Shared/Particles/Textures/Smoke/'

def expr(mat,cls,x,y,**properties):
    node=api.create_material_expression(mat,cls,x,y)
    assert node
    for key,value in properties.items():node.set_editor_property(key,value)
    return node

def link(a,out,b,pin):assert api.connect_material_expressions(a,out,b,pin),(out,pin)

def sample(mat,name,x,y):
    texture=unreal.load_asset(textures+name);assert texture,name
    sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    compression=texture.get_editor_property('compression_settings')
    if compression==unreal.TextureCompressionSettings.TC_MASKS:sampler=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    elif compression==unreal.TextureCompressionSettings.TC_GRAYSCALE:
        sampler=unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if texture.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    return expr(mat,unreal.MaterialExpressionTextureSample,x,y,texture=texture,sampler_type=sampler)

def material(name,fire):
    path=folder+'/'+name
    existing=unreal.load_asset(path)
    if existing:
        assert isinstance(existing,unreal.Material),path+' is occupied by another asset type'
        marker=unreal.EditorAssetLibrary.get_metadata_tag(existing,'NavalWreckFactory')
        if marker=='BillboardV1' and not globals().get('REVISE_OWN_MATERIALS',False):return existing
        file=root/'Content'/(path.removeprefix('/Game/')+'.uasset')
        revise=marker=='BillboardV1' and globals().get('REVISE_OWN_MATERIALS',False)
        assert revise or (marker=='BillboardV1-building' and globals().get('REPAIR_BUILDING',False) and not file.exists()),path+' exists without a completed factory marker; inspect before modifying'
        if file.exists():shutil.copy2(file,backup/file.name)
        mat=existing;mat.modify();api.delete_all_material_expressions(mat)
    else:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
    assert mat
    unreal.EditorAssetLibrary.set_metadata_tag(mat,'NavalWreckFactory','BillboardV1-building')
    mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE if fire else unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided',True)
    time=expr(mat,unreal.MaterialExpressionTime,-1100,400)
    phase=expr(mat,unreal.MaterialExpressionScalarParameter,-1100,550,parameter_name='Phase',default_value=0)
    add=expr(mat,unreal.MaterialExpressionAdd,-900,400);link(time,'',add,'A');link(phase,'',add,'B')
    pan=expr(mat,unreal.MaterialExpressionPanner,-700,300,speed_y=.35 if fire else .06)
    link(add,'',pan,'Time')
    image=sample(mat,'T_Fire_Seamless_02' if fire else 'T_Smoke_Variation_01_CH',-450,200)
    link(pan,'',image,'') # First texture input is UVs in UE 5.6.
    opacity=expr(mat,unreal.MaterialExpressionMultiply,0,350)
    if fire:
        mask=sample(mat,'T_Fire_Mask_01',-450,500)
        link(image,'R',opacity,'A');link(mask,'R',opacity,'B')
        brightness=expr(mat,unreal.MaterialExpressionScalarParameter,-400,-100,parameter_name='Brightness',default_value=25000)
        warm=expr(mat,unreal.MaterialExpressionConstant3Vector,-450,-300,constant=unreal.LinearColor(1,.22,.015,1))
        tint=expr(mat,unreal.MaterialExpressionMultiply,-200,-200)
        link(image,'R',tint,'A');link(warm,'',tint,'B')
        emission=expr(mat,unreal.MaterialExpressionMultiply,0,0)
        link(tint,'',emission,'A');link(brightness,'',emission,'B')
    else:
        uv=expr(mat,unreal.MaterialExpressionTextureCoordinate,-1100,800)
        center=expr(mat,unreal.MaterialExpressionConstant2Vector,-1100,950,r=.5,g=.5)
        distance=expr(mat,unreal.MaterialExpressionDistance,-850,800);link(uv,'',distance,'A');link(center,'',distance,'B')
        radius=expr(mat,unreal.MaterialExpressionMultiply,-650,800,const_b=2)
        link(distance,'',radius,'A')
        inverse=expr(mat,unreal.MaterialExpressionOneMinus,-450,800);link(radius,'',inverse,'')
        edge=expr(mat,unreal.MaterialExpressionSaturate,-250,800);link(inverse,'',edge,'')
        density=expr(mat,unreal.MaterialExpressionConstant,-400,650,r=.85)
        link(density,'',opacity,'A');link(edge,'',opacity,'B')
        emission=expr(mat,unreal.MaterialExpressionConstant3Vector,0,0,constant=unreal.LinearColor(.055,.06,.065,1))
    assert api.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert api.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    api.layout_material_expressions(mat);api.recompile_material(mat)
    unreal.EditorAssetLibrary.set_metadata_tag(mat,'NavalWreckFactory','BillboardV1')
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
    return mat

fire=material('M_WreckFlamesBillboard',True)
smoke=material('M_WreckSmokeBillboard',False)
report['materials']=[m.get_path_name() for m in [fire,smoke]]
if globals().get('APPLY_BLUEPRINT',False):
    bp=unreal.load_asset('/Game/Ship/BluePrint/BP_Ship')
    subsystem=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library=unreal.SubobjectDataBlueprintFunctionLibrary
    def objects():return [(h,library.get_object(library.get_data(h))) for h in subsystem.k2_gather_subobject_data_for_blueprint(bp)]
    hull_handle,hull=next((h,o) for h,o in objects() if isinstance(o,unreal.StaticMeshComponent) and o.get_name().startswith('ShipSM'))
    effect_class=unreal.load_class(None,'/Script/VDTPhase2Project.ShipWreckBillboardComponent')
    assert effect_class,'Build the dedicated wreck billboard component before applying Blueprint components'
    # Mesh bounds include the mast (~43 m); the actual deck is near 5 m.
    # Native placement keeps the plume rising along geographic up after capsize.
    deck=500
    specs=[('WreckFire1',fire,-3500,-300,1200,2200),('WreckFire2',fire,0,100,1500,2600),('WreckFire3',fire,3000,200,1000,1800),('WreckSmoke1',smoke,-2000,0,3000,5500),('WreckSmoke2',smoke,2000,0,2500,4500)]
    for name,mat,x,y,sx,sy in specs:
        component=next((o for h,o in objects() if isinstance(o,unreal.MaterialBillboardComponent) and o.get_name().removesuffix('_GEN_VARIABLE')==name),None)
        if not component:
            handle,error=subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=hull_handle,new_class=effect_class,blueprint_context=bp))
            assert not str(error),str(error)
            assert subsystem.rename_subobject(handle,unreal.Text(name))
            component=library.get_object(library.get_data(handle));component.modify()
            component.set_editor_property('deck_offset_cm',unreal.Vector(x,y,deck+sy))
            component.set_editor_property('phase',len(report['components'])*.37)
            component.set_editor_property('component_tags',[unreal.Name('NavalWreckFX')])
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            component.set_editor_property('cast_shadow',False)
            component.add_element(mat,None,False,sx,sy,None)
            component.set_visibility(False,True);component.set_hidden_in_game(True,True)
        report['components'].append(dict(name=component.get_name(),path=component.get_path_name(),hidden=component.get_editor_property('hidden_in_game')))
    result=unreal.ShipEnemyNavigationComponent.compile_battle_blueprints();assert result.startswith('OK:'),result
    validation=unreal.ShipEnemyNavigationComponent.validate_installed_blueprint();assert validation.startswith('OK:'),validation
    report['force_graph_validation']=validation
    assert unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_package()],False)
report['after_ship_sha256']=hashlib.sha256(ship_file.read_bytes()).hexdigest()
(root/'Docs/NavalCombat/WRECK_BILLBOARD_SETUP.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report))
