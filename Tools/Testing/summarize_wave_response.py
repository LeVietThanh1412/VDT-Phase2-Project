"""Offline statistics from unmodified PIE telemetry; no Unreal state mutation.

Run with the baseline and final Saved JSON paths. Absolute angle metrics include
trim; centered metrics describe oscillation independently. Initial baseline
surface readings came from a plane-only engine helper and are not compared.
"""
import argparse,json,math,statistics
from pathlib import Path

def percentile(values,q):
    values=sorted(values)
    return values[min(len(values)-1,int((len(values)-1)*q))]

def metrics(rows):
    out={'samples':len(rows)}
    for axis in ['roll','pitch']:
        values=[r[axis] for r in rows];mean=statistics.mean(values)
        out.update({axis+'_mean':mean,axis+'_rms':math.sqrt(statistics.mean(v*v for v in values)),
                    axis+'_p95':percentile([abs(v) for v in values],.95),
                    axis+'_max':max(abs(v) for v in values),
                    axis+'_motion_rms':math.sqrt(statistics.mean((v-mean)**2 for v in values)),
                    axis+'_motion_p95':percentile([abs(v-mean) for v in values],.95)})
    out.update(water_fraction=statistics.mean(r['water'] for r in rows),
               vertical_speed_p99=percentile([abs(r['vz']) for r in rows],.99),
               mean_forward_speed=statistics.mean(r['speed'] for r in rows),
               physics=all(r['physics'] and not r['dead'] for r in rows),
               shots=max(r['shots'] for r in rows))
    if 'angular_velocity' in rows[0]:
        out['angular_speed_p95']=percentile([math.sqrt(sum(v*v for v in r['angular_velocity'])) for r in rows],.95)
        out['wave_height_range']=max(r['surface_z'] for r in rows)-min(r['surface_z'] for r in rows)
        out['heave_relative_to_waves_range']=max(r['heave'] for r in rows)-min(r['heave'] for r in rows)
    return out

def main():
    parser=argparse.ArgumentParser();parser.add_argument('baseline',type=Path);parser.add_argument('after',type=Path)
    parser.add_argument('--output',type=Path,default=Path('Docs/Testing/BUOYANCY_METRICS.json'))
    args=parser.parse_args();data=[json.loads(p.read_text()) for p in [args.baseline,args.after]]
    assert all(d['status']=='PASSED' for d in data),'Only completed measurements are reportable'
    result={'definition':'Absolute RMS/P95 include mean trim; motion RMS/P95 are centered. Degrees and m/s.',
            'baseline_wave_sensor':'plane only; excluded from wave comparisons',
            'runs':[]}
    for label,d in zip(['baseline','tuned'],data):
        run={'label':label,'native_dll_sha256':d['native_dll_sha256'],'profiles':d['profiles'],'hulls':{}}
        for hull in ['Small','Medium','Large']:
            run['hulls'][hull]={phase:metrics([r for r in d['rows'] if r['cls']==hull and r['phase']==phase]) for phase in ['idle','cruise','turn','reverse','reverse_fire']}
        result['runs'].append(run)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,axes=plt.subplots(3,2,figsize=(12,8),sharex=True)
    for i,hull in enumerate(['Small','Medium','Large']):
        for label,d,color in zip(['Baseline','Tuned'],data,['#8898ac','#147da3']):
            rows=[r for r in d['rows'] if r['cls']==hull and r['phase']=='idle']
            for j,axis in enumerate(['roll','pitch']):
                axes[i,j].plot([r['age'] for r in rows],[r[axis] for r in rows],label=label,color=color,lw=1)
                axes[i,j].set_ylabel(hull+' '+axis+' (deg)');axes[i,j].grid(alpha=.2)
        for ax in axes[i]:ax.legend(loc='upper right',fontsize=8)
    for ax in axes[-1]:ax.set_xlabel('Simulation seconds after settling')
    fig.suptitle('Actual idle hull attitude in the existing Gerstner ocean; mean trim retained')
    fig.tight_layout();image=args.output.parent/'images/buoyancy-idle.png';image.parent.mkdir(exist_ok=True);fig.savefig(image,dpi=150);plt.close(fig)
    table=['| Hull | State | Roll RMS before/after | Pitch RMS before/after | Roll P95 before/after | Pitch P95 before/after |',
           '|---|---|---|---|---|---|']
    for hull in ['Small','Medium','Large']:
        for phase in ['idle','cruise','turn','reverse','reverse_fire']:
            a,b=[r['hulls'][hull][phase] for r in result['runs']]
            table.append('| '+hull+' | '+phase+' | '+' | '.join(f'{a[k]:.3f}/{b[k]:.3f}' for k in ['roll_rms','pitch_rms','roll_p95','pitch_p95'])+' |')
    args.output.with_suffix('.md').write_text('\n'.join(table)+'\n')
    print(args.output)

if __name__=='__main__':main()
