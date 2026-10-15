import csv,json,tempfile
from pathlib import Path
from native_run_metrics import summarize,stats
assert stats([10,20,30,40])['submission_fps']==40
assert stats([10,20,30,40])['p99_ms']==40
assert stats([])=={'intervals':0}
with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent,prefix='metrics-unit-') as tmp:
 assert Path(tmp).resolve().is_relative_to(Path(__file__).resolve().parent)
 p=Path(tmp);(p/'metrics-config.json').write_text(json.dumps({'performance_mode':'Old','startup_preparation':'Off','texture_optimization':False}))
 with (p/'frames.csv').open('w',newline='') as f:
  fields=['source','present_ns','ready_ns','draws','modal'];writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader()
  for i,(ns,ready,modal) in enumerate([(0,0,0),(10_000_000,1,0),(30_000_000,1,0),(60_000_000,1,1),(90_000_000,1,0),(130_000_000,1,0)]):
   writer.writerow(dict(source=i,present_ns=ns,ready_ns=ready,draws=1,modal=modal))
 result=summarize(p)
 assert result['all_submissions']['intervals']==5
 assert result['excluding_modal_and_initial_unready']['intervals']==2
 assert result['excluding_modal_and_initial_unready']['mean_ms']==30
 assert result['excluding_modal_and_initial_unready']['over_25ms']==1
 # Real PowerShell stderr logs can be UTF-16 and wrap a single record.
 log=('ac4 audio sync: field=600 queued_ms=24.125 peak_ms=26.833 '
      'produced=162563 consumed=161405\nactive_starved_ms=7.396 overflow=0 '
      'max_producer_gap_fields=11\n'
      'ac4 spike: field=500 op=disc-read\nms=44.125 lsn=20 sectors=64\n'
      'ac4 spike: op=gfx-drain ms=31.500\n'
      'vk: pipeline compile key=0000020001A9001E time_ms=755.066\nphase=gameplay\n')
 for encoding in ('utf-8','utf-8-sig','utf-16'):
  (p/'game.log').write_text(log,encoding=encoding)
  result=summarize(p)
  assert result['slow_operations']==[{'field':500,'operation':'disc-read','time_ms':44.125},{'field':None,'operation':'gfx-drain','time_ms':31.5}]
  assert len(json.loads((p/'metrics-spikes.json').read_text()))==1
  assert result['worst_intervals'][0]['ready_gap_ms']==0
  assert len(result['audio_sync_samples'])==1
  sample=result['audio_sync_samples'][0]
  assert sample['queued_ms']==24.125 and sample['consumed']==161405
  assert sample['active_starved_ms']==7.396 and sample['max_producer_gap_fields']==11
  assert result['pipeline_compiles']==[{'key':'0000020001A9001E','time_ms':755.066,'phase':'gameplay'}]
 (p/'frames.csv').unlink();result=summarize(p)
 assert not result['timeline_available'] and result['warning']
print('PASS metrics arithmetic, exclusions, missing trace, wrapped audio/pipeline records in UTF8/UTF8-BOM/UTF16')
