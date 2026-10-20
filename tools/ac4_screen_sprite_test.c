#include "rn_ac4_screen_sprite.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    ac4_screen_sprite in={0x8000,0x8100,0x100,0x200,0x80,0x40,0,0};
    uint64_t out[10],saved[10];memset(out,0x5a,sizeof out);memcpy(saved,out,sizeof out);
    assert(!ac4_screen_sprite_build(0,out,sizeof out));
    assert(!ac4_screen_sprite_build(&in,0,sizeof out));
    for(size_t n=0;n<64;n++)assert(!ac4_screen_sprite_build(&in,out,n));
    assert(!memcmp(saved,out,sizeof out));
    assert(ac4_screen_sprite_build(&in,out,sizeof out));
    assert(out[0]==UINT64_C(0x5400000000008001) && out[1]==0x53530 && out[2]==0x156 && out[7]==0);
    assert(out[3]==UINT64_C(0x02080108) && out[5]==UINT64_C(0x02480188));
    assert(out[4]==UINT64_C(0x00ffffff81008000) && out[6]==UINT64_C(0x00ffffff81408080));
    assert(out[8]==saved[8] && out[9]==saved[9]);
    in.display_flags=0x10000;assert(ac4_screen_sprite_build(&in,out,64));
    assert(out[3]==UINT64_C(0x01f800f8) && out[5]==UINT64_C(0x02480188));
    assert(out[4]==UINT64_C(0x00ffffff80f07ff0) && out[6]==UINT64_C(0x00ffffff81408060));
    in=(ac4_screen_sprite){0,0,0,0,65535,65535,0,0x100};
    assert(ac4_screen_sprite_build(&in,out,64));
    assert(out[3]==UINT64_C(0x0ff80ff8) && out[5]==UINT64_C(0x10071007));
    assert(out[4]==UINT64_C(0x00fffffffff0fff0) && out[6]==UINT64_C(0x00ffffffffffffff));
    in=(ac4_screen_sprite){65535,65535,65535,65535,65535,65535,0,0};
    assert(ac4_screen_sprite_build(&in,out,64));
    assert(out[3]==UINT64_C(0x00070007) && out[5]==UINT64_C(0x100070006));
    assert(out[6]==(UINT64_C(0xffffff00000000)|UINT64_C(0x1fffffffe)));
    puts("AC4 screen sprite: coordinates, display scaling, expansion, overflow and atomic capacity failure passed");
}
