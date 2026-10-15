from pathlib import Path
import os,sys,subprocess,shutil,time,json,hashlib,re
r=Path(__file__).resolve().parents[1];w=r/'work/native60-live-20261013'
import argparse,datetime
parser=argparse.ArgumentParser(description='Live native 4K performance test; isolated save, no mid-run screenshots.')
parser.add_argument('scene',choices=['opening','room'])
parser.add_argument('--cold-cache',action='store_true',help='Start with an empty application pipeline cache; driver/OS caches are unchanged.')
parser.add_argument('--no-image-prewarm',action='store_true',help='Disable startup image reservation for comparison.')
parser.add_argument('--legacy-texture-decode',action='store_true',help='Disable partial T4 decoding and source-digest filtering for comparison.')
parser.add_argument('--timeline',action='store_true',help='Save bounded in-memory CPU frame and field-clock timelines.')
parser.add_argument('--display-trace',action='store_true',help='Optional PresentMon ETW capture; inspect for lost events before using it.')
parser.add_argument('--startup-preparation',choices=['On','Off'],default='On',help='Prepare known pipelines and reserve rendering images before gameplay.')
args=parser.parse_args()
tag=args.scene+'-queue-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
opening=args.scene=='opening';run=w/tag;run.mkdir(exist_ok=False)
print('Live benchmark:',run,flush=True)
save=run/'saves';save.mkdir(exist_ok=True)
card=r/('work/native60-20261013/before/saves/card0.ps2mc' if opening else 'work/intro-perf-20261013/route-saves/card0.ps2mc')
shutil.copy2(card,save/'card0.ps2mc')
env=os.environ.copy();env['PATH']=str(r.parent/'toolchain/msys64/ucrt64/bin')+';'+env['PATH']
for k in list(env):
 if k.startswith('PS2_'):del env[k]
env.update(PS2_VU_RECOMP='1',PS2_SPEED_LOG='1',PS2_PAD_PLAY_STOP='0',PS2_SAVE_DIR=str(save),PS2_SETTINGS_FILE=str(r/'build/native-poc/ac4_settings.ini'),PS2_SHADER_DIR=str(r/'build/native-poc/shaders'),PS2_FPS_CAP='60',PS2_AC4_OPENING='Show' if opening else 'GameDefault',PS2_PIPELINE_CACHE=str(r/'build/native-poc/ac4_pipelines'),PS2_SHOT_EVERY='0',PS2_PAD_PLAY=str(r/'work/intro-perf-20261013/neutral.pad') if opening else str(w/'aircraft-route.pad'),PS2_PRESENT_QUEUE='1' if 'queue' in tag else '0')
# Give short recorded presses six fields so a delayed game update sees them.
# Preserve every original press and a two-field release before the next one.
if not opening:
 import struct
 pad=bytearray((w/'aircraft-route.pad').read_bytes())
 assert len(pad)>=8 and (len(pad)-8)%24==0
 n=(len(pad)-8)//24
 buttons=[struct.unpack_from('<H',pad,8+i*24+4)[0] for i in range(n)]
 runs=[];i=0
 while i<n:
  if not buttons[i]:i+=1;continue
  start=i;mask=buttons[i]
  while i<n and buttons[i]==mask:i+=1
  runs.append((start,i,mask))
 for j,(start,end,mask) in enumerate(runs):
  next_start=runs[j+1][0] if j+1<len(runs) else n
  for i in range(end,min(start+6,next_start-2)):
   assert buttons[i]==0
   struct.pack_into('<H',pad,8+i*24+4,mask)
 path=run/'room-input.pad';path.write_bytes(pad);env['PS2_PAD_PLAY']=str(path)
if args.timeline:
 env['PS2_FRAME_TIMELINE']=str(run/'frames.csv')
 env['PS2_CLOCK_TIMELINE']=str(run/'clock.csv')
