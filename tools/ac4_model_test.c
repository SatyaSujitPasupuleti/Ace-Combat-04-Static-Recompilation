#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../runtime/src/rn/rn_ac4_model.c"
void rn_ac4_mesh_terrain_append(const ps2_vf c[32],const ps2_vf r[3][4],const rn_ac4_model_vertex v[3],int secondary,int clipped) {(void)c;(void)r;(void)v;(void)secondary;(void)clipped;}
void rn_ac4_mesh_end(void) {}
void rn_ac4_mesh_report(void) {}
void rn_ac4_mesh_prepare_secondary(const ps2_vf c[32],const ps2_vf r[3][4],const rn_ac4_model_vertex v[3],u32 prim) {(void)c;(void)r;(void)v;(void)prim;}
static int prepared;
static int cockpit_test_kicks;
static void cockpit_test_kick(ps2_vu *v,u32 address) {
    assert(address==638 && v->vi[9]==0);
    const ps2_vf *p=(const ps2_vf *)(v->mem+address*16);
    assert(p[2].u[0]==0xffff8003u && p[5].s[2]==7680);
    assert(p[14].u[0]==0xffff8003u && p[17].s[2]==7680);
    cockpit_test_kicks++;
}
static rn_ac4_model_vertex submitted[3];
void rn_ac4_mesh_prepare(const ps2_vf c[32],const ps2_vf r[3][4],const rn_ac4_model_vertex v[3],u32 prim) {(void)c;(void)r;(void)prim;prepared++;memcpy(submitted,v,sizeof submitted);}
void rn_ac4_mesh_prepare_cockpit(const ps2_vf c[32],const ps2_vf r[3][4],const rn_ac4_model_vertex v[3],u32 prim) {(void)c;(void)r;(void)v;(void)prim;}
void rn_ac4_mesh_cockpit_append(const ps2_vf c[32],const ps2_vf r[3][4],const rn_ac4_model_vertex v[3],int secondary) {(void)c;(void)r;(void)v;(void)secondary;}
void ps2_log(const char *fmt,...) { (void)fmt; }
static u32 terrain_test_kicks;
static void terrain_test_kick(ps2_vu *v,u32 address) {
    assert(terrain_test_kicks<terrain_execution.plan.packets);
    assert(address==terrain_execution.plan.submissions[terrain_test_kicks].address);
    terrain_execution_kick(v,address);terrain_test_kicks++;
    assert(!terrain_submission_stats.bad&&!terrain_submission_stats.sequence);
}
static int lit_test_kicks;
static void lit_test_kick(ps2_vu *v,u32 address) {
    assert(address==638u+104u*(lit_test_kicks&1));
    const ps2_vf *p=(const ps2_vf *)(v->mem+address*16);
    assert((p[2].u[0]&0x7fff)==3);
    assert((p[14].u[0]&0x7fff)==3);
    assert(v->vi[9]==(u16)lit_test_kicks);
    lit_test_kicks++;
}
static int native_test_kicks;
static void native_test_kick(ps2_vu *v,u32 address) {
    assert(address==(native_test_kicks?230u:126u));
    ps2_vf *packet=(ps2_vf *)(v->mem+address*16);
    assert(packet[0].u[0]==0xffff8003u&&packet[0].u[2]==0x412u);
    assert(packet[3].s[0]==160);
    assert(packet[9].s[0]==(native_test_kicks?176:160));
    native_test_kicks++;
}
static int empty_test_kicks;
static void empty_test_kick(ps2_vu *v,u32 address) {
    assert(address==126);
    const ps2_vf *p=(const ps2_vf *)(v->mem+address*16);
    assert(p->u[0]==0xffff8000u&&!p->u[1]&&!p->u[2]&&!p->u[3]);
    empty_test_kicks++;
}
static int mixed_test_kicks;
static void mixed_test_kick(ps2_vu *v,u32 address) {
    assert(address==638u+104u*(mixed_test_kicks&1));
    const ps2_vf *p=(const ps2_vf *)(v->mem+address*16);
    assert((p->u[0]&0x7fff)==(mixed_test_kicks?3u:4u));
    assert(v->vi[9]==mixed_test_kicks);
    mixed_test_kicks++;
}
static int passthrough_test_kicks;
static void passthrough_test_kick(ps2_vu *v,u32 address) {
    const ps2_vf *packet=(const ps2_vf *)(v->mem+address*16);
    assert(address==638u+104u*(passthrough_test_kicks&1));
    assert(packet[0].u[0]==0xffff8003u&&packet[0].u[2]==0x512);
    passthrough_test_kicks++;
}
static void fixture(ps2_vf c[32],ps2_vf r[4]) {
    memset(c,0,sizeof(ps2_vf)*32);memset(r,0,sizeof(ps2_vf)*4);
    for(int k=0;k<4;k++) {c[k].f[k]=1;c[8+k].f[k]=1;}
    c[4].f[0]=.01f;c[5].f[1]=.01f;c[6].f[2]=.01f;c[7].f[3]=1;
    c[23].u[0]=3;c[30].f[0]=101;c[30].f[1]=2;
    r[0].s[0]=4096;r[0].s[1]=8192;r[0].s[2]=4096;
    r[1].s[0]=255;r[1].s[1]=128;r[1].s[2]=200;r[1].s[3]=64;
    r[2].f[0]=10;r[2].f[1]=20;r[2].f[2]=30;r[2].f[3]=1;
    r[3].s[0]=4096;
}
static u32 math_random(u32 *seed) { *seed=*seed*1664525u+1013904223u; return *seed; }
static void test_flag_results(void) {
    u32 seed=741;
    for(u32 n=0;n<100000;n++) {
        unlit_flags actual={0}, expected;
        actual.cycles=math_random(&seed)%16;
        actual.state.status=math_random(&seed);
        for(int j=0;j<4;j++) {actual.state.fpipe[j].valid=math_random(&seed)&1;actual.state.fpipe[j].status=math_random(&seed);actual.state.fpipe[j].mac=math_random(&seed);}
        expected=actual;u32 cycle=1+math_random(&seed)%64,mask=(n&1)?math_random(&seed):n&15;
        ps2_vf value;for(int j=0;j<4;j++)value.u[j]=n<256?((n>>(j*2))&3)==0?0:((n>>(j*2))&3)==1?0x80000000u:((n>>(j*2))&3)==2?0x7fc00000u:0xffc00000u:math_random(&seed);
        flags_result(&actual,cycle,mask,value.f);
        while(expected.cycles<cycle) {u32 slot=expected.cycles++&3;if(expected.state.fpipe[slot].valid) {expected.state.mac=expected.state.fpipe[slot].mac;expected.state.status=expected.state.fpipe[slot].status;expected.state.fpipe[slot].valid=0;}}
        u32 mac=0,st=0;
        for(int k=0;k<4;k++)if(mask&(8u>>k)) {if(!(value.u[k]&0x7fffffffu))mac|=8u>>k;else if(value.u[k]&0x80000000u)mac|=128u>>k;}
        if(mac&15)st|=1;
        if(mac&240)st|=2;
        u32 slot=(cycle-1)&3;expected.state.fpipe[slot].mac=mac;expected.state.fpipe[slot].status=(expected.state.status&0xff0u)|st|(st<<6);expected.state.fpipe[slot].valid=1;
        assert(!memcmp(&actual,&expected,sizeof actual));
    }
    puts("Model flag masks: 100000 scalar differential traces, all masks and signed-zero/NaN lanes passed");
}
static void test_model_math(void) {
    static const u32 edge[]={0,0x80000000u,1,0x807fffffu,0x00800000u,0x3f800000u,0xbf800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00000u,0xffc00000u,0x7f800001u};
    u32 seed=12345;
    for(u32 t=0;t<200000;t++) {
        ps2_vf m[4],p; float expected[4],actual[4];
        for(int j=0;j<4;j++)for(int k=0;k<4;k++)m[j].u[k]=t<196?edge[(t+j*4+k)%14]:math_random(&seed);
        for(int k=0;k<4;k++)p.u[k]=t<196?edge[(t/14+k)%14]:math_random(&seed);
        if(t>=100000) {
            /* Guard boundaries, signed zero, cancellations and bounded
             * finite matrices must exercise the actual fast path too. */
            static const u32 bound[]={0,0x80000000u,0x307fffffu,0x30800000u,0x30800001u,
                0x4e7fffffu,0x4e800000u,0x4e800001u,0x3f800000u,0xbf800000u};
            for(int j=0;j<4;j++)for(int k=0;k<4;k++) {
                u32 bits=t<100100?bound[(t+j*4+k)%10]:
                    ((97+math_random(&seed)%61)<<23)|(math_random(&seed)&0x807fffffu);
                m[j].u[k]=bits;
            }
            for(int k=0;k<4;k++)p.u[k]=t<100100?bound[(t/10+k)%10]:
                ((97+math_random(&seed)%61)<<23)|(math_random(&seed)&0x807fffffu);
        }
        for(int k=0;k<4;k++) {
            float a=ps2_fmul(m[0].f[k],p.f[0]);
            for(int j=1;j<4;j++)a=ps2_fadd(a,ps2_fmul(m[j].f[k],p.f[j]));
            expected[k]=a;
        }
        transform(m,p.f,actual);assert(!memcmp(expected,actual,sizeof actual));
        unlit_flags fast={0},reference;
        fast.cycles=math_random(&seed)%32;fast.state.status=math_random(&seed);
        for(int slot=0;slot<4;slot++) {fast.state.fpipe[slot].valid=math_random(&seed)&1;fast.state.fpipe[slot].mac=math_random(&seed);fast.state.fpipe[slot].status=math_random(&seed);}
        reference=fast;u32 cycle=1+math_random(&seed)%64;
        flags_transform(&fast,cycle,m,p.f,actual);
        float intermediate[4];
        for(int j=0;j<4;j++) {
            for(int k=0;k<4;k++) {float v=ps2_fmul(m[j].f[k],p.f[j]);intermediate[k]=j?ps2_fadd(intermediate[k],v):v;}
            flags_result(&reference,cycle+(u32)j,15,intermediate);
        }
        assert(!memcmp(actual,intermediate,sizeof actual));
        assert(!memcmp(&fast,&reference,sizeof fast));
    }
    for(u32 t=0;t<10000;t++) {
        unlit_flags fast={0},reference;
        fast.cycles=math_random(&seed)%32;
        for(int j=0;j<4;j++) {fast.state.fpipe[j].valid=math_random(&seed)&1;fast.state.fpipe[j].mac=math_random(&seed);fast.state.fpipe[j].status=math_random(&seed);}
        reference=fast;u32 until=math_random(&seed)%256;
        flags_advance(&fast,until);
        while(reference.cycles<until) {u32 slot=reference.cycles++&3;if(reference.state.fpipe[slot].valid) {reference.state.mac=reference.state.fpipe[slot].mac;reference.state.status=reference.state.fpipe[slot].status;reference.state.fpipe[slot].valid=0;}}
        assert(!memcmp(&fast,&reference,sizeof fast));
    }
    { ps2_vf m[4]={0},p={0};float result[4];u32 bits;
      m[0].u[0]=0xbf800002u;p.f[0]=1;m[1].u[0]=p.u[1]=0x3f800001u;
      transform(m,p.f,result);memcpy(&bits,result,4);assert(bits==0);
    }
    puts("Model SIMD: 200000 bit-exact scalar transforms/flag traces and 10000 delayed flag sequences passed");
}
int main(void) {
    test_model_math();
    test_flag_results();
#if defined(__SSE2__)
    register_flags_enabled=1;test_model_math();test_flag_results();
    register_flags_enabled=0;
#endif
    rn_ac4_model_audit_on=1;
    ps2_vf c[32],r[4];rn_ac4_model_vertex out;
    fixture(c,r);assert(rn_ac4_model_eval(c,r,&out));
    assert(out.xyzf[0]==160 && out.xyzf[1]==320 && out.xyzf[2]==480 && out.xyzf[3]==3200);
    assert(out.stq[0]==1 && out.stq[1]==2 && out.stq[2]==1 && out.rgba[3]==64);
    {
        rn_ac4_model_vertex cp;assert(cockpit_eval(c,r,&cp));
        assert(cp.xyzf[0]==160 && cp.xyzf[1]==320 && cp.xyzf[2]==7680);
        assert(!memcmp(cp.stq,out.stq,sizeof cp.stq) && !memcmp(cp.rgba,out.rgba,sizeof cp.rgba));
        rn_ac4_model_vertex secondary;assert(cockpit_secondary_eval(c,r,&secondary));
        assert(secondary.xyzf[2]==7680 && secondary.stq[0]==0 && secondary.stq[1]==.5f);
        assert(secondary.rgba[0]==128 && secondary.rgba[3]==128);
        ps2_vf memory[1024]={0},saved[1024];ps2_vu state={0};
        state.mem=(u8*)memory;state.mem_size=sizeof memory;
        memcpy(memory,c,sizeof c);memory[128].u[0]=memory[140].u[0]=3;
        for(int i=0;i<3;i++) {
            memcpy(memory+32+i*4,r,sizeof r);
            memcpy(memory[129+i*3].f,cp.stq,16);memcpy(memory[130+i*3].s,cp.rgba,16);
            memcpy(memory[131+i*3].s,cp.xyzf,16);
            memcpy(memory[141+i*3].f,secondary.stq,16);memcpy(memory[142+i*3].s,secondary.rgba,16);
            memcpy(memory[143+i*3].s,secondary.xyzf,16);
        }
        memcpy(saved,memory,sizeof saved);cockpit_audit_on=1;
        cockpit_begin(&state,16,0xbaf5487804fd6eebull);assert(cockpit.active);
        cockpit_kick(&state,126);assert(cockpit_stats.exact==3);
        assert(cockpit_second_stats.exact==3 && !cockpit_second_stats.bad && !cockpit_second_stats.headers);
        assert(!memcmp(saved,memory,sizeof saved));
        cockpit_begin(&state,4288,0xbaf5487804fd6eebull);assert(!cockpit.active);
        cockpit_begin(&state,4224,0xbaf5487804fd6eecull);assert(!cockpit.active);
        cockpit_begin(&state,4224,0xbaf5487804fd6eebull);assert(cockpit.active);
        memory[129].f[0]+=1;memory[142].s[0]++;
        cockpit_kick(&state,126);assert(cockpit_stats.uv==1 && cockpit_second_stats.bad==1);
        /* Predict from the activation snapshot, then check independently supplied
           packet/register state. Corruption outside packet storage must count. */
        rn_ac4_model_plan_on=1;
        for(int fault=0;fault<12;fault++) {
            memcpy(memory,saved,sizeof saved);memset(state.vi,0,sizeof state.vi);
            memset(state.vf,0,sizeof state.vf);memset(&state.acc,0,sizeof state.acc);
            memory[128].u[0]=memory[140].u[0]=0xffff8003u;
            cockpit_begin(&state,16,0xbaf5487804fd6eebull);
            state.q=state.q_pend=1;state.i=255;
            state.vf[1]=memory[128];state.vf[2]=memory[140];state.vf[10]=c[30];
            memcpy(state.vf[11].s,secondary.rgba,16);state.vf[15].f[3]=1;
            for(int k=0;k<4;k++)state.vf[17].s[k]=r[1].s[k];
            memcpy(state.vf[18].f,secondary.stq,16);memcpy(state.vf[19].f,cp.stq,16);
            state.vf[20].s[3]=64;memcpy(state.vf[21].s,cp.xyzf,16);
            memcpy(state.vf+22,c+8,4*sizeof(ps2_vf));
            const u16 expected[16]={0,2,0,48,126,150,0,2,0,0,0,1,3,3,0,0};
            memcpy(state.vi,expected,sizeof expected);
            u64 mb=cockpit_memory_stats.kick_bad,ib=cockpit_integer_stats.kick_bad;
            u64 seq=cockpit_memory_stats.sequence_bad,eb=cockpit_integer_stats.end_bad;
            u64 sb=cockpit_scalar_stats.kick_bad;
            u64 vb=cockpit_vector_stats.kick_bad;
            if(fault==1)memory[900].u[2]=1;
            if(fault==2)state.vi[5]++;
            if(fault==5)state.q=2;
            if(fault==6)state.q_pend=2;
            if(fault==7)state.i=16;
            if(fault==8)state.r++;
            if(fault==9)state.vf[31].u[3]=1;
            if(fault==10)state.acc.u[2]=1;
            cockpit_memory_kick(&state,fault==3?127:126);
            assert(cockpit_memory_stats.kick_bad==mb+(fault==1));
            assert(cockpit_integer_stats.kick_bad==ib+(fault==2));
            assert(cockpit_memory_stats.sequence_bad==seq+(fault==3));
            assert(cockpit_scalar_stats.kick_bad==sb+(fault>=5 && fault<=8));
            assert(cockpit_vector_stats.kick_bad==vb+(fault==9 || fault==10));
            memcpy(state.vi,expected,sizeof expected);state.vi[9]=1;
            state.tpc=fault==4?4288:4224;state.running=0;
            /* 15 entry pairs + 44 setup + 3*78 visible + 11 tail + 2 end. */
            assert(cockpit_memory.cycles==304);
            u64 cb=cockpit_cycle_stats.bad;
            cockpit_cycle_end(&state,fault==11?307:306);
            assert(cockpit_cycle_stats.bad==cb+(fault==11));
            cockpit_memory_end(&state);
            assert(cockpit_integer_stats.end_bad==eb+(fault==4));
            state.q=state.q_pend=0;state.i=0;state.r=0;
        }
        rn_ac4_model_plan_on=0;cockpit_audit_on=0;cockpit.active=0;
    }
    {
        ps2_vf cc[32],rr[3][4],packet[144],scratch[80];u8 mask[80];u32 words=0;
        memcpy(cc,c,sizeof cc);cc[4].f[0]=.2f;cc[23].u[1]=4;
        for(int i=0;i<3;i++)memcpy(rr[i],r,sizeof r);
        assert(cockpit_clip_build(cc,rr,0,packet,&words,scratch,mask));
        assert(words==1 && packet[0].u[0]==0xffff8000u);
        for(int i=0;i<80;i++)assert(!mask[i]); /* Trivial rejection preserves scratch. */
        rr[1][2].f[0]=rr[2][2].f[0]=0;rr[2][2].f[1]=30;
        assert(cockpit_clip_build(cc,rr,0,packet,&words,scratch,mask));
        assert(words==30 && (packet[2].u[0]&32767)==4);
        for(int i=0;i<4;i++)assert(packet[5+3*i].s[2]==7680 && packet[20+3*i].s[2]==7680);
        packet[2].u[1]=packet[17].u[1]=(3u<<28)|(1u<<14)|(13u<<15);
        packet[2].u[2]=packet[17].u[2]=0x412;
        terrain_triangle topology[12];u32 triangles=0;
        assert(cockpit_clip_topology(packet,4,topology,&triangles) && triangles==4);
        assert(topology[1].index[0]==0 && topology[1].index[1]==2 && topology[1].index[2]==3);
        packet[14].u[3]|=0x8000;
        assert(cockpit_clip_topology(packet,4,topology,&triangles) && triangles==3);
        packet[17].u[2]=0;triangles=99;
        assert(!cockpit_clip_topology(packet,4,topology,&triangles) && triangles==99);
    }
    {
        ps2_vf memory[1024]={0},before[1024];ps2_vu state={0};
        state.mem=(u8*)memory;state.mem_size=sizeof memory;state.top=512;
        memcpy(memory+512,c,sizeof c);
        for(int j=0;j<3;j++)memcpy(memory+544+j*4,r,sizeof r);
        memcpy(before,memory,sizeof before);ps2_vu saved_state=state;
        cockpit_audit_on=1;cockpit_native_on=1;
        cockpit_begin(&state,16,0xbaf5487804fd6eebull);
        assert(!cockpit_try_native(&state,305,cockpit_test_kick));
        assert(!cockpit_test_kicks && !memcmp(&state,&saved_state,sizeof state));
        assert(!memcmp(memory,before,sizeof memory));
        assert(cockpit_try_native(&state,306,cockpit_test_kick));
        assert(cockpit_test_kicks==1 && state.tpc==4224 && !state.running && state.vi[9]==1);
        assert(!memcmp(memory,cockpit_memory.memory,sizeof memory));
        assert(state.q==1 && state.q_pend==1 && state.i==255 && state.vf[21].s[2]==7680);
        state=saved_state;memcpy(memory,before,sizeof before);
        cockpit_begin(&state,16,0xbaf5487804fd6eecull);
        assert(!cockpit_try_native(&state,10000,cockpit_test_kick));
        memory[544+2].f[0]=NAN;memcpy(before,memory,sizeof before);
        cockpit_begin(&state,16,0xbaf5487804fd6eebull);
        assert(!cockpit_try_native(&state,10000,cockpit_test_kick));
        assert(cockpit_test_kicks==1 && !memcmp(memory,before,sizeof memory));
        assert(!memcmp(&state,&saved_state,sizeof state));
        cockpit_native_on=0;cockpit_audit_on=0;cockpit.active=cockpit_memory.active=0;
    }
    c[3].f[3]=2;assert(rn_ac4_model_eval(c,r,&out));
    assert(out.xyzf[0]==80 && out.stq[0]==.5f && out.xyzf[3]==3168);
    fixture(c,r);c[23].u[1]=2;c[12].f[0]=1;
    c[16].f[0]=.5f;c[16].f[1]=.25f;c[16].f[2]=2;
    c[20].f[0]=.1f;c[20].f[1]=.25f;
    assert(rn_ac4_model_eval(c,r,&out));
    assert(out.rgba[0]==153 && out.rgba[1]==64 && out.rgba[2]==255 && out.rgba[3]==64);
    c[12].f[0]=-1;assert(rn_ac4_model_eval(c,r,&out));
    assert(out.rgba[0]==25 && out.rgba[1]==32 && out.rgba[2]==0);
    fixture(c,r);c[30].f[0]=-10;assert(rn_ac4_model_eval(c,r,&out));assert(!out.xyzf[3]);
    c[30].f[0]=10000;assert(rn_ac4_model_eval(c,r,&out));assert(out.xyzf[3]==4080);
    rn_ac4_model_vertex unchanged=out;
    c[3].f[3]=0;assert(!rn_ac4_model_eval(c,r,&out));assert(!memcmp(&out,&unchanged,sizeof out));
    fixture(c,r);r[2].f[0]=NAN;assert(!rn_ac4_model_eval(c,r,&out));
    fixture(c,r);c[0].f[0]=1e30f;assert(!rn_ac4_model_eval(c,r,&out));
    /* Observer consumes a synthetic packet but never changes VU input/output. */
    fixture(c,r);assert(rn_ac4_model_eval(c,r,&out));
    ps2_vf mem[1024]={0},before[1024]; ps2_vu vu={0};vu.mem=(u8*)mem;vu.mem_size=sizeof mem;
    memcpy(mem,c,sizeof c);for(int k=0;k<3;k++) memcpy(mem+32+k*4,r,sizeof r);
    for(int k=0;k<3;k++) {
        memcpy(mem[129+k*3].f,out.stq,16);memcpy(mem[130+k*3].s,out.rgba,16);memcpy(mem[131+k*3].s,out.xyzf,16);
        mem[131+k*3].u[3]|=0x8000; /* ADC is outside projection/fog comparison. */
    }
    mem[128].u[0]=3;
    memcpy(before,mem,sizeof mem);
    rn_ac4_model_audit_begin(&vu,16,0x8068ddf28a39675full);
    ps2_vu other={0};rn_ac4_model_audit_kick(&other,126);assert(!stats.vertices);
    rn_ac4_model_audit_kick(&vu,126);
    assert(stats.exact==3 && stats.vertices==3 && !memcmp(before,mem,sizeof mem));
    rn_ac4_model_audit_kick(&vu,126);assert(stats.vertices==3);
    rn_ac4_model_audit_begin(&vu,16,123);assert(!sample.active && stats.unknown==1);
    rn_ac4_model_audit_begin(&vu,0,0x8068ddf28a39675full);assert(!sample.active && stats.entry==1);
    u64 checks_before_empty=stats.safety_checks;
    mem[29].u[1]=1;rn_ac4_model_audit_begin(&vu,16,0x8068ddf28a39675full);vu.vi[10]=1;rn_ac4_model_audit_kick(&vu,126);assert(stats.clipped==1 && stats.safety_checks==checks_before_empty);vu.vi[10]=0;
    mem[29].u[1]=0;mem[23].u[0]=24;rn_ac4_model_audit_begin(&vu,16,0x8068ddf28a39675full);assert(!sample.active && stats.layout==1);
    mem[23].u[0]=3;rn_ac4_model_audit_begin(&vu,16,0x8068ddf28a39675full);rn_ac4_model_audit_kick(&vu,127);assert(stats.wrong_kick==1);
    /* Split strip: each kick starts one vertex later, alternating output banks. */
    fixture(c,r);c[23].u[0]=4;c[29].u[1]=1;memcpy(mem,c,sizeof c);
    for(int i=0;i<4;i++) {r[2].f[0]=10.0f+i;memcpy(mem+32+i*4,r,sizeof r);}
    for(int bank=0;bank<2;bank++) {
        int a=126+bank*104;mem[a+2].u[0]=3;
        for(int i=0;i<3;i++) {
            mem[a+3+i*3].f[0]=1;mem[a+3+i*3].f[1]=2;mem[a+3+i*3].f[2]=1;
            memcpy(mem[a+4+i*3].s,r[1].s,16);
            mem[a+5+i*3].s[0]=160+16*(i+bank);mem[a+5+i*3].s[1]=320;
            mem[a+5+i*3].s[2]=480;mem[a+5+i*3].s[3]=3200;
        }
    }
    u64 exact_before=stats.exact;
    rn_ac4_model_audit_begin(&vu,4288,0x8068ddf28a39675full);
    vu.vi[9]=0;vu.vi[10]=8;rn_ac4_model_audit_kick(&vu,126);
    vu.vi[9]=1;rn_ac4_model_audit_kick(&vu,230);
    assert(stats.exact==exact_before+6);
    /* The unlit variant has one header qword and a different resume PC. */
    fixture(c,r);memcpy(mem,c,sizeof c);
    for(int i=0;i<3;i++) {
        memcpy(mem+32+i*4,r,sizeof r);
        mem[127+i*3].f[0]=1;mem[127+i*3].f[1]=2;mem[127+i*3].f[2]=1;
        memcpy(mem[128+i*3].s,r[1].s,16);
        mem[129+i*3].s[0]=160;mem[129+i*3].s[1]=320;mem[129+i*3].s[2]=480;mem[129+i*3].s[3]=3200;
    }
    mem[126].u[0]=3;exact_before=stats.exact;
    rn_ac4_model_audit_begin(&vu,3344,0x0b4ca6b56fd48dd8ull);rn_ac4_model_audit_kick(&vu,126);
    assert(stats.exact==exact_before+3);
    /* Even a three-vertex output can be a clipped polygon: reject by input. */
    mem[34].f[0]=1000;u64 clips_before=stats.clipped;
    rn_ac4_model_audit_begin(&vu,3344,0x0b4ca6b56fd48dd8ull);rn_ac4_model_audit_kick(&vu,126);
    assert(stats.clipped==clips_before+1 && stats.exact==exact_before+3);
    /* Fast mode forwards VU output; audit mode must reject a changed color. */
    mem[34].f[0]=10;mem[126].u[1]=(3u<<28)|(11u<<15)|0x4000u;mem[126].u[2]=0x412;
    mem[128].s[0]=17;rn_ac4_model_mesh_on=1;rn_ac4_model_audit_on=0;
    u64 checked=stats.vertices;
    rn_ac4_model_audit_begin(&vu,3344,0x0b4ca6b56fd48dd8ull);rn_ac4_model_audit_kick(&vu,126);
    assert(prepared==1 && submitted[0].rgba[0]==17 && stats.forwarded==3 && stats.vertices==checked);
    rn_ac4_model_audit_on=1;
    rn_ac4_model_audit_begin(&vu,3344,0x0b4ca6b56fd48dd8ull);rn_ac4_model_audit_kick(&vu,126);
    assert(prepared==1 && stats.bad_color==1);
    rn_ac4_model_audit_on=0;mem[127].f[0]=NAN;
    rn_ac4_model_audit_begin(&vu,3344,0x0b4ca6b56fd48dd8ull);rn_ac4_model_audit_kick(&vu,126);
    assert(prepared==1 && stats.rejected==1);
    mem[127].f[0]=1;mem[34].f[0]=1000;clips_before=stats.clipped;
    rn_ac4_model_audit_begin(&vu,3344,0x0b4ca6b56fd48dd8ull);rn_ac4_model_audit_kick(&vu,126);
    assert(prepared==1 && stats.clipped==clips_before+1);
    /* Shared strip vertices reuse immutable safety results, never packet UVs.
     * New activations invalidate both successful and failed results. */
    fixture(c,r);c[23].u[0]=4;c[29].u[1]=1;memcpy(mem,c,sizeof c);
    for(int i=0;i<4;i++) memcpy(mem+32+i*4,r,sizeof r);
    for(int bank=0;bank<2;bank++) {
        int a=126+bank*104;mem[a].u[0]=3;mem[a].u[1]=(3u<<28)|(11u<<15)|0x4000u;mem[a].u[2]=0x412;
        for(int i=0;i<3;i++) {
            memcpy(mem[a+1+i*3].f,out.stq,16);memcpy(mem[a+2+i*3].s,out.rgba,16);memcpy(mem[a+3+i*3].s,out.xyzf,16);
        }
    }
    u64 evaluated=stats.safety_checks,reused=stats.safety_reused;int old_prepared=prepared;
    rn_ac4_model_audit_begin(&vu,3344,0x0b4ca6b56fd48dd8ull);
    vu.vi[9]=0;rn_ac4_model_audit_kick(&vu,126);
    vu.vi[9]=1;rn_ac4_model_audit_kick(&vu,230);
    assert(prepared==old_prepared+2 && stats.safety_checks==evaluated+8 && stats.safety_reused==reused+4);
    mem[231].f[0]=NAN;rn_ac4_model_audit_kick(&vu,230);assert(prepared==old_prepared+2);
    mem[0].f[0]=1e30f;rn_ac4_model_audit_begin(&vu,3344,0x0b4ca6b56fd48dd8ull);
    assert(!vertex_safe(0,1));evaluated=stats.safety_checks;
    assert(!vertex_safe(0,1)&&stats.safety_checks==evaluated);
    mem[0].f[0]=1;assert(!vertex_safe(0,1)); /* Snapshot, not live memory. */
    rn_ac4_model_audit_begin(&vu,3344,0x0b4ca6b56fd48dd8ull);assert(vertex_safe(0,1));
    rn_ac4_model_audit_on=1;evaluated=stats.safety_checks;
    assert(vertex_safe(0,1)&&vertex_safe(0,1)&&stats.safety_checks==evaluated+2);
    /* Independent full unlit packet sequence: odd/even banks and wrapping. */
    fixture(c,r);c[22].u[1]=(3u<<28)|(11u<<15)|0x4000u;c[22].u[2]=0x412;
    ps2_vf records[23][4],packet[70];rn_ac4_unlit_plan plan;u32 address;
    for(int i=0;i<23;i++)memcpy(records[i],r,sizeof r);
    assert(rn_ac4_unlit_plan_build(c,records,1000,&plan));
    assert(plan.packets==1&&rn_ac4_unlit_plan_packet(&plan,0,packet,&address)==10);
    assert(address==102 && packet[0].u[0]==0xffff8003u&&packet[0].u[2]==0x412);
    assert(packet[3].s[0]==160&&packet[3].s[3]==3200);
    c[23].u[0]=4;
    assert(rn_ac4_unlit_plan_build(c,records,0,&plan));
    assert(rn_ac4_unlit_plan_packet(&plan,0,packet,&address)==13&&address==230);
    c[29].u[1]=1;records[3][2].f[0]=11;
    assert(rn_ac4_unlit_plan_build(c,records,0,&plan)&&plan.packets==2);
    assert(rn_ac4_unlit_plan_packet(&plan,0,packet,&address)==10&&address==126);
    assert(rn_ac4_unlit_plan_packet(&plan,1,packet,&address)==10&&address==230&&packet[9].s[0]==176);
    assert(!rn_ac4_unlit_plan_packet(&plan,2,packet,&address));
    rn_ac4_unlit_plan saved_plan=plan;records[3][2].f[0]=1000;
    assert(!rn_ac4_unlit_plan_build(c,records,0,&plan)&&!memcmp(&plan,&saved_plan,sizeof plan));
    records[3][2].f[0]=11;c[22].u[2]=0x512;
    assert(!rn_ac4_unlit_plan_build(c,records,0,&plan));c[22].u[2]=0x412;
    /* Whole-memory and ordered-packet observers detect omissions/corruption. */
    rn_ac4_model_plan_on=1;vu.top=0;memset(mem,0,sizeof mem);memcpy(mem,c,sizeof c);
    memcpy(mem+32,records,4*4*sizeof(ps2_vf));
    rn_ac4_model_audit_begin(&vu,16,0x0b4ca6b56fd48dd8ull);assert(plan_check.active);
    for(u32 j=0;j<2;j++) {
        u32 n=rn_ac4_unlit_plan_packet(&plan_check.plan,j,packet,&address);
        memcpy(mem+address,packet,n*sizeof(ps2_vf));unlit_plan_kick(&vu,address);
    }
    /* Hand-specified continuation state for this four-vertex split fixture. */
    vu.tpc=0xd10;vu.vi[1]=0;vu.vi[2]=0;vu.vi[3]=52;vu.vi[4]=230;vu.vi[5]=242;
    vu.vi[6]=0;vu.vi[7]=2;vu.vi[9]=2;vu.vi[10]=16;vu.vi[11]=0x8003;vu.vi[12]=4;vu.vi[13]=4;
    vu.vf[1]=c[22];vu.vf[1].u[0]=0xffff8003;vu.vf[2]=c[25];vu.vf[10]=c[30];vu.vf[16].f[3]=101;
    vu.vf[17]=packet[8];vu.vf[19]=packet[7];vu.vf[21]=packet[9];memcpy(vu.vf+22,c,4*sizeof(ps2_vf));
    memset(vu.vf+26,0,4*sizeof(ps2_vf));vu.acc.f[0]=11;vu.acc.f[1]=20;vu.acc.f[2]=30;vu.acc.f[3]=0;
    vu.q=1;vu.q_pend=1;vu.i=255;vu.clip=0;
    assert(unlit_plan_registers(&vu));vu.vf[17].u[0]^=1;assert(!unlit_plan_registers(&vu));vu.vf[17].u[0]^=1;
    vu.vi[3]++;assert(!unlit_plan_registers(&vu));vu.vi[3]--;
    unlit_flags expected_flags=unlit_plan_flags();
    assert(expected_flags.cycles==381&&expected_flags.state.fslot==0);
    assert(expected_flags.state.status==0xc1&&expected_flags.state.mac==1);
    vu.status=expected_flags.state.status;vu.mac=expected_flags.state.mac;vu.fslot=expected_flags.state.fslot;
    memcpy(vu.fpipe,expected_flags.state.fpipe,sizeof vu.fpipe);
    rn_ac4_model_audit_end(&vu,381);assert(!plan_stats.register_bad&&!plan_stats.flags_bad);
    assert(plan_stats.packets==2&&!plan_stats.packet_bad&&!plan_stats.memory_bad&&!plan_stats.control_bad);
    rn_ac4_model_audit_begin(&vu,3344,0x0b4ca6b56fd48dd8ull);
    unlit_plan_kick(&vu,230);mem[500].u[0]=1;rn_ac4_model_audit_end(&vu,381);
    assert(plan_stats.packet_bad==1&&plan_stats.memory_bad==1&&plan_stats.missing==1);
    /* Delayed updates and sticky flags use the status visible when issued. */
    unlit_flags delay={0};delay.state.status=0x800;
    flags_scalar(&delay,1,1,-0.0f);assert(delay.state.mac==0&&delay.state.status==0x800);
    flags_scalar(&delay,2,8,-1.0f);flags_advance(&delay,4);assert(delay.state.status==0x800);
    flags_advance(&delay,5);assert(delay.state.mac==1&&delay.state.status==0x841);
    flags_advance(&delay,6);assert(delay.state.mac==128&&delay.state.status==0x882);
    flags_scalar(&delay,7,1,0);flags_advance(&delay,11);assert(delay.state.status==0x8c1);
    /* Execute independently, then ensure decline/verification never mutate. */
    memset(&vu,0,sizeof vu);memset(mem,0,sizeof mem);vu.mem=(u8*)mem;vu.mem_size=sizeof mem;
    memcpy(mem,c,sizeof c);memcpy(mem+32,records,4*4*sizeof(ps2_vf));
    rn_ac4_model_native_on=1;rn_ac4_model_plan_on=1;
    rn_ac4_model_audit_begin(&vu,16,0x0b4ca6b56fd48dd8ull);
    memcpy(before,mem,sizeof before);ps2_vu saved_vu=vu;
    assert(!rn_ac4_model_try_native(&vu,100000,native_test_kick));
    assert(!memcmp(&vu,&saved_vu,sizeof vu)&&!memcmp(mem,before,sizeof mem)&&!native_test_kicks);
    rn_ac4_model_plan_on=0;
    rn_ac4_model_audit_begin(&vu,16,0x0b4ca6b56fd48dd8ull);
    assert(!rn_ac4_model_try_native(&vu,380,native_test_kick));
    assert(!memcmp(&vu,&saved_vu,sizeof vu)&&!memcmp(mem,before,sizeof mem)&&!native_test_kicks);
    assert(rn_ac4_model_try_native(&vu,381,native_test_kick)&&native_test_kicks==2);
    assert(vu.vi[3]==52&&vu.vi[9]==2&&vu.tpc==0xd10&&vu.status==0xc1&&vu.mac==1&&vu.fslot==0);
    assert(native_runs==1&&native_packets==2&&native_cycles==381);
    mem[34].f[0]=1000;memcpy(before,mem,sizeof before);saved_vu=vu;
    rn_ac4_model_audit_begin(&vu,16,0x0b4ca6b56fd48dd8ull);
    assert(!rn_ac4_model_try_native(&vu,100000,native_test_kick)&&!memcmp(mem,before,sizeof mem)&&!memcmp(&vu,&saved_vu,sizeof vu));
    rn_ac4_model_audit_begin(&vu,16,123);assert(!rn_ac4_model_try_native(&vu,100000,native_test_kick));
    /* All six whole-triangle cull planes preserve the empty kick and state. */
    for(int plane=0;plane<6;plane++) {
        fixture(c,r);c[29].u[1]=1;r[2].f[plane/2]=(plane&1)?-200:200;
        for(int i=0;i<3;i++)memcpy(records[i],r,sizeof r);
        assert(rn_ac4_unlit_plan_build(c,records,0,&plan)&&plan.culled&&plan.cull_plane[0]==plane);
        assert(rn_ac4_unlit_plan_packet(&plan,0,packet,&address)==1&&address==126&&packet[0].u[0]==0xffff8000u);
        memset(&vu,0,sizeof vu);memset(mem,0,sizeof mem);vu.mem=(u8*)mem;vu.mem_size=sizeof mem;
        memcpy(mem,c,sizeof c);memcpy(mem+32,records,3*4*sizeof(ps2_vf));memcpy(before,mem,sizeof before);
        rn_ac4_model_audit_begin(&vu,16,0x0b4ca6b56fd48dd8ull);
        unlit_flags culled_flags=unlit_plan_flags();assert(culled_flags.cycles==81u+2u*plane);
        assert(rn_ac4_model_try_native(&vu,100000,empty_test_kick));
        before[126]=packet[0];assert(!memcmp(before,mem,sizeof mem));
        assert(vu.vi[3]==32&&vu.vi[4]==126&&vu.vi[6]==3&&vu.vi[9]==1&&vu.vi[12]==3);
        assert(vu.vi[1]==(plane==5)&&vu.tpc==0xd10&&vu.vf[29].u[0]==0xffff8000u);
        assert(vu.clip==((1u<<plane)*0x1041u)&&vu.mac==15);
    }
    assert(empty_test_kicks==6&&native_culled==6);
    /* A partial intersection and the original far-W bypass stay on VU. */
    records[0][2].f[2]=0;
    assert(!rn_ac4_unlit_plan_build(c,records,0,&plan));
    fixture(c,r);c[29].u[1]=1;c[7].f[3]=10000;r[2].f[0]=2000000;
    for(int i=0;i<3;i++)memcpy(records[i],r,sizeof r);
    assert(!rn_ac4_unlit_plan_build(c,records,0,&plan));
    /* Polygon clipping interpolates then requantizes; all six planes. */
    for(int plane=0;plane<6;plane++) {
        fixture(c,r);memset(c+4,0,4*sizeof(ps2_vf));for(int k=0;k<4;k++)c[4+k].f[k]=1;
        c[21].u[1]=(3u<<28)|(11u<<15)|0x4000u;c[21].u[2]=0x412;
        for(int i=0;i<3;i++) {
            memcpy(records[i],r,sizeof r);memset(records[i][2].f,0,16);records[i][2].f[3]=1;
            records[i][2].f[(plane/2+1)%3]=(float)(i-1)*.5f;
            records[i][0].s[0]=i*2048;records[i][1].s[0]=i*100;
        }
        records[0][2].f[plane/2]=(plane&1)?-2:2;
        u32 qwords=0;assert(rn_ac4_unlit_clip_packet(c,records,packet,&qwords)&&qwords==13);
        assert(packet[0].u[0]==0xffff8004u&&packet[0].u[2]==0x412);
        int boundary=0;
        for(int i=0;i<4;i++) {
            int axis=packet[3+3*i].s[plane/2];assert(axis>=-16&&axis<=16);
            boundary+=axis==((plane&1)?-16:16);
            assert(packet[2+3*i].s[0]>=0&&packet[2+3*i].s[0]<=200);
            assert(packet[1+3*i].f[0]>=0&&packet[1+3*i].f[0]<=1);
        }
        assert(boundary==2);
        ps2_vf scratch[80],seed[85];u8 mask[80];unlit_clip_trace trace;
        for(int i=0;i<85;i++)for(int k=0;k<4;k++)seed[i].s[k]=4096*(i+1)+k;
        assert(unlit_clip_build(c,records,packet,&qwords,scratch,mask,NULL,seed,&trace,NULL,0,NULL,NULL));
        assert(trace.mask==0x3fffffe6u&&trace.has_q&&trace.i==255);
        assert(!memcmp(&trace.vf[1],packet,16)&&!memcmp(&trace.vf[21],packet+12,16));
        assert(!memcmp(trace.vf+22,c,4*sizeof(ps2_vf)));
        for(int i=0;i<4;i++)for(int j=0;j<4;j++)assert(mask[40+i*4+j]);
        for(int i=0;i<4;i++) {
            assert(scratch[41+i*4].s[0]==packet[2+i*3].s[0]);
            assert((float)scratch[40+i*4].s[0]/4096==packet[1+i*3].f[0]);
        }
        for(int i=0;i<3;i++)records[i][2].f[plane/2]=(plane&1)?-2:2;
        assert(rn_ac4_unlit_clip_packet(c,records,packet,&qwords)&&qwords==1&&packet[0].u[0]==0xffff8000u);
        assert(unlit_clip_build(c,records,packet,&qwords,scratch,mask,NULL,seed,&trace,NULL,0,NULL,NULL));
        assert(trace.mask==0x3bffffe6u&&!trace.has_q&&trace.i==-1);
        const u32 empty_cycles[6]={221,384,560,735,908,1080};
        assert(trace.cycles==empty_cycles[plane]);
        unlit_flags clip_flags={0};clip_flags.state.status=0xf30;clip_flags.cycles=7;
        clip_flags.state.fpipe[3].valid=1;clip_flags.state.fpipe[3].status=0xf30;
        assert(unlit_clip_build(c,records,packet,&qwords,scratch,mask,NULL,seed,&trace,&clip_flags,0,NULL,NULL));
        assert(clip_flags.cycles==7+empty_cycles[plane]);
        assert(clip_flags.state.status==0xff1&&clip_flags.state.mac==15);
        for(int slot=0;slot<4;slot++)assert(!clip_flags.state.fpipe[slot].valid);
        assert(trace.vf[29].u[0]==0xffff8000u&&trace.vf[29].u[1]==0);
        if(plane==0) {
            /* The final prefetch reads an unwritten record from activation input. */
            assert(!memcmp(trace.vf+11,seed+15,5*sizeof(ps2_vf)));
            assert(!mask[15]&&!memcmp(scratch+15,seed+15,5*sizeof(ps2_vf)));
        }
        ps2_vf saved_packet[70];memcpy(saved_packet,packet,sizeof packet);qwords=77;
        records[0][2].f[0]=NAN;
        assert(!rn_ac4_unlit_clip_packet(c,records,packet,&qwords)&&qwords==77&&!memcmp(packet,saved_packet,sizeof packet));
    }
    /* Integer continuation is checked at the kick, before vi9 increments. */
    u16 initial_vi[16],expected_vi[16];for(int i=0;i<16;i++)initial_vi[i]=(u16)(0x1000+i);
    unlit_clip_control ctrl={4,5};
    clip_kick_integers(&ctrl,512,1,7,initial_vi,expected_vi);
    assert(expected_vi[1]==0&&expected_vi[2]==512&&expected_vi[3]==92&&expected_vi[4]==742);
    assert(expected_vi[5]==757&&expected_vi[6]==0&&expected_vi[7]==2&&expected_vi[8]==88);
    assert(expected_vi[9]==1&&expected_vi[10]==1&&expected_vi[11]==0x8004&&expected_vi[12]==7&&expected_vi[13]==4);
    assert(expected_vi[0]==initial_vi[0]&&expected_vi[14]==initial_vi[14]&&expected_vi[15]==initial_vi[15]);
    for(u32 plane=0;plane<6;plane++)for(u32 count=0;count<3;count++) {
        ctrl=(unlit_clip_control){count,plane};clip_kick_integers(&ctrl,512,2,9,initial_vi,expected_vi);
        assert(expected_vi[3]==564&&expected_vi[4]==638&&expected_vi[6]==count);
        assert(expected_vi[5]==((plane+1)&1)&&expected_vi[7]==((plane&1)?32:72)&&expected_vi[8]==((plane&1)?72:32));
        assert(expected_vi[9]==2&&expected_vi[10]==(1u<<plane)&&expected_vi[11]==(u16)(count-1));
        assert(expected_vi[12]==9&&expected_vi[13]==5);
    }
    /* Scalar continuation must carry the previous division into an empty
     * polygon and preserve pending storage when latency is disabled. */
    ps2_vu scalar_v={0};scalar_v.q=2;scalar_v.q_pend=3;scalar_v.q_ready=7;
    scalar_v.p=4;scalar_v.p_pend=5;scalar_v.p_ready=13;scalar_v.clip=0x123456;
    unlit_scalar_state scalar=clip_scalar_begin(&scalar_v);
    assert(scalar.valid&&scalar.q==3&&scalar.p==5&&scalar.clip==0x123456);
    scalar_v.q=3;scalar_v.p=5;scalar_v.q_ready=scalar_v.p_ready=0;
    assert(clip_scalar_matches(&scalar,&scalar_v));
    clip_scalar_divide(&scalar,.25f,1);scalar_v.q=scalar_v.q_pend=.25f;
    assert(clip_scalar_matches(&scalar,&scalar_v));
    clip_scalar_divide(&scalar,.5f,0);scalar_v.q=.5f;
    assert(scalar.q_pend==.25f&&clip_scalar_matches(&scalar,&scalar_v));
    scalar_v.q_ready=1;assert(!clip_scalar_matches(&scalar,&scalar_v));
    scalar_v.q_ready=33;assert(!clip_scalar_begin(&scalar_v).valid);
    scalar_v.q_ready=0;scalar_v.p_ready=33;assert(!clip_scalar_begin(&scalar_v).valid);
    fixture(c,r);for(int j=0;j<3;j++)memcpy(records[j],r,sizeof r);
    records[0][1].s[0]=16777217; /* integer->float->integer conversion is observable */
    assert(unlit_triangle_packet(c,records,0,packet)==10);
    assert(packet[0].u[0]==0xffff8003u&&packet[2].s[0]==16777216);
    ps2_vf packet_tail=packet[1];
    assert(unlit_triangle_packet(c,records,1,packet)==1);
    assert(packet[0].u[0]==0xffff8000u&&!memcmp(packet+1,&packet_tail,16));
    static ps2_vf mixed_memory[1024];memset(mixed_memory,0x5a,sizeof mixed_memory);
    memcpy(sample.memory,mixed_memory,sizeof mixed_memory);sample.top=890;sample.packet_bad=0;
    ps2_vu mixed_v={0};mixed_v.mem=(u8 *)mixed_memory;
    assert(unlit_triangle_packet(c,records,0,packet)==10);
    for(int j=0;j<10;j++)mixed_memory[(1016+j)&1023]=packet[j];
    mixed_packet_record(&mixed_v,1016,0,packet,10);
    assert(!sample.packet_bad&&!memcmp(sample.memory,mixed_memory,sizeof mixed_memory));
    assert(sample.memory[2].u[0]==0x5a5a5a5a); /* boundary after wrapped payload */
    mixed_packet_record(&mixed_v,1015,0,packet,10);assert(sample.packet_bad==1);
    assert(unlit_triangle_packet(c,records,1,packet)==1);mixed_memory[96]=packet[0];
    mixed_packet_record(&mixed_v,96,1,packet,1);
    assert(sample.packet_bad==1&&!memcmp(sample.memory,mixed_memory,sizeof mixed_memory));
    mixed_memory[96].u[0]^=1;mixed_packet_record(&mixed_v,96,1,packet,1);
    assert(sample.packet_bad==2);
    /* Mixed paths retain registers not written by the next triangle. */
    memset(&sample.state,0,sizeof sample.state);sample.top=512;sample.count=3;
    memcpy(sample.constants,c,sizeof c);memcpy(sample.records,records,3*sizeof records[0]);
    sample.state.vi[5]=123;sample.state.vi[8]=456;sample.state.vf[3].u[0]=0xdeadbeef;
    sample.state.vf[26].u[0]=0x12345678;sample.state.vf[30].u[0]=0xaabbccdd;
    float clip_positions[3][4];for(int j=0;j<3;j++)transform(c+4,records[j][2].f,clip_positions[j]);
    mixed_prelude_state(0,clip_positions);
    assert(sample.state.vi[5]==123&&sample.state.vi[8]==456);
    assert(sample.state.vf[3].u[0]==0xdeadbeef&&sample.state.vf[26].u[0]==0x12345678);
    for(int j=0;j<4;j++)sample.memory[556+j]=r[j];
    assert(unlit_triangle_packet(c,records,0,packet)==10);mixed_ordinary_state(0,packet);
    assert(sample.state.vi[8]==456&&sample.state.vf[3].u[0]==0xdeadbeef&&sample.state.vf[30].u[0]==0xaabbccdd);
    assert(sample.state.vf[26].f[0]==1&&sample.state.vf[27].f[0]==255);
    assert(sample.state.vf[29].f[3]==0&&sample.state.vi[3]==560);
    ps2_vu same_state=sample.state;assert(!unlit_registers_differ(&sample.state,&same_state));
    same_state.vf[30].u[0]^=1;assert(unlit_registers_differ(&sample.state,&same_state));
    same_state=sample.state;same_state.vi[8]^=1;assert(unlit_registers_differ(&sample.state,&same_state));
    same_state=sample.state;same_state.q_ready=1;assert(unlit_registers_differ(&sample.state,&same_state));
    /* Actual mixed execution: first triangle clips, second is ordinary. */
    fixture(c,r);c[23].u[0]=4;c[29].u[1]=1;
    c[22].u[1]=3u<<28;c[22].u[2]=0x412;c[21]=c[22];
    memset(c+4,0,4*sizeof(ps2_vf));for(int k=0;k<4;k++)c[4+k].f[k]=1;
    for(int j=0;j<4;j++) {
        memcpy(records[j],r,sizeof r);records[j][2].f[0]=j?0:2;
        records[j][2].f[1]=.25f*(j-1);records[j][2].f[2]=0;
    }
    memset(mem,0,sizeof mem);memcpy(mem+512,c,sizeof c);memcpy(mem+544,records,4*sizeof records[0]);
    memset(&vu,0,sizeof vu);vu.mem=(u8*)mem;vu.mem_size=sizeof mem;vu.top=512;
    rn_ac4_model_plan_on=0;rn_ac4_model_native_on=1;
    rn_ac4_model_audit_begin(&vu,16,0x0b4ca6b56fd48dd8ull);assert(!plan_check.active);
    saved_vu=vu;memcpy(before,mem,sizeof mem);
    assert(!rn_ac4_model_try_native(&vu,1,mixed_test_kick));
    assert(!mixed_test_kicks&&!memcmp(&vu,&saved_vu,sizeof vu)&&!memcmp(mem,before,sizeof mem));
    rn_ac4_model_audit_begin(&vu,16,0x0b4ca6b56fd48dd8ull);
    assert(rn_ac4_model_try_native(&vu,100000,mixed_test_kick)&&mixed_test_kicks==2);
    assert(!memcmp(mem,sample.memory,sizeof mem)&&!unlit_registers_differ(&sample.state,&vu));
    assert(vu.tpc==0xd10&&!vu.running&&vu.vi[9]==2);
    memcpy(mem,before,sizeof mem);vu=saved_vu;mem[512].f[0]=NAN; /* invalid projection declines before any submission */
    memcpy(before,mem,sizeof mem);rn_ac4_model_audit_begin(&vu,16,0x0b4ca6b56fd48dd8ull);
    assert(!rn_ac4_model_try_native(&vu,100000,mixed_test_kick));
    assert(mixed_test_kicks==2&&!memcmp(&vu,&saved_vu,sizeof vu)&&!memcmp(mem,before,sizeof mem));
    /* A supported VU packet need not match the GPU model adapter's tag. */
    fixture(c,r);c[23].u[0]=4;c[29].u[1]=1;c[22].u[1]=3u<<28;c[22].u[2]=0x512;
    for(int j=0;j<4;j++)memcpy(records[j],r,sizeof r);
    memset(mem,0,sizeof mem);memcpy(mem+512,c,sizeof c);memcpy(mem+544,records,4*sizeof records[0]);
    vu=saved_vu;rn_ac4_model_audit_begin(&vu,16,0x0b4ca6b56fd48dd8ull);assert(!plan_check.active);
    assert(rn_ac4_model_try_native(&vu,100000,passthrough_test_kick)&&passthrough_test_kicks==2);
    assert(sample.clip_packets==0&&!memcmp(mem,sample.memory,sizeof mem));
    assert(!unlit_registers_differ(&sample.state,&vu));
    fixture(c,r);rn_ac4_model_vertex second;
    assert(rn_ac4_lit_secondary(c,r,&second));
    assert(second.stq[0]==0&&second.stq[1]==.5f&&second.stq[2]==1&&second.stq[3]==1);
    for(int k=0;k<4;k++)assert(second.rgba[k]==128);
    r[3].s[1]=-2048;r[3].s[2]=4096;c[3].f[3]=2;
    assert(rn_ac4_lit_secondary(c,r,&second));
    assert(second.stq[0]==0&&second.stq[1]==.5f&&second.stq[2]==.5f&&second.stq[3]==.5f);
    assert(second.rgba[0]==0&&second.rgba[1]==0&&second.rgba[2]==0&&second.rgba[3]==128);
    rn_ac4_model_vertex untouched=second;c[8].f[0]=NAN;
    assert(!rn_ac4_lit_secondary(c,r,&second)&&!memcmp(&untouched,&second,sizeof second));
    fixture(c,r);c[23].u[1]=1;c[30].f[0]=1.06249f;c[30].f[1]=1;
    for(int j=0;j<4;j++) {memcpy(records[j],r,sizeof r);records[j][2].f[0]=(float)(j&1);records[j][2].f[1]=(float)(j/2);records[j][2].f[2]=0;}
    s32 adc[23];assert(lit_fog_adc(c,records,4,0,0,adc));
    assert(adc[0]==0&&adc[1]==0&&adc[2]==32769&&adc[3]==32769);
    assert(lit_fog_adc(c,records,3,1,1,adc)&&adc[2]==0); /* strip parity */
    c[29].u[0]=1;assert(lit_fog_adc(c,records,3,0,1,adc)&&adc[2]==0);
    c[23].u[2]=1;assert(lit_fog_adc(c,records,3,0,1,adc)&&adc[2]==32769);
    c[23].u[1]=0;assert(lit_fog_adc(c,records,3,0,1,adc)&&adc[2]==0);
    c[23].u[1]=1;records[2][2]=records[1][2];assert(lit_fog_adc(c,records,3,0,1,adc)&&adc[2]==0);
    adc[0]=123;assert(!lit_fog_adc(c,records,2,0,1,adc)&&adc[0]==123);
    fixture(c,r);for(int j=0;j<23;j++)memcpy(records[j],r,sizeof r);
    c[26].u[0]=0x11;c[27].u[0]=0x22;c[28].u[0]=0x33;
    c[22].u[2]=0x412;c[25].u[2]=0x512;
    ps2_vf lit_packet[144],lit_saved[144];u32 lit_size=0;
    memset(lit_packet,0x5a,sizeof lit_packet);
    assert(rn_ac4_lit_packet(c,records,3,0,0,lit_packet,&lit_size)&&lit_size==24);
    assert(lit_packet[0].u[0]==0x11&&lit_packet[1].u[0]==0x33&&lit_packet[2].u[0]==0xffff8003u);
    assert(lit_packet[12].u[0]==0x11&&lit_packet[13].u[0]==0x22&&lit_packet[14].u[0]==0xffff8003u);
    assert(lit_packet[4].s[0]==255&&lit_packet[16].s[0]==128&&lit_packet[24].u[0]==0x5a5a5a5a);
    c[23].u[1]=6;c[20].f[0]=c[20].f[1]=c[20].f[2]=.5f;
    assert(rn_ac4_lit_packet(c,records,3,0,0,lit_packet,&lit_size));
    assert(lit_packet[2].u[0]==3&&lit_packet[14].u[0]==0xffff8003u);
    assert(lit_packet[4].s[0]==127&&lit_packet[4].s[1]==64&&lit_packet[4].s[2]==100&&lit_packet[4].s[3]==64);
    assert(rn_ac4_lit_packet(c,records,23,0,0,lit_packet,&lit_size)&&lit_size==144);
    memcpy(lit_saved,lit_packet,sizeof lit_packet);lit_size=123;records[22][2].f[0]=NAN;
    assert(!rn_ac4_lit_packet(c,records,23,0,0,lit_packet,&lit_size)&&lit_size==123&&!memcmp(lit_packet,lit_saved,sizeof lit_packet));
    for(int plane=0;plane<6;plane++) {
        fixture(c,r);memset(c+4,0,4*sizeof(ps2_vf));for(int k=0;k<4;k++)c[4+k].f[k]=1;
        c[21].u[2]=0x412;c[31].u[2]=0x512;
        for(int j=0;j<3;j++) {memcpy(records[j],r,sizeof r);memset(records[j][2].f,0,12);records[j][2].f[(plane/2+1)%3]=.5f*(j-1);}
        records[0][2].f[plane/2]=(plane&1)?-2:2;
        ps2_vf clip_scratch[80],lit_residual[5],saved_residual[5];u8 clip_mask[80];
        assert(lit_clip_build_state(c,records,0,lit_packet,&lit_size,clip_scratch,clip_mask,lit_residual,NULL)&&lit_size==30);
        for(int k=0;k<4;k++)assert(lit_residual[0].u[k]==0);
        assert(lit_packet[2].u[2]==0x412&&lit_packet[17].u[2]==0x512);
        int biased=0;
        for(int j=0;j<4;j++) {
            float axis=clip_scratch[42+4*j].f[plane/2];
            if(fabsf(axis)>1&&fabsf(axis)<1.01f)biased++;
        }
        assert(biased==2); /* Lit interpolation deliberately moves past the exact plane. */
        for(int j=0;j<3;j++)records[j][2].f[plane/2]=(plane&1)?-2:2;
        assert(lit_clip_build(c,records,0,lit_packet,&lit_size,NULL,NULL)&&lit_size==1&&lit_packet[0].u[0]==0xffff8000u);
        memcpy(lit_saved,lit_packet,sizeof lit_packet);lit_size=123;records[2][2].f[0]=NAN;
        memcpy(saved_residual,lit_residual,sizeof lit_residual);
        assert(!lit_clip_build_state(c,records,0,lit_packet,&lit_size,NULL,NULL,lit_residual,NULL)&&lit_size==123&&!memcmp(lit_saved,lit_packet,sizeof lit_packet));
        assert(!memcmp(lit_residual,saved_residual,sizeof lit_residual));
    }
    fixture(c,r);for(int j=0;j<4;j++) {memcpy(records[j],r,sizeof r);records[j][2].f[0]=(float)j;records[j][2].f[1]=(float)-j;}
    ps2_vf winding[2],saved_winding[2];memset(winding,0x5a,sizeof winding);memcpy(saved_winding,winding,sizeof winding);
    c[23].u[1]=0;assert(lit_winding_scratch(c,records,4,winding)&&!memcmp(winding,saved_winding,sizeof winding));
    c[23].u[1]=1;assert(lit_winding_scratch(c,records,4,winding));
    for(int j=0;j<2;j++) {
        float projected[4];transform(c,records[2+j][2].f,projected);
        assert(winding[j].f[0]==projected[0]/projected[3]&&winding[j].f[1]==projected[1]/projected[3]);
        assert(winding[j].f[2]==0&&winding[j].f[3]==1);
    }
    memcpy(saved_winding,winding,sizeof winding);records[3][2].f[0]=NAN;
    assert(!lit_winding_scratch(c,records,4,winding)&&!memcmp(winding,saved_winding,sizeof winding));
    assert(!lit_winding_scratch(c,records,2,winding)&&!memcmp(winding,saved_winding,sizeof winding));
    fixture(c,r);u16 lit_vi[16];for(int j=0;j<16;j++)lit_vi[j]=(u16)(100+j);
    c[23].u[1]=1;c[23].u[2]=0;c[29].u[0]=0;
    lit_kick_integers(c,512,4,1,3,548,0,0,0,lit_vi);
    assert(lit_vi[1]==2&&lit_vi[2]==512&&lit_vi[3]==564&&lit_vi[4]==742&&lit_vi[5]==766);
    assert(lit_vi[6]==0&&lit_vi[7]==65535&&lit_vi[8]==0&&lit_vi[9]==1&&lit_vi[10]==0);
    assert(lit_vi[11]==1&&lit_vi[12]==4&&lit_vi[13]==4&&lit_vi[14]==114&&lit_vi[15]==115);
    lit_kick_integers(c,512,4,0,3,544,16,1,1,lit_vi);
    assert(lit_vi[1]==0&&lit_vi[3]==544&&lit_vi[4]==638&&lit_vi[5]==766);
    assert(lit_vi[6]==3&&lit_vi[7]==65535&&lit_vi[8]==0&&lit_vi[10]==16&&lit_vi[11]==1);
    lit_kick_integers(c,512,4,1,4,72,1,0,0,lit_vi);
    assert(lit_vi[3]==92&&lit_vi[5]==772&&lit_vi[7]==65534&&lit_vi[8]==1&&lit_vi[10]==1);
    lit_kick_integers(c,512,4,0,3,544,16,1,3,lit_vi);assert(lit_vi[1]==1);
    lit_kick_integers(c,512,4,0,3,544,16,1,32,lit_vi);assert(lit_vi[1]==1);
    lit_kick_integers(c,512,4,0,3,544,16,1,16,lit_vi);assert(lit_vi[1]==0);
    c[23].u[1]=0;lit_kick_integers(c,512,4,1,4,544,0,0,0,lit_vi);
    assert(lit_vi[7]==2&&lit_vi[8]==1);
    ps2_vu lit_scalar_v={0};lit_scalar_v.q=2;lit_scalar_v.q_pend=3;lit_scalar_v.q_ready=7;
    lit_scalar_v.p=4;lit_scalar_v.p_pend=5;lit_scalar_v.p_ready=12;lit_scalar_v.i=255;lit_scalar_v.r=0x123456;lit_scalar_v.clip=0xabcdef;
    lit_memory.scalar=clip_scalar_begin(&lit_scalar_v);lit_memory.i=lit_scalar_v.i;lit_memory.r=lit_scalar_v.r;
    assert(!lit_scalar_matches(&lit_scalar_v));
    lit_scalar_v.q=3;lit_scalar_v.p=5;lit_scalar_v.q_ready=lit_scalar_v.p_ready=0;assert(lit_scalar_matches(&lit_scalar_v));
    lit_scalar_v.r++;assert(!lit_scalar_matches(&lit_scalar_v));lit_scalar_v.r--;
    lit_scalar_v.i=2048;assert(!lit_scalar_matches(&lit_scalar_v));lit_scalar_v.i=255;
    lit_scalar_v.clip^=1;assert(!lit_scalar_matches(&lit_scalar_v));lit_scalar_v.clip^=1;
    clip_scalar_divide(&lit_memory.scalar,.25f,0);lit_scalar_v.q=.25f;assert(lit_scalar_matches(&lit_scalar_v));
    clip_scalar_divide(&lit_memory.scalar,.5f,1);lit_scalar_v.q=lit_scalar_v.q_pend=.5f;assert(lit_scalar_matches(&lit_scalar_v));
    lit_scalar_v.p_ready=33;lit_memory.scalar=clip_scalar_begin(&lit_scalar_v);assert(!lit_memory.scalar.valid);
    fixture(c,r);c[23].u[1]=0;for(int j=0;j<3;j++)memcpy(records[j],r,sizeof r);
    memset(&lit_memory.vectors,0,sizeof lit_memory.vectors);lit_memory.vectors.vf[3].u[0]=0x12345678;
    for(int j=0;j<4;j++)lit_memory.memory[112+j]=r[j];
    assert(rn_ac4_lit_packet(c,records,3,0,0,lit_packet,&lit_size));
    assert(lit_visible_vectors(c,records,3,0,0,100,lit_packet));
    assert(lit_memory.vectors.vf[3].u[0]==0x12345678);
    assert(!memcmp(&lit_memory.vectors.vf[22],c+8,4*sizeof(ps2_vf)));
    assert(!memcmp(&lit_memory.vectors.vf[28],r+2,sizeof(ps2_vf)));
    assert(lit_memory.vectors.vf[29].f[3]==0&&lit_memory.vectors.vf[15].f[3]==1);
    ps2_vu vector_copy=lit_memory.vectors;assert(lit_vectors_match(&vector_copy));
    vector_copy.acc.u[0]^=1;assert(!lit_vectors_match(&vector_copy));vector_copy=lit_memory.vectors;
    vector_copy.vf[31].u[3]^=1;assert(!lit_vectors_match(&vector_copy));
    fixture(c,r);for(int j=0;j<3;j++)memcpy(records[j],r,sizeof r);
    c[23].u[1]=0;c[23].u[2]=0;c[29].u[0]=0;
    u32 ordinary_cycles=lit_visible_cycles(c,records,3,0,0);
    c[23].u[1]=2;assert(lit_visible_cycles(c,records,3,0,0)+3==ordinary_cycles);
    c[23].u[1]=0;assert(lit_visible_cycles(c,records,3,1,0)==ordinary_cycles+4);
    c[29].u[0]=1;assert(lit_visible_cycles(c,records,3,0,0)==ordinary_cycles+5);
    c[29].u[0]=0;c[23].u[1]=1;assert(lit_visible_cycles(c,records,3,0,0)==ordinary_cycles+30);
    fixture(c,r);c[23].u[1]=0;c[23].u[2]=0;c[29].u[0]=0;
    for(int j=0;j<3;j++)memcpy(records[j],r,sizeof r);
    unlit_flags lit_flags={0};lit_flags.state.status=0x500;
    lit_visible_flags(&lit_flags,19,c,records,3,0,0);
    assert(lit_flags.cycles==19+lit_visible_cycles(c,records,3,0,0));
    assert((lit_flags.state.status&0x500)==0x500);
    for(int j=0;j<4;j++)assert(!lit_flags.state.fpipe[j].valid);
    /* Cockpit MULi.z occupies pair 52 in the visible loop. Its lane mask and
       zero/sign flags remain in the drained slot after the last vertex. */
    for(int sign=0;sign<2;sign++) {
        records[2][2].f[2]=sign?-30:0;
        unlit_flags cockpit_flags={0};cockpit_flags.cockpit=1;
        lit_visible_flags(&cockpit_flags,19,c,records,3,0,0);
        u32 slot=(19+44+2*78+52-1)&3;
        assert(cockpit_flags.state.fpipe[slot].mac==(sign?0x20u:2u));
        assert(!cockpit_flags.state.fpipe[slot].valid);
        assert(cockpit_flags.cycles==19+lit_visible_cycles(c,records,3,0,0));
    }
    records[2][2].f[2]=30;
    c[23].u[1]=1;records[0][2].f[0]=0;records[0][2].f[1]=0;
    records[1][2].f[0]=1;records[1][2].f[1]=0;
    records[2][2].f[0]=0;records[2][2].f[1]=1;
    lit_flags=(unlit_flags){0};lit_visible_flags(&lit_flags,3,c,records,3,0,0);
    int cross_sign=0;for(int j=0;j<4;j++)cross_sign|=(lit_flags.state.fpipe[j].mac&0x20)!=0;
    assert(cross_sign); /* Negative winding must survive in a pipeline slot. */
    assert(lit_flags.cycles==3+lit_visible_cycles(c,records,3,0,0));
    ps2_vu sticky_v={0};assert(!lit_clipping_flags_supported(&sticky_v));
    sticky_v.status=0xc0;assert(lit_clipping_flags_supported(&sticky_v));
    sticky_v.fpipe[2].valid=1;sticky_v.fpipe[2].status=0x40;assert(!lit_clipping_flags_supported(&sticky_v));
    sticky_v.fpipe[2].status=0xc0;assert(lit_clipping_flags_supported(&sticky_v));
    sticky_v.status=0x80;assert(!lit_clipping_flags_supported(&sticky_v));
    fixture(c,r);c[23].u[0]=4;c[29].u[1]=1;
    for(int j=0;j<4;j++)memcpy(records[j],r,sizeof r);
    memset(mem,0,sizeof mem);memcpy(mem+512,c,sizeof c);memcpy(mem+544,records,4*sizeof records[0]);
    memset(&vu,0,sizeof vu);vu.mem=(u8*)mem;vu.mem_size=sizeof mem;vu.top=512;
    vu.vf[0].f[3]=1;vu.status=0xc0;
    rn_ac4_model_plan_on=0;rn_ac4_model_native_on=1;lit_native_on=1;
    saved_vu=vu;memcpy(before,mem,sizeof mem);
    rn_ac4_model_audit_begin(&vu,16,0x8068ddf28a39675full);
    assert(!rn_ac4_model_try_native(&vu,1,lit_test_kick));
    assert(!lit_test_kicks&&!memcmp(&vu,&saved_vu,sizeof vu)&&!memcmp(mem,before,sizeof mem));
    rn_ac4_model_audit_begin(&vu,16,0x8068ddf28a39675full);
    assert(rn_ac4_model_try_native(&vu,100000,lit_test_kick)&&lit_test_kicks==2);
    assert(!memcmp(mem,lit_memory.memory,sizeof mem));assert(vu.tpc==0x10c0&&!vu.running&&vu.vi[9]==2);
    memcpy(mem,before,sizeof mem);vu=saved_vu;mem[558].f[0]=NAN;memcpy(before,mem,sizeof mem);
    rn_ac4_model_audit_begin(&vu,16,0x8068ddf28a39675full);
    assert(!rn_ac4_model_try_native(&vu,100000,lit_test_kick));
    assert(lit_test_kicks==2&&!memcmp(&vu,&saved_vu,sizeof vu)&&!memcmp(mem,before,sizeof mem));
    ps2_vf terrain_c[32]={0},terrain_in[10][4]={0},terrain_out[10][4],terrain_saved[10][4];
    terrain_c[23].u[1]=4;terrain_c[12].f[0]=10;terrain_c[12].f[1]=20;terrain_c[12].f[2]=30;
    terrain_c[20].s[0]=64;terrain_c[20].s[1]=-128;
    for(int j=0;j<10;j++) {
        terrain_in[j][0].s[0]=32;terrain_in[j][0].s[1]=-16;terrain_in[j][1].s[0]=128;
        terrain_in[j][2].s[0]=j;terrain_in[j][2].s[1]=2;terrain_in[j][2].s[2]=-3;terrain_in[j][2].s[3]=1;
        terrain_in[j][3].s[0]=128;terrain_in[j][3].s[1]=64;terrain_in[j][3].s[3]=7;
    }
    assert(terrain_decode(terrain_c,terrain_in,4,terrain_out));
    assert(terrain_out[1][2].f[0]==1034&&terrain_out[1][2].f[1]==-140&&terrain_out[1][2].f[2]==-3042);
    assert(terrain_out[0][0].f[0]==2&&terrain_out[0][0].f[1]==-1&&terrain_out[0][1].f[0]==128);
    assert(terrain_out[0][3].f[0]==2&&terrain_out[0][3].f[1]==-1.5f&&terrain_out[0][3].f[2]==1&&terrain_out[0][3].f[3]==7);
    terrain_c[23].u[1]=2;terrain_c[13].f[0]=1;terrain_c[13].f[1]=-1;
    assert(terrain_decode(terrain_c,terrain_in,10,terrain_out));
    assert(terrain_out[0][2].f[0]==3082&&terrain_out[2][2].f[0]==2058&&terrain_out[8][2].f[0]==4106);
    assert(terrain_out[1][2].f[0]==1034); /* odd vertices remain unchanged */
    terrain_c[13].f[1]=1e30f;assert(terrain_decode(terrain_c,terrain_in,10,terrain_out));
    assert(terrain_out[0][2].f[0]==10&&terrain_out[8][2].f[0]==8202);
    memcpy(terrain_saved,terrain_out,sizeof terrain_out);
    assert(!terrain_decode(terrain_c,terrain_in,4,terrain_out)&&!memcmp(terrain_saved,terrain_out,sizeof terrain_out));
    terrain_c[12].f[0]=NAN;assert(!terrain_decode(terrain_c,terrain_in,10,terrain_out)&&!memcmp(terrain_saved,terrain_out,sizeof terrain_out));
    memset(terrain_c,0,sizeof terrain_c);memset(terrain_out,0,sizeof terrain_out);
    for(int k=0;k<4;k++)terrain_c[k].f[k]=1;
    terrain_c[23].u[1]=4;terrain_c[30].f[0]=256;terrain_c[30].f[1]=1;
    terrain_c[15].u[0]=111;terrain_c[16].u[0]=222;terrain_c[19].u[0]=333;
    for(int j=0;j<4;j++) {
        terrain_out[j][0].f[0]=.5f;terrain_out[j][3].f[0]=.25f;
        terrain_out[j][1].f[3]=255;terrain_out[j][2].f[0]=j;terrain_out[j][2].f[3]=1;
    }
    ps2_vf terrain_packet_out[65],terrain_packet_saved[65];u32 terrain_size=0;
    assert(terrain_packet(terrain_c,terrain_out,4,terrain_packet_out,&terrain_size)&&terrain_size==29);
    assert(terrain_packet_out[0].u[0]==111&&terrain_packet_out[1].u[0]==333&&terrain_packet_out[2].u[0]==222);
    assert(terrain_packet_out[3].u[0]==0xffff8004u&&terrain_packet_out[16].u[0]==0xffff8004u);
    assert(terrain_packet_out[4].f[0]==.5f&&terrain_packet_out[17].f[0]==.25f);
    assert(terrain_packet_out[5].s[3]==255&&terrain_packet_out[18].s[3]==254); /* exact VU alpha constant */
    assert(terrain_packet_out[6].s[3]==4080&&terrain_packet_out[18].s[0]==128);
    terrain_c[22].u[0]=1;assert(terrain_packet(terrain_c,terrain_out,4,terrain_packet_out,&terrain_size)&&terrain_packet_out[3].u[0]==4);
    /* Distinct fan/strip ordering, second-pass termination and ADC suppression. */
    terrain_packet_out[3].u[1]=(3u<<28)|(1u<<14)|(13u<<15);
    terrain_packet_out[3].u[2]=0xabcd0412;terrain_packet_out[3].u[3]=0x6c01801e; /* inactive register slots */
    terrain_packet_out[16].u[1]=(3u<<28)|(1u<<14)|(12u<<15);
    terrain_packet_out[16].u[2]=0x412;
    terrain_triangle topology[16],topology_saved[16];u32 triangles=99;
    assert(terrain_topology(terrain_packet_out,4,topology,&triangles)&&triangles==4);
    assert(topology[1].pass==0&&topology[1].index[0]==0&&topology[1].index[1]==2&&topology[1].index[2]==3);
    assert(topology[3].pass==1&&topology[3].index[0]==1&&topology[3].index[1]==2&&topology[3].index[2]==3);
    terrain_packet_out[12].u[3]|=0x8000;
    assert(terrain_topology(terrain_packet_out,4,topology,&triangles)&&triangles==3&&topology[0].index[2]==3);
    terrain_packet_out[3].u[0]|=0x8000;
    terrain_packet_out[16].u[2]=0; /* Unsubmitted second stream need not be supported. */
    assert(terrain_topology(terrain_packet_out,4,topology,&triangles)&&triangles==1);
    memcpy(topology_saved,topology,sizeof topology);triangles=99;
    terrain_packet_out[3].u[2]=0x512;
    assert(!terrain_topology(terrain_packet_out,4,topology,&triangles)&&triangles==99&&!memcmp(topology,topology_saved,sizeof topology));
    ps2_vf large_packet[65]={0};
    large_packet[3].u[0]=10;large_packet[34].u[0]=0x800a;
    large_packet[3].u[1]=large_packet[34].u[1]=(3u<<28)|(1u<<14)|(12u<<15);
    large_packet[3].u[2]=large_packet[34].u[2]=0x412;
    assert(terrain_topology(large_packet,10,topology,&triangles)&&triangles==16);
    assert(topology[15].pass==1&&topology[15].index[0]==7&&topology[15].index[2]==9);
    memcpy(topology_saved,topology,sizeof topology);triangles=99;
    large_packet[34].u[2]=0x512; /* Reject whole result if later pass is invalid. */
    assert(!terrain_topology(large_packet,10,topology,&triangles)&&triangles==99&&!memcmp(topology,topology_saved,sizeof topology));
    memcpy(terrain_packet_saved,terrain_packet_out,sizeof terrain_packet_out);terrain_size=123;
    terrain_out[3][2].f[3]=0;
    assert(!terrain_packet(terrain_c,terrain_out,4,terrain_packet_out,&terrain_size)&&terrain_size==123&&!memcmp(terrain_packet_saved,terrain_packet_out,sizeof terrain_packet_out));
    terrain_out[3][2].f[3]=1;terrain_c[23].u[1]=1;
    assert(!terrain_packet(terrain_c,terrain_out,4,terrain_packet_out,&terrain_size)&&terrain_size==123&&!memcmp(terrain_packet_saved,terrain_packet_out,sizeof terrain_packet_out));
    /* Homogeneous fan culling: each plane, boundaries and fan anchor. */
    memset(terrain_c,0,sizeof terrain_c);memset(terrain_out,0,sizeof terrain_out);
    terrain_c[23].u[1]=1;for(int k=0;k<4;k++)terrain_c[4+k].f[k]=1;
    u32 culled=99;
    for(int plane=0;plane<6;plane++) {
        for(int j=0;j<4;j++) {
            memset(&terrain_out[j][2],0,16);terrain_out[j][2].f[3]=1;
            terrain_out[j][2].f[plane/2]=(plane&1)?-2:2;
        }
        assert(terrain_culled_fan(terrain_c,terrain_out,4,&culled)&&culled==2);
        terrain_out[0][2].f[plane/2]=0;
        assert(terrain_culled_fan(terrain_c,terrain_out,4,&culled)&&culled==0);
        terrain_out[0][2].f[plane/2]=(plane&1)?-1:1; /* On-plane is inside. */
        assert(terrain_culled_fan(terrain_c,terrain_out,4,&culled)&&culled==0);
    }
    for(int j=0;j<4;j++) {terrain_out[j][2].f[2]=2;terrain_out[j][2].f[3]=-1;}
    assert(terrain_culled_fan(terrain_c,terrain_out,4,&culled)&&culled==2);
    terrain_out[3][2].f[2]=NAN;culled=99;
    assert(!terrain_culled_fan(terrain_c,terrain_out,4,&culled)&&culled==99);
    /* Clipped polygons can have counts absent from original 4/10-vertex patches. */
    for(u32 n=3;n<=8;n++) {
        ps2_vf clipped_packet[65]={0};u32 second=4+3*n;
        clipped_packet[3].u[0]=n;clipped_packet[second].u[0]=0x8000|n;
        clipped_packet[3].u[1]=clipped_packet[second].u[1]=(3u<<28)|(1u<<14)|(13u<<15);
        clipped_packet[3].u[2]=clipped_packet[second].u[2]=0x412;
        assert(terrain_topology(clipped_packet,n,topology,&triangles)&&triangles==2*(n-2));
        assert(topology[triangles-1].pass==1&&topology[triangles-1].index[0]==0&&topology[triangles-1].index[2]==n-1);
    }
    ps2_vf triangle[3][4]={0},polygon[10][4],polygon_saved[10][4];u32 polygon_count=99;
    memset(terrain_c,0,sizeof terrain_c);for(int k=0;k<4;k++)terrain_c[4+k].f[k]=1;
    for(int j=0;j<3;j++)triangle[j][2].f[3]=1;
    triangle[0][2].f[0]=-.5f;triangle[1][2].f[0]=2;triangle[2][2].f[1]=.5f;
    for(int j=0;j<3;j++) {
        triangle[j][0].f[0]=triangle[j][2].f[0];triangle[j][3].f[1]=triangle[j][2].f[1];
        triangle[j][1].f[0]=128;triangle[j][1].f[3]=255;
    }
    assert(terrain_clip_triangle(terrain_c,triangle,polygon,&polygon_count)&&polygon_count==4);
    int intersections=0;
    for(u32 j=0;j<polygon_count;j++) {
        assert(polygon[j][2].f[0]>=-1&&polygon[j][2].f[0]<=1);
        assert(polygon[j][0].f[0]==polygon[j][2].f[0]&&polygon[j][3].f[1]==polygon[j][2].f[1]);
        assert(polygon[j][1].f[0]==128&&polygon[j][1].f[3]==255);
        intersections+=polygon[j][2].f[0]==1;
    }
    assert(intersections==2);
    triangle[1][2].f[0]=.5f;
    assert(terrain_clip_triangle(terrain_c,triangle,polygon,&polygon_count)&&polygon_count==3&&!memcmp(polygon,triangle,sizeof triangle));
    for(int j=0;j<3;j++)triangle[j][2].f[0]=2;
    assert(terrain_clip_triangle(terrain_c,triangle,polygon,&polygon_count)&&polygon_count==0);
    memcpy(polygon_saved,polygon,sizeof polygon);polygon_count=99;triangle[2][0].f[0]=NAN;
    assert(!terrain_clip_triangle(terrain_c,triangle,polygon,&polygon_count)&&polygon_count==99&&!memcmp(polygon,polygon_saved,sizeof polygon));
    static ps2_vf mem_initial[1024],mem_result[1024],mem_saved[1024];
    for(u32 j=0;j<1024;j++)for(int k=0;k<4;k++)mem_initial[j].u[k]=j*17u+(u32)k;
    ps2_vu mem_v={0};mem_v.mem=(u8*)mem_initial;mem_v.mem_size=16384;mem_v.top=1000;
    memset(terrain_c,0,sizeof terrain_c);memset(terrain_out,0,sizeof terrain_out);
    for(int k=0;k<4;k++)terrain_c[k].f[k]=terrain_c[k+4].f[k]=1;
    terrain_c[23].u[1]=4;
    for(int j=0;j<4;j++)terrain_out[j][2].f[3]=1;
    assert(terrain_memory_build(&mem_v,terrain_c,terrain_out,4,mem_result));
    assert(!memcmp(mem_result+8,terrain_out,16*sizeof(ps2_vf))); /* TOP+32 wraps. */
    assert(!memcmp(mem_result+500,mem_initial+500,16)); /* Untouched memory preserved. */
    assert(terrain_packet(terrain_c,terrain_out,4,terrain_packet_out,&terrain_size));
    assert(!memcmp(mem_result+186,terrain_packet_out,terrain_size*sizeof(ps2_vf)));
    memcpy(mem_saved,mem_result,sizeof mem_saved);terrain_c[23].u[1]=1;
    assert(!terrain_memory_build(&mem_v,terrain_c,terrain_out,4,mem_result)&&!memcmp(mem_saved,mem_result,sizeof mem_saved));
    for(int j=0;j<4;j++)terrain_out[j][2].f[0]=2;
    assert(terrain_memory_build(&mem_v,terrain_c,terrain_out,4,mem_result));
    assert(!memcmp(mem_result+186,mem_initial+186,65*sizeof(ps2_vf))); /* Culled: no output bank writes. */
    assert(mem_initial[8].u[0]==8*17); /* Entry snapshot never mutated. */
    /* Exercise final scratch layout and publication only after all fan triangles. */
    mem_v.top=512;
    memset(triangle,0,sizeof triangle);
    for(int j=0;j<3;j++)triangle[j][2].f[3]=1;
    triangle[0][2].f[0]=-.5f;triangle[1][2].f[0]=2;triangle[2][2].f[1]=.5f;
    memcpy(mem_result,mem_initial,sizeof mem_result);
    assert(terrain_clip_triangle_state(terrain_c,triangle,polygon,&polygon_count,mem_result)&&polygon_count==4);
    assert(!memcmp(mem_result+72,polygon,polygon_count*4*sizeof(ps2_vf)));
    for(u32 j=0;j<polygon_count;j++) {
        assert(!memcmp(mem_result+32+5*j,polygon[j],4*sizeof(ps2_vf)));
        assert(!memcmp(mem_result+32+5*j+4,polygon[j]+2,sizeof(ps2_vf))); /* Identity clip matrix. */
    }
    assert(!memcmp(mem_result+112,mem_initial+112,16));
    memcpy(terrain_out,triangle,sizeof triangle);memcpy(terrain_out[3],triangle[2],sizeof triangle[2]);
    assert(terrain_memory_build(&mem_v,terrain_c,terrain_out,4,mem_result));
    assert(!memcmp(mem_result+544,terrain_out,16*sizeof(ps2_vf)));
    memcpy(mem_saved,mem_result,sizeof mem_saved);terrain_out[3][0].f[0]=NAN;
    assert(!terrain_memory_build(&mem_v,terrain_c,terrain_out,4,mem_result)&&!memcmp(mem_saved,mem_result,sizeof mem_saved));
    u16 integer_end[16],integer_kick[16],integer_saved[16];int has_kick=99;
    for(int j=0;j<16;j++)mem_v.vi[j]=(u16)(j+100);
    mem_v.vi[0]=0;terrain_c[23].u[1]=4;terrain_c[22].u[0]=0x12345;
    assert(terrain_integer_build(&mem_v,terrain_c,terrain_out,4,integer_end,integer_kick,&has_kick)&&has_kick);
    assert(integer_end[3]==564&&integer_end[4]==722&&integer_end[8]==751&&integer_end[9]==32);
    assert(integer_end[12]==0x2345&&integer_end[13]==0x8004&&integer_end[10]==110&&integer_end[15]==115);
    assert(integer_end[5]==65535&&integer_end[6]==4&&integer_kick[5]==0&&integer_kick[6]==65535);
    terrain_c[23].u[1]=1;memset(terrain_out,0,sizeof terrain_out);
    for(int plane=0;plane<6;plane++) {
        for(int j=0;j<4;j++) {memset(&terrain_out[j][2],0,16);terrain_out[j][2].f[3]=1;terrain_out[j][2].f[plane/2]=(plane&1)?-2:2;}
        assert(terrain_integer_build(&mem_v,terrain_c,terrain_out,4,integer_end,integer_kick,&has_kick)&&!has_kick);
        assert(integer_end[1]==(plane==5?1:0)&&integer_end[3]==544&&integer_end[4]==584&&integer_end[13]==552);
        assert(integer_end[8]==108&&integer_end[9]==109&&integer_end[10]==110);
    }
    memcpy(integer_saved,integer_end,sizeof integer_saved);terrain_out[0][2].f[2]=0;
    assert(!terrain_integer_build(&mem_v,terrain_c,terrain_out,4,integer_end,integer_kick,&has_kick)&&!memcmp(integer_saved,integer_end,sizeof integer_saved));
    u16 fan_kicks[8][16],fan_saved[8][16];u32 fan_count=99;
    memset(terrain_out,0,sizeof terrain_out);for(int j=0;j<10;j++)terrain_out[j][2].f[3]=1;
    assert(terrain_integer_plan(&mem_v,terrain_c,terrain_out,10,integer_end,fan_kicks,&fan_count)&&fan_count==8);
    for(u32 j=0;j<fan_count;j++)assert(fan_kicks[j][4]==512+((j&1)?126:210)&&fan_kicks[j][5]==7-j&&fan_kicks[j][6]==65535);
    assert(integer_end[4]==638&&integer_end[5]==65535&&integer_end[6]==10&&integer_end[10]==110&&integer_end[11]==4);
    terrain_out[0][2].f[0]=-.5f;terrain_out[1][2].f[0]=2;terrain_out[2][2].f[1]=.5f;terrain_out[3][2].f[1]=.5f;
    assert(terrain_integer_plan(&mem_v,terrain_c,terrain_out,4,integer_end,fan_kicks,&fan_count)&&fan_count==2);
    assert(fan_kicks[0][3]==92&&fan_kicks[0][10]==64&&fan_kicks[0][11]==65535);
    assert(fan_kicks[1][3]==16&&fan_kicks[1][10]==64&&fan_kicks[1][11]==65535); /* Inside triangle inherits earlier clip loop state. */
    memcpy(integer_saved,integer_end,sizeof integer_saved);memcpy(fan_saved,fan_kicks,sizeof fan_saved);fan_count=99;
    terrain_out[3][0].f[0]=NAN;
    assert(!terrain_integer_plan(&mem_v,terrain_c,terrain_out,4,integer_end,fan_kicks,&fan_count)&&fan_count==99&&!memcmp(integer_saved,integer_end,sizeof integer_saved)&&!memcmp(fan_saved,fan_kicks,sizeof fan_saved));
    terrain_scalar_value scalar_end,scalar_kicks[8],scalar_saved;u32 scalar_count;
    memset(terrain_c,0,sizeof terrain_c);memset(terrain_out,0,sizeof terrain_out);
    for(int k=0;k<4;k++)terrain_c[k].f[k]=terrain_c[k+4].f[k]=1;
    for(int j=0;j<4;j++)terrain_out[j][2].f[3]=2;
    terrain_c[23].u[1]=4;mem_v.q=3;mem_v.p=4;mem_v.q_pend=.25f;mem_v.p_pend=.75f;
    mem_v.q_ready=7;mem_v.p_ready=31;mem_v.clip=0xabcdef;mem_v.r=0x123456;
    for(int latency=0;latency<2;latency++) {
        model_qlat=latency;
        assert(terrain_scalar_build(&mem_v,terrain_c,terrain_out,4,&scalar_end,scalar_kicks,&scalar_count)&&scalar_count==1);
        assert(scalar_end.scalar.q==.5f&&scalar_end.scalar.q_pend==(latency?.5f:.25f));
        assert(scalar_end.scalar.p==.75f&&scalar_end.scalar.p_pend==.75f&&scalar_end.i==255&&scalar_end.r==mem_v.r&&scalar_end.scalar.clip==mem_v.clip);
    }
    model_qlat=1;terrain_c[23].u[1]=1;
    for(int j=0;j<4;j++) {terrain_out[j][2].f[0]=3;terrain_out[j][2].f[3]=1;}
    assert(terrain_scalar_build(&mem_v,terrain_c,terrain_out,4,&scalar_end,scalar_kicks,&scalar_count)&&!scalar_count);
    u32 expected_clip=mem_v.clip;for(int j=0;j<6;j++)expected_clip=((expected_clip<<6)|1)&0xffffff;
    assert(scalar_end.scalar.clip==expected_clip&&scalar_end.scalar.q==.25f&&scalar_end.i==1.0f/16);
    scalar_saved=scalar_end;mem_v.p_ready=33;scalar_count=99;
    assert(!terrain_scalar_build(&mem_v,terrain_c,terrain_out,4,&scalar_end,scalar_kicks,&scalar_count)&&scalar_count==99&&!memcmp(&scalar_saved,&scalar_end,sizeof scalar_end));mem_v.p_ready=0;
    memset(terrain_out,0,sizeof terrain_out);for(int j=0;j<3;j++)terrain_out[j][2].f[3]=1;
    terrain_out[0][2].f[0]=2;terrain_out[1][2].f[1]=2;terrain_out[2][2].f[0]=terrain_out[2][2].f[1]=2;
    assert(terrain_scalar_build(&mem_v,terrain_c,terrain_out,3,&scalar_end,scalar_kicks,&scalar_count)&&!scalar_count);
    assert(scalar_end.i==-1&&scalar_end.scalar.q==.5f&&scalar_end.scalar.q_pend==.5f);
    /* Instruction accounting checks both tag branches, entry trampolines and
     * all six common-plane rejection branches, without reading observed counts. */
    u32 instruction_count=99;memset(terrain_c,0,sizeof terrain_c);
    terrain_c[23].u[1]=4;
    assert(terrain_ordinary_cycles(16,terrain_c,4,&instruction_count)&&instruction_count==286);
    terrain_c[22].u[0]=1;
    assert(terrain_ordinary_cycles(4176,terrain_c,4,&instruction_count)&&instruction_count==289);
    terrain_c[23].u[1]=2;terrain_c[22].u[0]=0;
    assert(terrain_ordinary_cycles(16,terrain_c,9,&instruction_count)&&instruction_count==628);
    assert(terrain_ordinary_cycles(16,terrain_c,10,&instruction_count)&&instruction_count==681);
    instruction_count=99;
    assert(!terrain_ordinary_cycles(16,terrain_c,4,&instruction_count)&&instruction_count==99);
    assert(!terrain_ordinary_cycles(0,terrain_c,9,&instruction_count)&&instruction_count==99);
    terrain_c[23].u[1]=1;for(int k=0;k<4;k++)terrain_c[k+4].f[k]=1;
    for(int plane=0;plane<6;plane++) {
        memset(terrain_out,0,sizeof terrain_out);
        for(int j=0;j<4;j++) {terrain_out[j][2].f[3]=1;terrain_out[j][2].f[plane/2]=(plane&1)?-2:2;}
        assert(terrain_instruction_plan(16,terrain_c,terrain_out,4,&instruction_count)&&instruction_count==187u+4u*(u32)plane);
        assert(terrain_instruction_plan(4176,terrain_c,terrain_out,4,&instruction_count)&&instruction_count==189u+4u*(u32)plane);
    }
    /* A failed fan must not publish a count from its rejected prefix. */
    terrain_out[3][2].f[2]=NAN;instruction_count=99;
    assert(!terrain_instruction_plan(16,terrain_c,terrain_out,4,&instruction_count)&&instruction_count==99);
    memset(terrain_out,0,sizeof terrain_out);
    for(int j=0;j<4;j++)terrain_out[j][2].f[3]=1;
    assert(terrain_instruction_plan(16,terrain_c,terrain_out,4,&instruction_count)&&instruction_count==512);
    terrain_c[22].u[0]=1;
    assert(terrain_instruction_plan(4176,terrain_c,terrain_out,4,&instruction_count)&&instruction_count==516);
    memset(&terrain_cycle_stats,0,sizeof terrain_cycle_stats);
    terrain_cycles.owner=&mem_v;terrain_cycles.active=1;terrain_cycles.expected=286;
    terrain_cycle_end(&mem_v,286);terrain_cycle_end(&mem_v,286);
    assert(terrain_cycle_stats.ended==1&&!terrain_cycle_stats.bad&&terrain_cycle_stats.observed==286);
    terrain_vector_state vector_result,vector_saved;
    memset(terrain_c,0,sizeof terrain_c);memset(terrain_out,0,sizeof terrain_out);
    for(int j=0;j<32;j++)for(int k=0;k<4;k++)mem_v.vf[j].f[k]=(float)(j*4+k);
    memset(mem_v.vf,0,sizeof(ps2_vf));mem_v.vf[0].f[3]=1;
    for(int k=0;k<4;k++)terrain_c[k].f[k]=terrain_c[k+4].f[k]=1;
    for(int j=0;j<4;j++) {terrain_out[j][2].f[0]=(float)(j+1);terrain_out[j][2].f[1]=2;terrain_out[j][2].f[2]=3;terrain_out[j][2].f[3]=1;}
    terrain_c[23].u[1]=4;terrain_c[30].f[0]=100;terrain_c[30].f[1]=1;
    assert(terrain_vector_build(&mem_v,terrain_c,terrain_out,4,&vector_result));
    assert(!memcmp(vector_result.vf,mem_v.vf,15*sizeof(ps2_vf)));
    assert(!memcmp(vector_result.vf+30,mem_v.vf+30,2*sizeof(ps2_vf)));
    assert(vector_result.vf[15].f[0]==mem_v.vf[15].f[0]&&vector_result.vf[15].f[3]==100);
    assert(vector_result.acc.f[0]==4&&vector_result.acc.f[1]==2&&vector_result.acc.f[2]==3&&vector_result.acc.f[3]==0);
    assert(vector_result.vf[20].s[0]==64&&vector_result.vf[20].s[3]==1584);
    for(int j=0;j<4;j++)assert(!memcmp(vector_result.vf+26+((j+1)&3),mem_initial+((mem_v.top+48+j)&1023),16));
    terrain_c[23].u[1]=1;
    assert(terrain_vector_build(&mem_v,terrain_c,terrain_out,4,&vector_result));
    assert(!memcmp(vector_result.vf,mem_v.vf,11*sizeof(ps2_vf)));
    assert(!memcmp(vector_result.vf+11,terrain_out[0],4*sizeof(ps2_vf)));
    assert(!memcmp(vector_result.vf+16,terrain_out[2],4*sizeof(ps2_vf)));
    assert(!memcmp(vector_result.vf+21,terrain_out[3],4*sizeof(ps2_vf)));
    assert(vector_result.vf[25].f[0]==4&&vector_result.vf[25].f[3]==1);
    vector_saved=vector_result;memset(terrain_out[3][2].f,0,3*sizeof(float));
    assert(!terrain_vector_build(&mem_v,terrain_c,terrain_out,4,&vector_result)&&!memcmp(&vector_saved,&vector_result,sizeof vector_result));
    terrain_vector_state vector_kicks[8],vector_kicks_saved[8];u32 vector_count;
    memset(terrain_out,0,sizeof terrain_out);
    for(int j=0;j<4;j++) {terrain_out[j][2].f[3]=1;terrain_out[j][1].f[3]=200;}
    terrain_out[1][2].f[0]=2;terrain_out[2][2].f[1]=.5f;
    assert(terrain_vector_plan(&mem_v,terrain_c,terrain_out,4,&vector_result,vector_kicks,&vector_count)&&vector_count==2);
    /* The second triangle is directly inside and preserves clipping temporaries
     * from its predecessor, while replacing triangle attributes and projection. */
    assert(vector_result.vf[1].u[0]==vector_kicks[0].vf[1].u[0]);
    assert(!memcmp(vector_result.vf+10,vector_kicks[0].vf+10,16));
    assert(!memcmp(vector_result.vf+11,terrain_out[0],64));
    assert(!memcmp(&vector_result,vector_kicks+1,sizeof vector_result));
    vector_saved=vector_result;memcpy(vector_kicks_saved,vector_kicks,sizeof vector_kicks);
    terrain_out[3][2].f[0]=NAN;vector_count=99;
    assert(!terrain_vector_plan(&mem_v,terrain_c,terrain_out,4,&vector_result,vector_kicks,&vector_count)&&vector_count==99);
    assert(!memcmp(&vector_saved,&vector_result,sizeof vector_result)&&!memcmp(vector_kicks_saved,vector_kicks,sizeof vector_kicks));
    /* A fan clipped to empty still leaves edge distances and flipped plane
     * direction in vector registers, despite producing no packet. */
    memset(terrain_out,0,sizeof terrain_out);for(int j=0;j<4;j++)terrain_out[j][2].f[3]=1;
    terrain_out[0][2].f[0]=2;terrain_out[1][2].f[1]=2;
    terrain_out[2][2].f[0]=terrain_out[2][2].f[1]=2;memcpy(terrain_out[3],terrain_out[2],sizeof terrain_out[3]);
    assert(terrain_vector_plan(&mem_v,terrain_c,terrain_out,4,&vector_result,vector_kicks,&vector_count)&&vector_count==0);
    assert(vector_result.vf[10].f[0]==-1);
    /* Unsaturated inherited flags must be calculated, not assumed set. */
    unlit_flags terrain_f,terrain_f_shift,terrain_f_saved;
    memset(terrain_c,0,sizeof terrain_c);memset(terrain_in,0,sizeof terrain_in);
    for(int k=0;k<4;k++)terrain_c[k].f[k]=terrain_c[k+4].f[k]=1;
    terrain_c[23].u[1]=4;terrain_c[30].f[0]=100;terrain_c[30].f[1]=1;
    for(int j=0;j<4;j++) {terrain_in[j][0].s[0]=-16;terrain_in[j][1].s[3]=200;terrain_in[j][2].s[0]=1;terrain_in[j][2].s[3]=1;}
    for(int j=0;j<16;j++)mem_initial[(mem_v.top+32+j)&1023]=((ps2_vf*)terrain_in)[j];
    assert(terrain_decode(terrain_c,terrain_in,4,terrain_out));
    mem_v.status=0x800;mem_v.mac=0;memset(mem_v.fpipe,0,sizeof mem_v.fpipe);
    assert(terrain_flags_plan(&mem_v,16,terrain_c,terrain_out,4,&terrain_f));
    assert(terrain_f.cycles==286&&terrain_f.state.fslot==1&&terrain_f.state.status==0x840&&terrain_f.state.mac==0);
    assert(terrain_f.state.fpipe[0].mac==0&&terrain_f.state.fpipe[1].mac==13&&terrain_f.state.fpipe[2].mac==6&&terrain_f.state.fpipe[3].mac==0);
    for(int j=0;j<4;j++)assert(!terrain_f.state.fpipe[j].valid);
    assert(terrain_flags_plan(&mem_v,4176,terrain_c,terrain_out,4,&terrain_f_shift)&&terrain_f_shift.cycles==288&&terrain_f_shift.state.fslot==3);
    for(int j=0;j<4;j++)assert(!memcmp(terrain_f.state.fpipe+j,terrain_f_shift.state.fpipe+((j+2)&3),sizeof terrain_f.state.fpipe[0]));
    terrain_f_saved=terrain_f;terrain_c[23].u[1]=1;
    for(int j=0;j<4;j++) {terrain_out[j][2].f[0]=2;terrain_out[j][2].f[1]=terrain_out[j][2].f[2]=0;terrain_out[j][2].f[3]=1;}
    assert(terrain_flags_plan(&mem_v,16,terrain_c,terrain_out,4,&terrain_f)&&terrain_f.cycles==187);
    terrain_out[3][2].f[0]=0;
    assert(terrain_flags_plan(&mem_v,16,terrain_c,terrain_out,4,&terrain_f));
    assert(terrain_instruction_plan(16,terrain_c,terrain_out,4,&instruction_count)&&terrain_f.cycles==instruction_count);
    assert(terrain_flags_plan(&mem_v,4176,terrain_c,terrain_out,4,&terrain_f_shift));
    assert(terrain_f_shift.cycles==terrain_f.cycles+2&&terrain_f_shift.state.status==terrain_f.state.status);
    for(int j=0;j<4;j++)assert(!memcmp(terrain_f.state.fpipe+j,terrain_f_shift.state.fpipe+((j+2)&3),sizeof terrain_f.state.fpipe[0]));
    terrain_out[3][2].f[0]=NAN;terrain_f=terrain_f_saved;
    assert(!terrain_flags_plan(&mem_v,16,terrain_c,terrain_out,4,&terrain_f)&&!memcmp(&terrain_f,&terrain_f_saved,sizeof terrain_f));
    /* Empty clipped polygons still execute arithmetic and plane-direction
     * updates. No emitted packet is needed to require drained flag state. */
    memset(terrain_out,0,sizeof terrain_out);for(int j=0;j<4;j++)terrain_out[j][2].f[3]=1;
    terrain_out[0][2].f[0]=2;terrain_out[1][2].f[1]=2;terrain_out[2][2].f[0]=terrain_out[2][2].f[1]=2;
    memcpy(terrain_out[3],terrain_out[2],sizeof terrain_out[3]);
    assert(terrain_flags_plan(&mem_v,16,terrain_c,terrain_out,4,&terrain_f));
    assert(terrain_instruction_plan(16,terrain_c,terrain_out,4,&instruction_count)&&terrain_f.cycles==instruction_count);
    assert((terrain_f.state.status&0x800)!=0);for(int j=0;j<4;j++)assert(!terrain_f.state.fpipe[j].valid);
    terrain_execution_plan execution_plan,execution_saved;
    terrain_c[23].u[1]=4;assert(terrain_decode(terrain_c,terrain_in,4,terrain_out));
    ps2_vu execution_entry=mem_v;memcpy(mem_saved,mem_initial,sizeof mem_saved);
    assert(terrain_execution_build(&mem_v,16,terrain_c,terrain_out,4,&execution_plan));
    assert(!memcmp(&execution_entry,&mem_v,sizeof mem_v)&&!memcmp(mem_saved,mem_initial,sizeof mem_saved));
    assert(execution_plan.cycles==286&&execution_plan.packets==1&&execution_plan.state.tpc==0x1050&&!execution_plan.state.running);
    assert(execution_plan.state.mem==mem_v.mem&&execution_plan.state.micro==mem_v.micro&&execution_plan.state.cc==mem_v.cc);
    assert(execution_plan.state.top==mem_v.top&&execution_plan.state.itop==mem_v.itop&&execution_plan.state.cmsar==mem_v.cmsar);
    execution_saved=execution_plan;terrain_out[3][2].f[0]=NAN;
    assert(!terrain_execution_build(&mem_v,16,terrain_c,terrain_out,4,&execution_plan)&&!memcmp(&execution_plan,&execution_saved,sizeof execution_plan));
    assert(!memcmp(&execution_entry,&mem_v,sizeof mem_v)&&!memcmp(mem_saved,mem_initial,sizeof mem_saved));
    assert(terrain_decode(terrain_c,terrain_in,4,terrain_out));
    terrain_native_on=1;rn_ac4_model_plan_on=0;
    terrain_execution_begin(&mem_v,16,terrain_c,terrain_out,4);
    memset(&terrain_submission_stats,0,sizeof terrain_submission_stats);terrain_test_kicks=0;
    assert(!terrain_try_native(&mem_v,285,terrain_test_kick));
    assert(!terrain_try_native(&mem_v,286,NULL));
    rn_ac4_model_plan_on=1;assert(!terrain_try_native(&mem_v,286,terrain_test_kick));rn_ac4_model_plan_on=0;
    terrain_native_on=0;assert(!terrain_try_native(&mem_v,286,terrain_test_kick));terrain_native_on=1;
    assert(!terrain_test_kicks&&!memcmp(&execution_entry,&mem_v,sizeof mem_v)&&!memcmp(mem_saved,mem_initial,sizeof mem_saved));
    assert(terrain_try_native(&mem_v,286,terrain_test_kick)&&terrain_test_kicks==1);
    assert(!unlit_registers_differ(&execution_saved.state,&mem_v)&&!memcmp(execution_saved.memory,mem_v.mem,16384));
    assert(mem_v.mac==execution_saved.state.mac&&mem_v.status==execution_saved.state.status&&mem_v.fslot==execution_saved.state.fslot);
    assert(!memcmp(mem_v.fpipe,execution_saved.state.fpipe,sizeof mem_v.fpipe));
    assert(!terrain_try_native(&mem_v,286,terrain_test_kick));terrain_native_on=0;
    terrain_execution.active=1;terrain_begin(&mem_v,16,0);assert(!terrain_execution.active&&!terrain_native_ready);
    /* Cockpit-resident terrain code is identical through its continuation;
     * only the exact captured image is admitted, not adjacent signatures. */
    rn_ac4_model_plan_on=1;
    u64 terrain_seen_before=terrain_stats.seen;
    terrain_begin(&mem_v,16,0x758bae290dd231b2ull);
    assert(terrain_stats.seen==terrain_seen_before+1);
    terrain_begin(&mem_v,16,0x758bae290dd231b3ull);
    assert(terrain_stats.seen==terrain_seen_before+1 && !terrain_sample.active);
    terrain_begin(&mem_v,4224,0x758bae290dd231b2ull);
    assert(terrain_stats.seen==terrain_seen_before+1 && !terrain_sample.active);
    rn_ac4_model_plan_on=0;
    /* Sparse submission capacity failure must preserve both output and its
     * previous-memory snapshot. Unchanged qwords consume no capacity. */
    terrain_submission submission,submission_saved;memset(&submission,0x5a,sizeof submission);submission_saved=submission;
    memset(mem_result,0,sizeof mem_result);memcpy(mem_saved,mem_result,sizeof mem_saved);
    for(int j=0;j<257;j++)mem_result[j].u[0]=1;
    assert(!terrain_submission_append(mem_saved,mem_result,2049,&submission));
    assert(!memcmp(&submission,&submission_saved,sizeof submission));
    for(int j=0;j<1024;j++)assert(mem_saved[j].u[0]==0);
    mem_result[256].u[0]=0;
    assert(terrain_submission_append(mem_saved,mem_result,2049,&submission)&&submission.writes==256&&submission.address==1);
    assert(!memcmp(mem_saved,mem_result,sizeof mem_saved));
    assert(terrain_submission_append(mem_saved,mem_result,1,&submission)&&!submission.writes);
    ps2_vu cull_owner={0},other_owner={0};
    memset(&terrain_clip_stats,0,sizeof terrain_clip_stats);
    terrain_sample.owner=&cull_owner;terrain_sample.clip_active=terrain_sample.clip_valid=1;
    terrain_sample.count=4;terrain_sample.clip_culled=2;terrain_sample.clip_kicks=0;
    terrain_end(&other_owner);assert(terrain_sample.clip_active&&!terrain_clip_stats.ended);
    terrain_end(&cull_owner);assert(!terrain_sample.clip_active&&terrain_clip_stats.ended==1&&terrain_clip_stats.predicted_empty==1&&terrain_clip_stats.observed_empty==1&&!terrain_clip_stats.mismatch);
    terrain_end(&cull_owner);assert(terrain_clip_stats.ended==1); /* exactly once */
    terrain_sample.clip_active=1;terrain_sample.clip_kicks=1;
    terrain_end(&cull_owner);assert(terrain_clip_stats.mismatch==1);
    puts("AC4 native model: all synthetic checks passed");return 0;
}
