"""Reproducible AC4 generation/build/trace driver. Run from any directory."""
import argparse, hashlib, json, os, pathlib, subprocess, sys, time

ROOT=pathlib.Path(__file__).resolve().parents[1]
BASE=ROOT.parent
PROFILE=json.loads((ROOT/'config/ac4/profile.json').read_text())
WORK=ROOT/'work'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def inputs():
    files=[ROOT/'CMakeLists.txt']
    for directory in ('runtime','config/ac4','tools/ps2recomp'):
        files.extend(p for p in (ROOT/directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    return {p.relative_to(ROOT).as_posix():sha(p) for p in sorted(files)}

def environment():
    env=os.environ.copy()
    prefix=BASE/'toolchain/msys64/ucrt64'
    env['PATH']=str(prefix/'bin')+os.pathsep+env['PATH']
    env['CMAKE_PREFIX_PATH']=str(prefix)
    env['PKG_CONFIG_PATH']=str(prefix/'lib/pkgconfig')
    env['PYTHONPATH']=str(ROOT/'tools')
    return env

def command(argv, log, timeout=None):
    print('Running:', ' '.join(map(str,argv)),flush=True)
    with log.open('w') as f:
        try:
            p=subprocess.run(list(map(str,argv)),cwd=ROOT,env=environment(),stdout=f,stderr=subprocess.STDOUT,timeout=timeout)
            code=p.returncode
        except subprocess.TimeoutExpired:
            code=124
    print(log.read_text(errors='replace')[-10000:])
    return code

def generate(elf):
    if sha(elf)!=PROFILE['elf_sha256']:
        raise SystemExit('ELF hash does not match the supported AC4 executable.')
    cfg=ROOT/'config/ac4'
    # Preserve timestamps for identical outputs so small profile edits rebuild
    # only affected translation units, rather than all generated game code.
    old={p.name:(sha(p),p.stat().st_atime_ns,p.stat().st_mtime_ns) for p in (ROOT/'generated').glob('*') if p.is_file()}
    cmd=[sys.executable,'-m','ps2recomp',elf,'-o',ROOT/'generated','--ida-db',cfg/'control_flow.json','--ida-seeds',cfg/'seeds.json','--symbols',cfg/'symbols.json','--overrides',cfg/'overrides.json','--hooks',cfg/'hooks.json','--report',WORK/'recompile.json']
    rc=command(cmd,WORK/'generate.log')
    if rc: raise SystemExit(rc)
    for p in (ROOT/'generated').iterdir():
        if p.is_file() and p.name in old and sha(p)==old[p.name][0]:
            os.utime(p,ns=old[p.name][1:])
    report=json.loads((WORK/'recompile.json').read_text())
    if report['unhandled'] or report['missing_targets']:
        raise SystemExit('Generation contains unsupported instructions/targets.')
    files={p.name:sha(p) for p in sorted((ROOT/'generated').iterdir()) if p.is_file()}
    manifest={'profile':PROFILE,'elf_sha256':sha(elf),'outputs':files,'inputs':inputs()}
    (WORK/'generation_manifest.json').write_text(json.dumps(manifest,indent=2))

def build(jobs):
    if jobs<1: raise SystemExit('--jobs must be positive')
    if not (WORK/'generation_manifest.json').is_file(): raise SystemExit('Run generate first.')
    generation=json.loads((WORK/'generation_manifest.json').read_text())
    current=inputs()
    prefixes=('config/ac4/','tools/ps2recomp/')
    required={k:v for k,v in current.items() if k.startswith(prefixes)}
    recorded={k:v for k,v in generation.get('inputs',{}).items() if k.startswith(prefixes)}
    if required!=recorded:
        raise SystemExit('Recompiler or AC4 configuration changed; run generate first.')
    for name,digest in generation['outputs'].items():
        if sha(ROOT/'generated'/name)!=digest: raise SystemExit('Generated artifact changed: '+name)
    prefix=BASE/'toolchain/msys64/ucrt64'
    cmake=prefix/'bin/cmake.exe'
    cmd=[cmake,'-S',ROOT,'-B',ROOT/'build/ac4','-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DPS2_DIAG=ON','-DPS2_GENERATED_OPT=-O0',f'-DCMAKE_C_COMPILER={prefix}/bin/gcc.exe',f'-DCMAKE_CXX_COMPILER={prefix}/bin/g++.exe',f'-DCMAKE_MAKE_PROGRAM={prefix}/bin/ninja.exe']
    rc=command(cmd,WORK/'configure.log')
    if not rc: rc=command([cmake,'--build',ROOT/'build/ac4','--target','ac4','-j',jobs],WORK/'build.log')
    if rc: raise SystemExit(rc)
    version=subprocess.check_output([str(prefix/'bin/gcc.exe'),'--version'],env=environment(),text=True).splitlines()[0]
    (WORK/'build_manifest.json').write_text(json.dumps({'compiler':version,'executable_sha256':sha(ROOT/'build/ac4/ac4.exe'),'generation_manifest_sha256':sha(WORK/'generation_manifest.json'),'inputs':inputs()},indent=2))

def trace(disc,seconds,video,vblanks=300):
    if vblanks<0: raise SystemExit('--vblanks cannot be negative')
    if seconds<1: raise SystemExit('--seconds must be positive')
    if not disc.is_file(): raise SystemExit(f'Disc not found: {disc}')
    build_info=json.loads((WORK/'build_manifest.json').read_text())
    if build_info['executable_sha256']!=sha(ROOT/'build/ac4/ac4.exe'):
        raise SystemExit('Executable changed; rebuild.')
    if build_info['generation_manifest_sha256']!=sha(WORK/'generation_manifest.json') or build_info['inputs']!=inputs():
        raise SystemExit('Inputs changed since build; regenerate/rebuild before tracing.')
    manifest=json.loads((WORK/'generation_manifest.json').read_text())
    for name,digest in manifest['outputs'].items():
        if sha(ROOT/'generated'/name)!=digest: raise SystemExit('Generated artifact changed: '+name)
    log=WORK/'boot.log'
    cmd=[ROOT/'build/ac4/ac4.exe','--data',ROOT/'generated','--disc',disc,'--watchdog','10','--verbose']
    if not video: cmd.append('--novideo')
    if vblanks: cmd.extend(['--vblanks',str(vblanks),'--trace-depth','128'])
    start=time.monotonic()
    rc=command(cmd,log,seconds)
    (WORK/'boot_result.json').write_text(json.dumps({'exit_code':rc,'elapsed_seconds':time.monotonic()-start,'timeout_seconds':seconds,'vblank_limit':vblanks,'termination':'timeout' if rc==124 else ('bounded_exit' if rc==0 and vblanks else 'process_exit'),'log':str(log),'boot_verified':False},indent=2))
    return rc

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action',choices=['generate','build','trace','all'])
    ap.add_argument('--elf',type=pathlib.Path,default=BASE/'work/viability/SLUS_201.52')
    ap.add_argument('--disc',type=pathlib.Path,default=BASE/'Ace Combat 04 - Shattered Skies (USA).iso')
    ap.add_argument('--jobs',type=int,default=4)
    ap.add_argument('--seconds',type=int,default=30)
    ap.add_argument('--vblanks',type=int,default=300,help='Stop with state dump after N fields; 0 disables (hard timeout still applies).')
    ap.add_argument('--video',action='store_true')
    args=ap.parse_args(); WORK.mkdir(exist_ok=True)
    if args.action in ('generate','all'): generate(args.elf.resolve())
    if args.action in ('build','all'): build(args.jobs)
    if args.action in ('trace','all'): return trace(args.disc.resolve(),args.seconds,args.video,args.vblanks)
    return 0

if __name__=='__main__': sys.exit(main())
