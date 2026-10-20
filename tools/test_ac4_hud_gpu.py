"""Compare native fixed-HUD composition and centering against a private GS capture."""
import argparse
import tempfile
from pathlib import Path
import sys,json,subprocess,re,hashlib
sys.path.insert(0,'tools')
import setup_ac4 as setup
from test_ac4_model_gpu import require_complete_capture,require_visible_ppm
from test_ac4_screen_gpu import validate_screen_report
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture',type=Path)
    parser.add_argument('--settings',type=Path,required=True,help='Private INI used by the captured game run')
    parser.add_argument('--output-root',type=Path,required=True)
    parser.add_argument('--scale',type=int,choices=(1,3),default=1)
    parser.add_argument('--fields',type=int,default=4)
    args=parser.parse_args()
    root=setup.ROOT;cap=args.capture.resolve();scale=args.scale
    require_complete_capture(cap,args.fields)
    args.output_root.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix=f'hud-gpu-{scale}x-',dir=args.output_root.resolve()))
    print(f'Private evidence: {out}',flush=True)
    reference=None;reports=[]
    for name,screen,overlay,layout in [('gs',0,0,0),('stretched-inplace',1,0,0),('stretched-overlay',1,1,0),('centered-overlay',1,1,1)]:
     d=out/name;d.mkdir(exist_ok=True)
     text=args.settings.read_text()
     for key,value in [('hud_layout',str(layout)),('widescreen','1')]:
      text=re.sub(r'(?m)^'+key+r'\s*=.*$',key+' = '+value,text)
     (d/'settings.ini').write_text(text)
     env=setup.environment(json.loads(setup.LOCAL.read_text()))
     env.update(PS2_SHADER_DIR=str(root/'build/native-poc/shaders'),PS2_SETTINGS_FILE=str(d/'settings.ini'),PS2_UPSCALE=str(scale),PS2_LOCKSTEP='1',PS2_AC4_SCREEN_2D=str(screen),PS2_AC4_SCREEN_PRECOMPOSE=str(overlay),PS2_AC4_SCREEN_MASK='1',PS2_RN_2D_OVERLAY=str(overlay),PS2_RN_2D_SCALE=str(scale),PS2_RN_2D_LOG=str(d/'layout.txt'),PS2_AC4_SKY_MESH='1',PS2_AC4_CLOUD_MESH='1',PS2_AC4_SPRITE_MESH='1')
     with (d/'run.log').open('w') as f:
      subprocess.run([str(root/'build/native-poc/gsreplay.exe'),str(cap),'--hidden','--loop','1','--fields',str(args.fields),'--shot',str(d/'frame.ppm')],cwd=d,env=env,stdout=f,stderr=f,check=True,timeout=120)
     image=(d/'frame.ppm').read_bytes();require_visible_ppm(image)
     log=(d/'run.log').read_text(errors='replace')
     coverage=validate_screen_report(log,bool(overlay),bool(overlay)) if screen else {}
     if reference is None:reference=image
     exact=image==reference
     reports.append(dict(mode=name,exact=exact,sha256=hashlib.sha256(image).hexdigest(),coverage=coverage))
     (out/'comparison.json').write_text(json.dumps(reports,indent=2))
     if layout==0 and not exact:raise RuntimeError(name+' differs from GS; inspect '+str(d))
     if layout==1 and (exact or 'elements re-anchored' not in log):raise RuntimeError('centered HUD not exercised')
     print(name,exact,coverage,flush=True)

    print('PASS: stretched native HUD matches GS; centered output changes placement. Inspect centered images for clipping and relative alignment.')

if __name__=='__main__':main()
