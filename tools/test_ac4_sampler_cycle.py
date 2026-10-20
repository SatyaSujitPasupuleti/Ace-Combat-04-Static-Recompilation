"""Real-GPU native texture/HUD sampler transitions versus fresh-process controls."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import setup_ac4 as setup
from test_ac4_model_gpu import require_complete_capture, require_visible_ppm
from test_ac4_screen_gpu import validate_screen_report


def validate_cycle(report):
    rows=re.findall(r'\[REPLAY\] sampler cycle pass (\d+): (\d+)x',report)
    if rows!=[('1','16'),('2','0'),('3','4'),('4','16')]:
        raise ValueError('Missing or out-of-order sampler transitions')
    updates=re.findall(r'vk: anisotropic filtering (\d+)x',report)
    if updates!=['0','4','16']:raise ValueError('Sampler updates did not occur in the requested order')
    validate_screen_report(report,True,True)
    rows=re.findall(r'ac4 model textures: (\d+) native mip draws, (\d+) allocation fallbacks, (\d+) unsupported states',report)
    if len(rows)!=1 or not int(rows[0][0]) or int(rows[0][1]):
        raise ValueError('Missing native texture coverage or allocation fallbacks')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('capture',type=Path)
    ap.add_argument('--height',type=int,choices=(1080,2160),default=2160)
    ap.add_argument('--fields',type=int,default=2)
    ap.add_argument('--output-root',type=Path,required=True)
    args=ap.parse_args();require_complete_capture(args.capture,args.fields)
    root=setup.ROOT;build=root/'build/native-poc'
    args.output_root.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='sampler-cycle-',dir=args.output_root.resolve()))
    env=setup.environment(json.loads(setup.LOCAL.read_text()))
    env.update(PS2_SHADER_DIR=str(build/'shaders'),PS2_UPSCALE='1',PS2_RN_2D_SCALE='0',PS2_LOCKSTEP='1',PS2_VU_RECOMP='1',PS2_RN_2D_OVERLAY='1',PS2_AC4_SUPPLIED_MIPS='1',PS2_AC4_SUPPLIED_FILTER='2')
    for flag in ('MODEL_MESH','MODEL_MATERIAL','MODEL_TEXTURE','MODEL_NATIVE','LIT_NATIVE','TERRAIN_NATIVE','TERRAIN_MESH','SCREEN_2D','SCREEN_PRECOMPOSE'):
        env['PS2_AC4_'+flag]='1'
    print(f'Private evidence: {out}',flush=True)
    for mode,value in [('cycle',16),('fresh16',16),('fresh0',0),('fresh4',4)]:
        settings=out/f'{mode}.ini'
        settings.write_text(f'[graphics]\nanisotropy={value}\n[display]\nwindow_mode=0\nwindow_width={args.height*16//9}\nwindow_height={args.height}\n')
        env['PS2_SETTINGS_FILE']=str(settings)
        (out/f'{mode}-config.json').write_text(json.dumps({k:v for k,v in env.items() if k.startswith('PS2_')},indent=2))
        with (out/f'{mode}.log').open('w') as log:
            subprocess.run([str(build/'gsreplay.exe'),str(args.capture.resolve()),'--hidden','--loop','4' if mode=='cycle' else '1','--fields',str(args.fields),'--shot',str(out/f'{mode}.ppm')]+(['--aniso-cycle'] if mode=='cycle' else []),env=env,cwd=out,stdout=log,stderr=log,check=True,timeout=180)
        require_visible_ppm((out/f'{mode}.ppm').read_bytes())
    validate_cycle((out/'cycle.log').read_text(errors='replace'))
    for i,value in enumerate((16,0,4,16),1):
        actual=(out/f'cycle.ppm.pass{i}.ppm').read_bytes()
        expected=(out/f'fresh{value}.ppm').read_bytes();require_visible_ppm(actual)
        if actual!=expected:raise ValueError(f'Pass {i} differs from fresh {value}x control')
        print(f'Pass {i}: {value}x matches fresh control, SHA256 {hashlib.sha256(actual).hexdigest()}')
    print('PASS: loaded native textures and high-resolution display survive 16/off/4/16 sampler changes.')
    print('This tests the renderer transition, not mouse/keyboard interaction with the settings UI.')

if __name__=='__main__':main()
