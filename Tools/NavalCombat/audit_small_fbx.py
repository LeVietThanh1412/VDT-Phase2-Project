"""Read-only FBX binary structure audit; inspect actual joints/skin, not names.

No Autodesk SDK/import/reimport is needed. Numeric arrays are skipped except
geometry positions used for source-space bounds. Does not write source assets.
"""
import hashlib,json,struct,zlib
from pathlib import Path
import numpy as np
root=Path(__file__).resolve().parents[2];path=root/'Content/ShipSmall/Asset/sweedenmilitaryboat.fbx'
data=path.read_bytes();assert data[:23]==b'Kaydara FBX Binary  \x00\x1a\x00'
version=struct.unpack_from('<I',data,23)[0];wide=version>=7500;null_size=25 if wide else 13
objects=[];settings={};arrays={};connections=[]
def prop(pos,node_name):
    kind=chr(data[pos]);pos+=1
    if kind in 'YCLIFD':
        fmt={'Y':'h','C':'?','L':'q','I':'i','F':'f','D':'d'}[kind];n=struct.calcsize(fmt)
        return struct.unpack_from('<'+fmt,data,pos)[0],pos+n
    if kind in 'SR':
        n=struct.unpack_from('<I',data,pos)[0];pos+=4;assert pos+n<=len(data)
        return (data[pos:pos+n].decode('utf-8','replace') if kind=='S' else {'raw_bytes':n}),pos+n
    if kind in 'fdlibc':
        count,encoding,n=struct.unpack_from('<III',data,pos);pos+=12;assert pos+n<=len(data)
        result=dict(array_type=kind,count=count,encoding=encoding,bytes=n)
        if node_name=='Vertices':
            raw=zlib.decompress(data[pos:pos+n]) if encoding else data[pos:pos+n]
            dtype={'f':'<f4','d':'<f8','l':'<i8','i':'<i4','b':'u1','c':'u1'}[kind]
            values=np.frombuffer(raw,dtype=dtype);assert len(values)==count and count%3==0
            points=values.reshape(-1,3);result.update(bounds_min=points.min(axis=0).tolist(),bounds_max=points.max(axis=0).tolist())
            # Native Unreal long axis is -source Y. Compare actual hull end widths,
            # below the deck, to identify the pointed bow without trusting a name.
            slices=[]
            for y in [-11,-9,-6,0,6,9,11]:
                section=points[(abs(points[:,1]-y)<.35) & (points[:,2]<2.5)]
                slices.append(dict(source_y=y,count=len(section),width=float(np.ptp(section[:,0])) if len(section) else None))
            result['hull_cross_sections']=slices
        return result,pos+n
    raise ValueError('Unknown FBX property type '+repr(kind))
def node(pos,parent=None):
    start=pos
    end,nprops,nbytes,nlen=struct.unpack_from('<QQQB' if wide else '<IIIB',data,pos);pos+=null_size
    if end==0:return None,start+null_size
    assert pos+nlen<=end<=len(data)
    name=data[pos:pos+nlen].decode('utf-8','replace');pos+=nlen;values=[];prop_start=pos
    for _ in range(nprops):value,pos=prop(pos,name);values.append(value)
    assert pos-prop_start==nbytes
    obj=dict(node=name,properties=values,children=[])
    while pos<end-null_size:
        child,new_pos=node(pos,obj)
        if child is None:break
        obj['children'].append(child);pos=new_pos
    if name in ['Model','NodeAttribute','Deformer','Geometry','Pose','AnimationStack','AnimationLayer','AnimationCurve']:
        objects.append(obj)
    if name=='P' and values and values[0] in ['UpAxis','UpAxisSign','FrontAxis','FrontAxisSign','CoordAxis','CoordAxisSign','UnitScaleFactor','OriginalUnitScaleFactor']:
        settings[str(values[0])]=values[-1]
    if name=='C':connections.append(values)
    return obj,end
pos=27
while pos<len(data)-null_size:
    item,pos=node(pos)
    if item is None:break
def describe(obj):
    result=dict(node=obj['node'],properties=obj['properties'])
    if obj['node']=='Geometry':
        result['geometry_arrays']={c['node']:c['properties'] for c in obj['children'] if c['node'] in ['Vertices','PolygonVertexIndex']}
    if obj['node']=='Model':
        result['transform_properties']=[p['properties'] for c in obj['children'] if c['node']=='Properties70' for p in c['children'] if p['properties'] and p['properties'][0] in ['Lcl Translation','Lcl Rotation','Lcl Scaling','GeometricTranslation','GeometricRotation','GeometricScaling']]
    return result
models=[o for o in objects if o['node']=='Model'];attributes=[o for o in objects if o['node']=='NodeAttribute'];deformers=[o for o in objects if o['node']=='Deformer']
report=dict(source=path.relative_to(root).as_posix(),sha256=hashlib.sha256(data).hexdigest(),version=version,settings=settings,
    models=[describe(o) for o in models],node_attributes=[describe(o) for o in attributes],deformers=[describe(o) for o in deformers],
    geometry=[describe(o) for o in objects if o['node']=='Geometry'],
    joint_models=[describe(o) for o in models if any(t in str(o['properties'][2:]) for t in ['LimbNode','Root','Skeleton'])],
    skin_deformers=[describe(o) for o in deformers if any(t in str(o['properties'][2:]) for t in ['Skin','Cluster'])],
    animation_nodes={name:sum(o['node']==name for o in objects) for name in ['AnimationStack','AnimationLayer','AnimationCurve']},connections=connections)
dest=root/'Docs/NavalCombat/SMALL_FBX_SKELETON_AUDIT.json';dest.write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k not in ['connections','node_attributes','deformers']},indent=2))
