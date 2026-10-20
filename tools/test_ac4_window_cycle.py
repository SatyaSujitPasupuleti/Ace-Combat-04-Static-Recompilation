"""Real SDL/Vulkan window resizing with native geometry/HUD versus fresh controls."""
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
    expected=[('1','1280','720'),('2','1920','1080'),('3','3840','2160'),('4','1280','720')]
    rows=re.findall(r'\[REPLAY\] window cycle pass (\d+): (\d+)x(\d+)',report)
    if rows!=expected:raise ValueError('Missing or out-of-order window transitions')
    rows=re.findall(r'vk: replay window resized: requested (\d+)x(\d+), drawable (\d+)x(\d+)',report)
    if rows!=[(w,h,w,h) for _,w,h in expected]:raise ValueError('Requested window sizes were not applied to Vulkan')
    validate_screen_report(report,True,True)
    if 'overlay allocation failed' in report:raise ValueError('Overlay allocation failed during resize')
    if 'overlay list overflowed' in report:raise ValueError('Overlay overflow during resize')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('capture',type=Path)
    ap.add_argument('--fields',type=int,default=2)
    ap.add_argument('--output-root',type=Path,required=True)
    args=ap.parse_args();require_complete_capture(args.capture,args.fields)
    root=setup.ROOT;build=root/'build/native-poc'
    args.output_root.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='window-cycle-',dir=args.output_root.resolve()))
    env=setup.environment(json.loads(setup.LOCAL.read_text()))
    env.update(PS2_SHADER_DIR=str(build/'shaders'),PS2_UPSCALE='1',PS2_RN_2D_SCALE='0',PS2_LOCKSTEP='1',PS2_VU_RECOMP='1',PS2_RN_2D_OVERLAY='1',PS2_AC4_SUPPLIED_MIPS='1',PS2_AC4_SUPPLIED_FILTER='2')
    for flag in ('SKY_MESH','CLOUD_MESH','SPRITE_MESH','SCREEN_MASK','COCKPIT_MESH','COCKPIT_NATIVE','MODEL_MESH','MODEL_MATERIAL','MODEL_TEXTURE','MODEL_NATIVE','LIT_NATIVE','TERRAIN_NATIVE','TERRAIN_MESH','SCREEN_2D','SCREEN_PRECOMPOSE'):
        env['PS2_AC4_'+flag]='1'
    print(f'Private evidence: {out}',flush=True)
    for mode,height in [('cycle',720),('fresh720',720),('fresh1080',1080),('fresh2160',2160)]:
        settings=out/f'{mode}.ini'
        settings.write_text(f'[graphics]\nanisotropy=16\n[display]\nwindow_mode=0\nwindow_width={height*16//9}\nwindow_height={height}\n')
        env['PS2_SETTINGS_FILE']=str(settings)
        (out/f'{mode}-config.json').write_text(json.dumps({k:v for k,v in env.items() if k.startswith('PS2_')},indent=2))
        with (out/f'{mode}.log').open('w') as log:
            subprocess.run([str(build/'gsreplay.exe'),str(args.capture.resolve()),'--hidden','--loop','4' if mode=='cycle' else '1','--fields',str(args.fields),'--shot',str(out/f'{mode}.ppm')]+(['--window-cycle'] if mode=='cycle' else []),env=env,cwd=out,stdout=log,stderr=log,check=True,timeout=180)
        require_visible_ppm((out/f'{mode}.ppm').read_bytes())
    validate_cycle((out/'cycle.log').read_text(errors='replace'))
    for i,value in enumerate((720,1080,2160,720),1):
        actual=(out/f'cycle.ppm.pass{i}.ppm').read_bytes()
        expected=(out/f'fresh{value}.ppm').read_bytes();require_visible_ppm(actual)
        if actual!=expected:raise ValueError(f'Pass {i} differs from fresh {value}p control')
        print(f'Pass {i}: {value}p matches fresh control, SHA256 {hashlib.sha256(actual).hexdigest()}')
    print('PASS: native geometry and HUD survive 720p/1080p/4K/720p window changes.')
    print('This resizes a real hidden SDL/Vulkan window; interactive dragging, Alt-Tab and the options UI are separate tests.')

if __name__=='__main__':main()
