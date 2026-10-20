"""Run synthetic native renderer checks on Windows; no ISO or GPU required."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from test_2d_target_policy import run as run_target_policy
from test_sampler_settings import run as run_sampler_settings
from test_overlay_targets import run as run_overlay_targets
from test_window_resize import run as run_window_resize
from test_frame_handoff import run as run_frame_handoff
from test_present_semaphores import run as run_present_semaphores
from test_ordered_fields import run as run_ordered_fields
from test_image_prewarm import run as run_image_prewarm
from test_loading_reserve import run as run_loading_reserve
from test_trx_rows import run as run_trx_rows
from test_gfx_timeline import run as run_gfx_timeline
from test_mip_tiles import run as run_mip_tiles
from test_t8_decode import run as run_t8_decode
from test_pipeline_prewarm import run as run_pipeline_prewarm

ROOT = Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cc', type=Path, help='Path to UCRT64 gcc.exe (defaults to saved toolchain)')
    ap.add_argument('--work-dir', type=Path, help='Directory for temporary test binaries (defaults to repository work)')
    args = ap.parse_args()
    if os.name != 'nt':
        ap.error('These runtime tests currently target the Windows toolchain.')
    compiler = args.cc
    if compiler is None and (ROOT / 'local.json').is_file():
        cfg = json.loads((ROOT / 'local.json').read_text(encoding='utf-8-sig'))
        compiler = Path(cfg['toolchain']) / 'bin/gcc.exe'
    if compiler is None:
        found = shutil.which('gcc')
        if found:
            compiler = Path(found)
    if compiler is None or not compiler.is_file():
        ap.error('GCC was not found; pass --cc with the UCRT64 gcc.exe path.')
    compiler = compiler.resolve()
    env = os.environ.copy()
    env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')
    work = (args.work_dir or ROOT / 'work').resolve()
    work.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='renderer-tests-', dir=work) as tmp:
        assert Path(tmp).resolve().parent == work.resolve()
        for name in ('ac4_screen_shadow_test', 'ac4_screen_sprite_test', 'ac4_2d_audit_test', 'ac4_mesh_bridge_test', 'ac4_model_test', 'ac4_mesh_test', 'ac4_model_gif_test', 'ac4_sky_test', 'ac4_sky_audit_test', 'ac4_sky_project_test', 'ac4_sky_dome_test', 'ac4_sky_native_test', 'ac4_intent_transport_test', 'ac4_sky_haze_test', 'ac4_sprite_native_test', 'ac4_cloud_row_test', 'ac4_cloud_sprite_test', 'ac4_cloud_project_test', 'ac4_cloud_packet_test'):
            exe = Path(tmp) / (name + '.exe')
            subprocess.run([str(compiler), '-std=c2x', '-O2', '-ffp-contract=off', '-Wall', '-Wextra',
                            *(['-flto', '-fwhole-program'] if name in ('ac4_model_gif_test', 'ac4_intent_transport_test', 'ac4_sky_haze_test', 'ac4_sky_audit_test', 'ac4_sky_native_test', 'ac4_sprite_native_test', 'ac4_cloud_row_test', 'ac4_cloud_sprite_test', 'ac4_cloud_project_test') else []),
                            '-DPS2_GAME_AC4=1', '-I' + str(ROOT / 'runtime/include'),
                            str(ROOT / 'tools' / (name + '.c')),
                            *([str(ROOT / 'runtime/src/ps2_core.c')] if name in ('ac4_sky_haze_test', 'ac4_sky_audit_test', 'ac4_sky_native_test', 'ac4_sprite_native_test', 'ac4_cloud_row_test', 'ac4_cloud_sprite_test', 'ac4_cloud_project_test') else []),
                            '-o', str(exe), '-lm'],
                           cwd=ROOT, env=env, check=True)
            subprocess.run([str(exe)], cwd=ROOT, env=env, check=True)
        # Execute the actual shader's integer fog helper against a double-division
        # oracle for every possible GS fog byte. Only GLSL intrinsics/cast syntax
        # are adapted; do not duplicate the algorithm in this test.
        shader = (ROOT / 'runtime/shaders/mesh.vert').read_text()
        start = shader.index('float ac4_fog_normalized(uint value) {')
        end = shader.index('\n}', start) + 2
        helper = shader[start:end].replace('uint(findMSB(value))', '(uint)findMSB(value)')
        source = Path(tmp) / 'fog_normalization.c'
        source.write_text("""#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint32_t uint;
static uint findMSB(uint value) { return 31u - (uint)__builtin_clz(value); }
static float uintBitsToFloat(uint value) {float f;memcpy(&f,&value,4);return f;}
""" + helper + """
int main(void) {
    for(uint value=0;value<256;value++) {
        float expected=(float)((double)value/255.0),actual=ac4_fog_normalized(value);
        assert(!memcmp(&expected,&actual,sizeof actual));
    }
    puts(\"AC4 shader fog: all 256 values exactly match the division oracle\");
    return 0;
}
""")
        exe = Path(tmp) / 'fog_normalization.exe'
        subprocess.run([str(compiler), '-std=c2x', '-O2', '-ffp-contract=off', '-Wall', '-Wextra',
                        str(source), '-o', str(exe)], env=env, check=True)
        subprocess.run([str(exe)], env=env, check=True)
        run_target_policy(ROOT, compiler, tmp, env)
        run_sampler_settings(ROOT, compiler, tmp, env)
        run_overlay_targets(ROOT, compiler, tmp, env)
        run_window_resize(ROOT, compiler, tmp, env)
        run_frame_handoff(ROOT, compiler, tmp, env)
        run_present_semaphores(ROOT, compiler, tmp, env)
        run_ordered_fields(ROOT, compiler, tmp, env)
        run_image_prewarm(ROOT, compiler, tmp, env)
        run_loading_reserve(ROOT, compiler, tmp, env)
        run_trx_rows(ROOT, compiler, tmp, env)
        run_mip_tiles(ROOT, compiler, tmp, env)
        run_t8_decode(ROOT, compiler, tmp, env)
        run_pipeline_prewarm(ROOT, compiler, tmp, env)
        run_gfx_timeline(ROOT, compiler, tmp, env)
    subprocess.run([sys.executable, str(ROOT / 'tools/test_texture_retirement.py')],
                   cwd=ROOT, env=env, check=True)
    print('All thirty-seven native renderer test executables passed.')


if __name__ == '__main__':
    main()
