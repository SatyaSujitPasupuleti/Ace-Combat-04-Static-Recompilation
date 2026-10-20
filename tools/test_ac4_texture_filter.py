"""Private GPU comparison of preserved AC4 mip storage and hardware filtering.
Exact mode must match GS. Filtered images are evidence for review, not an
automatic visual-quality acceptance or a full-game parity claim.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import setup_ac4 as setup
from test_ac4_model_gpu import require_complete_capture, require_visible_ppm


def validate_filter_report(report, mode):
    supplied=re.findall(r'ac4 supplied mip chains: (\d+) draws',report)
    uploads=re.findall(r'ac4 model textures: (\d+) native mip draws, (\d+) allocation fallbacks, (\d+) unsupported states',report)
    if len(supplied)!=1 or not int(supplied[0]) or len(uploads)!=1 or int(uploads[0][1]):
        raise ValueError('Require supplied mip coverage with zero allocation fallbacks')
    count=int(supplied[0])
    if int(uploads[0][0])<count:
        raise ValueError('Supplied mip coverage exceeds total native mip draws')
    filters=re.findall(r'ac4 supplied filtering: (\d+) hardware, (\d+) anisotropic draws',report)
    if mode=='exact':
        if filters:raise ValueError('Exact reference unexpectedly enables hardware filtering')
    else:
        if mode not in ('trilinear','anisotropic'):raise ValueError('Unknown filter mode')
        if len(filters)!=1:raise ValueError('Missing or duplicate filter coverage')
        hardware,aniso=map(int,filters[0])
        if not hardware or hardware>count or aniso!=(hardware if mode=='anisotropic' else 0):
            raise ValueError('Invalid hardware/anisotropic filter coverage')
    if mode=='anisotropic':
        samplers=re.findall(r'vk: native mip sampler anisotropy ([0-9.]+)x',report)
        if len(samplers)!=1 or float(samplers[0])<2:
            raise ValueError('Anisotropic mode requires an enabled native anisotropic sampler')
    return count


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('capture',type=Path)
    ap.add_argument('--fields',type=int,default=2)
    ap.add_argument('--anisotropy',type=int,choices=(2,4,8,16),default=16)
    ap.add_argument('--scale',type=int,choices=range(1,9),default=1)
    ap.add_argument('--output-root',type=Path)
    args=ap.parse_args();require_complete_capture(args.capture,args.fields)
    root=setup.ROOT;build=root/'build/native-poc'
    work=(args.output_root or root/'work').resolve();work.mkdir(parents=True,exist_ok=True)
    out=Path(tempfile.mkdtemp(prefix='texture-filter-',dir=work))
    env=setup.environment(json.loads(setup.LOCAL.read_text()))
    env.update(PS2_SHADER_DIR=str(build/'shaders'),PS2_SETTINGS_FILE=str(out/'settings.ini'),
               PS2_UPSCALE=str(args.scale),PS2_LOCKSTEP='1',PS2_VU_RECOMP='1')
    flags=('MODEL_MESH','MODEL_MATERIAL','MODEL_NATIVE','LIT_NATIVE','TERRAIN_NATIVE','TERRAIN_MESH')
    (out/'settings.ini').write_text(f'[graphics]\nanisotropy={args.anisotropy}\n')
    baseline=None;counts=[]
    print(f'Private artifacts: {out}',flush=True)
    for mode,setting in [('baseline','0'),('exact','0'),('trilinear','1'),('anisotropic','2')]:
        for flag in flags:env['PS2_AC4_'+flag]='0' if mode=='baseline' else '1'
        env['PS2_AC4_MODEL_TEXTURE']='0' if mode=='baseline' else '1'
        env['PS2_AC4_SUPPLIED_MIPS']='1';env['PS2_AC4_SUPPLIED_FILTER']=setting
        (out/f'{mode}-config.json').write_text(json.dumps({k:v for k,v in env.items() if k.startswith('PS2_')},indent=2))
        with (out/f'{mode}.log').open('w') as log:
            subprocess.run([str(build/'gsreplay.exe'),str(args.capture.resolve()),'--loop','1',
                '--fields',str(args.fields),'--hidden','--shot',str(out/f'{mode}.ppm')],
                cwd=out,env=env,stdout=log,stderr=log,check=True,timeout=180)
        data=(out/f'{mode}.ppm').read_bytes();require_visible_ppm(data)
        print(mode,hashlib.sha256(data).hexdigest(),flush=True)
        if mode=='baseline':baseline=data
        else:counts.append(validate_filter_report((out/f'{mode}.log').read_text(errors='replace'),mode))
        if mode=='exact' and data!=baseline:
            raise ValueError('Preserved-mip reference differs from GS; do not accept hardware comparisons')
    if len(set(counts))!=1:raise ValueError('Supplied mip coverage differs across modes')
    print(f'PASS: exact reference and {counts[0]} supplied mip draws per mode.')
    print('Filtered images require visual/temporal review. No image tolerance or FPS claim is implied.')

if __name__=='__main__':main()