env['PS2_PIPELINE_PREWARM']='1' if args.startup_preparation=='On' else '0'
if args.startup_preparation=='On' and not args.no_image_prewarm:env.update(PS2_IMAGE_PREWARM='15',PS2_SNAPSHOT_PREWARM='8',PS2_RT_RESERVE_HEIGHT='896')
if args.cold_cache:env['PS2_PIPELINE_CACHE']=str(run/'cold-pipelines')
if not args.legacy_texture_decode:env.update(PS2_T4_PARTIAL='1',PS2_T4_SOURCE_HASH='1')
exe=r/'build/native-poc/ac4.exe';fields=5000 if opening else 10500
(run/'config.json').write_text(json.dumps({'exe':str(exe),'sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'fields':fields,'env':{k:v for k,v in env.items() if k.startswith('PS2_')}},indent=2))
def background_load_snapshot():
 # Observe the user-owned background game; never stop or restart it.
 command="@(Get-Process -Name doax2_native_guest -ErrorAction SilentlyContinue | Select-Object ProcessName,Id,CPU) | ConvertTo-Json -Compress"
 result=subprocess.run(['powershell.exe','-NoProfile','-Command',command],capture_output=True,text=True)
 try:return json.loads(result.stdout) if result.stdout.strip() else []
 except ValueError:return {'unavailable':True}
background_before=background_load_snapshot()
start=time.monotonic()
with (run/'run.log').open('wb') as log,(run/'presentmon.log').open('wb') as plog:
 game=subprocess.Popen([str(exe),'--data',str(r/'generated'),'--disc',str(r.parent/'Ace Combat 04 - Shattered Skies (USA).iso'),'--seconds','0','--vblanks',str(fields),'--watchdog','30'],env=env,cwd=run,stdout=log,stderr=subprocess.STDOUT)
 pm=subprocess.Popen([str(r.parent/'toolchain/presentmon/PresentMon-2.6.0-x64.exe'),'--process_id',str(game.pid),'--terminate_on_proc_exit','--timed','90' if opening else '185','--terminate_after_timed','--no_console_stats','--no_track_input','--output_file',str(run/'display.csv'),'--session_name','AC4Native60-'+tag],cwd=run,stdout=plog,stderr=subprocess.STDOUT) if args.display_trace else None
 try:rc=game.wait(timeout=250)
 except subprocess.TimeoutExpired:
  game.terminate();game.wait();raise
 finally:
  if pm is not None:
   try:pm.wait(timeout=20)
   except subprocess.TimeoutExpired:pm.terminate();pm.wait()
assert rc==0,rc
text=(run/'run.log').read_text(errors='replace')
summary={'exit':rc,'presentmon_exit':pm.returncode if pm is not None else None,'wall_seconds':time.monotonic()-start,'metrics':[x for x in text.splitlines() if any(s in x for s in ['vk-native-pace:','vk-handoff:','vk-pacing:','fields dropped','vk: ','vu1 recompiled:'])][-11:]}
m=re.search(r'vu1 recompiled: ([0-9]+) runs completed',text)
summary['background_game_before']=background_before
summary['background_game_after']=background_load_snapshot()
summary['display_trace_available']=(run/'display.csv').exists()
summary['native_vu_runs']=int(m.group(1)) if m else 0
summary['room_workload_reached']=None if opening else summary['native_vu_runs']>5000000
# These are recorded-input field ranges, not semantic scene detection.
# A successful workload guard and final image must still confirm navigation.
if args.timeline:
 import csv
 with (run/'frames.csv').open() as f:rows=list(csv.DictReader(f))
 ranges=([(395,1600),(1600,3500),(3500,4999)] if opening
         else [(1150,5300),(5800,6600),(6600,10000)])
 summary['recorded_route_field_ranges']={}
 for lo,hi in ranges:
  subset=[x for x in rows if lo<=int(x['source'])<=hi]
  gaps=sorted((int(b['present_ns'])-int(a['present_ns']))/1e6
              for a,b in zip(subset,subset[1:]))
  if not gaps:continue
  summary['recorded_route_field_ranges'][f'{lo}:{hi}']={
   'p95_ms':gaps[int(.95*len(gaps))], 'p99_ms':gaps[int(.99*len(gaps))],
   'max_ms':max(gaps), 'intervals_over_25ms':sum(x>25 for x in gaps),
   'empty_fields':sum(int(x['draws'])==0 for x in subset)}
summary['acceptance_note']='Check frame-time spikes and final image; average game clock alone does not prove locked 60.'
(run/'summary.json').write_text(json.dumps(summary,indent=2))
print(tag,json.dumps(summary),flush=True)

if not opening and not summary["room_workload_reached"]:
 raise RuntimeError("room navigation did not reach the expected 3D workload; inspect final image and input timing")
