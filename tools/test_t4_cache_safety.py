from pathlib import Path
import os, subprocess, tempfile
r=Path(__file__).resolve().parents[1]
gs=(r/'runtime/src/ps2_gs.c').read_text();vk=(r/'runtime/src/ps2_video.c').read_text()
def function(s,start):
 a=s.index(start);b=s.index('\n}\n',a)+3;return s[a:b]
source=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint32_t u32;typedef uint64_t u64;
#define VK_MAX_TEXTURES 8
#define TEXCACHE_N 8
#define GS_PAGE_BYTES 8192u
#define GS_VRAM_SIZE (4u*1024u*1024u)
static struct {u64 key;u32 hash,w,h;u8 pixels[64];} images[8];
static u32 image_count;
u32 ps2_vk_texture(u64 key,u32 hash,const u8 *rgba,u32 w,u32 h) {
 for(u32 i=0;i<image_count;i++)if(images[i].key==key && images[i].hash==hash)return i;
 if(image_count==8)return 0xFFFFFFFFu;
 u32 i=image_count++;images[i].key=key;images[i].hash=hash;
 images[i].w=w;images[i].h=h;memcpy(images[i].pixels,rgba,w*h*4u);return i;
}
static u8 old_pixels[256*256*4];
const u8 *ps2_vk_texture_pixels(u32 i){return i==99?old_pixels:i<image_count?images[i].pixels:NULL;}
void ps2_vk_texture_size(u32 i,u32 *w,u32 *h){*w=i==99?256:images[i].w;*h=i==99?256:images[i].h;}
static int valid=1,enabled=1,gs_swz4_uniform=1;
int ps2_vk_texture_is(u32 i,u64 key,u32 hash){(void)i;(void)key;(void)hash;return valid;}
#define PS2_ENV(x) enabled
static u32 tex_partial_slot;
static u64 t4_partial_reuses,t4_partial_pages;
static u8 gs_vram[GS_VRAM_SIZE],shadow[GS_VRAM_SIZE],snapshot[4*GS_PAGE_BYTES];
static u8 *gs_pagebase[GS_VRAM_SIZE/GS_PAGE_BYTES];
static u8 tex_scratch[sizeof old_pixels];
static struct {u32 lo,size,tw,th;u8 *bytes;} texsource[TEXCACHE_N];
static struct {u32 index;u64 skey;u32 shash;} texcache[TEXCACHE_N];
'''
source+=function(vk,'u32 ps2_vk_texture_exact(')
source+=function(gs,'static u64 prepare_t4_partial(')
source+=function(gs,'static u32 t4_source_hash(u32 tbp,')
source+=r'''
int main(void) {
 u8 a[16]={1},b[16]={2},c[16]={3};
 u32 ia=ps2_vk_texture_exact(123,456,a,2,2);
 u32 ib=ps2_vk_texture_exact(123,456,b,2,2);
 assert(ia!=ib && images[ia].hash!=images[ib].hash);
 assert(ps2_vk_texture_exact(123,456,a,2,2)==ia);
 assert(ps2_vk_texture_exact(123,456,b,2,2)==ib);
 assert(!memcmp(images[ia].pixels,a,16));
 while(image_count<8)ps2_vk_texture(image_count+1000,456,c,2,2);
 assert(ps2_vk_texture_exact(123,456,c,2,2)==0xFFFFFFFFu);
 texsource[0].lo=0;texsource[0].size=sizeof snapshot;
 texsource[0].tw=256;texsource[0].th=256;texsource[0].bytes=snapshot;
 texcache[0].index=99;memset(old_pixels,0xA5,sizeof old_pixels);
 for(u32 i=0;i<GS_VRAM_SIZE/GS_PAGE_BYTES;i++)gs_pagebase[i]=shadow;
 for(u32 page=0;page<4;page++) {
  memset(gs_vram,0,sizeof snapshot);gs_vram[page*GS_PAGE_BYTES]=1;
  assert(prepare_t4_partial(0,256,256,256,0)==(15u^(1u<<page)));
  assert(!memcmp(tex_scratch,old_pixels,sizeof old_pixels));
 }
 assert(prepare_t4_partial(0,128,256,256,0)==0);
 assert(prepare_t4_partial(1,256,256,256,0)==0);
 assert(prepare_t4_partial(0,256,256,255,0)==0);
 assert(prepare_t4_partial(0,2048,2048,2048,0)==0);
 valid=0;assert(prepare_t4_partial(0,256,256,256,0)==0);valid=1;
 enabled=0;assert(prepare_t4_partial(0,256,256,256,0)==0);enabled=1;
 gs_swz4_uniform=0;assert(prepare_t4_partial(0,256,256,256,0)==0);gs_swz4_uniform=1;
 memset(gs_vram,1,sizeof snapshot);assert(prepare_t4_partial(0,256,256,256,0)==0);
 shadow[GS_PAGE_BYTES]=7;assert(prepare_t4_partial(0,256,256,256,1)==13);
 u32 pal[16]={0},base=t4_source_hash(0,4,1,pal);
 pal[3]=1;assert(t4_source_hash(0,4,1,pal)!=base);pal[3]=0;
 for(u32 page=0;page<4;page++) {
  shadow[page*GS_PAGE_BYTES+100]++;
  assert(t4_source_hash(0,4,1,pal)!=base);
  shadow[page*GS_PAGE_BYTES+100]--;
 }
 assert(t4_source_hash(0,4,1,pal)==base);
 puts("PASS: collisions, exhausted cache, source-page reuse, bounds, layout, eviction, shadows, palette and page hash changes");
}
'''
bin=r.parent/'toolchain/msys64/ucrt64/bin';env=os.environ.copy();env['PATH']=str(bin)+';'+env['PATH']
with tempfile.TemporaryDirectory(prefix='ac4-t4-safety-') as d:
 p=Path(d);(p/'test.c').write_text(source)
 subprocess.run([str(bin/'gcc.exe'),'-O3','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test.exe')],env=env,check=True)
 subprocess.run([str(p/'test.exe')],env=env,check=True)
