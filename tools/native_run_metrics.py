"""Summarize native frame submissions; these are not monitor scanout metrics."""
import argparse,csv,json,math,re
from pathlib import Path

def stats(values):
    if not values:return {'intervals':0}
    a=sorted(values)
    return {'intervals':len(a),'submission_fps':1000*len(a)/sum(a),
            'mean_ms':sum(a)/len(a),'p95_ms':a[math.ceil(.95*len(a))-1],
            'p99_ms':a[math.ceil(.99*len(a))-1],'max_ms':a[-1],
            'over_25ms':sum(x>25 for x in a),'over_33_37ms':sum(x>33.37 for x in a),
            'over_50ms':sum(x>50 for x in a)}

def summarize(folder):
    folder=Path(folder);path=folder/'frames.csv';config=json.loads((folder/'metrics-config.json').read_text(encoding='utf-8-sig'))
    result={'mode':config['performance_mode'],'startup_preparation':config['startup_preparation'],
            'texture_optimization':config['texture_optimization'],
            'cold_application_cache':config.get('cold_application_cache'),
            'pipeline_cache_override':config.get('pipeline_cache_override'),
            'measurement':'CPU frame submissions; not unique game frames or monitor scanout.',
            'timeline_available':path.exists(),'rows':0,'empty_fields':0,
            'scope':'Whole manual run, including menus/loading. No automatic mission-boundary detection.'}
    intervals=[];all_intervals=[];windows={};worst=[];previous=None;start=None
    if path.exists():
        with path.open(newline='') as f:
            for raw in csv.DictReader(f):
                row={k:int(v) for k,v in raw.items()};result['rows']+=1
                if start is None:start=row['present_ns']
                elapsed=(row['present_ns']-start)/1e9
                result['duration_seconds']=elapsed
                result['empty_fields']+=row['draws']==0
                if previous:
                    ms=(row['present_ns']-previous['present_ns'])/1e6
                    if ms>0:
                        all_intervals.append(ms)
                        if not row['modal'] and not previous['modal'] and previous['ready_ns']:
                            intervals.append(ms);windows.setdefault(int(elapsed//10)*10,[]).append(ms)
                            if ms>25:worst.append({'elapsed_s':elapsed,'source_field':row['source'],'interval_ms':ms,
                                'ready_gap_ms':(row['ready_ns']-previous['ready_ns'])/1e6,
                                **{k.removesuffix('_ns')+'_ms':row.get(k,0)/1e6 for k in
                                   ('host_ns','record_ns','fence_ns','acquire_ns','alloc_ns','present_api_ns')}})
                previous=row
    result['all_submissions']=stats(all_intervals)
    result['excluding_modal_and_initial_unready']=stats(intervals)
    result['spike_threshold_ms']=25
    (folder/'metrics-spikes.json').write_text(json.dumps(worst,indent=2),encoding='utf-8')
    result['worst_intervals']=sorted(worst,key=lambda x:x['interval_ms'],reverse=True)[:30]
    raw_log=(folder/'game.log').read_bytes() if (folder/'game.log').exists() else b''
    log=raw_log.decode('utf-16' if raw_log.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig',errors='replace')
    # PowerShell 5 can wrap stderr records across lines: parse whitespace, not lines.
    sync_pattern = (r'ac4 audio sync:\s*field=(\d+)\s+queued_ms=([\d.]+)\s+'
                    r'peak_ms=([\d.]+)\s+produced=(\d+)\s+consumed=(\d+)\s+'
                    r'active_starved_ms=([\d.]+)\s+overflow=(\d+)\s+'
                    r'max_producer_gap_fields=(\d+)')
    sync = [dict(zip(('field','queued_ms','peak_ms','produced','consumed',
                      'active_starved_ms','overflow','max_producer_gap_fields'),
                     (int(a),float(b),float(c),int(d),int(e),float(f),int(g),int(h))))
            for a,b,c,d,e,f,g,h in re.findall(sync_pattern,log)]
    result['audio_sync_samples'] = sync
    result['audio_sync_note'] = ('Software stream queue only; excludes device latency. '
        'Starvation counts requested frames missing while a source was marked active; '
        'it does not measure lip sync or prove audible corruption.')
    result['pipeline_compiles'] = [dict(key=k,time_ms=float(ms),phase=phase)
        for k,ms,phase in re.findall(r'vk: pipeline compile key=([0-9A-Fa-f]+)\s+'
                                    r'time_ms=([\d.]+)\s+phase=(startup|gameplay)',log)]
    result['slow_operations']=[{'field':int(field) if field else None,'operation':op,'time_ms':float(ms)}
        for field,op,ms in re.findall(r'ac4 spike:\s*(?:field=(\d+)\s+)?op=([\w-]+)\s+ms=([\d.]+)',log)]
    result['capacity_reached']='frame timeline rows (capacity reached)' in log
    result['runtime_warnings']=[line for line in log.splitlines() if any(k in line.lower() for k in ('store exhausted','staging exhausted','allocation failed','could not write frame timeline'))]
    result['runtime_metrics']=[line for line in log.splitlines() if any(k in line for k in ('vk-pacing:','vk-handoff:','fields dropped','vu1 recompiled:','texture pressure collection:'))]
    result['warning']=('Timeline missing: abrupt termination may prevent the in-memory trace from being saved.' if not path.exists()
                       else 'Timeline capacity reached; summary covers the recorded prefix only.' if result['capacity_reached'] else None)
    (folder/'metrics-summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    with (folder/'metrics-windows.csv').open('w',newline='') as f:
        columns=['start_seconds','intervals','submission_fps','mean_ms','p95_ms','p99_ms','max_ms','over_25ms','over_33_37ms','over_50ms']
        writer=csv.DictWriter(f,fieldnames=columns);writer.writeheader()
        for sec,values in sorted(windows.items()):writer.writerow({'start_seconds':sec,**stats(values)})
    print('Metrics saved:',folder/'metrics-summary.json')
    print(json.dumps(result['excluding_modal_and_initial_unready']))
    if result['warning']:print(result['warning'])
    return result

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('run',type=Path);args=parser.parse_args();summarize(args.run)
