#include "rn_ac4_model.h"
#include "rn_ac4_mesh.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__SSE2__)
#include <emmintrin.h>
/* Match ps2_ftz/ps2_fclamp after EACH operation. No fused multiply/add:
 * float products fit exactly in double, and both paths round to float before
 * the next instruction. NaNs saturate positive; infinities retain their sign. */
static int regular_transform_enabled=1;
static int register_flags_enabled;
static inline __m128 model_ftz4(__m128 v) {
    __m128i b=_mm_castps_si128(v);
    __m128i zero=_mm_cmpeq_epi32(_mm_and_si128(b,_mm_set1_epi32(0x7f800000)),_mm_setzero_si128());
    return _mm_castsi128_ps(_mm_andnot_si128(_mm_and_si128(zero,_mm_set1_epi32(0x7fffffff)),b));
}
static inline __m128 model_clamp4(__m128 v) {
    __m128i b=_mm_castps_si128(v), exp=_mm_set1_epi32(0x7f800000);
    __m128i special=_mm_cmpeq_epi32(_mm_and_si128(b,exp),exp);
    __m128i nan=_mm_castps_si128(_mm_cmpunord_ps(v,v));
    __m128i sign=_mm_andnot_si128(nan,_mm_and_si128(b,_mm_set1_epi32((int)0x80000000u)));
    __m128i limit=_mm_or_si128(sign,_mm_set1_epi32(0x7f7fffff));
    return model_ftz4(_mm_castsi128_ps(_mm_or_si128(_mm_and_si128(special,limit),_mm_andnot_si128(special,b))));
}
static inline __m128 model_mul4(__m128 a,__m128 b) {
    return model_clamp4(_mm_mul_ps(model_ftz4(a),model_ftz4(b)));
}
static inline __m128 model_add4(__m128 a,__m128 b) {
    return model_clamp4(_mm_add_ps(model_ftz4(a),model_ftz4(b)));
}
/* With each nonzero input in [2^-30,2^30], products have magnitude
 * <=2^60 and are multiples of at least 2^-83 after float rounding.
 * Four ordered sums cannot overflow or produce a nonzero subnormal.
 * Preserve separate operations; never fuse multiply/add. Exceptional and
 * very small/large inputs continue through the fully clamped path. */
static int model_regular_transform(const ps2_vf m[4],const float p[4]) {
    if(!regular_transform_enabled)return 0;
    __m128i bad=_mm_setzero_si128();
    for(int j=0;j<5;j++) {
        __m128i a=_mm_and_si128(_mm_castps_si128(_mm_loadu_ps(j<4?m[j].f:p)),_mm_set1_epi32(0x7fffffff));
        __m128i small=_mm_andnot_si128(_mm_cmpeq_epi32(a,_mm_setzero_si128()),
            _mm_cmpgt_epi32(_mm_set1_epi32(0x30800000),a));
        bad=_mm_or_si128(bad,_mm_or_si128(small,_mm_cmpgt_epi32(a,_mm_set1_epi32(0x4e800000))));
    }
    return !_mm_movemask_epi8(bad);
}

#endif

static int terrain_native_on,terrain_native_ready,model_native_requested;
/* AC4 terrain input preparation, independent of the later packet/clipping
 * program. This observer never mutates guest memory or claims GPU draws. */
static int terrain_decode(const ps2_vf c[32],const ps2_vf input[][4],u32 count,ps2_vf output[10][4]) {
    if(!c || !input || !output || (count!=4 && count!=9 && count!=10))return 0;
    ps2_vf result[10][4];u32 mode=c[23].u[1]&65535;
    if(mode!=1 && mode!=4 && count<9)return 0;
    for(u32 j=0;j<count;j++) {
        for(int a=0;a<4;a++)for(int k=0;k<4;k++)result[j][a].f[k]=(float)input[j][a].s[k];
        for(int k=0;k<2;k++) {
            result[j][0].f[k]=ps2_fmul(result[j][0].f[k],1.0f/16);
            result[j][3].f[k]=ps2_fadd(ps2_fmul(result[j][3].f[k],1.0f/128),ps2_fmul((float)c[20].s[k],1.0f/64));
        }
        for(int k=0;k<3;k++) {
            if(!isfinite(c[12].f[k]))return 0;
            result[j][2].f[k]=ps2_fadd(ps2_fmul(result[j][2].f[k],k==1?-80.0f:1024.0f),c[12].f[k]);
        }
        result[j][3].f[2]=1;
    }
    if(mode!=1 && mode!=4) {
        static const u8 neighbours[5][2]={{1,5},{1,3},{3,5},{5,7},{7,1}};
        ps2_vf positions[5];
        for(int j=0;j<5;j++) {
            float distance=0;
            for(int k=0;k<3;k++) {
                if(!isfinite(c[14].f[k]))return 0;
                float d=ps2_fsub(result[2*j][2].f[k],c[14].f[k]);
                float square=ps2_fmul(d,d);distance=k?ps2_fadd(distance,square):square;
            }
            if(!isfinite(c[13].f[0]) || !isfinite(c[13].f[1]))return 0;
            float factor=fminf(fmaxf(ps2_fmul(ps2_fsub(distance,c[13].f[1]),c[13].f[0]),0),1);
            for(int k=0;k<4;k++) {
                float average=ps2_fmul(ps2_fadd(result[neighbours[j][0]][2].f[k],result[neighbours[j][1]][2].f[k]),.5f);
                positions[j].f[k]=ps2_fadd(ps2_fmul(ps2_fsub(average,result[2*j][2].f[k]),factor),result[2*j][2].f[k]);
            }
        }
        for(int j=0;j<5;j++)result[2*j][2]=positions[j];
    }
    memcpy(output,result,count*4*sizeof(ps2_vf));return 1;
}
static void transform(const ps2_vf *m,const float *p,float *r);
/* Ordinary terrain has three setup records, a primary tag and one secondary
 * tag. Both streams are written even when the primary tag ends submission. */
static int terrain_packet(const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,ps2_vf output[65],u32 *size) {
    if(!c || !decoded || !output || !size || (count<3 || count>10))return 0;
    if((c[23].u[1]&65535)==1)return 0; /* clipped fan path needs separate records */
    ps2_vf packet[65]={0};u32 second=4+3*count;
    packet[0]=c[15];packet[1]=c[19];packet[2]=c[16];packet[3]=c[17];packet[second]=c[18];
    packet[3].s[0]=(c[22].u[0]&65535)?(s32)count:(s16)(0x8000u|count);
    packet[second].s[0]=(s16)(0x8000u|count);
    for(u32 j=0;j<count;j++) {
        const ps2_vf *r=decoded[j];float p[4];
        for(int k=0;k<4;k++)if(!isfinite(r[2].f[k]))return 0;
        transform(c,r[2].f,p);
        if(!isfinite(p[3]) || p[3]==0)return 0;
        float q=1.0f/p[3],fog=ps2_fmul(ps2_fsub(c[30].f[0],p[3]),c[30].f[1]);
        if(!isfinite(fog))return 0;
        fog=fminf(fmaxf(fog,0),255);
        u32 a=4+3*j,b=second+1+3*j;
        for(int k=0;k<4;k++) {
            packet[a].f[k]=ps2_fmul(r[0].f[k],q);
            packet[b].f[k]=ps2_fmul(r[3].f[k],q);
            packet[a+1].s[k]=ps2_cvt_w_s(r[1].f[k]);
            packet[b+1].s[k]=k==3?ps2_cvt_w_s(ps2_fmul(ps2_fmul(r[1].f[3],0x1.010100p-8f),fog)):128;
            float packed=ps2_fmul(k==3?fog:ps2_fmul(p[k],q),16);
            if(!isfinite(packed) || !isfinite(packet[a].f[k]) || !isfinite(packet[b].f[k]))return 0;
            packet[a+2].s[k]=packet[b+2].s[k]=ps2_cvt_w_s(packed);
        }
    }
    *size=5+6*count;memcpy(output,packet,*size*sizeof(ps2_vf));return 1;
}
/* Native terrain draw topology follows GS queue order (no alternating strip
 * index reversal). Keep pass identity and source indices for GPU record upload.
 * ADC-suppressed triangles advance the queue but must not become native draws. */
typedef struct {u8 pass,index[3];} terrain_triangle;
static int terrain_topology(const ps2_vf packet[65],u32 count,
                            terrain_triangle output[16],u32 *triangles) {
    if(!packet || !output || !triangles || (count<3 || count>10))return 0;
    terrain_triangle result[16];u32 used=0;
    for(u32 pass=0;pass<2;pass++) {
        u32 start=pass?4+3*count:3;
        const ps2_vf *tag=packet+start;
        u32 prim=(tag->u[1]>>15)&2047,kind=prim&7;
        if((tag->u[0]&32767)!=count || !(tag->u[1]&(1u<<14))
           || ((tag->u[1]>>26)&3)!=0 || (tag->u[1]>>28)!=3
           || (tag->u[2]&0xfff)!=0x412 || !(prim&8) || (prim&256)
           || (kind!=4 && kind!=5))return 0;
        for(u32 j=2;j<count;j++) {
            if(packet[start+3+3*j].u[3]&0x8000)continue;
            terrain_triangle t={(u8)pass,{(u8)(kind==5?0:j-2),(u8)(j-1),(u8)j}};
            result[used++]=t;
        }
        if(tag->u[0]&32768)break;
    }
    memcpy(output,result,used*sizeof *output);*triangles=used;return 1;
}
/* Mode-1 terrain splits a fan around vertex zero. A triangle is trivially
 * rejected only when all three clip codes share an outside plane. */
static int terrain_culled_fan(const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,u32 *culled) {
    if(!c || !decoded || !culled || count<3 || count>10 || (c[23].u[1]&65535)!=1)return 0;
    for(int j=4;j<8;j++)for(int k=0;k<4;k++)if(!isfinite(c[j].f[k]))return 0;
    u32 codes[10]={0},rejected=0;
    for(u32 j=0;j<count;j++) {
        for(int k=0;k<4;k++)if(!isfinite(decoded[j][2].f[k]))return 0;
        float clip[4];transform(c+4,decoded[j][2].f,clip);
        for(int k=0;k<4;k++)if(!isfinite(clip[k]))return 0;
        float w=fabsf(clip[3]);
        for(int k=0;k<3;k++) {
            if(clip[k]>w)codes[j]|=1u<<(2*k);
            if(clip[k]<-w)codes[j]|=2u<<(2*k);
        }
    }
    for(u32 j=1;j+1<count;j++)rejected+=(codes[0]&codes[j]&codes[j+1])!=0;
    *culled=rejected;return 1;
}
typedef struct {ps2_vu state;u32 cycles;int cockpit;} unlit_flags;
static void flags_advance(unlit_flags *f,u32 until) {
    if(!f || f->cycles>=until)return;
    /* No new writes during this gap: four slots can retire at most once. */
    u32 steps=until-f->cycles; if(steps>4)steps=4;
    while(steps--) {
        u32 slot=f->cycles++&3u;
        if(f->state.fpipe[slot].valid) {
            f->state.mac=f->state.fpipe[slot].mac;
            f->state.status=f->state.fpipe[slot].status;
            f->state.fpipe[slot].valid=0;
        }
    }
    f->cycles=until;
}
static void flags_result_memory(unlit_flags *f,u32 cycle,u32 mask,const float value[4]) {
    if(!f)return;
    flags_advance(f,cycle);u32 mac=0,st=0,slot=(cycle-1)&3u;
#if defined(__SSE2__)
    /* VU lane order is the reverse of movemask order. Negative zero sets
     * only zero, matching the scalar path; NaN signs remain observable. */
    static const u8 reverse[16]={0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};
    __m128i bits=_mm_castps_si128(_mm_loadu_ps(value));
    __m128i zero=_mm_cmpeq_epi32(_mm_and_si128(bits,_mm_set1_epi32(0x7fffffff)),_mm_setzero_si128());
    u32 z=(u32)_mm_movemask_ps(_mm_castsi128_ps(zero));
    u32 neg=(u32)_mm_movemask_ps(_mm_castsi128_ps(bits)) & ~z & 15u;
    u32 lanes=mask&15u;
    mac=(reverse[z] | ((u32)reverse[neg]<<4)) & (lanes | (lanes<<4));
#else
    for(int k=0;k<4;k++)if(mask&(8u>>k)) {
        u32 bits;memcpy(&bits,value+k,4);
        if(!(bits&0x7fffffffu))mac|=8u>>k;
        else if(bits&0x80000000u)mac|=128u>>k;
    }
#endif
    if(mac&15u)st|=1;
    if(mac&240u)st|=2;
    f->state.fpipe[slot].mac=mac;
    f->state.fpipe[slot].status=(f->state.status&0xff0u)|st|(st<<6);
    f->state.fpipe[slot].valid=1;
}
#if defined(__SSE2__)
/* Keep intermediate values in SIMD registers across a transform's flag
 * writes. Reversing lanes makes movemask match VU x/y/z/w flag order. */
PS2_INLINE void flags_vector_result(unlit_flags *f,u32 cycle,u32 mask,__m128 value) {
    if(!f)return;
    flags_advance(f,cycle);
    __m128i bits=_mm_shuffle_epi32(_mm_castps_si128(value),0x1b);
    __m128i zero=_mm_cmpeq_epi32(_mm_and_si128(bits,_mm_set1_epi32(0x7fffffff)),_mm_setzero_si128());
    u32 z=(u32)_mm_movemask_ps(_mm_castsi128_ps(zero));
    u32 neg=(u32)_mm_movemask_ps(_mm_castsi128_ps(bits))&~z;
    u32 lanes=mask&15u,mac=(z|(neg<<4))&(lanes|(lanes<<4));
    u32 st=((mac&15u)?1u:0u)|((mac&240u)?2u:0u),slot=(cycle-1)&3u;
    f->state.fpipe[slot].mac=mac;
    f->state.fpipe[slot].status=(f->state.status&0xff0u)|st|(st<<6);
    f->state.fpipe[slot].valid=1;
}
#endif
static void flags_result(unlit_flags *f,u32 cycle,u32 mask,const float value[4]) {
    if(!f)return;
#if defined(__SSE2__)
    if(register_flags_enabled) {flags_vector_result(f,cycle,mask,_mm_loadu_ps(value));return;}
#endif
    flags_result_memory(f,cycle,mask,value);
}
static void flags_scalar(unlit_flags *f,u32 cycle,u32 mask,float value) {
    float r[4]={value,value,value,value};flags_result(f,cycle,mask,r);
}
static void flags_transform(unlit_flags *f,u32 cycle,const ps2_vf m[4],const float p[4],float out[4]) {
#if defined(__SSE2__)
    if(register_flags_enabled) {
        int regular=model_regular_transform(m,p);__m128 a=_mm_setzero_ps();
        for(int j=0;j<4;j++) {
            __m128 x=_mm_loadu_ps(m[j].f),y=_mm_set1_ps(p[j]);
            __m128 product=regular?_mm_mul_ps(x,y):model_mul4(x,y);
            a=j?(regular?_mm_add_ps(a,product):model_add4(a,product)):product;
            flags_vector_result(f,cycle+(u32)j,15,a);
        }
        _mm_storeu_ps(out,a);return;
    }
    if(model_regular_transform(m,p)) {
        __m128 a=_mm_mul_ps(_mm_loadu_ps(m[0].f),_mm_set1_ps(p[0]));
        _mm_storeu_ps(out,a);flags_result(f,cycle,15,out);
        for(int j=1;j<4;j++) {
            a=_mm_add_ps(a,_mm_mul_ps(_mm_loadu_ps(m[j].f),_mm_set1_ps(p[j])));
            _mm_storeu_ps(out,a);flags_result(f,cycle+(u32)j,15,out);
        }
        return;
    }
#endif
    for(int j=0;j<4;j++) {
#if defined(__SSE2__)
        __m128 product=model_mul4(_mm_loadu_ps(m[j].f),_mm_set1_ps(p[j]));
        _mm_storeu_ps(out,j?model_add4(_mm_loadu_ps(out),product):product);
#else
        for(int k=0;k<4;k++) {
            float product=ps2_fmul(m[j].f[k],p[j]);
            out[k]=j?ps2_fadd(out[k],product):product;
        }
#endif
        flags_result(f,cycle+(u32)j,15,out);
    }
}
typedef struct {ps2_vf vf[32],acc;} terrain_vector_state;
static void terrain_vector_acc(terrain_vector_state *state,const ps2_vf m[4],const float p[4]) {
    for(int k=0;k<4;k++) {
        float value=ps2_fmul(m[0].f[k],p[0]);
        value=ps2_fadd(value,ps2_fmul(m[1].f[k],p[1]));
        state->acc.f[k]=ps2_fadd(value,ps2_fmul(m[2].f[k],p[2]));
    }
}
typedef struct {ps2_vf records[4];float clip[4];} terrain_clip_vertex;
typedef struct {u32 any,common,plane,count,edge,cycles;float q;int has_q;} terrain_clip_control;
static int terrain_clip_triangle_control(const ps2_vf c[32],const ps2_vf input[3][4],ps2_vf output[10][4],u32 *vertices,ps2_vf *memory,terrain_clip_control *control,terrain_vector_state *trace,unlit_flags *flags,u32 origin) {
    if(!c || !input || !output || !vertices)return 0;
    terrain_clip_control local_control;if(flags && !control)control=&local_control;
    terrain_clip_vertex a[10],b[10];u32 count=3,any=0,common=63;
    for(int j=4;j<8;j++)for(int k=0;k<4;k++)if(!isfinite(c[j].f[k]))return 0;
    for(int j=0;j<3;j++) {
        memcpy(a[j].records,input[j],sizeof a[j].records);
        for(int q=0;q<4;q++)for(int k=0;k<4;k++)if(!isfinite(input[j][q].f[k]))return 0;
        flags_transform(flags,origin+15+4*(u32)j,c+4,input[j][2].f,a[j].clip);u32 code=0;
        for(int k=0;k<4;k++)if(!isfinite(a[j].clip[k]))return 0;
        float w=fabsf(a[j].clip[3]);
        for(int k=0;k<3;k++) {if(a[j].clip[k]>w)code|=1u<<(2*k);if(a[j].clip[k]<-w)code|=2u<<(2*k);}
        any|=code;common&=code;
    }
    if(trace) {
        for(int j=0;j<3;j++) {memcpy(trace->vf+11+5*j,a[j].records,64);memcpy(trace->vf[15+5*j].f,a[j].clip,16);}
        memcpy(trace->vf+26,c+4,64);terrain_vector_acc(trace,c+4,a[2].records[2].f);
    }
    if(control)*control=(terrain_clip_control){.any=any,.common=common,.count=3};
    if(common) {
        u32 plane=0;while(!(common&(1u<<plane)))plane++;
        if(control)control->cycles=39+2*plane;
        *vertices=0;return 1;
    }
    if(control)control->cycles=any?70:53;
    if(memory && any)for(u32 j=0;j<3;j++) {
        memcpy(memory+32+5*j,a[j].records,64);memcpy(memory[32+5*j+4].f,a[j].clip,16);
    }
    if(any)flags_scalar(flags,origin+70,8,1);
    if(trace && any)trace->vf[10].f[0]=1;
    if(any)for(int plane=0;plane<6;plane++) {
        int axis=plane/2;float sign=(plane&1)?-1:1;u32 next=0;
        if(control)control->cycles+=plane?14:15;
        for(u32 j=0;j<count;j++) {
            const terrain_clip_vertex *prev=&a[(j+count-1)%count],*cur=&a[j];
            float d0=ps2_fsub(ps2_fmul(sign,prev->clip[axis]),prev->clip[3]);
            float d1=ps2_fsub(ps2_fmul(sign,cur->clip[axis]),cur->clip[3]);
            int in0=d0<0,in1=d1<0;
            u32 edge=origin+(control?control->cycles:0),axis_pairs=axis==0?14u:axis==1?18u:17u;
            flags_scalar(flags,edge+(axis==0?6:10),4,ps2_fadd(0,prev->clip[axis]));
            flags_scalar(flags,edge+(axis==0?10:14),2,ps2_fadd(0,cur->clip[axis]));
            flags_scalar(flags,edge+axis_pairs+1,8,ps2_fmul(sign,prev->clip[axis]));
            flags_scalar(flags,edge+axis_pairs+2,8,ps2_fmul(sign,cur->clip[axis]));
            flags_scalar(flags,edge+axis_pairs+5,8,d0);flags_scalar(flags,edge+axis_pairs+6,8,d1);
            if(trace) {
                trace->vf[10].f[1]=ps2_fadd(0,prev->clip[axis]);trace->vf[10].f[2]=ps2_fadd(0,cur->clip[axis]);
                trace->vf[1].f[0]=d0;trace->vf[2].f[0]=d1;
            }
            if(control) {
                control->edge=(u32)!in0+2u*(u32)!in1;
                u32 edge_pairs=in0?(in1?19u:67u):(in1?68u:20u);
                control->cycles+=axis_pairs+19u-(u32)in0-(u32)in1+edge_pairs;
            }
            if(in0) {if(next==10)return 0;b[next++]=*prev;}
            if(in0!=in1) {
                if(next==10)return 0;
                const terrain_clip_vertex *inside=in0?prev:cur,*outside=in0?cur:prev;
                float x0=ps2_fadd(ps2_fmul(ps2_fmul(sign,inside->clip[3]),-1),inside->clip[axis]);
                float x1=ps2_fadd(ps2_fmul(ps2_fmul(sign,outside->clip[3]),-1),outside->clip[axis]);
                float divisor=ps2_fsub(x1,x0);if(!isfinite(divisor) || divisor==0)return 0;
                float quotient=1.0f/divisor;
                if(control) {control->q=quotient;control->has_q=1;}
                float factor=fabsf(ps2_fmul(x0,quotient));
                u32 branch=edge+axis_pairs+19-(u32)in0-(u32)in1;
                float prev_w=ps2_fmul(sign,prev->clip[3]),cur_w=ps2_fmul(sign,cur->clip[3]);
                flags_scalar(flags,branch+(in0?4:7),8,prev_w);flags_scalar(flags,branch+(in0?5:8),8,cur_w);
                flags_scalar(flags,branch+(in0?8:11),8,ps2_fmul(prev_w,-1));flags_scalar(flags,branch+(in0?9:12),8,ps2_fmul(cur_w,-1));
                flags_scalar(flags,branch+(in0?13:15),8,in0?x0:x1);flags_scalar(flags,branch+(in0?14:16),8,in0?x1:x0);
                flags_scalar(flags,branch+(in0?18:20),8,divisor);
                float differences[4][4],products[4][4];
                for(int q=0;q<4;q++) {
                    for(int k=0;k<4;k++)differences[q][k]=ps2_fsub(outside->records[q].f[k],inside->records[q].f[k]);
                    flags_result(flags,branch+(in0?23:25)+(u32)q,15,differences[q]);
                }
                flags_scalar(flags,branch+(in0?27:30),8,ps2_fmul(x0,quotient));
                for(int q=0;q<4;q++) {
                    for(int k=0;k<4;k++)products[q][k]=ps2_fmul(differences[q][k],factor);
                    flags_result(flags,branch+(in0?35:38)+(u32)q,15,products[q]);
                }

                for(int q=0;q<4;q++)for(int k=0;k<4;k++) {
                    float delta=ps2_fsub(outside->records[q].f[k],inside->records[q].f[k]);
                    b[next].records[q].f[k]=ps2_fadd(ps2_fmul(delta,factor),inside->records[q].f[k]);
                    if(!isfinite(b[next].records[q].f[k]))return 0;
                }
                for(int q=0;q<4;q++)flags_result(flags,branch+(in0?39:42)+(u32)q,15,b[next].records[q].f);
                flags_transform(flags,branch+(in0?45:46),c+4,b[next].records[2].f,b[next].clip);
                if(trace) {
                    trace->vf[1].f[0]=in0?x0:factor;trace->vf[2].f[0]=in0?factor:x0;
                    memcpy(trace->vf+21,b[next].records,64);memcpy(trace->vf[25].f,b[next].clip,16);
                    memcpy(trace->vf+26,c+4,64);terrain_vector_acc(trace,c+4,b[next].records[2].f);
                }
                next++;
            }
        }
        if(trace) {
            if(!memory || count>8)return 0;
            memcpy(trace->vf+11,a[count-1].records,64);memcpy(trace->vf[15].f,a[count-1].clip,16);
            trace->vf[10].f[0]=ps2_fmul(sign,-1);
        }
        if(memory) {
            if(next>8)return 0; /* Do not model overlapping scratch banks. */
            u32 base=(plane&1)?32:72;
            for(u32 j=0;j<next;j++) {memcpy(memory+base+5*j,b[j].records,64);memcpy(memory[base+5*j+4].f,b[j].clip,16);}
        }
        /* The last prefetch follows writes from the final edge. At eight
         * input vertices it can reach the other scratch bank. */
        if(trace)memcpy(trace->vf+16,memory+((plane&1)?72:32)+5*count,5*sizeof(ps2_vf));
        flags_scalar(flags,origin+(control?control->cycles:0)+((plane&1)?8:9),8,ps2_fmul(sign,-1));
        if(control) {
            control->plane=(u32)plane;control->count=next;
            control->cycles+=(plane&1)?11:12;
            if(next>=3)control->cycles+=2;
        }
        count=next;if(count<3) {*vertices=0;return 1;}
        memcpy(a,b,count*sizeof *a);
    }
    if(control && any)control->cycles+=11*count+6;
    for(u32 j=0;j<count;j++) {
        memcpy(output[j],a[j].records,sizeof a[j].records);
        if(memory)memcpy(memory+(any?72:0)+4*j,a[j].records,64);
    }
    if(trace && any)memcpy(trace->vf+26,a[count-1].records,64);
    *vertices=count;return 1;
}
static int terrain_clip_triangle_state(const ps2_vf c[32],const ps2_vf input[3][4],ps2_vf output[10][4],u32 *vertices,ps2_vf *memory) {
    return terrain_clip_triangle_control(c,input,output,vertices,memory,NULL,NULL,NULL,0);
}
static int terrain_clip_triangle(const ps2_vf c[32],const ps2_vf input[3][4],ps2_vf output[10][4],u32 *vertices) {
    return terrain_clip_triangle_state(c,input,output,vertices,NULL);
}
static struct {int valid;u32 count,next,size[8],address[8],vertices[8];ps2_vf packet[8][65],polygon[8][10][4];} terrain_clip_plan;
static struct {u64 built,declined,packets,bad,sequence;} terrain_polygon_stats;
static void terrain_build_clip_plan(const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,u32 top) {
    terrain_clip_plan.valid=0;terrain_clip_plan.count=terrain_clip_plan.next=0;
    ps2_vf constants[32];memcpy(constants,c,sizeof constants);constants[23].u[1]=4;
    for(u32 j=1;j+1<count;j++) {
        ps2_vf input[3][4],polygon[10][4];u32 n;
        memcpy(input[0],decoded[0],sizeof input[0]);memcpy(input[1],decoded[j],sizeof input[1]);memcpy(input[2],decoded[j+1],sizeof input[2]);
        if(!terrain_clip_triangle(c,input,polygon,&n)) {terrain_polygon_stats.declined++;return;}
        if(!n)continue;
        u32 at=terrain_clip_plan.count;
        if(at==8 || !terrain_packet(constants,polygon,n,terrain_clip_plan.packet[at],&terrain_clip_plan.size[at])) {terrain_polygon_stats.declined++;return;}
        terrain_clip_plan.vertices[at]=n;memcpy(terrain_clip_plan.polygon[at],polygon,n*sizeof polygon[0]);
        terrain_clip_plan.address[at]=(top+((at&1)?126:210))&1023;
        terrain_clip_plan.count++;
    }
    terrain_clip_plan.valid=1;terrain_polygon_stats.built++;
}
/* Reconstruct all writes from the entry snapshot. Publish only after the
 * entire fan succeeds, including alternating clipping scratch and packet banks. */
static int terrain_memory_build(const ps2_vu *v,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,ps2_vf output[1024]) {
    if(!v || !v->mem || v->mem_size!=16384 || !c || !decoded || !output || count<3 || count>10)return 0;
    ps2_vf result[1024],packet[65];u32 size=0;
    memcpy(result,v->mem,sizeof result);
    for(u32 j=0;j<count*4;j++)result[(v->top+32+j)&1023]=((const ps2_vf*)decoded)[j];
    if((c[23].u[1]&65535)==1) {
        u32 culled;if(!terrain_culled_fan(c,decoded,count,&culled))return 0;
        if(culled!=count-2) {
            if(v->top<112 || v->top+275>1024)return 0;
            ps2_vf constants[32];memcpy(constants,c,sizeof constants);constants[23].u[1]=4;u32 emitted=0;
            for(u32 j=1;j+1<count;j++) {
                ps2_vf input[3][4],polygon[10][4];u32 n;
                memcpy(input[0],decoded[0],sizeof input[0]);memcpy(input[1],decoded[j],sizeof input[1]);memcpy(input[2],decoded[j+1],sizeof input[2]);
                if(!terrain_clip_triangle_state(c,input,polygon,&n,result))return 0;
                if(!n)continue;
                if(!terrain_packet(constants,polygon,n,packet,&size))return 0;
                u32 bank=v->top+((emitted++&1)?126:210);
                for(u32 k=0;k<size;k++)result[(bank+k)&1023]=packet[k];
            }
        }
    } else {
        if(!terrain_packet(c,decoded,count,packet,&size))return 0;
        for(u32 j=0;j<size;j++)result[(v->top+210+j)&1023]=packet[j];
    }
    memcpy(output,result,sizeof result);return 1;
}
static struct {const ps2_vu *owner;int active;ps2_vf expected[1024];} terrain_memory;
static struct {u64 built,ended,bad,declined;} terrain_memory_stats;
static void terrain_memory_end(const ps2_vu *v) {
    if(!terrain_memory.active || terrain_memory.owner!=v)return;
    terrain_memory.active=0;terrain_memory_stats.ended++;
    if(memcmp(v->mem,terrain_memory.expected,16384)) {
        if(!terrain_memory_stats.bad)for(u32 j=0;j<1024;j++) {
            ps2_vf actual;memcpy(&actual,v->mem+j*16,16);
            if(memcmp(&actual,terrain_memory.expected+j,16)) {
                ps2_log("ac4 terrain memory difference: qw%u",j);break;
            }
        }
        terrain_memory_stats.bad++;
    }
}
static struct {const ps2_vu *owner;int active,clip_active,clip_valid;u32 top,count,clip_culled,clip_kicks;ps2_vf decoded[10][4],constants[32];} terrain_sample;
static struct {u64 seen,compared,vertices,bad,declined,packets,packet_bad,packet_declined,bank_bad,topologies,triangles,topology_declined;u64 counts[24],modes[8];} terrain_stats;
static struct {u64 seen,ended,predicted_empty,observed_empty,mismatch,partial,invalid;} terrain_clip_stats;
static int terrain_integer_build(const ps2_vu *v,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,u16 end[16],u16 kick[16],int *has_kick) {
    if(!v || !c || !decoded || !end || !kick || !has_kick || count<3 || count>10)return 0;
    u16 r[16];memcpy(r,v->vi,sizeof r);u32 mode=c[23].u[1]&65535;
    r[2]=(u16)v->top;r[5]=65535;r[6]=(u16)count;r[11]=4;
    if(mode==1) {
        u32 culled;if(!terrain_culled_fan(c,decoded,count,&culled) || culled!=count-2)return 0;
        u32 common=63;
        for(int j=0;j<3;j++) {
            u32 index=j?count-3+(u32)j:0;float clip[4];u32 code=0;
            transform(c+4,decoded[index][2].f,clip);float w=fabsf(clip[3]);
            for(int k=0;k<3;k++) {if(clip[k]>w)code|=1u<<(2*k);if(clip[k]<-w)code|=2u<<(2*k);}
            common&=code;
        }
        u32 plane=0;while(plane<5 && !(common&(1u<<plane)))plane++;
        /* FCOR for the next plane executes in the taken branch delay slot. */
        r[1]=(u16)(plane==5?1:((common>>(plane+1))&1));r[3]=(u16)(v->top+32);r[4]=(u16)(v->top+72);r[7]=0;r[12]=1;
        r[13]=(u16)(v->top+32+4*(count-2));*has_kick=0;
    } else {
        r[3]=(u16)(v->top+32+4*(count+1));r[4]=(u16)(v->top+210);r[7]=1;
        r[8]=(u16)(v->top+215+6*count);r[9]=(u16)(8+6*count);
        r[12]=(u16)c[22].u[0];r[13]=(u16)(0x8000|count);
        memcpy(kick,r,sizeof r);kick[5]=0;kick[6]=65535;*has_kick=1;
    }
    memcpy(end,r,sizeof r);return 1;
}
/* Walk each fan triangle, preserving registers left by earlier emitted or
 * rejected polygons. Record every kick, rather than only the final polygon. */
static int terrain_integer_plan(const ps2_vu *v,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,u16 end[16],u16 kicks[8][16],u32 *kick_count) {
    if(!v || !c || !decoded || !end || !kicks || !kick_count || count<3 || count>10)return 0;
    if((c[23].u[1]&65535)!=1) {
        int has=0;if(!terrain_integer_build(v,c,decoded,count,end,kicks[0],&has))return 0;
        *kick_count=(u32)has;return 1;
    }
    if(!v || !decoded || count<3 || count>10)return 0;
    u16 r[16],packets[8][16];memcpy(r,v->vi,sizeof r);u32 emitted=0;
    r[2]=(u16)v->top;r[4]=(u16)(v->top+72);r[7]=0;r[11]=4;r[12]=1;
    for(u32 j=1;j+1<count;j++) {
        r[3]=(u16)(v->top+32);r[13]=(u16)(v->top+32+4*j);r[5]=(u16)(count-2-j);r[6]=(u16)count;
        ps2_vf input[3][4],polygon[10][4];u32 n;terrain_clip_control control;
        memcpy(input[0],decoded[0],sizeof input[0]);memcpy(input[1],decoded[j],sizeof input[1]);memcpy(input[2],decoded[j+1],sizeof input[2]);
        if(!terrain_clip_triangle_control(c,input,polygon,&n,NULL,&control,NULL,NULL,0))return 0;
        if(control.common) {
            u32 plane=0;while(plane<5 && !(control.common&(1u<<plane)))plane++;
            r[1]=(u16)(plane==5?1:((control.common>>(plane+1))&1));continue;
        }
        r[1]=0;
        if(control.any) {
            if(!n) {
                r[3]=(u16)control.edge;r[4]=(u16)((control.plane&1)?0:1);
                r[8]=(u16)((control.plane&1)?32:72);r[9]=(u16)((control.plane&1)?72:32);
                r[10]=(u16)(1u<<control.plane);r[11]=(u16)(control.count-1);r[12]=32;r[13]=(u16)(control.count-2);continue;
            }
            r[10]=64;r[11]=65535;
        }
        u32 bank=v->top+((r[7]!=0)?126:210);r[7]=(u16)!r[7];
        r[3]=(u16)((control.any?72:0)+4*(n+1));r[4]=(u16)bank;r[6]=65535;
        r[8]=(u16)(bank+5+6*n);r[9]=(u16)(8+6*n);r[12]=(u16)c[22].u[0];r[13]=(u16)(0x8000|n);
        if(emitted==8)return 0;
        memcpy(packets[emitted++],r,sizeof r);
    }
    r[5]=65535;r[6]=(u16)count;memcpy(end,r,sizeof r);memcpy(kicks,packets,emitted*sizeof packets[0]);*kick_count=emitted;return 1;
}
static struct {const ps2_vu *owner;int active;u32 expected_kicks,kicks;u16 end[16],kick[8][16];} terrain_integer;
static struct {u64 built,ended,kicks,bad,kick_bad,sequence,declined;} terrain_integer_stats;
static void terrain_integer_end(const ps2_vu *v) {
    if(!terrain_integer.active || terrain_integer.owner!=v)return;
    terrain_integer.active=0;terrain_integer_stats.ended++;
    terrain_integer_stats.sequence+=terrain_integer.kicks!=terrain_integer.expected_kicks;
    int bad=0;for(int j=0;j<16;j++)if(v->vi[j]!=terrain_integer.end[j]) {
        if(!terrain_integer_stats.bad)ps2_log("ac4 terrain integer end: vi%d %04x/%04x",j,terrain_integer.end[j],v->vi[j]);
        bad=1;
    }
    terrain_integer_stats.bad+=bad;
}
/* Count executed VU instruction pairs from the ordinary branch schedule.
 * This is guest execution accounting, not elapsed time or a frame-rate estimate.
 * The clipping schedule is composed separately from its actual edge cases. */
static int terrain_ordinary_cycles(u32 entry,const ps2_vf c[32],u32 count,u32 *out) {
    if(!c || !out || (entry!=16 && entry!=4176) || (count!=4 && count!=9 && count!=10))return 0;
    u32 mode=c[23].u[1]&65535;
    if(mode!=2 && mode!=4)return 0;
    if(mode==2 && count<9)return 0;
    *out=74+53*count+(mode==2?77:0)+((c[22].u[0]&65535)!=0)+(entry==4176?2:0);
    return 1;
}
/* Include clipping decisions, scratch compaction and alternating packet-bank
 * branch lengths. The clip kernel counts scheduled instruction pairs, including
 * branch delay slots; WAITQ remains one instruction in this accounting. */
static int terrain_instruction_plan(u32 entry,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,u32 *out) {
    if(terrain_ordinary_cycles(entry,c,count,out))return 1;
    if(!c || !decoded || !out || (entry!=16 && entry!=4176) ||
       (count!=4 && count!=9 && count!=10) || (c[23].u[1]&65535)!=1)return 0;
    u32 cycles=31+18*count+(entry==4176?2:0),emitted=0;
    for(u32 j=1;j+1<count;j++) {
        ps2_vf triangle[3][4],polygon[10][4];u32 n;terrain_clip_control control;
        memcpy(triangle[0],decoded[0],sizeof triangle[0]);
        memcpy(triangle[1],decoded[j],sizeof triangle[1]);
        memcpy(triangle[2],decoded[j+1],sizeof triangle[2]);
        if(!terrain_clip_triangle_control(c,triangle,polygon,&n,NULL,&control,NULL,NULL,0))return 0;
        cycles+=control.cycles+3;
        if(n) {
            cycles+=38+((c[22].u[0]&65535)!=0)+35*n+6-(emitted&1);
            emitted++;
        }
    }
    *out=cycles;return 1;
}
static struct {const ps2_vu *owner;int active;u32 expected;} terrain_cycles;
static struct {u64 built,ended,bad,excluded,predicted,observed;} terrain_cycle_stats;
static void terrain_cycle_end(const ps2_vu *v,u32 cycles) {
    if(!terrain_cycles.active || terrain_cycles.owner!=v)return;
    terrain_cycles.active=0;terrain_cycle_stats.ended++;
    terrain_cycle_stats.predicted+=terrain_cycles.expected;terrain_cycle_stats.observed+=cycles;
    if(cycles!=terrain_cycles.expected) {
        if(terrain_cycle_stats.bad<4)ps2_log("ac4 terrain instruction difference: %u/%u",terrain_cycles.expected,cycles);
        terrain_cycle_stats.bad++;
    }
}
static int terrain_vector_project(terrain_vector_state *state,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,const ps2_vf *memory,u32 lookahead) {
    ps2_vf packet[65];u32 size;if(!terrain_packet(c,decoded,count,packet,&size))return 0;
    u32 a=4+3*(count-1),b=5+3*count+3*(count-1);
    state->vf[15].f[3]=ps2_fmul(state->vf[0].f[3],c[30].f[0]);
    state->vf[16]=packet[b+1];state->vf[17]=packet[b];state->vf[18]=packet[a];
    state->vf[19]=packet[a+1];state->vf[20]=packet[a+2];state->vf[21]=c[30];
    memcpy(state->vf+22,c,4*sizeof(ps2_vf));
    for(u32 j=0;j<4;j++)memcpy(state->vf+26+((j+1)&3),memory+((lookahead+j)&1023),16);
    terrain_vector_acc(state,c,decoded[count-1][2].f);
    return 1;
}
static int terrain_vector_build(const ps2_vu *v,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,terrain_vector_state *out) {
    if(!v || !v->mem || !c || !decoded || !out || (count!=4 && count!=9 && count!=10))return 0;
    u32 mode=c[23].u[1]&65535;
    if(mode!=1 && mode!=2 && mode!=4)return 0;
    terrain_vector_state result;memcpy(result.vf,v->vf,sizeof result.vf);result.acc=v->acc;
    if(mode==1) {
        u32 culled;if(!terrain_culled_fan(c,decoded,count,&culled) || culled!=count-2)return 0;
        const u32 last[3]={0,count-2,count-1};
        for(int j=0;j<3;j++) {
            memcpy(result.vf+11+5*j,decoded[last[j]],4*sizeof(ps2_vf));
            transform(c+4,decoded[last[j]][2].f,result.vf[15+5*j].f);
        }
        memcpy(result.vf+26,c+4,4*sizeof(ps2_vf));
        for(int k=0;k<4;k++) {
            float value=ps2_fmul(c[4].f[k],decoded[count-1][2].f[0]);
            value=ps2_fadd(value,ps2_fmul(c[5].f[k],decoded[count-1][2].f[1]));
            result.acc.f[k]=ps2_fadd(value,ps2_fmul(c[6].f[k],decoded[count-1][2].f[2]));
        }
        *out=result;return 1;
    }
    if(mode==2) {
        ps2_vf unmorphed[10][4],input[10][4],constants[32];memcpy(constants,c,sizeof constants);constants[23].u[1]=4;
        for(u32 j=0;j<count*4;j++)memcpy((ps2_vf*)input+j,v->mem+((v->top+32+j)&1023)*16,16);
        if(!terrain_decode(constants,input,count,unmorphed) || count<9)return 0;
        for(u32 j=0;j<5;j++) {
            ps2_vf *distance=result.vf+9+j;
            for(int k=0;k<4;k++)distance->f[k]=ps2_fsub(unmorphed[2*j][2].f[k],c[14].f[k]);
            for(int k=0;k<3;k++)distance->f[k]=ps2_fmul(distance->f[k],distance->f[k]);
            float sum=ps2_fadd(ps2_fadd(distance->f[0],distance->f[1]),distance->f[2]);
            distance->f[0]=fminf(fmaxf(ps2_fmul(ps2_fsub(sum,c[13].f[1]),c[13].f[0]),0),1);
        }
        result.vf[14]=decoded[0][2];result.vf[15]=decoded[2][2];
    }
    if(!terrain_vector_project(&result,c,decoded,count,(const ps2_vf*)v->mem,v->top+32+4*count))return 0;
    *out=result;return 1;
}
static int terrain_vector_plan(const ps2_vu *v,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,
                               terrain_vector_state *end,terrain_vector_state kicks[8],u32 *kick_count) {
    if(!v || !v->mem || v->mem_size!=16384 || !c || !decoded || !end || !kicks || !kick_count)return 0;
    if((c[23].u[1]&65535)!=1) {
        terrain_vector_state result;if(!terrain_vector_build(v,c,decoded,count,&result))return 0;
        *end=result;kicks[0]=result;*kick_count=1;return 1;
    }
    if((count!=4 && count!=9 && count!=10) || v->top<112 || v->top+275>1024)return 0;
    ps2_vf memory[1024],constants[32];memcpy(memory,v->mem,sizeof memory);memcpy(constants,c,sizeof constants);constants[23].u[1]=4;
    for(u32 j=0;j<count*4;j++)memory[(v->top+32+j)&1023]=((const ps2_vf*)decoded)[j];
    terrain_vector_state state,packets[8];memcpy(state.vf,v->vf,sizeof state.vf);state.acc=v->acc;u32 emitted=0;
    for(u32 j=1;j+1<count;j++) {
        ps2_vf input[3][4],polygon[10][4],packet[65];u32 n,size;terrain_clip_control control;
        memcpy(input[0],decoded[0],64);memcpy(input[1],decoded[j],64);memcpy(input[2],decoded[j+1],64);
        if(!terrain_clip_triangle_control(c,input,polygon,&n,memory,&control,&state,NULL,0))return 0;
        if(!n)continue;
        if(emitted==8 || !terrain_packet(constants,polygon,n,packet,&size))return 0;
        u32 bank=v->top+((emitted&1)?126:210);
        memcpy(memory+bank,packet,size*sizeof(ps2_vf));
        if(!terrain_vector_project(&state,constants,polygon,n,memory,(control.any?72:0)+4*n))return 0;
        packets[emitted++]=state;
    }
    *end=state;memcpy(kicks,packets,emitted*sizeof packets[0]);*kick_count=emitted;return 1;
}
static struct {const ps2_vu *owner;int active;u32 count,next;terrain_vector_state expected,kick[8];} terrain_vector;
static struct {u64 built,ended,kicks,bad,kick_bad,excluded;} terrain_vector_stats;
static int terrain_vector_matches(const ps2_vu *v,const terrain_vector_state *expected) {
    return !memcmp(v->vf,expected->vf,sizeof v->vf) && !memcmp(&v->acc,&expected->acc,sizeof v->acc);
}
static void terrain_vector_end(const ps2_vu *v) {
    if(!terrain_vector.active || terrain_vector.owner!=v)return;
    terrain_vector.active=0;terrain_vector_stats.ended++;
    if(terrain_vector.next!=terrain_vector.count || !terrain_vector_matches(v,&terrain_vector.expected)) {
        if(!terrain_vector_stats.bad)for(int j=0;j<32;j++)for(int k=0;k<4;k++)if(v->vf[j].u[k]!=terrain_vector.expected.vf[j].u[k])ps2_log("ac4 terrain vector difference: vf%d.%d %08x/%08x",j,k,terrain_vector.expected.vf[j].u[k],v->vf[j].u[k]);
        terrain_vector_stats.bad++;
    }
}
static void terrain_execution_reset(void);
static void terrain_execution_kick(const ps2_vu *v,u32 address);
static void terrain_execution_begin(const ps2_vu *v,u32 entry,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count);
static void terrain_execution_end(const ps2_vu *v,u32 cycles);
static void terrain_flags_begin(const ps2_vu *v,u32 entry,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count);
static void terrain_flags_end(const ps2_vu *v);
static void terrain_scalar_begin(const ps2_vu *v,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count);
static void terrain_scalar_end(const ps2_vu *v);
static void terrain_scalar_kick(const ps2_vu *v);
static void terrain_begin(const ps2_vu *v,u32 entry,u64 signature) {
    terrain_native_ready=0;terrain_execution_reset();
    terrain_sample.active=terrain_sample.clip_active=0;terrain_memory.active=terrain_integer.active=terrain_cycles.active=terrain_vector.active=0;
    /* The cockpit's resident image has different stale words at 4192..4235.
     * Its complete terrain body [0,4192), including the 4176 continuation,
     * is byte-identical to the verified images. Keep an exact image whitelist;
     * do not accept arbitrary programs merely sharing a short entry prefix. */
    if((!rn_ac4_model_plan_on && !rn_ac4_terrain_mesh_on)
       || (signature!=0xba581f12988de18bull && signature!=0x6632fc9c7a80a952ull
           && signature!=0x758bae290dd231b2ull))return;
    if(entry!=16 && entry!=4176) {static u32 skips;if(rn_ac4_model_plan_on && skips++<4)ps2_log("ac4 terrain unsupported entry: %u",entry);return;}
    terrain_stats.seen++;
    if(!v || !v->mem || v->mem_size!=16384)return;
    ps2_vf c[32],input[10][4];
    for(u32 j=0;j<32;j++)memcpy(c+j,v->mem+((v->top+j)&1023)*16,16);
    u32 count=c[23].u[0]&65535,mode=c[23].u[1]&65535;
    if(count<24)terrain_stats.counts[count]++;
    if(mode<8)terrain_stats.modes[mode]++;
    if(count>10) {terrain_stats.declined++;return;}
    for(u32 j=0;j<count*4;j++)memcpy((ps2_vf*)input+j,v->mem+((v->top+32+j)&1023)*16,16);
    if(!terrain_decode(c,input,count,terrain_sample.decoded)) {terrain_stats.declined++;return;}
    memcpy(terrain_sample.constants,c,sizeof c);
    terrain_sample.owner=v;terrain_sample.top=v->top;terrain_sample.count=count;terrain_sample.active=1;
    if(rn_ac4_model_plan_on) {
        if(terrain_instruction_plan(entry,c,terrain_sample.decoded,count,&terrain_cycles.expected)) {
            terrain_cycles.owner=v;terrain_cycles.active=1;terrain_cycle_stats.built++;
        } else terrain_cycle_stats.excluded++;
        if(terrain_vector_plan(v,c,terrain_sample.decoded,count,&terrain_vector.expected,terrain_vector.kick,&terrain_vector.count)) {
            terrain_vector.owner=v;terrain_vector.active=1;terrain_vector.next=0;terrain_vector_stats.built++;
        } else terrain_vector_stats.excluded++;
        terrain_flags_begin(v,entry,c,terrain_sample.decoded,count);
        terrain_scalar_begin(v,c,terrain_sample.decoded,count);
        if(terrain_integer_plan(v,c,terrain_sample.decoded,count,terrain_integer.end,terrain_integer.kick,&terrain_integer.expected_kicks)) {
            terrain_integer.owner=v;terrain_integer.active=1;terrain_integer.kicks=0;terrain_integer_stats.built++;
        } else terrain_integer_stats.declined++;
        if(terrain_memory_build(v,c,terrain_sample.decoded,count,terrain_memory.expected)) {
            terrain_memory.owner=v;terrain_memory.active=1;terrain_memory_stats.built++;
        } else terrain_memory_stats.declined++;
        terrain_execution_begin(v,entry,c,terrain_sample.decoded,count);
    }
    if(terrain_native_on && !rn_ac4_model_plan_on)terrain_execution_begin(v,entry,c,terrain_sample.decoded,count);
    if(mode==1) {
        terrain_build_clip_plan(c,terrain_sample.decoded,count,v->top);
        terrain_clip_stats.seen++;terrain_sample.clip_active=1;terrain_sample.clip_kicks=0;
        terrain_sample.clip_valid=terrain_culled_fan(c,terrain_sample.decoded,count,&terrain_sample.clip_culled);
    }
}
static void terrain_compare_preparation(const ps2_vu *v) {
    if(!terrain_sample.active || terrain_sample.owner!=v)return;
    terrain_sample.active=0;terrain_stats.compared++;terrain_stats.vertices+=terrain_sample.count;
    int bad=0;
    for(u32 j=0;j<terrain_sample.count*4;j++) {
        ps2_vf actual;memcpy(&actual,v->mem+((terrain_sample.top+32+j)&1023)*16,16);
        if(memcmp((const ps2_vf*)terrain_sample.decoded+j,&actual,16))bad=1;
    }
    terrain_stats.bad+=bad;
}
static u64 terrain_clipped_topologies,terrain_clipped_triangles,terrain_clipped_topology_declined;
static void terrain_submit_polygon(const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,const ps2_vf packet[65],const terrain_triangle *topology,u32 triangles,int clipped) {
    if(!rn_ac4_terrain_mesh_on)return;
    for(u32 t=0;t<triangles;t++) {
        ps2_vf records[3][4];rn_ac4_model_vertex expected[3];
        u32 pass=topology[t].pass,start=pass?5+3*count:4;
        for(int j=0;j<3;j++) {
            u32 index=topology[t].index[j],at=start+3*index;
            memcpy(records[j],decoded[index],sizeof records[j]);
            memcpy(expected[j].stq,packet[at].f,16);memcpy(expected[j].rgba,packet[at+1].s,16);memcpy(expected[j].xyzf,packet[at+2].s,16);
        }
        rn_ac4_mesh_terrain_append(c,records,expected,(int)pass,clipped);
    }
}
static void terrain_end(const ps2_vu *v) {
    terrain_memory_end(v);terrain_integer_end(v);terrain_scalar_end(v);terrain_vector_end(v);
    terrain_compare_preparation(v);
    if(!terrain_sample.clip_active || terrain_sample.owner!=v)return;
    terrain_sample.clip_active=0;terrain_clip_stats.ended++;
    if(terrain_clip_plan.valid && terrain_clip_plan.next!=terrain_clip_plan.count)terrain_polygon_stats.sequence++;
    terrain_clip_stats.observed_empty+=terrain_sample.clip_kicks==0;
    if(!terrain_sample.clip_valid) {terrain_clip_stats.invalid++;return;}
    if(terrain_sample.clip_culled==terrain_sample.count-2) {
        terrain_clip_stats.predicted_empty++;
        terrain_clip_stats.mismatch+=terrain_sample.clip_kicks!=0;
    } else terrain_clip_stats.partial++;
}
static void terrain_kick(const ps2_vu *v,u32 address) {
    terrain_execution_kick(v,address);
    if(terrain_vector.active && terrain_vector.owner==v) {
        u32 at=terrain_vector.next++;terrain_vector_stats.kicks++;
        terrain_vector_stats.kick_bad+=at>=terrain_vector.count || !terrain_vector_matches(v,terrain_vector.kick+at);
    }
    terrain_scalar_kick(v);
    if(terrain_integer.active && terrain_integer.owner==v) {
        u32 at=terrain_integer.kicks++;terrain_integer_stats.kicks++;
        if(at>=terrain_integer.expected_kicks || memcmp(v->vi,terrain_integer.kick[at],sizeof v->vi)) {
            if(at<terrain_integer.expected_kicks && !terrain_integer_stats.kick_bad)for(int j=0;j<16;j++)if(v->vi[j]!=terrain_integer.kick[at][j])ps2_log("ac4 terrain integer kick: vi%d %04x/%04x",j,terrain_integer.kick[at][j],v->vi[j]);
            terrain_integer_stats.kick_bad++;
        }
    }
    if(terrain_sample.clip_active && terrain_sample.owner==v) {
        terrain_sample.clip_kicks++;
        if(terrain_clip_plan.valid) {
            u32 at=terrain_clip_plan.next++;
            if(at>=terrain_clip_plan.count)terrain_polygon_stats.sequence++;
            else {
                terrain_polygon_stats.packets++;int bad=0;
                if((address&1023)!=terrain_clip_plan.address[at])terrain_polygon_stats.sequence++;
                for(u32 j=0;j<terrain_clip_plan.size[at];j++) {
                    ps2_vf actual;memcpy(&actual,v->mem+((address+j)&1023)*16,16);
                    if(memcmp(&actual,terrain_clip_plan.packet[at]+j,16)) {
                        bad=1;
                        if(!terrain_polygon_stats.bad)for(int k=0;k<4;k++)if(actual.u[k]!=terrain_clip_plan.packet[at][j].u[k])ps2_log("ac4 terrain clipped packet: qw%u.%d %08x/%08x",j,k,terrain_clip_plan.packet[at][j].u[k],actual.u[k]);
                    }
                }
                terrain_polygon_stats.bad+=bad;
                terrain_triangle topology[16];u32 triangles=0;
                if(terrain_topology(terrain_clip_plan.packet[at],terrain_clip_plan.vertices[at],topology,&triangles)) {
                    terrain_clipped_topologies++;terrain_clipped_triangles+=triangles;
                    if(!bad && (address&1023)==terrain_clip_plan.address[at])
                        terrain_submit_polygon(terrain_sample.constants,terrain_clip_plan.polygon[at],terrain_clip_plan.vertices[at],terrain_clip_plan.packet[at],topology,triangles,1);
                } else terrain_clipped_topology_declined++;
            }
        }
    }
    if(!terrain_sample.active || terrain_sample.owner!=v)return;
    terrain_compare_preparation(v);
    if((terrain_sample.constants[23].u[1]&65535)==1)return;
    int bad=0;
    ps2_vf packet[65];u32 size=0;
    if(!terrain_packet(terrain_sample.constants,terrain_sample.decoded,terrain_sample.count,packet,&size)) {terrain_stats.packet_declined++;return;}
    terrain_stats.packets++;terrain_stats.bank_bad+=(address&1023)!=((terrain_sample.top+210)&1023);
    bad=0;
    for(u32 j=0;j<size;j++) {
        ps2_vf actual;memcpy(&actual,v->mem+((address+j)&1023)*16,16);
        if(memcmp(packet+j,&actual,16)) {
            bad=1;if(terrain_stats.packet_bad<2)for(int k=0;k<4;k++)if(packet[j].u[k]!=actual.u[k])ps2_log("ac4 terrain packet: qw%u.%d %08x/%08x",j,k,packet[j].u[k],actual.u[k]);
        }
    }
    terrain_stats.packet_bad+=bad;
    terrain_triangle topology[16];u32 triangles;
    if(terrain_topology(packet,terrain_sample.count,topology,&triangles)) {
        terrain_stats.topologies++;terrain_stats.triangles+=triangles;
        if(rn_ac4_terrain_mesh_on && !bad && (address&1023)==((terrain_sample.top+210)&1023)) {
            terrain_submit_polygon(terrain_sample.constants,terrain_sample.decoded,terrain_sample.count,packet,topology,triangles,0);
        }
    } else {
        if(!terrain_stats.topology_declined)ps2_log("ac4 terrain topology tag: %08x %08x %08x %08x",packet[3].u[0],packet[3].u[1],packet[3].u[2],packet[3].u[3]);
        terrain_stats.topology_declined++;
    }
}

/* Keep separate multiply/add rounding, as in the original VU program. */
static void transform(const ps2_vf *m, const float *p, float *r) {
#if defined(__SSE2__)
    if(model_regular_transform(m,p)) {
        __m128 a=_mm_mul_ps(_mm_loadu_ps(m[0].f),_mm_set1_ps(p[0]));
        a=_mm_add_ps(a,_mm_mul_ps(_mm_loadu_ps(m[1].f),_mm_set1_ps(p[1])));
        a=_mm_add_ps(a,_mm_mul_ps(_mm_loadu_ps(m[2].f),_mm_set1_ps(p[2])));
        a=_mm_add_ps(a,_mm_mul_ps(_mm_loadu_ps(m[3].f),_mm_set1_ps(p[3])));
        _mm_storeu_ps(r,a);return;
    }
    __m128 a=model_mul4(_mm_loadu_ps(m[0].f),_mm_set1_ps(p[0]));
    a=model_add4(a,model_mul4(_mm_loadu_ps(m[1].f),_mm_set1_ps(p[1])));
    a=model_add4(a,model_mul4(_mm_loadu_ps(m[2].f),_mm_set1_ps(p[2])));
    a=model_add4(a,model_mul4(_mm_loadu_ps(m[3].f),_mm_set1_ps(p[3])));
    _mm_storeu_ps(r,a);
#else
    for (int k=0;k<4;k++) {
        float a=ps2_fmul(m[0].f[k],p[0]);
        a=ps2_fadd(a,ps2_fmul(m[1].f[k],p[1]));
        a=ps2_fadd(a,ps2_fmul(m[2].f[k],p[2]));
        r[k]=ps2_fadd(a,ps2_fmul(m[3].f[k],p[3]));
    }
#endif
}
static float clamp(float x,float lo,float hi) { return fminf(fmaxf(x,lo),hi); }

int rn_ac4_model_eval(const ps2_vf c[32],const ps2_vf r[4],rn_ac4_model_vertex *out) {
    float p[4],normal[4],n[4],light[4],color[4];
    rn_ac4_model_vertex v;
    if (!c || !r || !out) return 0;
    for(int k=0;k<4;k++) if(!isfinite(r[2].f[k])) return 0;
    transform(c,r[2].f,p);
    if(!isfinite(p[3]) || p[3]<=0) return 0;
    float q=1.0f/p[3];
    for(int k=0;k<3;k++) {
        float x=ps2_fmul(ps2_fmul(p[k],q),16.0f);
        if(!isfinite(x) || x <= -2147483648.0f || x >= 2147483648.0f) return 0;
        v.xyzf[k]=ps2_cvt_w_s(x);
    }
    float fog=ps2_fmul(ps2_fadd(-p[3],c[30].f[0]),c[30].f[1]);
    if(!isfinite(fog)) return 0;
    v.xyzf[3]=ps2_cvt_w_s(clamp(fog,0,255)*16.0f);
    for(int k=0;k<4;k++) {
        v.stq[k]=ps2_fmul((float)r[0].s[k]*(1.0f/4096.0f),q);
        if(!isfinite(v.stq[k])) return 0;
        v.rgba[k]=r[1].s[k];
    }
    if(c[23].u[1]&2u) {
        for(int k=0;k<3;k++) normal[k]=(float)r[3].s[k]*(1.0f/4096.0f);
        normal[3]=0;
        transform(c+8,normal,n);
        for(int k=0;k<4;k++) {
            float a=fmaxf(ps2_fmul(c[12].f[k],n[0]),0);
            a=ps2_fadd(a,fmaxf(ps2_fmul(c[13].f[k],n[1]),0));
            a=ps2_fadd(a,fmaxf(ps2_fmul(c[14].f[k],n[2]),0));
            light[k]=ps2_fadd(a,fmaxf(ps2_fmul(c[15].f[k],n[3]),0));
        }
        transform(c+16,light,color);
        for(int k=0;k<3;k++) {
            float value=ps2_fadd(fmaxf(color[k],0),c[20].f[k]);
            value=ps2_fmul((float)r[1].s[k],value);
            if(!isfinite(value)) return 0;
            v.rgba[k]=ps2_cvt_w_s(clamp(value,0,255));
        }
    }
    *out=v;
    return 1;
}

/* The lit program's second stream uses transformed normals as environment
 * coordinates. A downward-facing normal suppresses RGB, preserving alpha. */
int rn_ac4_lit_secondary(const ps2_vf c[32],const ps2_vf r[4],rn_ac4_model_vertex *out) {
    if(!c || !r || !out)return 0;
    ps2_vf constants[32];memcpy(constants,c,sizeof constants);constants[23].u[1]&=~2u;
    rn_ac4_model_vertex vertex;if(!rn_ac4_model_eval(constants,r,&vertex))return 0;
    float normal[4],n[4],p[4];
    for(int j=8;j<12;j++)for(int k=0;k<4;k++)if(!isfinite(c[j].f[k]))return 0;
    for(int k=0;k<3;k++)normal[k]=(float)r[3].s[k]/4096.0f;
    normal[3]=0;
    transform(c+8,normal,n);transform(c,r[2].f,p);
    for(int k=0;k<4;k++)if(!isfinite(n[k]))return 0;
    float q=1.0f/p[3];
    vertex.stq[0]=ps2_fmul(ps2_fmul(ps2_fadd(ps2_fsub(0,n[0]),1),.5f),q);
    vertex.stq[1]=ps2_fmul(ps2_fmul(ps2_fadd(ps2_fadd(0,n[2]),1),.5f),q);
    vertex.stq[2]=q;vertex.stq[3]=q;
    for(int k=0;k<3;k++)vertex.rgba[k]=n[1]<0?0:128;
    vertex.rgba[3]=128;
    for(int k=0;k<4;k++)if(!isfinite(vertex.stq[k]))return 0;
    *out=vertex;return 1;
}

/* Reproduce the lit strip's ADC bit before FTOI4. Adding 2048 in float
 * matters: simply ORing the packed flag can miss rounding of fractional fog. */
static int lit_fog_adc(const ps2_vf c[32],const ps2_vf records[][4],u32 count,u32 parity,int split,s32 out[23]) {
    if(count<3 || count>23)return 0;
    float projected[23][4];s32 result[23];
    for(u32 j=0;j<count;j++) {
        transform(c,records[j][2].f,projected[j]);
        if(!isfinite(projected[j][3]) || projected[j][3]<=0)return 0;
        float q=1.0f/projected[j][3];
        for(int k=0;k<2;k++) {projected[j][k]=ps2_fmul(projected[j][k],q);if(!isfinite(projected[j][k]))return 0;}
    }
    int winding=(c[23].u[2]&1u)^((c[29].u[0]&65535u)!=0)^(parity&1u);
    int alternate=1;
    if(split==2)alternate=0; /* clipped polygons keep their winding order */
    else if(split) {
        float cp[4];transform(c+4,records[0][2].f,cp);
        alternate=cp[3]>8192; /* vi10 is the near-W MAC result. */
    }
    for(u32 j=0;j<count;j++) {
        int suppress=0;
        if((c[23].u[1]&1u) && j>=2) {
            float ax=ps2_fsub(projected[j-2][0],projected[j-1][0]);
            float ay=ps2_fsub(projected[j-2][1],projected[j-1][1]);
            float bx=ps2_fsub(projected[j][0],projected[j-1][0]);
            float by=ps2_fsub(projected[j][1],projected[j-1][1]);
            float cross=winding?ps2_fsub(ps2_fmul(bx,ay),ps2_fmul(ax,by)):ps2_fsub(ps2_fmul(ax,by),ps2_fmul(bx,ay));
            suppress=cross<0;
            if(alternate)winding^=1;
        }
        float fog=clamp(ps2_fmul(ps2_fadd(ps2_fmul(projected[j][3],-1),c[30].f[0]),c[30].f[1]),0,255);
        if(suppress)fog=ps2_fadd(fog,2048);
        if(!isfinite(fog))return 0;
        result[j]=ps2_cvt_w_s(ps2_fmul(fog,16));
    }
    memcpy(out,result,count*sizeof *out);return 1;
}

/* Both material streams are written even when the first GIF tag terminates
 * submission. Publish only after every vertex has been computed successfully. */
int rn_ac4_lit_packet(const ps2_vf c[32],const ps2_vf records[][4],u32 count,u32 parity,int split,
                      ps2_vf output[144],u32 *qwords) {
    if(!c || !records || !output || !qwords || count<3 || count>23)return 0;
    s32 fog[23];if(!lit_fog_adc(c,records,count,parity,split,fog))return 0;
    ps2_vf packet[144];u32 second=3+3*count;
    packet[0]=c[26];packet[1]=c[28];packet[2]=c[22];
    packet[second]=c[26];packet[second+1]=c[27];packet[second+2]=c[25];
    packet[2].s[0]=(c[23].u[1]&4u)?(s32)count:(s16)(0x8000u|count);
    packet[second+2].s[0]=(s16)(0x8000u|count);
    for(u32 j=0;j<count;j++) {
        rn_ac4_model_vertex primary,secondary;
        if(!rn_ac4_model_eval(c,records[j],&primary) || !rn_ac4_lit_secondary(c,records[j],&secondary))return 0;
        primary.rgba[3]=ps2_cvt_w_s((float)records[j][1].s[3]);
        if(!(c[23].u[1]&2u))for(int k=0;k<3;k++)primary.rgba[k]=ps2_cvt_w_s((float)records[j][1].s[k]);
        primary.xyzf[3]=secondary.xyzf[3]=fog[j];
        memcpy(packet[3+3*j].f,primary.stq,16);memcpy(packet[4+3*j].s,primary.rgba,16);memcpy(packet[5+3*j].s,primary.xyzf,16);
        memcpy(packet[second+3+3*j].f,secondary.stq,16);memcpy(packet[second+4+3*j].s,secondary.rgba,16);memcpy(packet[second+5+3*j].s,secondary.xyzf,16);
    }
    memcpy(output,packet,2*second*sizeof *output);*qwords=2*second;return 1;
}

/* Native arithmetic scheduling for the supported unlit paths.
 * Advance only the four-slot flag pipeline; no instruction decoding/execution. */
static void flags_project(unlit_flags *f,u32 start,const ps2_vf c[32],const ps2_vf r[4]) {
    float projected[4],scaled[4],uv[4];
    flags_transform(f,start+1,c,r[2].f,projected);
    float fog=ps2_fadd(c[30].f[0],-projected[3]);
    flags_scalar(f,start+8,1,fog);
    flags_scalar(f,start+12,1,ps2_fmul(fog,c[30].f[1]));
    float q=1.0f/projected[3];
    for(int k=0;k<4;k++) {
        scaled[k]=ps2_fmul(projected[k],q);
        uv[k]=ps2_fmul((float)r[0].s[k]*(1.0f/4096.0f),q);
    }
    flags_result(f,start+15,14,scaled);flags_result(f,start+17,15,uv);
    flags_scalar(f,start+25,1,c[30].f[0]);flags_scalar(f,start+29,1,0);
    flags_advance(f,start+33);
}
/* Native terrain arithmetic scheduling. Conversions and min/max do not
 * produce arithmetic flags in this runtime; masks and pair positions matter. */
static u32 terrain_projection_flags(unlit_flags *f,u32 start,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count) {
    float value[4];
    for(u32 j=0;j<4;j++)flags_scalar(f,start+17+j,15,0);
    for(u32 j=0;j<4;j++)flags_scalar(f,start+21+j,8u>>j,1);
    start+=38+((c[22].u[0]&65535)!=0);
    flags_scalar(f,start,1,c[30].f[0]);
    for(u32 j=0;j<count;j++,start+=35) {
        float projected[4];flags_transform(f,start+1,c,decoded[j][2].f,projected);
        float q=1.0f/projected[3],fog=ps2_fsub(c[30].f[0],projected[3]);
        flags_scalar(f,start+8,1,fog);
        float alpha=ps2_fmul(decoded[j][1].f[3],0x1.010100p-8f);
        flags_scalar(f,start+9,1,alpha);fog=ps2_fmul(fog,c[30].f[1]);
        flags_scalar(f,start+12,1,fog);flags_scalar(f,start+13,14,128);
        for(int k=0;k<4;k++)value[k]=ps2_fmul(projected[k],q);
        flags_result(f,start+15,14,value);
        for(int k=0;k<4;k++)value[k]=ps2_fmul(decoded[j][0].f[k],q);
        flags_result(f,start+17,15,value);
        for(int k=0;k<4;k++)value[k]=ps2_fmul(decoded[j][3].f[k],q);
        flags_result(f,start+18,15,value);flags_scalar(f,start+21,1,c[30].f[0]);
        flags_scalar(f,start+24,1,ps2_fmul(alpha,fminf(fmaxf(fog,0),255)));
    }
    return start;
}
static int terrain_flags_plan(const ps2_vu *v,u32 entry,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,unlit_flags *out) {
    u32 total;if(!v || !v->mem || v->mem_size!=16384 || !out || !terrain_instruction_plan(entry,c,decoded,count,&total))return 0;
    ps2_vf input[10][4],raw[10][4],constants[32];memcpy(constants,c,sizeof constants);constants[23].u[1]=4;
    for(u32 j=0;j<count*4;j++)memcpy((ps2_vf*)input+j,v->mem+((v->top+32+j)&1023)*16,16);
    if(!terrain_decode(constants,input,count,raw))return 0;
    unlit_flags f={*v,0};u32 start=entry==4176?2:0;float value[4],uv[4];
    for(int k=0;k<4;k++)uv[k]=ps2_fmul((float)c[20].s[k],1.0f/64);
    flags_result(&f,start+15,12,uv);
    start+=19;
    for(u32 j=0;j<count;j++,start+=18) {
        for(int k=0;k<4;k++)value[k]=ps2_fmul((float)input[j][0].s[k],1.0f/16);
        flags_result(&f,start+1,12,value);
        for(int k=0;k<4;k++)value[k]=ps2_fmul((float)input[j][2].s[k],1024);
        flags_result(&f,start+3,10,value);
        for(int k=0;k<4;k++)value[k]=ps2_fmul((float)input[j][3].s[k],1.0f/128);
        flags_result(&f,start+4,12,value);
        flags_scalar(&f,start+7,4,ps2_fmul((float)input[j][2].s[1],-80));
        for(int k=0;k<4;k++)value[k]=ps2_fadd(value[k],uv[k]);
        flags_result(&f,start+8,12,value);
        flags_result(&f,start+11,14,raw[j][2].f);flags_scalar(&f,start+12,2,1);
    }
    start+=6;
    if((c[23].u[1]&65535)==1) {
        start+=4;u32 emitted=0;
        for(u32 j=1;j+1<count;j++) {
            ps2_vf input_triangle[3][4],polygon[10][4];u32 n;terrain_clip_control control;
            memcpy(input_triangle[0],decoded[0],64);memcpy(input_triangle[1],decoded[j],64);memcpy(input_triangle[2],decoded[j+1],64);
            if(!terrain_clip_triangle_control(c,input_triangle,polygon,&n,NULL,&control,NULL,&f,start))return 0;
            start+=control.cycles;
            if(n) {start=terrain_projection_flags(&f,start,c,polygon,n);start+=6-(emitted&1);emitted++;}
            start+=3;
        }
        if(start+2!=total)return 0;
        goto finish_terrain_flags;
    }
    if((c[23].u[1]&65535)==2) {
        static const u8 neighbours[5][2]={{1,5},{1,3},{3,5},{5,7},{7,1}};
        float average[5][4],distance[5][4],delta[5][4];u32 base=start+2;
        for(int j=0;j<5;j++) {
            for(int k=0;k<4;k++)average[j][k]=ps2_fadd(raw[neighbours[j][0]][2].f[k],raw[neighbours[j][1]][2].f[k]);
            flags_result(&f,base+5+j,15,average[j]);
        }
        for(int j=0;j<5;j++) {for(int k=0;k<4;k++)average[j][k]=ps2_fmul(average[j][k],.5f);flags_result(&f,base+10+j,15,average[j]);}
        for(int j=0;j<5;j++) {for(int k=0;k<4;k++)distance[j][k]=ps2_fsub(raw[2*j][2].f[k],c[14].f[k]);flags_result(&f,base+15+j,15,distance[j]);}
        for(int j=0;j<5;j++) {for(int k=0;k<3;k++)distance[j][k]=ps2_fmul(distance[j][k],distance[j][k]);flags_result(&f,base+20+j,14,distance[j]);}
        for(int j=0;j<5;j++) {distance[j][0]=ps2_fadd(distance[j][0],distance[j][1]);flags_result(&f,base+25+j,8,distance[j]);}
        for(int j=0;j<5;j++) {distance[j][0]=ps2_fadd(distance[j][0],distance[j][2]);flags_result(&f,base+30+j,8,distance[j]);}
        for(int j=0;j<5;j++) {distance[j][0]=ps2_fsub(distance[j][0],c[13].f[1]);flags_result(&f,base+35+j,8,distance[j]);}
        for(int j=0;j<5;j++) {distance[j][0]=ps2_fmul(distance[j][0],c[13].f[0]);flags_result(&f,base+40+j,8,distance[j]);distance[j][0]=fminf(fmaxf(distance[j][0],0),1);}
        for(int j=0;j<5;j++) {for(int k=0;k<4;k++)delta[j][k]=ps2_fsub(average[j][k],raw[2*j][2].f[k]);flags_result(&f,base+55+j,15,delta[j]);}
        for(int j=0;j<5;j++) {for(int k=0;k<4;k++)delta[j][k]=ps2_fmul(delta[j][k],distance[j][0]);flags_result(&f,base+60+j,15,delta[j]);}
        for(int j=0;j<5;j++)flags_result(&f,base+65+j,15,decoded[2*j][2].f);
        start+=77;
    }
    start=terrain_projection_flags(&f,start,c,decoded,count);
    if(start+11!=total)return 0;
finish_terrain_flags:
    /* Terrain counts already include E and its delay pair. */
    flags_advance(&f,total);f.state.fslot=(total-1)&3;*out=f;return 1;
}
static struct {const ps2_vu *owner;int active;unlit_flags expected;} terrain_flags;
static struct {u64 built,ended,bad,excluded;} terrain_flag_stats;
static void terrain_flags_begin(const ps2_vu *v,u32 entry,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count) {
    terrain_flags.active=0;
    if(terrain_flags_plan(v,entry,c,decoded,count,&terrain_flags.expected)) {
        terrain_flags.owner=v;terrain_flags.active=1;terrain_flag_stats.built++;
    } else terrain_flag_stats.excluded++;
}
static void terrain_flags_end(const ps2_vu *v) {
    if(!terrain_flags.active || terrain_flags.owner!=v)return;
    terrain_flags.active=0;terrain_flag_stats.ended++;const ps2_vu *p=&terrain_flags.expected.state;
    if(p->mac!=v->mac || p->status!=v->status || p->fslot!=v->fslot || memcmp(p->fpipe,v->fpipe,sizeof v->fpipe)) {
        if(terrain_flag_stats.bad++<2) {
            ps2_log("ac4 terrain flags difference: mac %x/%x status %x/%x slot %u/%u",p->mac,v->mac,p->status,v->status,p->fslot,v->fslot);
            for(int j=0;j<4;j++)ps2_log("ac4 terrain flags slot%d: %x/%x %x/%x %d/%d",j,p->fpipe[j].mac,v->fpipe[j].mac,p->fpipe[j].status,v->fpipe[j].status,p->fpipe[j].valid,v->fpipe[j].valid);
        }
    }
}
/* AC4 clips decoded attributes, then requantizes UV/colour/normals before
 * projection. Intersections recompute clip coordinates from model positions. */
typedef struct {ps2_vf attr[4];float clip[4];} unlit_clip_vertex;
typedef struct {ps2_vf vf[32],acc;u32 mask,cycles,crossings;float q,i;int has_q;} unlit_clip_trace;
static void clip_accumulator(const ps2_vf m[4],const float pos[4],ps2_vf *out) {
    for(int k=0;k<4;k++) {
        float v=ps2_fmul(m[0].f[k],pos[0]);v=ps2_fadd(v,ps2_fmul(m[1].f[k],pos[1]));
        out->f[k]=ps2_fadd(v,ps2_fmul(m[2].f[k],pos[2]));
    }
}

static int clip_intersection(const ps2_vf c[32],const unlit_clip_vertex *inside,
                             const unlit_clip_vertex *outside,int axis,float sign,
                             unlit_clip_vertex *out,unlit_clip_trace *trace,int inside_previous,float bias) {
    float a=ps2_fadd(ps2_fmul(ps2_fmul(sign,inside->clip[3]),-1),inside->clip[axis]);
    float b=ps2_fadd(ps2_fmul(ps2_fmul(sign,outside->clip[3]),-1),outside->clip[axis]);
    float denominator=ps2_fsub(b,a);
    if(!isfinite(denominator) || denominator==0)return 0;
    float inverse=1.0f/denominator;if(!isfinite(inverse))return 0;
    float t=fabsf(ps2_fmul(a,inverse));if(bias)t=ps2_fadd(t,bias);if(!isfinite(t))return 0;
    for(int j=0;j<4;j++)for(int k=0;k<4;k++) {
        float d=ps2_fsub(outside->attr[j].f[k],inside->attr[j].f[k]);
        out->attr[j].f[k]=ps2_fadd(ps2_fmul(d,t),inside->attr[j].f[k]);
        if(!isfinite(out->attr[j].f[k]))return 0;
    }
    transform(c+4,out->attr[2].f,out->clip);
    for(int k=0;k<4;k++)if(!isfinite(out->clip[k]))return 0;
    if(trace) {
        trace->vf[1].f[0]=inside_previous?a:t;trace->vf[2].f[0]=inside_previous?t:a;
        memcpy(trace->vf+16,out->attr,4*sizeof(ps2_vf));memcpy(trace->vf[20].f,out->clip,16);
        memcpy(trace->vf+22,c+4,4*sizeof(ps2_vf));
        clip_accumulator(c+4,out->attr[2].f,&trace->acc);
        trace->q=inverse;trace->has_q=1;
    }
    return 1;
}
static void clip_scratch_store(ps2_vf scratch[80],u8 mask[80],u32 base,
                               const unlit_clip_vertex *v,u32 n) {
    for(u32 i=0;i<n;i++) {
        memcpy(scratch+base+i*5,v[i].attr,4*sizeof(ps2_vf));
        memcpy(scratch[base+i*5+4].f,v[i].clip,16);
        memset(mask+base+i*5,1,5);
    }
}
typedef struct {u32 count,last_plane;} unlit_clip_control;
static int unlit_clip_build(const ps2_vf c[32],const ps2_vf input[3][4],
                            ps2_vf output[70],u32 *qwords,ps2_vf scratch_out[80],u8 mask_out[80],unlit_clip_control *control,
                            const ps2_vf scratch_in[85],unlit_clip_trace *trace_out,unlit_flags *flags,float bias,ps2_vf residual_out[5],u32 *polygon_cycles) {
    if(!c || !input || !output || !qwords || (bias && (trace_out || flags)))return 0;
    unlit_clip_vertex a[8],b[8];u32 count=3;ps2_vf scratch[85]={0};u8 mask[80]={0};unlit_clip_trace trace={0};
    if(scratch_in)memcpy(scratch,scratch_in,sizeof scratch);
    u32 origin=flags?flags->cycles:0;
    flags_scalar(flags,origin+1,15,0);flags_scalar(flags,origin+30,8,1);
    u32 bias_cycles=bias?1:0;
    trace.cycles=35+(flags&&flags->cockpit);trace.mask=0x3bffffe6u;trace.vf[1]=c[22];trace.vf[2]=c[25];
    for(int i=0;i<3;i++) {
        a[i].attr[2]=input[i][2];
        for(int k=0;k<4;k++) {
            a[i].attr[0].f[k]=(float)input[i][0].s[k]*(1.0f/4096.0f);
            a[i].attr[1].f[k]=(float)input[i][1].s[k];
            a[i].attr[3].f[k]=(float)input[i][3].s[k]*(1.0f/4096.0f);
            if(!isfinite(a[i].attr[2].f[k]))return 0;
        }
        transform(c+4,a[i].attr[2].f,a[i].clip);
        for(int k=0;k<4;k++)if(!isfinite(a[i].clip[k]))return 0;
    }
    memcpy(trace.vf+22,a[0].attr,4*sizeof(ps2_vf));
    memcpy(trace.vf+18,a[1].attr,4*sizeof(ps2_vf));
    memcpy(trace.vf+14,a[2].attr,4*sizeof(ps2_vf));
    for(int i=0;i<3;i++)memcpy(trace.vf[27+i].f,a[i].clip,16);
    trace.vf[21].f[0]=1;
    clip_accumulator(c+4,a[2].attr[2].f,&trace.acc);
    clip_scratch_store(scratch,mask,0,a,3);
    for(int plane=0;plane<6;plane++) {
        trace.cycles+=14;
        int axis=plane/2;float sign=(plane&1)?-1:1;u32 next=0;
        memcpy(trace.vf+6,a[count-1].attr,4*sizeof(ps2_vf));memcpy(trace.vf[10].f,a[count-1].clip,16);
        memcpy(trace.vf+11,a[0].attr,4*sizeof(ps2_vf));memcpy(trace.vf[15].f,a[0].clip,16);
        for(u32 i=0;i<count;i++) {
            const unlit_clip_vertex *prev=&a[(i+count-1)%count],*cur=&a[i];
            float d0=ps2_fsub(ps2_fmul(sign,prev->clip[axis]),prev->clip[3]);
            float d1=ps2_fsub(ps2_fmul(sign,cur->clip[axis]),cur->clip[3]);
            trace.vf[21].f[1]=ps2_fadd(0,prev->clip[axis]);trace.vf[21].f[2]=ps2_fadd(0,cur->clip[axis]);
            trace.vf[1].f[0]=d0;trace.vf[2].f[0]=d1;
            int in0=d0<0,in1=d1<0;
            u32 edge=origin+trace.cycles,axis_cycles=axis==0?14u:axis==1?18u:17u;
            flags_scalar(flags,edge+(axis==0?6u:10u),4,trace.vf[21].f[1]);
            flags_scalar(flags,edge+(axis==0?10u:14u),2,trace.vf[21].f[2]);
            flags_scalar(flags,edge+axis_cycles+1,8,ps2_fmul(sign,prev->clip[axis]));
            flags_scalar(flags,edge+axis_cycles+2,8,ps2_fmul(sign,cur->clip[axis]));
            flags_scalar(flags,edge+axis_cycles+5,8,d0);flags_scalar(flags,edge+axis_cycles+6,8,d1);
            trace.cycles+=(axis==0?14u:axis==1?18u:17u)+17u+!in0+!in1;
            /* Branch targets include a padding instruction on crossing/outside paths. */
            trace.cycles+=in0?(in1?7u:52u):(in1?55u:8u);
            trace.cycles+=7;
            if(in0) {if(next==8)return 0;b[next++]=*prev;}
            if(in0!=in1) {
                trace.crossings++;
                if(flags&&flags->cockpit)trace.cycles++;
                if(bias)bias_cycles+=5; /* bias instruction, padding and longer divide wait */
                if(next==8 || !clip_intersection(c,in0?prev:cur,in0?cur:prev,axis,sign,&b[next],&trace,in0,bias))return 0;
                if(flags) {
                    const unlit_clip_vertex *inside=in0?prev:cur,*outside=in0?cur:prev;
                    u32 start=edge+axis_cycles+17u+!in0+!in1+(in0?3u:6u);
                    float w0=ps2_fmul(sign,prev->clip[3]),w1=ps2_fmul(sign,cur->clip[3]);
                    flags_scalar(flags,start+1,8,w0);flags_scalar(flags,start+2,8,w1);
                    flags_scalar(flags,start+5,8,ps2_fmul(w0,-1));flags_scalar(flags,start+6,8,ps2_fmul(w1,-1));
                    float a0=ps2_fadd(ps2_fmul(w0,-1),prev->clip[axis]);
                    float a1=ps2_fadd(ps2_fmul(w1,-1),cur->clip[axis]);
                    flags_scalar(flags,start+9,8,a0);flags_scalar(flags,start+10,8,a1);
                    flags_scalar(flags,start+14,8,in0?ps2_fsub(a1,a0):ps2_fsub(a0,a1));
                    float delta[4][4],scaled[4][4];
                    for(int j=0;j<4;j++) {
                        for(int k=0;k<4;k++)delta[j][k]=ps2_fsub(outside->attr[j].f[k],inside->attr[j].f[k]);
                        flags_result(flags,start+19+j,15,delta[j]);
                    }
                    float factor=ps2_fmul(in0?a0:a1,trace.q);
                    if(flags->cockpit)start++; /* Retained divide-wait pair. */
                    flags_scalar(flags,start+24,8,factor);
                    for(int j=0;j<4;j++) {
                        for(int k=0;k<4;k++)scaled[j][k]=ps2_fmul(delta[j][k],fabsf(factor));
                        flags_result(flags,start+32+j,15,scaled[j]);
                    }
                    for(int j=0;j<4;j++)flags_result(flags,start+36+j,15,b[next].attr[j].f);
                    float projected[4];flags_transform(flags,start+40,c+4,b[next].attr[2].f,projected);
                }
                next++;
            }
            clip_scratch_store(scratch,mask,(plane&1)?0:40,b,next);
            for(int j=0;j<4;j++)for(int k=0;k<4;k++)trace.vf[6+j].f[k]=ps2_fadd(cur->attr[j].f[k],0);
            for(int k=0;k<4;k++)trace.vf[10].f[k]=ps2_fadd(cur->clip[k],0);
            for(int j=0;j<5;j++)flags_result(flags,origin+trace.cycles-6+j,15,trace.vf[6+j].f);
            u32 at=((plane&1)?40:0)+(i+1)*5;
            memcpy(trace.vf+11,scratch+at,5*sizeof(ps2_vf));
        }
        trace.vf[21].f[0]=ps2_fmul(trace.vf[21].f[0],-1);trace.i=-1;
        clip_scratch_store(scratch,mask,(plane&1)?0:40,b,next);
        /* Odd-plane swap lands on a padding instruction before the swap. */
        flags_scalar(flags,origin+trace.cycles+((plane&1)?7u:8u)+1,8,trace.vf[21].f[0]);
        trace.cycles+=((plane&1)?7u:8u)+5u;
        count=next;
        if(count<3) {
            ps2_vf empty={0};empty.u[0]=0xffff8000u;output[0]=empty;*qwords=1;
            if(scratch_out)memcpy(scratch_out,scratch,80*sizeof(ps2_vf));
            if(mask_out)memcpy(mask_out,mask,sizeof mask);
            if(control)*control=(unlit_clip_control){count,(u32)plane};
            flags_scalar(flags,origin+trace.cycles+1,15,0);
            trace.cycles+=15;
            flags_advance(flags,origin+trace.cycles);
            memset(&trace.vf[29],0,16);trace.vf[29].u[0]=0xffff8000u;
            if(trace_out)*trace_out=trace;
            if(residual_out)memcpy(residual_out,trace.vf+5,5*sizeof(ps2_vf));
            if(polygon_cycles)*polygon_cycles=trace.cycles+bias_cycles;
            return 1;
        }
        trace.cycles+=2;
        memcpy(a,b,count*sizeof *a);
    }
    for(u32 j=0;j<count;j++) {
        float pos[4];for(int k=0;k<4;k++)pos[k]=ps2_fadd(a[j].attr[2].f[k],0);
        flags_result(flags,origin+trace.cycles+8+11*j,15,pos);
    }
    if(polygon_cycles)*polygon_cycles=trace.cycles+bias_cycles+5+11*count+7;
    if(flags&&flags->cockpit) {
        flags_advance(flags,origin+trace.cycles+5+11*count+7);
        flags=NULL; /* Cockpit continues into its lit loop, not unlit projection. */
    }
    u32 projection=origin+trace.cycles+5+11*count+4;
    flags_scalar(flags,projection+25,1,0);flags_scalar(flags,projection+29,1,c[30].f[0]);
    projection+=29;
    ps2_vf packet[70]={0},constants[32];memcpy(constants,c,sizeof constants);constants[23].u[1]&=~2u;
    packet[0]=c[21];packet[0].s[0]=(s16)(0x8000u|count);
    for(u32 i=0;i<count;i++) {
        ps2_vf r[4];r[2]=a[i].attr[2];
        for(int k=0;k<4;k++) {
            r[0].s[k]=ps2_cvt_w_s(a[i].attr[0].f[k]*4096.0f);
            r[1].s[k]=ps2_cvt_w_s(a[i].attr[1].f[k]);
            r[3].s[k]=ps2_cvt_w_s(a[i].attr[3].f[k]*4096.0f);
        }
        memcpy(scratch+40+i*4,r,sizeof r);memset(mask+40+i*4,1,4);
        flags_project(flags,projection+33*i,c,r);
        rn_ac4_model_vertex v;if(!rn_ac4_model_eval(constants,r,&v))return 0;
        memcpy(packet[1+3*i].f,v.stq,16);memcpy(packet[2+3*i].s,v.rgba,16);memcpy(packet[3+3*i].s,v.xyzf,16);
    }
    trace.cycles+=47+44*count;
    flags_advance(flags,origin+trace.cycles);
    trace.mask|=1u<<26;trace.vf[1]=packet[0];trace.vf[2]=c[31];trace.vf[10]=c[30];
    trace.vf[16].f[3]=c[30].f[0];
    trace.vf[17]=packet[2+3*(count-1)];trace.vf[19]=packet[1+3*(count-1)];trace.vf[21]=packet[3+3*(count-1)];
    memcpy(trace.vf+22,c,4*sizeof(ps2_vf));memcpy(trace.vf+26,scratch+40+4*count,4*sizeof(ps2_vf));
    trace.vf[29].s[3]=0;
    for(int k=0;k<4;k++) {
        trace.vf[26].f[k]=(float)trace.vf[26].s[k]*(1.0f/4096.0f);
        trace.vf[27].f[k]=(float)trace.vf[27].s[k];
        trace.vf[29].f[k]=(float)trace.vf[29].s[k]*(1.0f/4096.0f);
    }
    float final_pos[4];transform(c,a[count-1].attr[2].f,final_pos);
    clip_accumulator(c,a[count-1].attr[2].f,&trace.acc);
    trace.q=1.0f/final_pos[3];trace.has_q=1;trace.i=255;
    if(trace_out)*trace_out=trace;
    if(residual_out)memcpy(residual_out,trace.vf+5,5*sizeof(ps2_vf));
    memcpy(output,packet,(1+3*count)*sizeof *output);*qwords=1+3*count;
    if(scratch_out)memcpy(scratch_out,scratch,80*sizeof(ps2_vf));
    if(mask_out)memcpy(mask_out,mask,sizeof mask);
    if(control)*control=(unlit_clip_control){count,5};
    return 1;
}
static int lit_clip_build_state(const ps2_vf c[32],const ps2_vf records[3][4],u32 parity,
                          ps2_vf output[144],u32 *qwords,ps2_vf scratch_out[80],u8 mask_out[80],ps2_vf residual_out[5],u32 *polygon_cycles) {
    ps2_vf scratch[80],unused[70],packet[144],residual[5];u8 mask[80];u32 n=0,cycles=0;unlit_clip_control control;
    if(!c || !records || !output || !qwords)return 0;
    if(!unlit_clip_build(c,records,unused,&n,scratch,mask,&control,NULL,NULL,NULL,.001f,residual,&cycles))return 0;
    if(control.count<3) {packet[0]=unused[0];n=1;}
    else {
        ps2_vf constants[32],quantized[8][4];memcpy(constants,c,sizeof constants);
        constants[22]=c[21];constants[25]=c[31];
        for(u32 j=0;j<control.count;j++)memcpy(quantized[j],scratch+40+4*j,4*sizeof(ps2_vf));
        if(!rn_ac4_lit_packet(constants,quantized,control.count,parity,2,packet,&n))return 0;
    }
    memcpy(output,packet,n*sizeof *output);*qwords=n;
    if(scratch_out)memcpy(scratch_out,scratch,sizeof scratch);
    if(mask_out)memcpy(mask_out,mask,sizeof mask);
    if(residual_out)memcpy(residual_out,residual,sizeof residual);
    if(polygon_cycles)*polygon_cycles=cycles;
    return 1;
}

static int lit_clip_build(const ps2_vf c[32],const ps2_vf records[3][4],u32 parity,
                          ps2_vf output[144],u32 *qwords,ps2_vf scratch_out[80],u8 mask_out[80]) {
    return lit_clip_build_state(c,records,parity,output,qwords,scratch_out,mask_out,NULL,NULL);
}

int rn_ac4_unlit_clip_packet(const ps2_vf c[32],const ps2_vf input[3][4],
                            ps2_vf output[70],u32 *qwords) {
    return unlit_clip_build(c,input,output,qwords,NULL,NULL,NULL,NULL,NULL,NULL,0,NULL,NULL);
}

/* Integer state at XGKICK, before the outer strip loop advances vi9. */
static void clip_kick_integers(const unlit_clip_control *c,u32 top,u32 first,
                              u32 input_count,const u16 initial[16],u16 out[16]) {
    memcpy(out,initial,16*sizeof(u16));u32 bank=top+126+((first&1u)?104:0);
    out[1]=0;out[2]=(u16)top;out[4]=(u16)bank;out[9]=(u16)first;
    out[12]=(u16)input_count;out[13]=(u16)(first+3);
    if(c->count<3) {
        out[3]=(u16)(top+32+4*(first+3));
        out[5]=(u16)((c->last_plane+1)&1u);out[6]=(u16)c->count;
        out[7]=(u16)((c->last_plane&1u)?32:72);
        out[8]=(u16)((c->last_plane&1u)?72:32);
        out[10]=(u16)(1u<<c->last_plane);out[11]=(u16)(c->count-1);
    } else {
        out[3]=(u16)(72+4*(c->count+1));out[5]=(u16)(bank+3*c->count+3);
        out[6]=0;out[7]=2;out[8]=(u16)(72+4*c->count);
        out[10]=1;out[11]=(u16)(0x8000u+c->count);
    }
}

/* This is a native packet producer, not a copy of the observed output.
 * Unclipped and wholly culled unlit batches are covered; execution is opt-in. */
int rn_ac4_unlit_plan_build(const ps2_vf c[32],const ps2_vf r[][4],
                           u32 top,rn_ac4_unlit_plan *out) {
    if(!c || !r || !out)return 0;
    u32 n=c[23].u[0]&65535u;
    if(n<3 || n>23)return 0;
    rn_ac4_unlit_plan p={0};ps2_vf constants[32];
    memcpy(constants,c,sizeof constants);constants[23].u[1]&=~2u;
    p.count=n;p.top=top&1023u;p.split=(c[29].u[1]&65535u)!=0;
    p.packets=p.split?n-2:1;p.tag=c[22];
    int inside=1;
    for(u32 i=0;i<n;i++) {
        transform(c+4,r[i][2].f,p.clip[i]);
        for(int k=0;k<4;k++)if(!isfinite(p.clip[i][k]))return 0;
        float w=fabsf(p.clip[i][3]);
        for(int k=0;k<3;k++) {
            if(p.clip[i][k]>w)p.clip_bits[i]|=1u<<(k*2);
            if(p.clip[i][k]<-w)p.clip_bits[i]|=2u<<(k*2);
        }
        if(p.clip_bits[i] || p.clip[i][3]<=0)inside=0;
    }
    if(!inside) {
        if(!p.split)return 0;
        for(u32 k=0;k<p.packets;k++) {
            u32 common=p.clip_bits[k]&p.clip_bits[k+1]&p.clip_bits[k+2];
            /* The original far-W branch does not run the clip-plane culler. */
            if(!common || (p.clip[k][3]>8192 && p.clip[k+1][3]>8192 && p.clip[k+2][3]>8192))return 0;
            u32 plane=0;while(!(common&(1u<<plane)))plane++;
            p.cull_plane[k]=(u8)plane;
        }
        p.culled=1;*out=p;return 1;
    }
    /* Known packed ST/RGBA/XYZF stream, no extra material or REGLIST payload. */
    if(((p.tag.u[1]>>26)&3u)!=0 || (p.tag.u[1]>>28)!=3u
       || p.tag.u[2]!=0x412u || p.tag.u[3]!=0)return 0;
    for(u32 i=0;i<n;i++) {
        if(!rn_ac4_model_eval(constants,r[i],&p.vertices[i]))return 0;
        /* The unlit program converts integer colour to float and back. */
        for(int k=0;k<4;k++)p.vertices[i].rgba[k]=ps2_cvt_w_s((float)r[i][1].s[k]);
    }
    *out=p;return 1;
}
u32 rn_ac4_unlit_plan_packet(const rn_ac4_unlit_plan *p,u32 index,
                            ps2_vf output[70],u32 *address) {
    if(!p || !output || !address || index>=p->packets)return 0;
    u32 first=p->split?index:0,n=p->split?3:p->count;
    u32 parity=p->split?index:p->count-3;
    *address=(p->top+126+((parity&1u)?104:0))&1023u;
    if(p->culled) {
        memset(output,0,sizeof *output);output[0].u[0]=0xffff8000u;return 1;
    }
    output[0]=p->tag;
    /* MFIR sign-extends the EOP/count lane from the 16-bit integer register. */
    output[0].s[0]=(s16)(0x8000u|n);
    for(u32 i=0;i<n;i++) {
        const rn_ac4_model_vertex *v=&p->vertices[first+i];
        memcpy(output[1+i*3].f,v->stq,16);
        memcpy(output[2+i*3].s,v->rgba,16);
        memcpy(output[3+i*3].s,v->xyzf,16);
    }
    return 1+n*3;
}

int rn_ac4_model_plan_on, rn_ac4_model_native_on;
static u64 native_runs,native_packets,native_cycles,native_culled;
static int model_qlat=1;
static struct {
    const ps2_vu *owner;
    rn_ac4_unlit_plan plan;
    ps2_vu initial;
    ps2_vf constants[32], last_record[4], records[23][4];
    u32 entry;
    ps2_vf expected_mem[1024];
    u32 next;
    int active;
} plan_check;
static struct {u64 built,declined,packets,packet_bad,memory_bad,ended,missing,control_bad,register_bad,flags_bad,cycles_bad,culled;} plan_stats;
static void unlit_plan_kick(const ps2_vu *v,u32 address) {
    if(!plan_check.active || plan_check.owner!=v)return;
    ps2_vf packet[70];u32 target=0;
    u32 n=rn_ac4_unlit_plan_packet(&plan_check.plan,plan_check.next,packet,&target);
    plan_stats.packets++;
    int bad=!n || target!=(address&1023u);
    for(u32 i=0;i<n;i++) {
        u32 at=(target+i)&1023u;
        if(memcmp(packet+i,v->mem+at*16u,16))bad=1;
        plan_check.expected_mem[at]=packet[i];
    }
    if(bad && ++plan_stats.packet_bad<=3)
        ps2_log("ac4 unlit plan: packet %u differs (expected %u, actual %u, qwords %u)",plan_check.next,target,address,n);
    plan_check.next++;
}
static unlit_flags unlit_plan_flags(void) {
    unlit_flags f={plan_check.initial,0};
    const rn_ac4_unlit_plan *p=&plan_check.plan;const ps2_vf *c=plan_check.constants;
    /* Entry trampoline, setup, bank selection and split dispatch. */
    flags_advance(&f,(plan_check.entry==3344?2u:0u)+15+((p->count-3)&1u)+(p->split?1u:0u));
    for(u32 packet=0;packet<p->packets;packet++) {
        u32 first=p->split?packet:0,n=p->split?3:p->count;
        if(p->split) {
            flags_advance(&f,f.cycles+10+(packet&1u));
            u32 base=f.cycles;float cp[3][4];
            flags_scalar(&f,base+9,2,ps2_fmul((float)c[29].s[2],64));
            for(int i=0;i<3;i++)flags_transform(&f,base+10+i*4,c+4,plan_check.records[first+i][2].f,cp[i]);
            for(int i=0;i<3;i++)flags_scalar(&f,base+23+i,1,ps2_fadd(cp[i][3],-8192));
            flags_advance(&f,base+29); /* through the third FMAND */
            int which=0;while(which<3 && cp[which][3]>8192)which++;
            flags_advance(&f,f.cycles+(which<3?2u*(which+1)+3u:8u));
        }
        if(p->culled) {
            flags_advance(&f,f.cycles+2u*(p->cull_plane[packet]+1u));
            u32 start=f.cycles;flags_scalar(&f,start+1,15,0);
            flags_advance(&f,start+8+9+(packet&1u));
            continue;
        }
        u32 base=f.cycles;
        flags_scalar(&f,base+22,1,0); /* zero normal W */
        flags_scalar(&f,base+26,1,c[30].f[0]);
        for(u32 i=0;i<n;i++) {
            u32 start=f.cycles;float projected[4],uv[4],scaled[4];
            const ps2_vf *r=plan_check.records[first+i];
            flags_transform(&f,start+1,c,r[2].f,projected);
            float fog=ps2_fadd(c[30].f[0],-projected[3]);
            flags_scalar(&f,start+8,1,fog);
            flags_scalar(&f,start+12,1,ps2_fmul(fog,c[30].f[1]));
            float q=1.0f/projected[3];
            for(int k=0;k<4;k++) {
                scaled[k]=ps2_fmul(projected[k],q);
                uv[k]=ps2_fmul((float)r[0].s[k]*(1.0f/4096.0f),q);
            }
            flags_result(&f,start+15,14,scaled);
            flags_result(&f,start+17,15,uv);
            flags_scalar(&f,start+25,1,c[30].f[0]);
            flags_scalar(&f,start+29,1,0);
            flags_advance(&f,start+33);
        }
        /* Loop exit, bank reset, kick and triangle-loop branch/delay slot. */
        u32 parity=p->split?packet:p->count-3;
        flags_advance(&f,f.cycles+11+(parity&1u));
    }
    flags_advance(&f,f.cycles+2); /* E-bit and delay slot */
    f.state.fslot=(f.cycles-1)&3u;
    return f;
}
static ps2_vu unlit_culled_state(void) {
    const rn_ac4_unlit_plan *p=&plan_check.plan;const ps2_vf *c=plan_check.constants;
    ps2_vu v=plan_check.initial;u32 first=p->count-3;
    u32 plane=p->cull_plane[p->packets-1];
    u32 common=p->clip_bits[first]&p->clip_bits[first+1]&p->clip_bits[first+2];
    v.vi[1]=plane==5?1:(common&(1u<<(plane+1)))!=0;
    v.vi[2]=(u16)p->top;v.vi[3]=(u16)(p->top+32+first*4);
    v.vi[4]=(u16)(p->top+126+((first&1u)?104:0));v.vi[6]=3;v.vi[9]=(u16)(p->count-2);
    v.vi[10]=p->clip[first][3]<8192?16:p->clip[first][3]==8192?1:0;
    v.vi[11]=p->clip[first+1][3]<8192?16:p->clip[first+1][3]==8192?1:0;
    v.vi[12]=v.vi[13]=(u16)p->count;
    v.vf[1]=c[22];v.vf[2]=c[25];v.vf[21]=c[29];
    v.vf[21].f[2]=ps2_fmul((float)c[29].s[2],64);
    memcpy(v.vf+22,c+4,4*sizeof(ps2_vf));
    memcpy(v.vf[27].f,p->clip[first],16);memcpy(v.vf[28].f,p->clip[first+1],16);
    memset(&v.vf[29],0,16);v.vf[29].u[0]=0xffff8000u;
    for(u32 j=0;j<p->packets;j++)for(u32 i=0;i<3;i++)v.clip=((v.clip<<6)|p->clip_bits[j+i])&0xffffffu;
    for(int k=0;k<4;k++) {
        const float *pos=plan_check.last_record[2].f;
        float a=ps2_fmul(c[4].f[k],pos[0]);a=ps2_fadd(a,ps2_fmul(c[5].f[k],pos[1]));
        v.acc.f[k]=ps2_fadd(a,ps2_fmul(c[6].f[k],pos[2]));
    }
    v.i=8192;v.tpc=0xd10;v.running=0;
    if(v.q_ready)v.q=v.q_pend;
    if(v.p_ready)v.p=v.p_pend;
    v.q_ready=v.p_ready=0;return v;
}
static ps2_vu unlit_plan_state(void) {
    if(plan_check.plan.culled)return unlit_culled_state();
    const rn_ac4_unlit_plan *p=&plan_check.plan;
    const ps2_vf *c=plan_check.constants;
    ps2_vu predicted=plan_check.initial;
    u32 first=p->split?p->count-3:0, n=p->split?3:p->count;
    u32 bank=p->top+126+(((p->count-3)&1u)?104:0);
    predicted.vi[2]=(u16)p->top;
    predicted.vi[3]=(u16)(p->top+32+4*(p->count+1));
    predicted.vi[4]=(u16)bank;predicted.vi[5]=(u16)(bank+3*n+3);
    predicted.vi[6]=0;predicted.vi[7]=2;predicted.vi[9]=(u16)(p->count-2);
    predicted.vi[10]=0;predicted.vi[11]=(u16)(0x8000u+n);
    predicted.vi[12]=(u16)p->count;predicted.vi[13]=(u16)p->count;
    if(p->split) {
        for(u32 i=0;i<p->count;i++) {
            ps2_vf rec=plan_check.expected_mem[(p->top+34+4*i)&1023u];float clip[4];
            transform(c+4,rec.f,clip);
            if(clip[3]<=8192.0f)predicted.vi[1]=0;
            if(i==first)predicted.vi[10]=clip[3]<8192.0f?16:clip[3]==8192.0f?1:0;
        }
        predicted.clip=p->count==3?(predicted.clip<<18)&0xffffffu:0;
    }
    predicted.vf[1]=p->tag;predicted.vf[1].s[0]=(s16)(0x8000u+n);
    predicted.vf[2]=c[25];predicted.vf[10]=c[30];predicted.vf[16].f[3]=c[30].f[0];
    const rn_ac4_model_vertex *last=&p->vertices[p->count-1];
    memcpy(predicted.vf[17].s,last->rgba,16);memcpy(predicted.vf[19].f,last->stq,16);
    memcpy(predicted.vf[21].s,last->xyzf,16);memcpy(predicted.vf+22,c,4*sizeof(ps2_vf));
    for(u32 j=0;j<4;j++)predicted.vf[26+j]=plan_check.expected_mem[(p->top+32+4*p->count+j)&1023u];
    predicted.vf[29].s[3]=0;
    for(int k=0;k<4;k++) {
        predicted.vf[26].f[k]=(float)predicted.vf[26].s[k]*(1.0f/4096.0f);
        predicted.vf[27].f[k]=(float)predicted.vf[27].s[k];
        predicted.vf[29].f[k]=(float)predicted.vf[29].s[k]*(1.0f/4096.0f);
        const float *pos=plan_check.last_record[2].f;
        float a=ps2_fmul(c[0].f[k],pos[0]);
        a=ps2_fadd(a,ps2_fmul(c[1].f[k],pos[1]));
        predicted.acc.f[k]=ps2_fadd(a,ps2_fmul(c[2].f[k],pos[2]));
    }
    float projected[4];transform(c,plan_check.last_record[2].f,projected);
    predicted.q=1.0f/projected[3];predicted.i=255.0f;
    if(model_qlat)predicted.q_pend=predicted.q;
    if(predicted.p_ready)predicted.p=predicted.p_pend;
    predicted.q_ready=predicted.p_ready=0;
    predicted.tpc=0xd10u;predicted.running=0;
    return predicted;
}
static int unlit_registers_differ(const ps2_vu *predicted,const ps2_vu *v) {
    return memcmp(predicted->vi,v->vi,sizeof v->vi)!=0
        || memcmp(predicted->vf,v->vf,sizeof v->vf)!=0 || memcmp(&predicted->acc,&v->acc,16)!=0
        || memcmp(&predicted->q,&v->q,4)!=0 || memcmp(&predicted->p,&v->p,4)!=0
        || memcmp(&predicted->i,&v->i,4)!=0 || predicted->clip!=v->clip || predicted->r!=v->r
        || memcmp(&predicted->q_pend,&v->q_pend,4)!=0 || memcmp(&predicted->p_pend,&v->p_pend,4)!=0
        || predicted->q_ready!=v->q_ready || predicted->p_ready!=v->p_ready
        || predicted->top!=v->top || predicted->itop!=v->itop || predicted->cmsar!=v->cmsar
        || predicted->tpc!=v->tpc || predicted->cc!=v->cc || predicted->running!=v->running;
}
static int unlit_plan_registers(const ps2_vu *v) {
    ps2_vu predicted=unlit_plan_state();
    int bad=unlit_registers_differ(&predicted,v);
    if(bad && plan_stats.register_bad<3) {
        for(int i=0;i<16;i++)if(predicted.vi[i]!=v->vi[i])ps2_log("ac4 unlit state: vi%d expected %04x actual %04x",i,predicted.vi[i],v->vi[i]);
        for(int i=0;i<32;i++)for(int k=0;k<4;k++)if(predicted.vf[i].u[k]!=v->vf[i].u[k])ps2_log("ac4 unlit state: vf%d.%d expected %08x actual %08x",i,k,predicted.vf[i].u[k],v->vf[i].u[k]);
    }
    return !bad;
}
static void clip_cycle_end(const ps2_vu *v,u32 cycles);
static void lit_memory_end(const ps2_vu *v,u32 cycles);
static void cockpit_memory_end(const ps2_vu *v);
static void cockpit_cycle_end(const ps2_vu *v,u32 cycles);
void rn_ac4_model_audit_end(const ps2_vu *v,u32 cycles) {
    cockpit_cycle_end(v,cycles);
    cockpit_memory_end(v);
    terrain_execution_end(v,cycles);
    terrain_flags_end(v);
    terrain_cycle_end(v,cycles);
    terrain_end(v);
    clip_cycle_end(v,cycles);
    lit_memory_end(v,cycles);
    if(!plan_check.active || plan_check.owner!=v)return;
    plan_stats.ended++;
    if(plan_check.next!=plan_check.plan.packets)plan_stats.missing++;
    if(memcmp(plan_check.expected_mem,v->mem,sizeof plan_check.expected_mem))plan_stats.memory_bad++;
    if(!unlit_plan_registers(v))plan_stats.register_bad++;
    unlit_flags f=unlit_plan_flags();
    if(f.cycles!=cycles)plan_stats.cycles_bad++;
    if(f.state.mac!=v->mac || f.state.status!=v->status || f.state.fslot!=v->fslot
       || memcmp(f.state.fpipe,v->fpipe,sizeof v->fpipe)) {
        if(plan_stats.flags_bad++<3)ps2_log("ac4 unlit flags: cycles %u, status %x/%x mac %x/%x slot %u/%u",f.cycles,f.state.status,v->status,f.state.mac,v->mac,f.state.fslot,v->fslot);
    }
    /* Separate summary retained for the original continuation gate. */
    if(v->tpc!=0xd10u || v->vi[6]!=(plan_check.plan.culled?3:0) || v->vi[9]!=plan_check.plan.count-2)plan_stats.control_bad++;
    plan_check.active=0;
}

/* All rejection happens before touching guest state or submitting a packet.
 * Verification mode always retains the original execution for comparison. */
static int mixed_try_native(ps2_vu *v,u32 limit,void (*kick)(ps2_vu *,u32));
static int lit_try_native(ps2_vu *v,u32 limit,void (*kick)(ps2_vu *,u32));
static int terrain_try_native(ps2_vu *v,u32 limit,void (*kick)(ps2_vu *,u32));
static int lit_native_on,cockpit_native_on;
static int cockpit_try_native(ps2_vu *v,u32 limit,void (*kick)(ps2_vu *,u32));
int rn_ac4_model_try_native(ps2_vu *v,u32 limit,void (*kick)(ps2_vu *,u32)) {
    if(!rn_ac4_model_native_on || rn_ac4_model_plan_on || !kick)return 0;
    if(cockpit_native_on && cockpit_try_native(v,limit,kick))return 1;
    if(terrain_native_on && terrain_try_native(v,limit,kick))return 1;
    if(terrain_native_on && !model_native_requested && !lit_native_on)return 0;
    if(lit_native_on && lit_try_native(v,limit,kick))return 1;
    if(!plan_check.active || plan_check.owner!=v)return mixed_try_native(v,limit,kick);
    const rn_ac4_unlit_plan *p=&plan_check.plan;
    ps2_vf packet[70];u32 address;
    for(u32 k=0;k<p->packets;k++) {
        u32 n=rn_ac4_unlit_plan_packet(p,k,packet,&address);
        for(u32 j=0;j<n;j++)plan_check.expected_mem[(address+j)&1023u]=packet[j];
    }
    unlit_flags flags=unlit_plan_flags();
    if(flags.cycles>limit)return 0;
    ps2_vu final=unlit_plan_state();
    final.mac=flags.state.mac;final.status=flags.state.status;final.fslot=flags.state.fslot;
    memcpy(final.fpipe,flags.state.fpipe,sizeof final.fpipe);
    *v=final;
    for(u32 k=0;k<p->packets;k++) {
        u32 n=rn_ac4_unlit_plan_packet(p,k,packet,&address);
        for(u32 j=0;j<n;j++)memcpy(v->mem+((address+j)&1023u)*16u,packet+j,16);
        u32 first=p->split?k:0;
        v->vi[9]=(u16)(p->split?k:p->count-3);
        v->vi[10]=0;
        if(p->split) {
            float clip[4];transform(plan_check.constants+4,plan_check.records[first][2].f,clip);
            v->vi[10]=clip[3]<8192?16:clip[3]==8192?1:0;
        }
        kick(v,address);native_packets++;
    }
    *v=final;plan_check.active=0;
    native_runs++;native_cycles+=flags.cycles;if(p->culled)native_culled++;
    return 1;
}

/* Observe only: never suppress a kick or modify guest/GS state. */
int rn_ac4_terrain_mesh_on;
int rn_ac4_model_audit_on, rn_ac4_model_mesh_on, rn_ac4_model_capture_on, rn_ac4_model_material_on;
int rn_ac4_model_texture_on;
static struct {
    u64 seen,unknown,entry,clipped,layout,eligible,kicks,vertices;
    u64 lit_vertices,unlit_vertices;
    u64 exact,bad_xyz,bad_fog,bad_uv,bad_color,rejected,wrong_kick;
    u64 forwarded;
    u64 safety_checks,safety_reused;
} stats;
/* No EFU operations occur in this program. Every kick follows enough work to
 * drain architectural Q/P latencies. Reject unusual inherited deadlines until
 * full cycle accounting can prove them, rather than borrowing observed state. */
typedef struct {float q,p,q_pend,p_pend;u32 clip;int valid;} unlit_scalar_state;
static unlit_scalar_state clip_scalar_begin(const ps2_vu *v) {
    unlit_scalar_state s={v->q_ready?v->q_pend:v->q,v->p_ready?v->p_pend:v->p,
        v->q_pend,v->p_pend,v->clip,v->q_ready<=32 && v->p_ready<=32};
    return s;
}
static void clip_scalar_divide(unlit_scalar_state *s,float q,int latency) {
    s->q=q;if(latency)s->q_pend=q;
}
static int clip_scalar_matches(const unlit_scalar_state *s,const ps2_vu *v) {
    return s->valid && !v->q_ready && !v->p_ready && s->clip==v->clip
        && !memcmp(&s->q,&v->q,4) && !memcmp(&s->p,&v->p,4)
        && !memcmp(&s->q_pend,&v->q_pend,4) && !memcmp(&s->p_pend,&v->p_pend,4);
}
typedef struct {unlit_scalar_state scalar;float i;u32 r;} terrain_scalar_value;
static struct {const ps2_vu *owner;int active;u32 count,next;terrain_scalar_value end,kick[8];} terrain_scalar;
static struct {u64 built,ended,kicks,bad,kick_bad,sequence,declined;} terrain_scalar_stats;
static int terrain_scalar_build(const ps2_vu *v,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,terrain_scalar_value *end,terrain_scalar_value kicks[8],u32 *kick_count) {
    if(!v || !c || !decoded || !end || !kicks || !kick_count || count<3 || count>10)return 0;
    terrain_scalar_value state={clip_scalar_begin(v),1.0f/16,v->r},pending[8];u32 emitted=0;
    if(!state.scalar.valid)return 0;
    u32 mode=c[23].u[1]&65535;
    for(u32 j=1;j+1<count;j++) {
        ps2_vf polygon[10][4];u32 n=count;
        if(mode==1) {
            ps2_vf input[3][4];
            memcpy(input[0],decoded[0],sizeof input[0]);memcpy(input[1],decoded[j],sizeof input[1]);memcpy(input[2],decoded[j+1],sizeof input[2]);
            for(int k=0;k<3;k++) {
                float clip[4];u32 code=0;transform(c+4,input[k][2].f,clip);
                for(int q=0;q<4;q++)if(!isfinite(clip[q]))return 0;
                float w=fabsf(clip[3]);
                for(int q=0;q<3;q++) {if(clip[q]>w)code|=1u<<(2*q);if(clip[q]<-w)code|=2u<<(2*q);}
                state.scalar.clip=((state.scalar.clip<<6)|code)&0xffffff;
            }
            terrain_clip_control control;
            if(!terrain_clip_triangle_control(c,input,polygon,&n,NULL,&control,NULL,NULL,0))return 0;
            if(control.any && !control.common)state.i=-1;
            if(control.has_q)clip_scalar_divide(&state.scalar,control.q,model_qlat);
            if(!n)continue;
        } else memcpy(polygon,decoded,count*sizeof polygon[0]);
        float projected[4];transform(c,polygon[n-1][2].f,projected);
        if(!isfinite(projected[3]) || projected[3]==0 || emitted==8)return 0;
        clip_scalar_divide(&state.scalar,1.0f/projected[3],model_qlat);state.i=255;
        pending[emitted++]=state;
        if(mode!=1)break;
    }
    *end=state;memcpy(kicks,pending,emitted*sizeof pending[0]);*kick_count=emitted;return 1;
}
static int terrain_scalar_match(const terrain_scalar_value *s,const ps2_vu *v) {
    return clip_scalar_matches(&s->scalar,v) && !memcmp(&s->i,&v->i,4) && s->r==v->r;
}
/* Assemble one atomic continuation snapshot before considering execution
 * bypass. Guest memory and registers remain untouched until a caller commits. */
/* A packet commit stores only changed qwords, preserving scratch writes and
 * packet-bank order without copying the whole 16-KiB VU memory per draw. */
typedef struct {ps2_vf values[256];u16 addresses[256];u32 writes,address;} terrain_submission;
typedef struct {ps2_vu state;ps2_vf memory[1024];u32 cycles,packets;terrain_submission submissions[8];} terrain_execution_plan;
static int terrain_submission_append(ps2_vf before[1024],const ps2_vf after[1024],u32 address,terrain_submission *out) {
    terrain_submission result={0};result.address=address&1023;
    for(u32 j=0;j<1024;j++)if(memcmp(before+j,after+j,16)) {
        if(result.writes==256)return 0;
        result.addresses[result.writes]=(u16)j;result.values[result.writes++]=after[j];
    }
    memcpy(before,after,16384);*out=result;return 1;
}
static int terrain_submissions_build(const ps2_vu *v,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,terrain_execution_plan *plan) {
    ps2_vf memory[1024],previous[1024],constants[32];memcpy(memory,v->mem,sizeof memory);memcpy(previous,memory,sizeof previous);
    memcpy(constants,c,sizeof constants);constants[23].u[1]=4;
    for(u32 j=0;j<count*4;j++)memory[(v->top+32+j)&1023]=((const ps2_vf*)decoded)[j];
    u32 emitted=0;int clipped=(c[23].u[1]&65535)==1;
    for(u32 j=1;j+1<count;j++) {
        ps2_vf polygon[10][4],packet[65];u32 n=count,size;
        if(clipped) {
            ps2_vf triangle[3][4];memcpy(triangle[0],decoded[0],64);memcpy(triangle[1],decoded[j],64);memcpy(triangle[2],decoded[j+1],64);
            if(!terrain_clip_triangle_state(c,triangle,polygon,&n,memory))return 0;
        } else memcpy(polygon,decoded,count*sizeof polygon[0]);
        if(!n)continue;
        if(emitted==8 || !terrain_packet(constants,polygon,n,packet,&size))return 0;
        u32 address=v->top+((emitted&1)?126:210);
        for(u32 k=0;k<size;k++)memory[(address+k)&1023]=packet[k];
        if(!terrain_submission_append(previous,memory,address,plan->submissions+emitted))return 0;
        emitted++;if(!clipped)break;
    }
    return emitted==plan->packets && !memcmp(memory,plan->memory,sizeof memory);
}
static int terrain_execution_build(const ps2_vu *v,u32 entry,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count,terrain_execution_plan *out) {
    if(!v || !v->mem || v->mem_size!=16384 || !out)return 0;
    terrain_execution_plan result;result.state=*v;
    terrain_vector_state vectors,vector_kicks[8];terrain_scalar_value scalar,scalar_kicks[8];
    u16 integers[16],integer_kicks[8][16];u32 vector_count,scalar_count,integer_count;unlit_flags flags;
    if(!terrain_memory_build(v,c,decoded,count,result.memory)
       || !terrain_integer_plan(v,c,decoded,count,integers,integer_kicks,&integer_count)
       || !terrain_vector_plan(v,c,decoded,count,&vectors,vector_kicks,&vector_count)
       || !terrain_scalar_build(v,c,decoded,count,&scalar,scalar_kicks,&scalar_count)
       || !terrain_instruction_plan(entry,c,decoded,count,&result.cycles)
       || !terrain_flags_plan(v,entry,c,decoded,count,&flags)
       || integer_count!=vector_count || integer_count!=scalar_count || flags.cycles!=result.cycles)return 0;
    memcpy(result.state.vi,integers,sizeof integers);memcpy(result.state.vf,vectors.vf,sizeof vectors.vf);result.state.acc=vectors.acc;
    result.state.q=scalar.scalar.q;result.state.p=scalar.scalar.p;
    result.state.q_pend=scalar.scalar.q_pend;result.state.p_pend=scalar.scalar.p_pend;
    result.state.q_ready=result.state.p_ready=0;result.state.i=scalar.i;result.state.r=scalar.r;result.state.clip=scalar.scalar.clip;
    result.state.mac=flags.state.mac;result.state.status=flags.state.status;result.state.fslot=flags.state.fslot;
    memcpy(result.state.fpipe,flags.state.fpipe,sizeof flags.state.fpipe);
    result.state.tpc=0x1050;result.state.running=0;result.packets=integer_count;
    if(!terrain_submissions_build(v,c,decoded,count,&result))return 0;
    *out=result;return 1;
}
static struct {const ps2_vu *owner;int active;u32 next;ps2_vf kick_memory[1024];terrain_execution_plan plan;} terrain_execution;
static struct {u64 packets,bad,sequence;} terrain_submission_stats;
static void terrain_execution_reset(void) {terrain_execution.active=0;}
static struct {u64 built,ended,bad,declined,packets,cycles;} terrain_execution_stats;
static void terrain_execution_begin(const ps2_vu *v,u32 entry,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count) {
    terrain_execution.active=0;
    if(terrain_execution_build(v,entry,c,decoded,count,&terrain_execution.plan)) {
        terrain_native_ready=1;terrain_execution.owner=v;terrain_execution.active=1;terrain_execution.next=0;memcpy(terrain_execution.kick_memory,v->mem,16384);terrain_execution_stats.built++;
    } else terrain_execution_stats.declined++;
}
static void terrain_execution_end(const ps2_vu *v,u32 cycles) {
    if(!terrain_execution.active || terrain_execution.owner!=v)return;
    terrain_execution.active=0;terrain_execution_stats.ended++;
    terrain_submission_stats.sequence+=terrain_execution.next!=terrain_execution.plan.packets;
    const terrain_execution_plan *p=&terrain_execution.plan;
    terrain_execution_stats.packets+=p->packets;terrain_execution_stats.cycles+=p->cycles;
    if(unlit_registers_differ(&p->state,v) || p->state.status!=v->status || p->state.mac!=v->mac || p->state.fslot!=v->fslot
       || memcmp(p->state.fpipe,v->fpipe,sizeof v->fpipe) || memcmp(p->memory,v->mem,sizeof p->memory) || p->cycles!=cycles
       || p->state.mem!=v->mem || p->state.micro!=v->micro || p->state.mem_size!=v->mem_size || p->state.micro_size!=v->micro_size) {
        if(terrain_execution_stats.bad++<3)ps2_log("ac4 terrain continuation difference: tpc %x/%x cc %x/%x running %u/%u",p->state.tpc,v->tpc,p->state.cc,v->cc,p->state.running,v->running);
    }
}
static u64 terrain_native_runs,terrain_native_packets,terrain_native_cycles;
static int terrain_try_native(ps2_vu *v,u32 limit,void (*kick)(ps2_vu *,u32)) {
    if(!terrain_native_on || rn_ac4_model_plan_on || !kick || !terrain_native_ready || !terrain_execution.active || terrain_execution.owner!=v)return 0;
    const terrain_execution_plan *p=&terrain_execution.plan;
    if(p->cycles>limit || p->state.mem!=v->mem || p->state.micro!=v->micro)return 0;
    /* Construction and all eligibility checks precede guest-visible changes.
     * Keep the original GIF/GS callback while bypassing only VU execution. */
    for(u32 j=0;j<p->packets;j++) {
        const terrain_submission *submission=p->submissions+j;
        for(u32 k=0;k<submission->writes;k++)memcpy(v->mem+submission->addresses[k]*16,submission->values+k,16);
        kick(v,submission->address);
    }
    memcpy(v->mem,p->memory,sizeof p->memory);*v=p->state;
    terrain_native_runs++;terrain_native_packets+=p->packets;terrain_native_cycles+=p->cycles;
    terrain_execution.active=terrain_native_ready=0;return 1;
}
static void terrain_execution_kick(const ps2_vu *v,u32 address) {
    if(!terrain_execution.active || terrain_execution.owner!=v)return;
    u32 at=terrain_execution.next++;
    if(at>=terrain_execution.plan.packets) {terrain_submission_stats.sequence++;return;}
    const terrain_submission *p=terrain_execution.plan.submissions+at;
    for(u32 j=0;j<p->writes;j++)terrain_execution.kick_memory[p->addresses[j]]=p->values[j];
    terrain_submission_stats.packets++;
    terrain_submission_stats.sequence+=p->address!=(address&1023);
    if(memcmp(terrain_execution.kick_memory,v->mem,16384))terrain_submission_stats.bad++;
}
static void terrain_scalar_begin(const ps2_vu *v,const ps2_vf c[32],const ps2_vf decoded[][4],u32 count) {
    terrain_scalar.active=0;
    if(terrain_scalar_build(v,c,decoded,count,&terrain_scalar.end,terrain_scalar.kick,&terrain_scalar.count)) {
        terrain_scalar.owner=v;terrain_scalar.next=0;terrain_scalar.active=1;terrain_scalar_stats.built++;
    } else terrain_scalar_stats.declined++;
}
static void terrain_scalar_kick(const ps2_vu *v) {
    if(!terrain_scalar.active || terrain_scalar.owner!=v)return;
    u32 at=terrain_scalar.next++;terrain_scalar_stats.kicks++;
    if(at>=terrain_scalar.count || !terrain_scalar_match(terrain_scalar.kick+at,v))terrain_scalar_stats.kick_bad++;
}
static void terrain_scalar_end(const ps2_vu *v) {
    if(!terrain_scalar.active || terrain_scalar.owner!=v)return;
    terrain_scalar.active=0;terrain_scalar_stats.ended++;
    terrain_scalar_stats.sequence+=terrain_scalar.next!=terrain_scalar.count;
    if(!terrain_scalar_match(&terrain_scalar.end,v)) {
        if(!terrain_scalar_stats.bad)ps2_log("ac4 terrain scalar difference: Q %g/%g Qpending %g/%g P %g/%g I %g/%g clip %x/%x",terrain_scalar.end.scalar.q,v->q,terrain_scalar.end.scalar.q_pend,v->q_pend,terrain_scalar.end.scalar.p,v->p,terrain_scalar.end.i,v->i,terrain_scalar.end.scalar.clip,v->clip);
        terrain_scalar_stats.bad++;
    }
}
static struct { const ps2_vu *owner; int active; u32 count,top; int split,lit; ps2_vf constants[32], records[23][4]; u8 clip_ok[23],projection_ok[23]; u16 initial_vi[16]; ps2_vf clip_scratch[85]; int scratch_valid,native_safe; unlit_scalar_state scalar; u32 cycles,clip_packets,next,packet_bad,sequence_bad; ps2_vf memory[1024]; ps2_vu state; unlit_flags flags; } sample;

static ps2_vf read_qw(const ps2_vu *v,u32 addr);
static struct {const ps2_vu *owner; int active; u32 next; ps2_vf memory[1024];u16 vi[16];int integers_valid;unlit_scalar_state scalar;float i;u32 r;ps2_vu vectors;int vectors_valid;u32 cycles;int cycles_valid,flags_valid;unlit_flags flags;} lit_memory;
typedef struct {ps2_vf packet[144],scratch[80],winding[2];u8 mask[80];u32 n,address;u16 vi9,vi10;int writes_winding;} lit_submission;
static lit_submission lit_submissions[21];
static u64 lit_native_runs,lit_native_packets,lit_native_cycles;
static struct {u64 ended,packets,memory_bad,sequence_bad,declined,packet_bad;} lit_memory_stats;
static struct {u64 packets,ended,bad,end_bad,declined;} lit_integer_stats;
static struct {u64 packets,ended,bad,end_bad,declined;} lit_scalar_stats;
static int lit_scalar_matches(const ps2_vu *v) {
    return clip_scalar_matches(&lit_memory.scalar,v) && !memcmp(&lit_memory.i,&v->i,4) && lit_memory.r==v->r;
}
static struct {u64 packets,ended,bad,end_bad,skipped;} lit_vector_stats;
static struct {u64 ended,bad,declined,total;} lit_cycle_stats;
static struct {u64 ended,bad,declined;} lit_flag_stats;
static int lit_vectors_match(const ps2_vu *v) {
    return !memcmp(lit_memory.vectors.vf,v->vf,sizeof v->vf) && !memcmp(&lit_memory.vectors.acc,&v->acc,sizeof v->acc);
}
/* Count executed instruction pairs, including branch delay and padding slots. */
static u32 lit_visible_cycles(const ps2_vf c[32],const ps2_vf records[][4],u32 count,u32 parity,u16 near_first) {
    int winding=(c[23].u[2]&1)^((c[29].u[0]&65535)!=0);
    u32 cycles=44;
    if(c[29].u[0]&65535)cycles+=(c[23].u[2]&1)?4:5;
    if(parity&1) {cycles+=winding?4:3;winding^=1;}
    s32 fog[23];lit_fog_adc(c,records,count,parity,near_first?2:0,fog);
    for(u32 j=0;j<count;j++) {
        float normal[4],n[4];for(int k=0;k<3;k++)normal[k]=(float)records[j][3].s[k]/4096.0f;normal[3]=0;
        transform(c+8,normal,n);
        cycles+=77+((c[23].u[1]&2)?0:1)+(n[1]<0);
        if(c[23].u[1]&1) {
            if(j<2)cycles+=6;
            else {cycles+=18+(winding && !near_first)+((fog[j]&0x8000)!=0);if(!near_first)winding^=1;}
        }
    }
    return cycles+2+9+(parity&1);
}
static int lit_clipping_flags_supported(const ps2_vu *v) {
    if((v->status&0xc0)!=0xc0)return 0;
    for(int k=0;k<4;k++)if(v->fpipe[k].valid && (v->fpipe[k].status&0xc0)!=0xc0)return 0;
    return 1;
}
/* Model visible-loop arithmetic independently of packet writes. OPMULA,
 * conversions and min/max do not write flags in this runtime. Clipping is
 * eligible only after sticky Z/S saturation; its transient pipeline slots are
 * overwritten by the visible loop. Unsaturated clipping remains explicit
 * unsupported state until its full arithmetic schedule is modeled. */
static void lit_visible_flags(unlit_flags *f,u32 origin,const ps2_vf c[32],const ps2_vf records[][4],u32 count,u32 parity,u16 near_first) {
    int winding=(c[23].u[2]&1)^((c[29].u[0]&65535)!=0);
    u32 start=origin+44;
    if(c[29].u[0]&65535)start+=(c[23].u[2]&1)?4:5;
    if(parity&1) {start+=winding?4:3;winding^=1;}
    float xy[23][4];s32 fog_adc[23];lit_fog_adc(c,records,count,parity,near_first?2:0,fog_adc);
    for(u32 j=0;j<count;j++) {
        const ps2_vf *r=records[j];float normal[4],n[4],p[4],v[4],light[4],terms[4][4],color[4],env[4]={0,0,1,1};
        for(int k=0;k<3;k++)normal[k]=(float)r[3].s[k]/4096.0f;
        normal[3]=0;
        flags_transform(f,start+1,c+8,normal,n);flags_transform(f,start+5,c,r[2].f,p);
        for(int t=0;t<4;t++) {
            for(int k=0;k<4;k++)terms[t][k]=ps2_fmul(c[12+t].f[k],n[t]);
            flags_result(f,start+9+t,15,terms[t]);
            for(int k=0;k<4;k++)terms[t][k]=fmaxf(terms[t][k],0);
        }
        for(int t=0;t<4;t++) {
            for(int k=0;k<4;k++)light[k]=t?ps2_fadd(light[k],terms[t][k]):ps2_fmul(terms[t][k],1);
            flags_result(f,start+17+t,15,light);
        }
        for(int k=0;k<4;k++)v[k]=ps2_fadd(k==3?1:0,n[k]);
        flags_result(f,start+21,15,v);
        for(int k=0;k<4;k++)v[k]=k==3?1:0;
        flags_result(f,start+22,15,v);flags_scalar(f,start+23,15,0);
        float fog=ps2_fmul(p[3],-1);flags_scalar(f,start+24,1,fog);
        flags_scalar(f,start+25,1,(float)r[1].s[3]);
        env[0]=ps2_fsub(0,n[0]);flags_scalar(f,start+26,8,env[0]);
        flags_scalar(f,start+27,15,128);
        for(int t=0;t<4;t++) {
            for(int k=0;k<4;k++) {float product=ps2_fmul(c[16+t].f[k],light[t]);color[k]=t?ps2_fadd(color[k],product):product;}
            flags_result(f,start+28+t+(t>=2),15,color);
            if(t==1) {env[1]=ps2_fadd(0,n[2]);flags_scalar(f,start+30,4,env[1]);}
        }
        fog=ps2_fadd(fog,c[30].f[0]);flags_scalar(f,start+33,1,fog);
        for(int k=0;k<3;k++)env[k]=ps2_fadd(k<2?env[k]:0,1);
        flags_result(f,start+34,14,env);
        float q=1.0f/p[3];
        for(int k=0;k<4;k++)v[k]=ps2_fmul((float)r[0].s[k]/4096.0f,q);
        flags_result(f,start+35,15,v);
        for(int k=0;k<3;k++)v[k]=ps2_fmul(p[k],q);
        flags_result(f,start+37,14,v);
        xy[j][0]=ps2_fadd(0,v[0]);xy[j][1]=ps2_fadd(0,v[1]);xy[j][2]=0;xy[j][3]=1;
        for(int k=0;k<2;k++)env[k]=ps2_fmul(env[k],.5f);
        flags_result(f,start+38,12,env);
        int down=n[1]<0;
        if(down)flags_scalar(f,start+39,14,0);
        for(int k=0;k<3;k++)color[k]=ps2_fadd(fmaxf(color[k],0),c[20].f[k]);
        flags_result(f,start+39+down,14,color);
        fog=ps2_fmul(fog,c[30].f[1]);flags_scalar(f,start+40+down,1,fog);
        for(int k=0;k<4;k++)env[k]=ps2_fmul(env[k],q);
        flags_result(f,start+41+down,15,env);
        for(int k=0;k<4;k++)v[k]=ps2_fadd(0,(float)r[1].s[k]);
        flags_result(f,start+42+down,14,v);flags_scalar(f,start+43+down,1,(float)r[1].s[3]);
        for(int k=0;k<3;k++)color[k]=ps2_fmul((float)r[1].s[k],color[k]);
        flags_result(f,start+47+down,14,color);
        if(f->cockpit)flags_scalar(f,start+52+down,2,ps2_fmul(ps2_fmul(p[2],q),16));
        flags_scalar(f,start+53+down,1,0);
        u32 extra=0;
        if(c[23].u[1]&1) {
            flags_result(f,start+60+down,12,xy[j]);
            if(j<2)extra=6;
            else {
                float a[4],b[4],acc[4],cross[4];
                for(int k=0;k<4;k++) {a[k]=ps2_fsub(xy[j-2][k],xy[j-1][k]);b[k]=ps2_fsub(xy[j][k],xy[j-1][k]);}
                flags_result(f,start+64+down,15,a);flags_result(f,start+65+down,15,b);
                for(int k=0;k<3;k++) {
                    int y=(k+1)%3,z=(k+2)%3;
                    acc[k]=winding?ps2_fmul(b[y],a[z]):ps2_fmul(a[y],b[z]);
                    cross[k]=ps2_fsub(acc[k],winding?ps2_fmul(a[y],b[z]):ps2_fmul(b[y],a[z]));
                }
                /* OPMULA changes ACC without writing flags. */
                flags_result(f,start+69+down,14,cross);
                int suppressed=(fog_adc[j]&0x8000)!=0;
                if(suppressed)flags_scalar(f,start+76+down+(winding&&!near_first),1,ps2_fadd(clamp(fog,0,255),2048));
                extra=18+(winding&&!near_first)+suppressed;if(!near_first)winding^=1;
            }
        }
        start+=77+((c[23].u[1]&2)?0:1)+down+extra;
    }
    flags_advance(f,start+2+9+(parity&1));
}
static int model_visible_vectors(ps2_vu *v,const ps2_vf memory[1024],const ps2_vf c[32],const ps2_vf records[][4],u32 count,u32 parity,u16 near_first,u32 input_start,const ps2_vf packet[144]) {
    const ps2_vf *r=records[count-1];
    float normal[4],n[4],light[4];
    for(int k=0;k<3;k++)normal[k]=(float)r[3].s[k]/4096.0f;
    normal[3]=0;
    transform(c+8,normal,n);
    for(int k=0;k<4;k++) {
        for(int j=0;j<4;j++)v->vf[12+j].f[k]=fmaxf(ps2_fmul(c[12+j].f[k],n[j]),0);
        light[k]=ps2_fadd(ps2_fadd(ps2_fadd(v->vf[12].f[k],v->vf[13].f[k]),v->vf[14].f[k]),v->vf[15].f[k]);
    }
    clip_accumulator(c+16,light,&v->acc);
    v->vf[1]=packet[2];v->vf[2]=packet[5+3*count];v->vf[10]=c[30];v->vf[16]=c[20];
    u32 last=3+3*(count-1),second=3+3*count;
    v->vf[11]=packet[second+last+1];v->vf[18]=packet[second+last];v->vf[19]=packet[last];v->vf[21]=packet[last+2];
    ps2_vf constants[32];memcpy(constants,c,sizeof constants);constants[23].u[1]|=2;
    rn_ac4_model_vertex lit;if(!rn_ac4_model_eval(constants,r,&lit))return 0;
    memcpy(v->vf[20].s,lit.rgba,16);
    for(int k=0;k<4;k++)v->vf[17].s[k]=ps2_cvt_w_s((float)r[1].s[k]);
    v->vf[15]=(ps2_vf){0};v->vf[15].f[3]=1;
    if(c[23].u[1]&1) {
        ps2_vf projected[3]={0};
        for(int j=0;j<3;j++) {
            float p[4];transform(c,records[count-3+j][2].f,p);float q=1.0f/p[3];
            projected[j].f[0]=ps2_fadd(0,ps2_fmul(p[0],q));projected[j].f[1]=ps2_fadd(0,ps2_fmul(p[1],q));projected[j].f[3]=1;
        }
        v->vf[14]=projected[1];v->vf[15]=projected[2];
        for(int k=0;k<4;k++) {v->vf[12].f[k]=ps2_fsub(projected[0].f[k],projected[1].f[k]);v->vf[13].f[k]=ps2_fsub(projected[2].f[k],projected[1].f[k]);}
        int winding=(c[23].u[2]&1)^((c[29].u[0]&65535)!=0)^(parity&1);
        if(!near_first)winding^=(count-3)&1;
        for(int k=0;k<3;k++) {
            int a=(k+1)%3,b=(k+2)%3;
            v->acc.f[k]=winding?ps2_fmul(v->vf[13].f[a],v->vf[12].f[b]):ps2_fmul(v->vf[12].f[a],v->vf[13].f[b]);
        }
    }
    memcpy(v->vf+22,c+8,4*sizeof(ps2_vf));
    for(int j=0;j<4;j++)v->vf[26+j]=memory[(input_start+4*count+j)&1023u];
    v->vf[29].s[3]=0;
    for(int k=0;k<4;k++) {v->vf[26].f[k]=(float)v->vf[26].s[k]/4096.0f;v->vf[27].f[k]=(float)v->vf[27].s[k];v->vf[29].f[k]=(float)v->vf[29].s[k]/4096.0f;}
    return 1;
}
static int lit_visible_vectors(const ps2_vf c[32],const ps2_vf records[][4],u32 count,u32 parity,u16 near_first,u32 input_start,const ps2_vf packet[144]) {
    return model_visible_vectors(&lit_memory.vectors,lit_memory.memory,c,records,count,parity,near_first,input_start,packet);
}
/* Mutate only registers written by the lit program. Culled triangles preserve
 * vi5/7/8 from the preceding triangle (or inherited activation state). */
static void lit_kick_integers(const ps2_vf c[32],u32 top,u32 input_count,u32 parity,
                              u32 count,u32 input_start,u16 near_first,u16 near_second,
                              u32 common,u16 out[16]) {
    int culled=common!=0;
    out[1]=2;
    if(culled) {
        /* The following plane's FCOR occupies the taken branch delay slot.
         * The sixth/final plane has no following FCOR and retains one. */
        u32 plane=0;while(plane<5 && !(common&(1u<<plane)))plane++;
        out[1]=(u16)(plane==5 || (common&(1u<<(plane+1)))!=0);
    }
    out[2]=(u16)top;
    out[3]=(u16)(input_start+(culled?0:4*(count+1)));
    out[4]=(u16)(top+126+104*(parity&1));
    out[6]=culled?3:0;out[9]=(u16)parity;out[10]=near_first;
    out[11]=culled?near_second:1;out[12]=(u16)input_count;out[13]=(u16)(parity+3);
    if(!culled) {
        out[5]=(u16)(out[4]+6*count+6);
        int winding=(c[23].u[2]&1)^((c[29].u[0]&65535)!=0)^(parity&1);
        if((c[23].u[1]&1) && !near_first)winding^=(count-2)&1;
        out[8]=(u16)winding;out[7]=(u16)((c[23].u[1]&1)?2-count:2);
    }
}
static void lit_memory_end(const ps2_vu *v,u32 cycles) {
    if(!lit_memory.active || lit_memory.owner!=v)return;
    lit_memory.active=0;lit_memory_stats.ended++;
    if(lit_memory.cycles_valid && lit_memory.flags_valid) {
        flags_advance(&lit_memory.flags,lit_memory.cycles+2);lit_flag_stats.ended++;
        const ps2_vu *f=&lit_memory.flags.state;
        if(f->mac!=v->mac || f->status!=v->status || ((lit_memory.cycles+1)&3)!=v->fslot || memcmp(f->fpipe,v->fpipe,sizeof v->fpipe)) {
            if(lit_flag_stats.bad++<3) {
                ps2_log("ac4 lit flags mismatch: mac %x/%x status %x/%x slot %u/%u",f->mac,v->mac,f->status,v->status,(lit_memory.cycles+1)&3,v->fslot);
                for(int k=0;k<4;k++)ps2_log("ac4 lit flags slot%d: %x/%x %x/%x %d/%d",k,f->fpipe[k].mac,v->fpipe[k].mac,f->fpipe[k].status,v->fpipe[k].status,f->fpipe[k].valid,v->fpipe[k].valid);
            }
        }
    }
    if(lit_memory.cycles_valid) {
        lit_cycle_stats.ended++;lit_cycle_stats.total+=lit_memory.cycles+2;if(lit_memory.cycles+2!=cycles && lit_cycle_stats.bad++<5)ps2_log("ac4 lit cycles mismatch: %u/%u split%d count%u",lit_memory.cycles+2,cycles,sample.split,sample.count);
    } else lit_cycle_stats.declined++;
    if(!lit_memory.cycles_valid || !lit_memory.flags_valid)lit_flag_stats.declined++;
    if(lit_memory.vectors_valid) {lit_vector_stats.ended++;lit_vector_stats.end_bad+=!lit_vectors_match(v);}
    if(lit_memory.scalar.valid) {lit_scalar_stats.ended++;lit_scalar_stats.end_bad+=!lit_scalar_matches(v);}
    else lit_scalar_stats.declined++;
    if(lit_memory.integers_valid) {
        lit_integer_stats.ended++;lit_memory.vi[9]++;
        if(memcmp(lit_memory.vi,v->vi,sizeof lit_memory.vi) || v->tpc!=0x10c0 || v->running)lit_integer_stats.end_bad++;
    }
    if(lit_memory.next!=(sample.split?sample.count-2:1))lit_memory_stats.sequence_bad++;
    if(memcmp(lit_memory.memory,v->mem,sizeof lit_memory.memory)) {
        if(lit_memory_stats.memory_bad++<3)for(u32 i=0;i<1024;i++) {
            ps2_vf actual=read_qw(v,i);
            if(memcmp(&actual,lit_memory.memory+i,16))ps2_log("ac4 lit memory difference: qw%u expected %08x/%08x/%08x/%08x actual %08x/%08x/%08x/%08x",i,lit_memory.memory[i].u[0],lit_memory.memory[i].u[1],lit_memory.memory[i].u[2],lit_memory.memory[i].u[3],actual.u[0],actual.u[1],actual.u[2],actual.u[3]);
        }
    }
}
/* Winding scratch retains the final two projected XY values; disabled winding
 * and empty polygons leave both records untouched. */
static int lit_winding_scratch(const ps2_vf c[32],const ps2_vf records[][4],u32 n,ps2_vf out[2]) {
    if(!(c[23].u[1]&1u))return 1;
    if(n<3 || n>23)return 0;
    ps2_vf result[2]={0};
    for(u32 j=0;j<2;j++) {
        for(int k=0;k<4;k++)if(!isfinite(records[n-2+j][2].f[k]))return 0;
        float p[4];transform(c,records[n-2+j][2].f,p);
        if(!isfinite(p[3]) || p[3]<=0)return 0;
        float q=1.0f/p[3];
        for(int k=0;k<2;k++) {
            result[j].f[k]=ps2_fadd(ps2_fmul(p[k],q),0);
            if(!isfinite(result[j].f[k]))return 0;
        }
        result[j].f[3]=1;
    }
    memcpy(out,result,sizeof result);return 1;
}
static void lit_memory_kick(const ps2_vu *v,u32 address) {
    if(!lit_memory.active || (v && lit_memory.owner!=v))return;
    u32 first=sample.split?lit_memory.next:0,count=sample.split?3:sample.count;
    if(first+count>sample.count || (!sample.split && lit_memory.next)) {lit_memory_stats.sequence_bad++;return;}
    u32 parity=sample.split?first:count-3,expected=(sample.top+126+104*(parity&1u))&1023u;
    if((address&1023u)!=expected || (v && sample.split && v->vi[9]!=first))lit_memory_stats.sequence_bad++;
    lit_memory.next++;
    ps2_vf packet[144],scratch[80];u8 mask[80];u32 n=0,any=0,common=63;int near_w=0;u16 near_values[3]={0};int clipped=0;u32 polygon_cycles=0;
    const ps2_vf (*records)[4]=sample.records+first;
    if(sample.split)for(u32 j=0;j<3;j++) {
        float cp[4];transform(sample.constants+4,records[j][2].f,cp);
        u32 bits=0;for(int k=0;k<3;k++) {if(cp[k]>fabsf(cp[3]))bits|=1u<<(k*2);if(cp[k]<-fabsf(cp[3]))bits|=2u<<(k*2);}
        any|=bits;common&=bits;near_w|=cp[3]<=8192;
        near_values[j]=cp[3]<8192?16:(cp[3]==8192?1:0);
        lit_memory.scalar.clip=((lit_memory.scalar.clip<<6)|bits)&0xffffffu;
    }
    if(sample.split && common && near_w) {packet[0]=(ps2_vf){0};packet[0].u[0]=0xffff8000u;n=1;}
    else if(sample.split && any && near_w) {
        if(!lit_clip_build_state(sample.constants,records,parity,packet,&n,scratch,mask,lit_memory.vectors.vf+5,&polygon_cycles)) {lit_memory_stats.declined++;return;}
        for(u32 j=0;j<80;j++)if(mask[j])lit_memory.memory[32+j]=scratch[j];
        records=(const ps2_vf (*)[4])(scratch+40);count=n==1?0:(n-6)/6;clipped=1;
    } else if(!rn_ac4_lit_packet(sample.constants,records,count,parity,sample.split,packet,&n)) {lit_memory_stats.declined++;return;}
    if(sample.split) {
        u32 base=lit_memory.cycles+10+(first&1);float cp[4];
        flags_scalar(&lit_memory.flags,base+5,2,(float)sample.constants[29].s[2]);
        flags_scalar(&lit_memory.flags,base+9,2,ps2_fmul((float)sample.constants[29].s[2],64));
        for(int j=0;j<3;j++)flags_transform(&lit_memory.flags,base+10+j*4,sample.constants+4,sample.records[first+j][2].f,cp);
        for(int j=0;j<3;j++) {transform(sample.constants+4,sample.records[first+j][2].f,cp);flags_scalar(&lit_memory.flags,base+23+j,1,ps2_fadd(cp[3],-8192));}
        u32 which=0;while(which<3 && !near_values[which])which++;
        lit_memory.cycles+=10+(first&1)+31+(which<3?2*(which+1)+3:8);
        if(clipped) {
            flags_advance(&lit_memory.flags,lit_memory.cycles+12);
            if(!lit_clipping_flags_supported(&lit_memory.flags.state))lit_memory.flags_valid=0;
            lit_memory.cycles+=12+polygon_cycles;
        }
        else if(n==1) {u32 plane=0;while(!(common&(1u<<plane)))plane++;flags_scalar(&lit_memory.flags,lit_memory.cycles+2*(plane+1)+1,15,0);lit_memory.cycles+=2*(plane+1)+17+(first&1);}
    }
    if(n!=1)lit_visible_flags(&lit_memory.flags,lit_memory.cycles,sample.constants,records,count,parity,clipped?1:near_values[0]);
    if(n!=1)lit_memory.cycles+=lit_visible_cycles(sample.constants,records,count,parity,clipped?1:near_values[0]);
    if(clipped && n==1) {lit_memory.vectors_valid=0;lit_memory.cycles_valid=0;}
    if(lit_memory.vectors_valid) {
        ps2_vu *pred=&lit_memory.vectors;
        pred->vf[1]=sample.constants[22];pred->vf[2]=sample.constants[25];
        if(sample.split && !clipped) {
            pred->vf[21]=sample.constants[29];pred->vf[21].f[2]=ps2_fmul((float)sample.constants[29].s[2],64);
            memcpy(pred->vf+22,sample.constants+4,4*sizeof(ps2_vf));
            for(int j=0;j<3;j++)transform(sample.constants+4,records[j][2].f,pred->vf[27+j].f);
            clip_accumulator(sample.constants+4,records[2][2].f,&pred->acc);
        }
        if(n==1)pred->vf[29]=packet[0];
    }
    if(n!=1 && !lit_winding_scratch(sample.constants,records,count,lit_memory.memory)) {lit_memory_stats.declined++;return;}
    for(u32 j=0;j<n;j++)lit_memory.memory[(expected+j)&1023u]=packet[j];
    if(lit_memory.vectors_valid && n!=1 && !lit_visible_vectors(sample.constants,records,count,parity,clipped?1:near_values[0],clipped?72:sample.top+32+4*first,packet))lit_memory.vectors_valid=0;
    if(lit_memory.vectors_valid) {
        lit_vector_stats.packets++;
        if(v && !lit_vectors_match(v)) {
            if(lit_vector_stats.bad++<3) {
                for(int j=0;j<32;j++)for(int k=0;k<4;k++)if(lit_memory.vectors.vf[j].u[k]!=v->vf[j].u[k])ps2_log("ac4 lit vector: vf%d.%d %08x/%08x",j,k,lit_memory.vectors.vf[j].u[k],v->vf[j].u[k]);
                for(int k=0;k<4;k++)if(lit_memory.vectors.acc.u[k]!=v->acc.u[k])ps2_log("ac4 lit vector: acc%d %08x/%08x",k,lit_memory.vectors.acc.u[k],v->acc.u[k]);
            }
        }
    } else lit_vector_stats.skipped++;
    int bad=0;if(v)for(u32 j=0;j<n;j++) {ps2_vf actual=read_qw(v,address+j);bad|=memcmp(packet+j,&actual,16)!=0;}
    lit_memory_stats.packet_bad+=bad;lit_memory_stats.packets++;
    if(clipped && n==1) {lit_integer_stats.declined++;lit_memory.integers_valid=0;lit_memory.scalar.valid=0;}
    if(n!=1) {
        float projected[4];transform(sample.constants,records[count-1][2].f,projected);
        clip_scalar_divide(&lit_memory.scalar,1.0f/projected[3],model_qlat);
        lit_memory.i=(sample.constants[23].u[1]&1)?2048.0f:255.0f;
    } else if(!clipped)lit_memory.i=8192.0f;
    if(lit_memory.scalar.valid) {
        lit_scalar_stats.packets++;
        if(v && !lit_scalar_matches(v)) {
            if(lit_scalar_stats.bad++<3)ps2_log("ac4 lit scalar: Q %g/%g pending %g/%g I %g/%g clip %x/%x",lit_memory.scalar.q,v->q,lit_memory.scalar.q_pend,v->q_pend,lit_memory.i,v->i,lit_memory.scalar.clip,v->clip);
        }
    }
    if(lit_memory.integers_valid) {
        lit_kick_integers(sample.constants,sample.top,sample.count,parity,count,
            clipped?72:sample.top+32+4*first,clipped?1:near_values[0],near_values[1],n==1?common:0,lit_memory.vi);
        lit_integer_stats.packets++;
        if(v && memcmp(lit_memory.vi,v->vi,sizeof lit_memory.vi)) {
            if(lit_integer_stats.bad++<3)for(int k=0;k<16;k++)if(lit_memory.vi[k]!=v->vi[k])ps2_log("ac4 lit integer: vi%d %04x/%04x clipped%d count%u",k,lit_memory.vi[k],v->vi[k],clipped,count);
        }
    }
    if(!v) {
        lit_submission *p=lit_submissions+(lit_memory.next-1);
        p->address=expected;p->n=n;memcpy(p->packet,packet,n*sizeof(ps2_vf));
        if(clipped) {memcpy(p->scratch,scratch,sizeof scratch);memcpy(p->mask,mask,sizeof mask);}
        p->writes_winding=n!=1 && (sample.constants[23].u[1]&1);
        if(p->writes_winding)memcpy(p->winding,lit_memory.memory,sizeof p->winding);
        p->vi9=lit_memory.vi[9];p->vi10=lit_memory.vi[10];
    }
}
static int lit_try_native(ps2_vu *v,u32 limit,void (*kick)(ps2_vu *,u32)) {
    if(!sample.active || !sample.lit || sample.owner!=v || !lit_memory.active)return 0;
    if(sample.top<117 || sample.top+374>1024)return 0;
    for(int j=0;j<21;j++)for(int k=0;k<4;k++)if(!isfinite(sample.constants[j].f[k]))return 0;
    for(u32 j=0;j<sample.count;j++)for(int k=0;k<4;k++)if(!isfinite(sample.records[j][2].f[k]))return 0;
    memset(lit_submissions,0,sizeof lit_submissions);
    u32 packets=sample.split?sample.count-2:1;
    for(u32 j=0;j<packets;j++) {
        u32 parity=sample.split?j:sample.count-3;
        lit_memory_kick(NULL,sample.top+126+104*(parity&1));
        if(!lit_submissions[j].n || !lit_memory.integers_valid || !lit_memory.vectors_valid
           || !lit_memory.scalar.valid || !lit_memory.cycles_valid || !lit_memory.flags_valid)return 0;
    }
    if(lit_memory.cycles+2>limit)return 0;
    flags_advance(&lit_memory.flags,lit_memory.cycles+2);
    ps2_vu final=lit_memory.vectors;
    memcpy(final.vi,lit_memory.vi,sizeof final.vi);final.vi[9]++;
    final.q=lit_memory.scalar.q;final.p=lit_memory.scalar.p;
    final.q_pend=lit_memory.scalar.q_pend;final.p_pend=lit_memory.scalar.p_pend;
    final.q_ready=final.p_ready=0;final.clip=lit_memory.scalar.clip;final.i=lit_memory.i;final.r=lit_memory.r;
    final.mac=lit_memory.flags.state.mac;final.status=lit_memory.flags.state.status;
    memcpy(final.fpipe,lit_memory.flags.state.fpipe,sizeof final.fpipe);final.fslot=(lit_memory.cycles+1)&3;
    final.tpc=0x10c0;final.running=0;
    /* Nothing before this point mutates guest state or calls GIF/GS. Preserve
     * packet order and original-format fallback side effects during commit. */
    for(u32 j=0;j<packets;j++) {
        const lit_submission *p=lit_submissions+j;
        for(u32 k=0;k<80;k++)if(p->mask[k])memcpy(v->mem+(32+k)*16,p->scratch+k,16);
        if(p->writes_winding)memcpy(v->mem,p->winding,sizeof p->winding);
        memcpy(v->mem+p->address*16,p->packet,p->n*16);
        v->vi[9]=p->vi9;v->vi[10]=p->vi10;kick(v,p->address);
    }
    *v=final;lit_memory.active=0;
    lit_native_runs++;lit_native_packets+=packets;lit_native_cycles+=lit_memory.cycles+2;
    return 1;
}

/* Strip triangles reuse input vertices. These checks depend only on the saved
 * inputs/constants, never on the mutable VU packet or current GS state. Cache
 * positive AND negative results for this activation only; audit stays uncached. */
static int vertex_safe(u32 i,int projection) {
    u8 *cached=projection?&sample.projection_ok[i]:&sample.clip_ok[i];
    if(!rn_ac4_model_audit_on && *cached) {stats.safety_reused++;return *cached==1;}
    stats.safety_checks++;
    float p[4];transform(sample.constants+(projection?0:4),sample.records[i][2].f,p);
    int valid=isfinite(p[3]) && p[3]>0;
    if(valid && projection) {
        float q=1.0f/p[3];
        for(int k=0;k<3;k++) {
            float x=ps2_fmul(ps2_fmul(p[k],q),16.0f);
            valid &= isfinite(x) && x>-2147483648.0f && x<2147483648.0f;
        }
    } else if(valid) {
        for(int k=0;k<3;k++) valid &= isfinite(p[k]) && fabsf(p[k])<=p[3];
    }
    if(!rn_ac4_model_audit_on) *cached=valid?1:2;
    return valid;
}

void rn_ac4_model_init(void) {
#if defined(__SSE2__)
    const char *regular=getenv("PS2_AC4_TRANSFORM_CONSERVATIVE");
    regular_transform_enabled=!(regular && atoi(regular)!=0);
    const char *register_flags=getenv("PS2_AC4_FLAGS_REGISTER_PATH");
    register_flags_enabled=register_flags && atoi(register_flags)!=0;
#endif
    const char *e=getenv("PS2_VU_QLAT");
    model_qlat=(e && *e)?strtol(e,NULL,0)!=0:1;
    e=getenv("PS2_AC4_MODEL_AUDIT");
    rn_ac4_model_audit_on=e && !strcmp(e,"1");
    e=getenv("PS2_AC4_MODEL_MESH");
    rn_ac4_model_mesh_on=e && !strcmp(e,"1");
    e=getenv("PS2_AC4_MODEL_MATERIAL");
    rn_ac4_model_material_on=e && !strcmp(e,"1");
    e=getenv("PS2_AC4_MODEL_TEXTURE");
    rn_ac4_model_texture_on=e && !strcmp(e,"1");
    rn_ac4_model_material_on |= rn_ac4_model_texture_on;
    rn_ac4_model_mesh_on |= rn_ac4_model_material_on;
    e=getenv("PS2_AC4_MODEL_PLAN");
    rn_ac4_model_plan_on=e && !strcmp(e,"1");
    e=getenv("PS2_AC4_MODEL_NATIVE");
    rn_ac4_model_native_on=e && !strcmp(e,"1");model_native_requested=rn_ac4_model_native_on;
    e=getenv("PS2_AC4_TERRAIN_NATIVE");terrain_native_on=e && !strcmp(e,"1");
    rn_ac4_model_native_on |= terrain_native_on;
    e=getenv("PS2_AC4_LIT_NATIVE");lit_native_on=e && !strcmp(e,"1");
    e=getenv("PS2_AC4_COCKPIT_NATIVE");cockpit_native_on=e && !strcmp(e,"1");
    rn_ac4_model_native_on |= lit_native_on || cockpit_native_on;
    rn_ac4_model_mesh_on |= rn_ac4_model_native_on;
    e=getenv("PS2_AC4_TERRAIN_MESH");rn_ac4_terrain_mesh_on=terrain_native_on || (e && !strcmp(e,"1"));
    rn_ac4_model_mesh_on |= rn_ac4_terrain_mesh_on;
    rn_ac4_model_capture_on = rn_ac4_model_audit_on || rn_ac4_model_mesh_on || rn_ac4_model_plan_on;
}
static ps2_vf read_qw(const ps2_vu *v,u32 addr) {
    ps2_vf r; memcpy(&r,v->mem+(addr&1023u)*16u,16);return r;
}
/* The cockpit has distinct clipping/projection instructions. Keep its observer
 * entirely separate from the supported lit program and native execution state. */
static int cockpit_audit_on=-1,cockpit_mesh_on;
static lit_submission cockpit_submissions[21];
static u64 cockpit_native_runs,cockpit_native_packets,cockpit_native_cycles;
static struct {const ps2_vu *owner;int active,split;u32 count,top;ps2_vf c[32],r[23][4],scratch[80];} cockpit;
static struct {u64 activations,packets,vertices,exact,xyz,fog,uv,color,skipped,rejected;} cockpit_stats;
static struct {u64 vertices,exact,bad,headers,rejected;} cockpit_second_stats;
static struct {u64 packets,exact,bad,scratch_bad,declined,empty;} cockpit_clip_stats;
static struct {const ps2_vu *owner;int active;u32 next;ps2_vf memory[1024];u16 vi[16];unlit_scalar_state scalar;float i;u32 r;ps2_vu vectors;int vectors_valid;u32 cycles;int cycles_valid;unlit_flags flags;} cockpit_memory;
static struct {u64 begun,ended,packets,kick_bad,end_bad,sequence_bad,declined;} cockpit_memory_stats;
static struct {u64 packets,kick_bad,ended,end_bad,declined;} cockpit_integer_stats;
static struct {u64 packets,kick_bad,ended,end_bad,declined;} cockpit_scalar_stats;
static struct {u64 packets,kick_bad,ended,end_bad,declined;} cockpit_vector_stats;
static struct {u64 ended,bad,declined;} cockpit_cycle_stats;
static struct {u64 ended,bad;} cockpit_flag_stats;
static void cockpit_cycle_end(const ps2_vu *v,u32 cycles) {
    if(!cockpit_memory.active || cockpit_memory.owner!=v)return;
    if(!cockpit_memory.cycles_valid) {cockpit_cycle_stats.declined++;return;}
    cockpit_cycle_stats.ended++;
    flags_advance(&cockpit_memory.flags,cockpit_memory.cycles+2);
    const ps2_vu *f=&cockpit_memory.flags.state;cockpit_flag_stats.ended++;
    if(f->mac!=v->mac || f->status!=v->status || ((cockpit_memory.cycles+1)&3)!=v->fslot || memcmp(f->fpipe,v->fpipe,sizeof v->fpipe)) {
        if(cockpit_flag_stats.bad++<4) {
            ps2_log("ac4 cockpit flag difference: mac %x/%x status %x/%x slot %u/%u",f->mac,v->mac,f->status,v->status,(cockpit_memory.cycles+1)&3,v->fslot);
            for(int k=0;k<4;k++)ps2_log("ac4 cockpit flag slot%d: %x/%x %x/%x %d/%d",k,f->fpipe[k].mac,v->fpipe[k].mac,f->fpipe[k].status,v->fpipe[k].status,f->fpipe[k].valid,v->fpipe[k].valid);
        }
    }
    if(cockpit_memory.cycles+2!=cycles && cockpit_cycle_stats.bad++<8)
        ps2_log("ac4 cockpit cycle difference: %u/%u split%d count%u",cockpit_memory.cycles+2,cycles,cockpit.split,cockpit.count);
}
static int cockpit_vectors_match(const ps2_vu *v) {
    return !memcmp(cockpit_memory.vectors.vf,v->vf,sizeof v->vf)
        && !memcmp(&cockpit_memory.vectors.acc,&v->acc,sizeof v->acc);
}
static int cockpit_scalar_matches(const ps2_vu *v) {
    return clip_scalar_matches(&cockpit_memory.scalar,v)
        && !memcmp(&cockpit_memory.i,&v->i,4) && cockpit_memory.r==v->r;
}
static int cockpit_eval(const ps2_vf c[32],const ps2_vf r[4],rn_ac4_model_vertex *out) {
    rn_ac4_model_vertex result;
    if(!rn_ac4_model_eval(c,r,&result))return 0;
    float p[4];transform(c,r[2].f,p);
    /* Cockpit MULi.z by 16 precedes its common FTOI4 conversion. */
    float z=ps2_fmul(ps2_fmul(ps2_fmul(p[2],1.0f/p[3]),16.0f),16.0f);
    if(!isfinite(z) || z<=-2147483648.0f || z>=2147483648.0f)return 0;
    result.xyzf[2]=ps2_cvt_w_s(z);*out=result;return 1;
}
static int cockpit_secondary_eval(const ps2_vf c[32],const ps2_vf r[4],rn_ac4_model_vertex *out) {
    rn_ac4_model_vertex primary,secondary;
    if(!cockpit_eval(c,r,&primary) || !rn_ac4_lit_secondary(c,r,&secondary))return 0;
    secondary.xyzf[2]=primary.xyzf[2];*out=secondary;return 1;
}
static int cockpit_clip_build(const ps2_vf c[32],const ps2_vf records[3][4],u32 parity,
                             ps2_vf output[144],u32 *qwords,ps2_vf scratch_out[80],u8 mask_out[80]) {
    ps2_vf scratch[80],unused[70],packet[144];u8 mask[80];u32 n=0;unlit_clip_control control;
    u32 common=63;
    for(u32 i=0;i<3;i++) {
        float p[4];transform(c+4,records[i][2].f,p);u32 bits=0;
        for(int k=0;k<4;k++)if(!isfinite(p[k]))return 0;
        for(int k=0;k<3;k++) {if(p[k]>fabsf(p[3]))bits|=1u<<(2*k);if(p[k]<-fabsf(p[3]))bits|=2u<<(2*k);}
        common&=bits;
    }
    if(common) {
        memset(output,0,sizeof *output);output[0].u[0]=0xffff8000u;*qwords=1;
        memset(scratch_out,0,80*sizeof *scratch_out);memset(mask_out,0,80);return 1;
    }
    if(!unlit_clip_build(c,records,unused,&n,scratch,mask,&control,NULL,NULL,NULL,0,NULL,NULL))return 0;
    if(control.count<3) {packet[0]=unused[0];n=1;}
    else {
        ps2_vf constants[32],quantized[8][4];memcpy(constants,c,sizeof constants);
        constants[22]=c[21];constants[25]=c[31];
        for(u32 i=0;i<control.count;i++)memcpy(quantized[i],scratch+40+4*i,4*sizeof(ps2_vf));
        if(!rn_ac4_lit_packet(constants,quantized,control.count,parity,2,packet,&n))return 0;
        for(u32 i=0;i<control.count;i++) {
            rn_ac4_model_vertex vertex;if(!cockpit_eval(constants,quantized[i],&vertex))return 0;
            packet[5+3*i].s[2]=packet[8+3*control.count+3*i].s[2]=vertex.xyzf[2];
        }
    }
    memcpy(output,packet,n*sizeof *output);*qwords=n;
    memcpy(scratch_out,scratch,sizeof scratch);memcpy(mask_out,mask,sizeof mask);return 1;
}
static int cockpit_clip_topology(const ps2_vf packet[144],u32 count,terrain_triangle output[12],u32 *triangles) {
    if(count<3 || count>8)return 0;
    terrain_triangle result[12];u32 used=0;
    for(u32 pass=0;pass<2;pass++) {
        u32 start=pass?5+3*count:2;const ps2_vf *tag=packet+start;
        u32 prim=(tag->u[1]>>15)&2047,kind=prim&7;
        if((tag->u[0]&32767)!=count || !(tag->u[1]&(1u<<14))
           || ((tag->u[1]>>26)&3)!=0 || (tag->u[1]>>28)!=3
           || (tag->u[2]&0xfff)!=0x412 || !(prim&8) || (prim&256)
           || (kind!=4 && kind!=5))return 0;
        for(u32 i=2;i<count;i++) {
            if(packet[start+3+3*i].u[3]&0x8000)continue;
            result[used++]=(terrain_triangle){(u8)pass,{(u8)(kind==5?0:i-2),(u8)(i-1),(u8)i}};
        }
        if(tag->u[0]&32768)break;
    }
    memcpy(output,result,used*sizeof *output);*triangles=used;return 1;
}
static void cockpit_clip_observe(const ps2_vu *v,u32 address,u32 first) {
    ps2_vf packet[144],scratch[80];u8 mask[80];u32 n=0;
    if(!cockpit.split || first+3>cockpit.count)return;
    if(!cockpit_clip_build(cockpit.c,cockpit.r+first,first,packet,&n,scratch,mask)) {
        cockpit_clip_stats.declined++;return;
    }
    int bad=0,scratch_bad=0;
    for(u32 i=0;i<n;i++) {
        ps2_vf actual=read_qw(v,address+i);
        if(memcmp(packet+i,&actual,16)) {
            bad=1;
            if(cockpit_clip_stats.bad<2)for(int k=0;k<4;k++)if(packet[i].u[k]!=actual.u[k])
                ps2_log("ac4 cockpit clip difference: qw%u.%d %08x/%08x",i,k,packet[i].u[k],actual.u[k]);
        }
    }
    for(u32 i=0;i<80;i++) {
        ps2_vf actual=read_qw(v,32+i);
        scratch_bad|=memcmp(mask[i]?scratch+i:cockpit.scratch+i,&actual,16)!=0;
    }
    cockpit_clip_stats.packets++;cockpit_clip_stats.empty+=n==1;
    cockpit_clip_stats.bad+=bad;cockpit_clip_stats.scratch_bad+=scratch_bad;
    cockpit_clip_stats.exact+=!bad&&!scratch_bad;
    if(cockpit_mesh_on && !bad && !scratch_bad && n>1) {
        u32 count=packet[2].u[0]&32767,triangles=0;terrain_triangle topology[12];
        if(!cockpit_clip_topology(packet,count,topology,&triangles))return;
        for(u32 t=0;t<triangles;t++) {
            ps2_vf records[3][4];rn_ac4_model_vertex expected[3];
            u32 pass=topology[t].pass,start=pass?6+3*count:3;
            for(u32 j=0;j<3;j++) {
                u32 index=topology[t].index[j],at=start+3*index;
                memcpy(records[j],scratch+40+4*index,sizeof records[j]);
                memcpy(expected[j].stq,packet[at].f,16);memcpy(expected[j].rgba,packet[at+1].s,16);
                memcpy(expected[j].xyzf,packet[at+2].s,16);
            }
            rn_ac4_mesh_cockpit_append(cockpit.c,records,expected,pass);
        }
    }
}
static void cockpit_memory_kick(const ps2_vu *v,u32 address) {
    if(!cockpit_memory.active || (v && cockpit_memory.owner!=v))return;
    ps2_vf initial_scratch[85];memcpy(initial_scratch,cockpit_memory.memory+32,sizeof initial_scratch);
    u32 first=cockpit.split?cockpit_memory.next:0,count=cockpit.split?3:cockpit.count;
    if(first+count>cockpit.count || (!cockpit.split && cockpit_memory.next)) {
        cockpit_memory_stats.sequence_bad++;return;
    }
    u32 parity=cockpit.split?first:count-3,expected=(cockpit.top+126+104*(parity&1u))&1023u;
    if((address&1023u)!=expected || (v && cockpit.split && v->vi[9]!=first))cockpit_memory_stats.sequence_bad++;
    cockpit_memory.next++;
    ps2_vf packet[144],scratch[80];u8 mask[80];u32 n=0,any=0,common=63;int near_w=0,clipped=0;
    u16 near_values[3]={0};
    const ps2_vf (*records)[4]=cockpit.r+first;
    if(cockpit.split)for(u32 i=0;i<3;i++) {
        float p[4];transform(cockpit.c+4,records[i][2].f,p);u32 bits=0;
        for(int k=0;k<4;k++)if(!isfinite(p[k])) {cockpit_memory_stats.declined++;cockpit_memory.active=0;return;}
        for(int k=0;k<3;k++) {if(p[k]>fabsf(p[3]))bits|=1u<<(2*k);if(p[k]<-fabsf(p[3]))bits|=2u<<(2*k);}
        any|=bits;common&=bits;near_w|=p[3]<=8192;
        cockpit_memory.scalar.clip=((cockpit_memory.scalar.clip<<6)|bits)&0xffffffu;
        near_values[i]=p[3]<8192?16:(p[3]==8192?1:0);
    }
    if(cockpit.split && common && near_w) {packet[0]=(ps2_vf){0};packet[0].u[0]=0xffff8000u;n=1;}
    else if(cockpit.split && any && near_w) {
        clipped=1;
        if(!cockpit_clip_build(cockpit.c,records,parity,packet,&n,scratch,mask))goto declined;
        for(u32 i=0;i<80;i++)if(mask[i])cockpit_memory.memory[32+i]=scratch[i];
        records=(const ps2_vf (*)[4])(scratch+40);count=n==1?0:(n-6)/6;
    } else {
        if(!rn_ac4_lit_packet(cockpit.c,records,count,parity,cockpit.split,packet,&n))goto declined;
        for(u32 i=0;i<count;i++) {
            rn_ac4_model_vertex projected;if(!cockpit_eval(cockpit.c,records[i],&projected))goto declined;
            packet[5+3*i].s[2]=packet[8+3*count+3*i].s[2]=projected.xyzf[2];
        }
    }
    if(n!=1 && !lit_winding_scratch(cockpit.c,records,count,cockpit_memory.memory))goto declined;
    if(cockpit.split) {
        u32 base=cockpit_memory.cycles+10+(first&1);float cp[4];
        flags_scalar(&cockpit_memory.flags,base+5,2,(float)cockpit.c[29].s[2]);
        flags_scalar(&cockpit_memory.flags,base+9,2,ps2_fmul((float)cockpit.c[29].s[2],64));
        for(int j=0;j<3;j++)flags_transform(&cockpit_memory.flags,base+10+j*4,cockpit.c+4,cockpit.r[first+j][2].f,cp);
        for(int j=0;j<3;j++) {transform(cockpit.c+4,cockpit.r[first+j][2].f,cp);flags_scalar(&cockpit_memory.flags,base+23+j,1,ps2_fadd(cp[3],-8192));}
        u32 which=0;while(which<3 && !near_values[which])which++;
        cockpit_memory.cycles+=10+(first&1)+31+(which<3?2*(which+1)+3:8);
        if(clipped) {
            ps2_vf unused[70];u32 unused_n,polygon_cycles;unlit_clip_trace trace;
            flags_advance(&cockpit_memory.flags,cockpit_memory.cycles+12);
            if(!unlit_clip_build(cockpit.c,cockpit.r+first,unused,&unused_n,NULL,NULL,NULL,initial_scratch,&trace,&cockpit_memory.flags,0,NULL,&polygon_cycles))cockpit_memory.cycles_valid=0;
            /* The lit/cockpit clipping entry has one extra padding pair.
               Cockpit removes four bias pairs from each lit crossing path,
               whose schedule is five pairs longer than the unlit path. */
            else cockpit_memory.cycles+=12+polygon_cycles+(n==1?2+(first&1):0);
        } else if(n==1) {
            u32 plane=0;while(!(common&(1u<<plane)))plane++;
            flags_scalar(&cockpit_memory.flags,cockpit_memory.cycles+2*(plane+1)+1,15,0);
            cockpit_memory.cycles+=2*(plane+1)+17+(first&1);
        }
    }
    if(n!=1) {
        lit_visible_flags(&cockpit_memory.flags,cockpit_memory.cycles,cockpit.c,records,count,parity,clipped?1:near_values[0]);
        cockpit_memory.cycles+=lit_visible_cycles(cockpit.c,records,count,parity,clipped?1:near_values[0]);
    }
    for(u32 i=0;i<n;i++)cockpit_memory.memory[(expected+i)&1023u]=packet[i];
    ps2_vu *pred=&cockpit_memory.vectors;
    pred->vf[1]=cockpit.c[22];pred->vf[2]=cockpit.c[25];
    if(cockpit.split && !clipped) {
        pred->vf[21]=cockpit.c[29];pred->vf[21].f[2]=ps2_fmul((float)cockpit.c[29].s[2],64);
        memcpy(pred->vf+22,cockpit.c+4,4*sizeof(ps2_vf));
        for(int j=0;j<3;j++)transform(cockpit.c+4,records[j][2].f,pred->vf[27+j].f);
        clip_accumulator(cockpit.c+4,records[2][2].f,&pred->acc);
    }
    if(clipped) {
        ps2_vf unused[70];u32 unused_n;unlit_clip_trace trace;
        if(!unlit_clip_build(cockpit.c,cockpit.r+first,unused,&unused_n,NULL,NULL,NULL,initial_scratch,&trace,NULL,0,NULL,NULL))cockpit_memory.vectors_valid=0;
        else if(n==1) {
            for(int j=0;j<32;j++)if(trace.mask&(1u<<j))pred->vf[j]=trace.vf[j];
            pred->acc=trace.acc;
        } else memcpy(pred->vf+5,trace.vf+5,5*sizeof(ps2_vf));
    }
    if(n==1)pred->vf[29]=packet[0];
    else if(!model_visible_vectors(pred,cockpit_memory.memory,cockpit.c,records,count,parity,clipped?1:near_values[0],clipped?72:cockpit.top+32+4*first,packet))cockpit_memory.vectors_valid=0;
    if(cockpit_memory.vectors_valid) {
        cockpit_vector_stats.packets++;
        if(v && !cockpit_vectors_match(v) && cockpit_vector_stats.kick_bad++<3) {
            for(int j=0;j<32;j++)for(int k=0;k<4;k++)if(pred->vf[j].u[k]!=v->vf[j].u[k])ps2_log("ac4 cockpit vector difference: vf%d.%d %08x/%08x clipped%d empty%d",j,k,pred->vf[j].u[k],v->vf[j].u[k],clipped,n==1);
            for(int k=0;k<4;k++)if(pred->acc.u[k]!=v->acc.u[k])ps2_log("ac4 cockpit accumulator difference: %d %08x/%08x",k,pred->acc.u[k],v->acc.u[k]);
        }
    } else cockpit_vector_stats.declined++;
    int integers_valid=1;
    if(clipped && n==1) {
        ps2_vf unused[70];u32 unused_n;unlit_clip_control control;u16 initial[16];
        memcpy(initial,cockpit_memory.vi,sizeof initial);
        if(unlit_clip_build(cockpit.c,cockpit.r+first,unused,&unused_n,NULL,NULL,&control,NULL,NULL,NULL,0,NULL,NULL))
            clip_kick_integers(&control,cockpit.top,first,cockpit.count,initial,cockpit_memory.vi);
        else {integers_valid=0;cockpit_integer_stats.declined++;}
    } else lit_kick_integers(cockpit.c,cockpit.top,cockpit.count,parity,count,
                clipped?72:cockpit.top+32+4*first,clipped?1:near_values[0],near_values[1],n==1?common:0,cockpit_memory.vi);
    if(n!=1) {
        float projected[4];transform(cockpit.c,records[count-1][2].f,projected);
        clip_scalar_divide(&cockpit_memory.scalar,1.0f/projected[3],model_qlat);
        cockpit_memory.i=(cockpit.c[23].u[1]&1)?2048.0f:255.0f;
    } else if(!clipped)cockpit_memory.i=8192.0f;
    else {
        ps2_vf unused[70];u32 unused_n;unlit_clip_trace trace;
        if(unlit_clip_build(cockpit.c,cockpit.r+first,unused,&unused_n,NULL,NULL,NULL,NULL,&trace,NULL,0,NULL,NULL)) {
            if(trace.has_q)clip_scalar_divide(&cockpit_memory.scalar,trace.q,model_qlat);
            cockpit_memory.i=trace.i;
        } else cockpit_memory.scalar.valid=0;
    }
    if(cockpit_memory.scalar.valid) {
        cockpit_scalar_stats.packets++;
        if(v && !cockpit_scalar_matches(v) && cockpit_scalar_stats.kick_bad++<3)
            ps2_log("ac4 cockpit scalar difference: Q %g/%g pending %g/%g I %g/%g clip %x/%x",cockpit_memory.scalar.q,v->q,cockpit_memory.scalar.q_pend,v->q_pend,cockpit_memory.i,v->i,cockpit_memory.scalar.clip,v->clip);
    } else cockpit_scalar_stats.declined++;
    if(integers_valid) {
        cockpit_integer_stats.packets++;
        if(v && memcmp(cockpit_memory.vi,v->vi,sizeof cockpit_memory.vi)) {
            if(cockpit_integer_stats.kick_bad++<3)for(int i=0;i<16;i++)if(cockpit_memory.vi[i]!=v->vi[i])
                ps2_log("ac4 cockpit integer difference: vi%d %04x/%04x clipped %d count %u",i,cockpit_memory.vi[i],v->vi[i],clipped,count);
        }
    }
    cockpit_memory_stats.packets++;
    if(v && memcmp(cockpit_memory.memory,v->mem,sizeof cockpit_memory.memory)) {
        if(cockpit_memory_stats.kick_bad++<2)for(u32 i=0;i<1024;i++) {
            ps2_vf actual=read_qw(v,i);
            if(memcmp(cockpit_memory.memory+i,&actual,16))
                ps2_log("ac4 cockpit memory difference: qw%u %08x/%08x %08x/%08x %08x/%08x %08x/%08x",i,
                    cockpit_memory.memory[i].u[0],actual.u[0],cockpit_memory.memory[i].u[1],actual.u[1],
                    cockpit_memory.memory[i].u[2],actual.u[2],cockpit_memory.memory[i].u[3],actual.u[3]);
        }
    }
    if(!v) {
        if(!integers_valid)goto declined;
        lit_submission *p=cockpit_submissions+(cockpit_memory.next-1);
        p->address=expected;p->n=n;memcpy(p->packet,packet,n*sizeof(ps2_vf));
        if(clipped) {memcpy(p->scratch,scratch,sizeof scratch);memcpy(p->mask,mask,sizeof mask);}
        p->writes_winding=n!=1 && (cockpit.c[23].u[1]&1);
        if(p->writes_winding)memcpy(p->winding,cockpit_memory.memory,sizeof p->winding);
        p->vi9=cockpit_memory.vi[9];p->vi10=cockpit_memory.vi[10];
    }
    return;
declined:
    cockpit_memory_stats.declined++;cockpit_memory.active=0;
}
static int cockpit_try_native(ps2_vu *v,u32 limit,void (*kick)(ps2_vu *,u32)) {
    if(!v || !kick || !cockpit.active || cockpit.owner!=v || !cockpit_memory.active)return 0;
    if(cockpit.top<117 || cockpit.top+374>1024)return 0;
    for(int j=0;j<21;j++)for(int k=0;k<4;k++)if(!isfinite(cockpit.c[j].f[k]))return 0;
    for(u32 j=0;j<cockpit.count;j++)for(int k=0;k<4;k++)if(!isfinite(cockpit.r[j][2].f[k]))return 0;
    const typeof(cockpit_memory) initial=cockpit_memory;
    memset(cockpit_submissions,0,sizeof cockpit_submissions);
    u32 packets=cockpit.split?cockpit.count-2:1;
    for(u32 j=0;j<packets;j++) {
        u32 parity=cockpit.split?j:cockpit.count-3;
        cockpit_memory_kick(NULL,cockpit.top+126+104*(parity&1));
        if(!cockpit_memory.active || !cockpit_submissions[j].n || !cockpit_memory.vectors_valid
           || !cockpit_memory.scalar.valid || !cockpit_memory.cycles_valid)goto rejected;
    }
    if(cockpit_memory.cycles+2>limit)goto rejected;
    flags_advance(&cockpit_memory.flags,cockpit_memory.cycles+2);
    ps2_vu final=cockpit_memory.vectors;
    memcpy(final.vi,cockpit_memory.vi,sizeof final.vi);final.vi[9]++;
    final.q=cockpit_memory.scalar.q;final.p=cockpit_memory.scalar.p;
    final.q_pend=cockpit_memory.scalar.q_pend;final.p_pend=cockpit_memory.scalar.p_pend;
    final.q_ready=final.p_ready=0;final.clip=cockpit_memory.scalar.clip;
    final.i=cockpit_memory.i;final.r=cockpit_memory.r;
    final.mac=cockpit_memory.flags.state.mac;final.status=cockpit_memory.flags.state.status;
    memcpy(final.fpipe,cockpit_memory.flags.state.fpipe,sizeof final.fpipe);
    final.fslot=(cockpit_memory.cycles+1)&3;final.tpc=4224;final.running=0;
    /* All guards and packet/state predictions finish before the first write.
       The existing kick path preserves original-format GS fallback effects. */
    cockpit_memory.active=0;
    for(u32 j=0;j<packets;j++) {
        const lit_submission *p=cockpit_submissions+j;
        for(u32 k=0;k<80;k++)if(p->mask[k])memcpy(v->mem+(32+k)*16,p->scratch+k,16);
        if(p->writes_winding)memcpy(v->mem,p->winding,sizeof p->winding);
        memcpy(v->mem+p->address*16,p->packet,p->n*16);
        v->vi[9]=p->vi9;v->vi[10]=p->vi10;kick(v,p->address);
    }
    *v=final;cockpit_native_runs++;cockpit_native_packets+=packets;
    cockpit_native_cycles+=cockpit_memory.cycles+2;
    return 1;
rejected:
    cockpit_memory=initial;
    return 0;
}
static void cockpit_memory_end(const ps2_vu *v) {
    if(!cockpit_memory.active || cockpit_memory.owner!=v)return;
    cockpit_memory.active=0;cockpit_memory_stats.ended++;
    cockpit_memory_stats.sequence_bad+=cockpit_memory.next!=(cockpit.split?cockpit.count-2:1);
    cockpit_memory_stats.end_bad+=memcmp(cockpit_memory.memory,v->mem,sizeof cockpit_memory.memory)!=0;
    cockpit_memory.vi[9]++;cockpit_integer_stats.ended++;
    cockpit_integer_stats.end_bad+=memcmp(cockpit_memory.vi,v->vi,sizeof cockpit_memory.vi)!=0 || v->tpc!=4224 || v->running;
    if(cockpit_memory.scalar.valid) {cockpit_scalar_stats.ended++;cockpit_scalar_stats.end_bad+=!cockpit_scalar_matches(v);}
    if(cockpit_memory.vectors_valid) {cockpit_vector_stats.ended++;cockpit_vector_stats.end_bad+=!cockpit_vectors_match(v);}
}
static void cockpit_begin(const ps2_vu *v,u32 entry,u64 signature) {
    cockpit.active=0;cockpit.owner=NULL;
    cockpit_memory.active=0;
    if(cockpit_audit_on<0) {
        const char *e=getenv("PS2_AC4_COCKPIT_MESH");cockpit_mesh_on=cockpit_native_on||(e&&atoi(e)!=0);
        e=getenv("PS2_AC4_COCKPIT_AUDIT");cockpit_audit_on=cockpit_mesh_on||(e&&atoi(e)!=0);
    }
    if(!cockpit_audit_on || signature!=0xbaf5487804fd6eebull
       || (entry!=16 && entry!=4224) || !v || !v->mem || v->mem_size!=16384)return;
    for(u32 i=0;i<32;i++)cockpit.c[i]=read_qw(v,v->top+i);
    u32 n=cockpit.c[23].u[0]&65535u;
    if(n<3 || n>23){cockpit_stats.rejected++;return;}
    for(u32 i=0;i<n;i++)for(u32 j=0;j<4;j++)cockpit.r[i][j]=read_qw(v,v->top+32+i*4+j);
    for(u32 i=0;i<80;i++)cockpit.scratch[i]=read_qw(v,32+i);
    cockpit.owner=v;cockpit.count=n;cockpit.top=v->top;
    cockpit.split=(cockpit.c[29].u[1]&65535u)!=0;
    cockpit.active=1;cockpit_stats.activations++;
    if(rn_ac4_model_plan_on || cockpit_native_on) {
        cockpit_memory.owner=v;cockpit_memory.active=1;cockpit_memory.next=0;
        memcpy(cockpit_memory.memory,v->mem,sizeof cockpit_memory.memory);cockpit_memory_stats.begun++;
        memcpy(cockpit_memory.vi,v->vi,sizeof cockpit_memory.vi);
        cockpit_memory.scalar=clip_scalar_begin(v);cockpit_memory.i=v->i;cockpit_memory.r=v->r;
        cockpit_memory.vectors=*v;cockpit_memory.vectors_valid=1;
        cockpit_memory.cycles=(entry==4224?2:0)+15+((n-3)&1)+cockpit.split;cockpit_memory.cycles_valid=1;
        cockpit_memory.flags=(unlit_flags){*v,0,1};
    }
}
static void cockpit_kick(const ps2_vu *v,u32 address) {
    if(!cockpit.active || cockpit.owner!=v)return;
    u32 first=cockpit.split?v->vi[9]:0,count=cockpit.split?3:cockpit.count;
    u32 parity=cockpit.split?first:count-3;
    if(!cockpit.split)cockpit.active=0;
    if(first+count>cockpit.count || (address&1023u)!=((cockpit.top+126+((parity&1u)?104:0))&1023u)) {
        cockpit_stats.skipped++;return;
    }
    ps2_vf tag=read_qw(v,address+2);
    if(v->vi[10]==1 || (tag.u[0]&0x7fffu)!=count){cockpit_clip_observe(v,address,first);cockpit_stats.skipped++;return;}
    for(u32 i=first;i<first+count;i++) {
        float p[4];transform(cockpit.c+4,cockpit.r[i][2].f,p);
        int safe=isfinite(p[3]) && p[3]>0;
        for(int k=0;k<3;k++)safe &= isfinite(p[k]) && fabsf(p[k])<=p[3];
        if(!safe){cockpit_clip_observe(v,address,first);cockpit_stats.skipped++;return;}
    }
    cockpit_stats.packets++;
    rn_ac4_model_vertex verified[3];int match=count==3;
    for(u32 i=0;i<count;i++) {
        rn_ac4_model_vertex predicted;
        if(!cockpit_eval(cockpit.c,cockpit.r[first+i],&predicted)){cockpit_stats.rejected++;match=0;continue;}
        ps2_vf uv=read_qw(v,address+3+i*3),color=read_qw(v,address+4+i*3),xyz=read_qw(v,address+5+i*3);
        int bx=memcmp(xyz.s,predicted.xyzf,12)!=0;
        int bf=(xyz.u[3]&0x7fffu)!=(u32)predicted.xyzf[3];
        int bu=memcmp(uv.f,predicted.stq,12)!=0,bc=memcmp(color.s,predicted.rgba,16)!=0;
        if(bx||bf||bu||bc)match=0;
        if(count==3)verified[i]=predicted;
        cockpit_stats.vertices++;cockpit_stats.xyz+=bx;cockpit_stats.fog+=bf;
        cockpit_stats.uv+=bu;cockpit_stats.color+=bc;
        if(!(bx||bf||bu||bc))cockpit_stats.exact++;
        else if(cockpit_stats.vertices-cockpit_stats.exact<=6)
            ps2_log("ac4 cockpit difference: xyz %d/%d %d/%d %d/%d fog %u/%d uv %g/%g color %d/%d",
                    xyz.s[0],predicted.xyzf[0],xyz.s[1],predicted.xyzf[1],xyz.s[2],predicted.xyzf[2],
                    xyz.u[3]&0x7fff,predicted.xyzf[3],uv.f[0],predicted.stq[0],color.s[0],predicted.rgba[0]);
    }
    int primary_mesh=match && cockpit_mesh_on && (tag.u[1]&0x4000u)
       && ((tag.u[1]>>28)&15u)==3 && (tag.u[2]&0xfffu)==0x412u;
    if(primary_mesh)
        rn_ac4_mesh_prepare_cockpit(cockpit.c,cockpit.r+first,verified,(tag.u[1]>>15)&0x7ffu);
    u32 second_address=address+3+3*count;
    ps2_vf second_tag=read_qw(v,second_address+2);
    if((second_tag.u[0]&0x7fffu)!=count) {cockpit_second_stats.headers++;return;}
    rn_ac4_model_vertex second[3];int second_match=count==3;
    for(u32 i=0;i<count;i++) {
        rn_ac4_model_vertex predicted;
        if(!cockpit_secondary_eval(cockpit.c,cockpit.r[first+i],&predicted)) {
            cockpit_second_stats.rejected++;second_match=0;continue;
        }
        ps2_vf uv=read_qw(v,second_address+3+i*3),color=read_qw(v,second_address+4+i*3),xyz=read_qw(v,second_address+5+i*3);
        int bad=memcmp(uv.f,predicted.stq,12)||memcmp(color.s,predicted.rgba,16)
            ||memcmp(xyz.s,predicted.xyzf,12)||(xyz.u[3]&0x7fffu)!=(u32)predicted.xyzf[3];
        cockpit_second_stats.vertices++;cockpit_second_stats.bad+=bad;
        cockpit_second_stats.exact+=!bad;
        if(bad)second_match=0;
        if(count==3)second[i]=predicted;
        if(bad && cockpit_second_stats.bad<=3)
            ps2_log("ac4 cockpit secondary difference: z %d/%d uv %g/%g %g/%g color %d/%d",
                    xyz.s[2],predicted.xyzf[2],uv.f[0],predicted.stq[0],uv.f[1],predicted.stq[1],color.s[0],predicted.rgba[0]);
    }
    if(primary_mesh && second_match && !(tag.u[0]&0x8000u) && (second_tag.u[1]&0x4000u)
       && ((second_tag.u[1]>>28)&15u)==3 && (second_tag.u[2]&0xfffu)==0x412u)
        rn_ac4_mesh_prepare_secondary(cockpit.c,cockpit.r+first,second,(second_tag.u[1]>>15)&0x7ffu);
}
void rn_ac4_model_audit_begin(const ps2_vu *v,u32 entry,u64 signature) {
    cockpit_begin(v,entry,signature);
    terrain_begin(v,entry,signature);
    sample.active=0; plan_check.active=0; lit_memory.active=0; stats.seen++;
    if(signature!=0x8068ddf28a39675full && signature!=0x0b4ca6b56fd48dd8ull) {stats.unknown++;return;}
    sample.lit=signature==0x8068ddf28a39675full;
    if(entry!=16 && entry!=(sample.lit?4288u:3344u)) {stats.entry++;return;}
    if(!v || !v->mem || v->mem_size!=16384) {stats.layout++;return;}
    for(u32 k=0;k<32;k++) sample.constants[k]=read_qw(v,v->top+k);
    u32 n=sample.constants[23].u[0]&65535u;
    sample.split=(sample.constants[29].u[1]&65535u)!=0;
    if(!sample.lit) sample.constants[23].u[1]&=~2u;
    /* Input must not overlap the first output packet at TOP+0x7e. */
    if(n<3 || n>23) {stats.layout++;return;}
    memset(sample.clip_ok,0,sizeof sample.clip_ok);
    memset(sample.projection_ok,0,sizeof sample.projection_ok);
    for(u32 i=0;i<n;i++) for(u32 k=0;k<4;k++) sample.records[i][k]=read_qw(v,v->top+32+i*4+k);
    sample.count=n; sample.top=v->top;memcpy(sample.initial_vi,v->vi,sizeof sample.initial_vi);
    sample.owner=v;sample.active=1;stats.eligible++;
    if((rn_ac4_model_plan_on || lit_native_on) && sample.lit) {lit_memory.owner=v;lit_memory.active=1;lit_memory.next=0;lit_memory.integers_valid=1;lit_memory.cycles_valid=1;lit_memory.flags_valid=1;lit_memory.flags=(unlit_flags){*v,0};lit_memory.cycles=(entry==4288?2:0)+15+((n-3)&1)+sample.split;lit_memory.vectors=*v;lit_memory.vectors_valid=1;lit_memory.scalar=clip_scalar_begin(v);lit_memory.i=v->i;lit_memory.r=v->r;memcpy(lit_memory.vi,v->vi,sizeof lit_memory.vi);memcpy(lit_memory.memory,v->mem,sizeof lit_memory.memory);}
    sample.native_safe=sample.top>=117 && sample.top+300<=1024;
    sample.state=*v;sample.next=sample.packet_bad=sample.sequence_bad=0;
    if((rn_ac4_model_plan_on || rn_ac4_model_native_on) && !sample.lit && sample.split)memcpy(sample.memory,v->mem,sizeof sample.memory);
    sample.cycles=(entry==3344?2u:0u)+16+((n-3)&1u);sample.clip_packets=0;sample.flags=(unlit_flags){*v,0};flags_advance(&sample.flags,sample.cycles);
    sample.scalar=clip_scalar_begin(v);sample.scratch_valid=1;memcpy(sample.clip_scratch,v->mem+32*16,sizeof sample.clip_scratch);
    if((rn_ac4_model_plan_on || rn_ac4_model_native_on) && !sample.lit) {
        if(rn_ac4_unlit_plan_build(sample.constants,sample.records,sample.top,&plan_check.plan)) {
            plan_check.entry=entry;memcpy(plan_check.records,sample.records,sizeof plan_check.records);
            plan_check.initial=*v;memcpy(plan_check.constants,sample.constants,sizeof plan_check.constants);
            memcpy(plan_check.last_record,sample.records[n-1],sizeof plan_check.last_record);
            plan_check.owner=v;plan_check.next=0;plan_check.active=1;
            memcpy(plan_check.expected_mem,v->mem,sizeof plan_check.expected_mem);plan_stats.built++;
            if(plan_check.plan.culled)plan_stats.culled++;
        } else plan_stats.declined++;
    }
}
typedef struct {ps2_vf packet[70],scratch[80];u8 mask[80];u32 n,address;u16 vi10;} mixed_submission;
static mixed_submission mixed_submissions[21];
static u64 mixed_native_runs,mixed_native_packets,mixed_native_cycles;
static void mixed_finalize_state(void) {
    sample.state.q=sample.scalar.q;sample.state.p=sample.scalar.p;
    sample.state.q_pend=sample.scalar.q_pend;sample.state.p_pend=sample.scalar.p_pend;
    sample.state.q_ready=sample.state.p_ready=0;sample.state.clip=sample.scalar.clip;
    sample.state.tpc=0xd10;sample.state.running=0;
    flags_advance(&sample.flags,sample.cycles+2);
    sample.state.mac=sample.flags.state.mac;sample.state.status=sample.flags.state.status;
    sample.state.fslot=(sample.cycles+1)&3u;
    memcpy(sample.state.fpipe,sample.flags.state.fpipe,sizeof sample.state.fpipe);
}
static struct {u64 compared,exact,mismatch,declined,scratch_bad,integer_bad,vector_bad,scalar_bad,drain_bad,cycle_runs,cycle_bad,flags_bad,memory_bad,ordered_bad,sequence_bad,register_bad,ready_runs,ready_packets,ready_cycles;} clip_stats;
static void clip_cycle_end(const ps2_vu *v,u32 cycles) {
    if(!sample.active || sample.owner!=v || sample.lit || !sample.split || plan_check.active || !sample.scalar.valid || !sample.scratch_valid)return;
    clip_stats.cycle_runs++;
    if(sample.native_safe && sample.next==sample.count-2) {
        clip_stats.ready_runs++;clip_stats.ready_packets+=sample.next;clip_stats.ready_cycles+=sample.cycles+2;
    }
    if(memcmp(sample.memory,v->mem,sizeof sample.memory)) {
        if(clip_stats.memory_bad++<3)for(u32 j=0;j<1024;j++)if(memcmp(sample.memory+j,v->mem+j*16,16))ps2_log("ac4 unlit mixed memory: qword %u differs",j);
    }
    if(sample.packet_bad)clip_stats.ordered_bad++;
    if(sample.sequence_bad || sample.next!=sample.count-2)clip_stats.sequence_bad++;

    mixed_finalize_state();
    if(unlit_registers_differ(&sample.state,v)) {
        if(clip_stats.register_bad++<3) {
            for(int j=0;j<16;j++)if(sample.state.vi[j]!=v->vi[j])ps2_log("ac4 unlit mixed registers: vi%d %04x/%04x",j,sample.state.vi[j],v->vi[j]);
            for(int j=0;j<32;j++)for(int k=0;k<4;k++)if(sample.state.vf[j].u[k]!=v->vf[j].u[k])ps2_log("ac4 unlit mixed registers: vf%d.%d %08x/%08x",j,k,sample.state.vf[j].u[k],v->vf[j].u[k]);
        }
    }
    flags_advance(&sample.flags,sample.cycles+2);
    const ps2_vu *pred=&sample.flags.state;
    if(pred->status!=v->status || pred->mac!=v->mac || ((sample.cycles+1)&3u)!=v->fslot || memcmp(pred->fpipe,v->fpipe,sizeof v->fpipe)) {
        if(clip_stats.flags_bad++<3) {
            ps2_log("ac4 unlit clip flags: status %x/%x mac %x/%x cycles %u",pred->status,v->status,pred->mac,v->mac,sample.cycles+2);
            for(int j=0;j<4;j++)ps2_log("ac4 unlit clip flags: slot%d mac %x/%x status %x/%x valid %u/%u",j,pred->fpipe[j].mac,v->fpipe[j].mac,pred->fpipe[j].status,v->fpipe[j].status,pred->fpipe[j].valid,v->fpipe[j].valid);
        }
    }
    if(sample.cycles+2!=cycles && clip_stats.cycle_bad++<3)ps2_log("ac4 unlit clip cycles: predicted %u actual %u packets %u clipped %u",sample.cycles+2,cycles,sample.count-2,sample.clip_packets);
}
/* Independently compose each packet, including packets surrounding clipping.
 * Record all writes in an activation-input copy to verify untouched memory too. */
static int unlit_triangle_packet(const ps2_vf c[32],const ps2_vf records[3][4],int culled,ps2_vf packet[70]) {
    if(culled) {memset(packet,0,16);packet[0].u[0]=0xffff8000u;return 1;}
    packet[0]=c[22];packet[0].s[0]=(s16)0x8003;
    ps2_vf constants[32];memcpy(constants,c,sizeof constants);constants[23].u[1]&=~2u;
    for(int i=0;i<3;i++) {
        rn_ac4_model_vertex vertex;if(!rn_ac4_model_eval(constants,records[i],&vertex))return 0;
        memcpy(packet[1+i*3].f,vertex.stq,16);memcpy(packet[3+i*3].s,vertex.xyzf,16);
        for(int k=0;k<4;k++)packet[2+i*3].s[k]=ps2_cvt_w_s((float)records[i][1].s[k]);
    }
    return 10;
}
static void mixed_packet_record(const ps2_vu *v,u32 observed_address,u32 first,const ps2_vf *packet,u32 n) {
    u32 target=(sample.top+126+104*(first&1u))&1023u;
    int bad=v && target!=(observed_address&1023u);
    if(!v) {mixed_submissions[first].n=n;mixed_submissions[first].address=target;memcpy(mixed_submissions[first].packet,packet,n*sizeof *packet);}
    for(u32 j=0;j<n;j++) {
        u32 at=(target+j)&1023u;
        sample.memory[at]=packet[j];
        if(v && memcmp(packet+j,v->mem+at*16,16))bad=1;
    }
    sample.packet_bad+=bad;
}
static void mixed_prelude_state(u32 first,const float cp[3][4]) {
    ps2_vu *v=&sample.state;const ps2_vf *c=sample.constants;
    v->vi[2]=(u16)sample.top;v->vi[3]=(u16)(sample.top+32+4*first);
    v->vi[4]=(u16)(sample.top+126+104*(first&1u));v->vi[6]=3;v->vi[9]=(u16)first;
    v->vi[10]=cp[0][3]<8192?16:cp[0][3]==8192?1:0;
    v->vi[11]=cp[1][3]<8192?16:cp[1][3]==8192?1:0;
    v->vi[12]=(u16)sample.count;v->vi[13]=(u16)(first+3);
    v->vf[1]=c[22];v->vf[2]=c[25];v->vf[21]=c[29];
    v->vf[21].f[2]=ps2_fmul((float)c[29].s[2],64);
    memcpy(v->vf+22,c+4,4*sizeof(ps2_vf));
    for(int j=0;j<3;j++)memcpy(v->vf[27+j].f,cp[j],16);
    clip_accumulator(c+4,sample.records[first+2][2].f,&v->acc);v->i=8192;
}
static void mixed_ordinary_state(u32 first,const ps2_vf packet[70]) {
    ps2_vu *v=&sample.state;const ps2_vf *c=sample.constants;
    u32 bank=sample.top+126+104*(first&1u);
    v->vi[3]=(u16)(sample.top+32+4*(first+4));v->vi[5]=(u16)(bank+12);
    v->vi[6]=0;v->vi[7]=2;v->vi[11]=0x8003;
    v->vf[1]=packet[0];v->vf[10]=c[30];v->vf[16].f[3]=c[30].f[0];
    v->vf[17]=packet[8];v->vf[19]=packet[7];v->vf[21]=packet[9];
    memcpy(v->vf+22,c,4*sizeof(ps2_vf));
    for(int j=0;j<4;j++)v->vf[26+j]=sample.memory[(sample.top+32+4*(first+3)+j)&1023u];
    v->vf[29].s[3]=0;
    for(int k=0;k<4;k++) {
        v->vf[26].f[k]=(float)v->vf[26].s[k]/4096.0f;
        v->vf[27].f[k]=(float)v->vf[27].s[k];
        v->vf[29].f[k]=(float)v->vf[29].s[k]/4096.0f;
    }
    clip_accumulator(c,sample.records[first+2][2].f,&v->acc);v->i=255;
}
static void unlit_clip_observe(const ps2_vu *v,u32 address) {
    if(!sample.active || (v && sample.owner!=v) || sample.lit || !sample.split)return;
    u32 first=sample.next++;
    if(v && first!=v->vi[9])sample.sequence_bad++;
    if(first+3>sample.count) {sample.sequence_bad++;return;}
    /* Packet writes must not overlap the scratch input being independently tracked. */
    for(u32 j=0;j<70;j++) {u32 a=(address+j)&1023u;if(a>=32 && a<117)sample.scratch_valid=0;}
    u32 any=0,common=63;int near_w=0,which=3;
    for(u32 i=first;i<first+3;i++) {
        float cp[4];transform(sample.constants+4,sample.records[i][2].f,cp);
        u32 bits=0;for(int k=0;k<3;k++) {if(cp[k]>fabsf(cp[3]))bits|=1u<<(k*2);if(cp[k]<-fabsf(cp[3]))bits|=2u<<(k*2);}
        if(cp[3]<=8192 && which==3)which=(int)(i-first);
        any|=bits;common&=bits;near_w|=cp[3]<=8192;
        sample.scalar.clip=((sample.scalar.clip<<6)|bits)&0xffffffu;
    }
    u32 prelude=sample.cycles+10+(first&1u);float cp_flags[3][4];
    flags_scalar(&sample.flags,prelude+9,2,ps2_fmul((float)sample.constants[29].s[2],64));
    for(int j=0;j<3;j++)flags_transform(&sample.flags,prelude+10+j*4,sample.constants+4,sample.records[first+j][2].f,cp_flags[j]);
    for(int j=0;j<3;j++)flags_scalar(&sample.flags,prelude+23+j,1,ps2_fadd(cp_flags[j][3],-8192));
    mixed_prelude_state(first,cp_flags);
    if(near_w)sample.state.vi[1]=0;
    sample.cycles+=10+(first&1u)+29+(which<3?2u*(which+1)+3u:8u);
    if(!any || common || !near_w) {
        if(!any || !near_w) {
            float projected[4];transform(sample.constants,sample.records[first+2][2].f,projected);
            clip_scalar_divide(&sample.scalar,1.0f/projected[3],model_qlat);
            flags_scalar(&sample.flags,sample.cycles+22,1,0);flags_scalar(&sample.flags,sample.cycles+26,1,sample.constants[30].f[0]);
            for(int j=0;j<3;j++)flags_project(&sample.flags,sample.cycles+26+j*33,sample.constants,sample.records[first+j]);
            sample.cycles+=26+99+11+(first&1u);
        } else {
            u32 plane=0;while(!(common&(1u<<plane)))plane++;
            sample.state.vi[1]=plane==5?1:(common&(1u<<(plane+1)))!=0;
            flags_scalar(&sample.flags,sample.cycles+2*(plane+1)+1,15,0);
            sample.cycles+=2*(plane+1)+17+(first&1u);
        }
        flags_advance(&sample.flags,sample.cycles);
        ps2_vf ordinary[70];u32 n=unlit_triangle_packet(sample.constants,sample.records+first,common && near_w,ordinary);
        if(!n) {sample.scalar.valid=0;if(v)clip_stats.declined++;return;}
        mixed_packet_record(v,address,first,ordinary,n);
        if(n==1)sample.state.vf[29]=ordinary[0];else mixed_ordinary_state(first,ordinary);
        sample.state.vi[9]=(u16)(first+1);
        if(!v)mixed_submissions[first].vi10=sample.state.vi[10];
        return;
    }
    sample.cycles+=12;flags_advance(&sample.flags,sample.cycles);
    ps2_vf packet[70],scratch[80];u8 mask[80];u32 n=0;unlit_clip_control control;unlit_clip_trace trace;
    if(!sample.scratch_valid || !sample.scalar.valid || !unlit_clip_build(sample.constants,sample.records+first,packet,&n,scratch,mask,&control,sample.clip_scratch,&trace,&sample.flags,0,NULL,NULL)) {
        sample.scalar.valid=0;sample.scratch_valid=0;if(v)clip_stats.declined++;return;
    }
    for(u32 j=0;j<80;j++)if(mask[j]) {sample.clip_scratch[j]=scratch[j];sample.memory[32+j]=scratch[j];}
    mixed_packet_record(v,address,first,packet,n);
    sample.cycles+=trace.cycles+2+(first&1u);sample.clip_packets++;flags_advance(&sample.flags,sample.cycles);
    if(trace.has_q)clip_scalar_divide(&sample.scalar,trace.q,model_qlat);
    u16 integers[16];clip_kick_integers(&control,sample.top,first,sample.count,sample.initial_vi,integers);
    memcpy(sample.state.vi,integers,sizeof integers);sample.state.vi[9]=(u16)(first+1);
    for(int j=0;j<32;j++)if(trace.mask&(1u<<j))sample.state.vf[j]=trace.vf[j];
    sample.state.acc=trace.acc;sample.state.i=trace.i;
    if(!v) {
        memcpy(mixed_submissions[first].scratch,scratch,sizeof scratch);
        memcpy(mixed_submissions[first].mask,mask,sizeof mask);
        mixed_submissions[first].vi10=sample.state.vi[10];return;
    }
    int bad=0;
    for(u32 i=0;i<n;i++) {ps2_vf actual=read_qw(v,address+i);if(memcmp(&actual,packet+i,16))bad=1;}
    clip_stats.compared++;
    if(!clip_scalar_matches(&sample.scalar,v)) {
        if(clip_stats.scalar_bad++<3)ps2_log("ac4 unlit clip scalar: first %u Q %g/%g pending %g/%g P %g/%g clip %06x/%06x ready %u/%u",
            first,sample.scalar.q,v->q,sample.scalar.q_pend,v->q_pend,sample.scalar.p,v->p,sample.scalar.clip,v->clip,v->q_ready,v->p_ready);
    }
    int drain_bad=v->mac!=(control.count<3?15u:1u) || (v->status&3u)!=1u;
    for(int j=0;j<4;j++)drain_bad|=v->fpipe[j].valid!=0;
    if(drain_bad)clip_stats.drain_bad++;

    int vectors_bad=memcmp(&trace.acc,&v->acc,16)!=0 || memcmp(&trace.i,&v->i,4)!=0
        || (trace.has_q && memcmp(&trace.q,&v->q,4)!=0);
    for(int i=0;i<32;i++)if(trace.mask&(1u<<i))for(int k=0;k<4;k++)if(trace.vf[i].u[k]!=v->vf[i].u[k]) {
        vectors_bad=1;if(clip_stats.vector_bad<3)ps2_log("ac4 unlit clip vector: vf%d.%d %08x/%08x",i,k,trace.vf[i].u[k],v->vf[i].u[k]);
    }
    if(vectors_bad)clip_stats.vector_bad++;
    if(memcmp(integers,v->vi,sizeof integers)) {
        if(clip_stats.integer_bad++<3)for(int i=0;i<16;i++)if(integers[i]!=v->vi[i])ps2_log("ac4 unlit clip control: vi%d %04x/%04x",i,integers[i],v->vi[i]);
    }
    int scratch_bad=0;
    for(u32 i=0;i<80;i++)if(mask[i]) {
        ps2_vf actual=read_qw(v,32+i);
        if(memcmp(&actual,scratch+i,16)) {
            scratch_bad=1;
            if(clip_stats.scratch_bad<3)for(int k=0;k<4;k++)if(actual.u[k]!=scratch[i].u[k])ps2_log("ac4 unlit clip scratch: qw%u.%d %08x/%08x",32+i,k,scratch[i].u[k],actual.u[k]);
        }
    }
    if(scratch_bad)clip_stats.scratch_bad++;
    if(bad) {
        if(clip_stats.mismatch++<3) {
            ps2_vf actual=read_qw(v,address);
            ps2_log("ac4 unlit clip: first %u predicted %u qwords, tag %08x/%08x",first,n,packet[0].u[0],actual.u[0]);
            for(u32 i=0;i<n;i++) {actual=read_qw(v,address+i);for(int k=0;k<4;k++)if(actual.u[k]!=packet[i].u[k])ps2_log("ac4 unlit clip: qw%u.%d %08x/%08x",i,k,packet[i].u[k],actual.u[k]);}
        }
    } else clip_stats.exact++;
}
/* Generate and validate every submission before the first guest write/kick.
 * Scratch, constants/input and both maximum-sized output banks must be disjoint.
 * The original callback still handles GIF/GS side effects in packet order. */
static int mixed_try_native(ps2_vu *v,u32 limit,void (*kick)(ps2_vu *,u32)) {
    if(!sample.active || sample.owner!=v || sample.lit || !sample.split || !sample.scalar.valid)return 0;
    if(sample.top<117 || sample.top+300>1024)return 0;
    for(u32 j=0;j<sample.count;j++) {
        float cp[4];transform(sample.constants+4,sample.records[j][2].f,cp);
        for(int k=0;k<4;k++)if(!isfinite(cp[k]))return 0;
    }
    memset(mixed_submissions,0,sizeof mixed_submissions);
    for(u32 j=0;j<sample.count-2;j++) {
        u32 address=sample.top+126+104*(j&1u);
        unlit_clip_observe(NULL,address);
        if(!sample.scalar.valid || !sample.scratch_valid || !mixed_submissions[j].n)return 0;
    }
    if(sample.cycles+2>limit)return 0;
    mixed_finalize_state();ps2_vu final=sample.state;
    for(u32 j=0;j<sample.count-2;j++) {
        const mixed_submission *p=mixed_submissions+j;
        for(u32 k=0;k<80;k++)if(p->mask[k])memcpy(v->mem+(32+k)*16,p->scratch+k,16);
        for(u32 k=0;k<p->n;k++)memcpy(v->mem+(p->address+k)*16,p->packet+k,16);
        v->vi[9]=(u16)j;v->vi[10]=p->vi10;
        kick(v,p->address);mixed_native_packets++;
    }
    *v=final;mixed_native_runs++;mixed_native_cycles+=sample.cycles+2;
    return 1;
}
static struct {u64 vertices,headers,uv,color,geometry,rejected,skipped,adc_set,adc_bad,packets,packet_bad;} lit_secondary_stats;
static struct {u64 packets,bad,scratch_bad,declined,empty;} lit_clip_stats;
static void lit_clip_observe(const ps2_vu *v,u32 address) {
    if(!sample.active || sample.owner!=v || !sample.lit || !sample.split)return;
    u32 first=v->vi[9];if(first+3>sample.count)return;
    u32 any=0,common=63;int near_w=0;
    for(u32 j=0;j<3;j++) {
        float cp[4];transform(sample.constants+4,sample.records[first+j][2].f,cp);
        u32 bits=0;for(int k=0;k<3;k++) {if(cp[k]>fabsf(cp[3]))bits|=1u<<(k*2);if(cp[k]<-fabsf(cp[3]))bits|=2u<<(k*2);}
        any|=bits;common&=bits;near_w|=cp[3]<=8192;
    }
    if(!any || common || !near_w)return;
    ps2_vf packet[144],scratch[80];u8 mask[80];u32 n=0;
    if(!lit_clip_build(sample.constants,sample.records+first,first,packet,&n,scratch,mask)) {lit_clip_stats.declined++;return;}
    lit_clip_stats.packets++;lit_clip_stats.empty+=n==1;
    int bad=0,scratch_bad=0;
    for(u32 j=0;j<n;j++) {
        ps2_vf actual=read_qw(v,address+j);
        if(memcmp(packet+j,&actual,16)) {
            bad=1;if(lit_clip_stats.bad<3)for(int k=0;k<4;k++)if(packet[j].u[k]!=actual.u[k])ps2_log("ac4 lit clip packet: qw%u.%d %08x/%08x",j,k,packet[j].u[k],actual.u[k]);
        }
    }
    for(u32 j=0;j<80;j++)if(mask[j]) {
        ps2_vf actual=read_qw(v,32+j);
        if(memcmp(scratch+j,&actual,16))scratch_bad=1;
    }
    lit_clip_stats.bad+=bad;lit_clip_stats.scratch_bad+=scratch_bad;
}
static void lit_secondary_observe(const ps2_vu *v,u32 address) {
    if(!sample.active || sample.owner!=v || !sample.lit)return;
    u32 first=sample.split?v->vi[9]:0,n=sample.split?3:sample.count;
    if(first+n>sample.count || v->vi[10]==1) {lit_secondary_stats.skipped++;return;}
    for(u32 j=0;j<n;j++) {
        float cp[4];transform(sample.constants+4,sample.records[first+j][2].f,cp);
        if(!isfinite(cp[3]) || cp[3]<=0) {lit_secondary_stats.skipped++;return;}
        for(int k=0;k<3;k++)if(!isfinite(cp[k]) || fabsf(cp[k])>cp[3]) {lit_secondary_stats.skipped++;return;}
    }
    const ps2_vf *c=sample.constants;u32 second=address+3+3*n;
    ps2_vf predicted_packet[144];u32 predicted_size=0;
    if(!rn_ac4_lit_packet(c,sample.records+first,n,sample.split?first:sample.count-3,sample.split,predicted_packet,&predicted_size)) {lit_secondary_stats.rejected+=n;return;}
    lit_secondary_stats.packets++;int packet_bad=0;
    for(u32 j=0;j<predicted_size;j++) {
        ps2_vf actual=read_qw(v,address+j);
        if(memcmp(predicted_packet+j,&actual,16)) {
            packet_bad=1;
            if(lit_secondary_stats.packet_bad<3)for(int k=0;k<4;k++)if(predicted_packet[j].u[k]!=actual.u[k])ps2_log("ac4 lit packet: qw%u.%d %08x/%08x",j,k,predicted_packet[j].u[k],actual.u[k]);
        }
    }
    lit_secondary_stats.packet_bad+=packet_bad;
    ps2_vf tags[6]={c[26],c[28],c[22],c[26],c[27],c[25]};
    tags[2].s[0]=(c[23].u[1]&4u)?(s32)n:(s16)(0x8000u|n);
    tags[5].s[0]=(s16)(0x8000u|n);
    int bad=0;
    for(int j=0;j<6;j++) {ps2_vf actual=read_qw(v,(j<3?address:second)+(j%3));if(memcmp(tags+j,&actual,16))bad=1;}
    if(bad)lit_secondary_stats.headers++;
    s32 fog_adc[23];
    if(!lit_fog_adc(c,sample.records+first,n,sample.split?first:sample.count-3,sample.split,fog_adc)) {lit_secondary_stats.rejected+=n;return;}
    for(u32 j=0;j<n;j++) {
        rn_ac4_model_vertex expected;
        if(!rn_ac4_lit_secondary(c,sample.records[first+j],&expected)) {lit_secondary_stats.rejected++;continue;}
        ps2_vf uv=read_qw(v,second+3+3*j),color=read_qw(v,second+4+3*j),xyz=read_qw(v,second+5+3*j);
        expected.xyzf[3]=fog_adc[j];
        lit_secondary_stats.vertices++;lit_secondary_stats.adc_set+=(fog_adc[j]&0x8000)!=0;
        ps2_vf primary_xyz=read_qw(v,address+5+3*j);
        if(primary_xyz.s[3]!=fog_adc[j] || xyz.s[3]!=fog_adc[j]) {
            if(lit_secondary_stats.adc_bad++<3)ps2_log("ac4 lit ADC: vertex %u expected %08x actual %08x/%08x parity %u split %d",j,(u32)fog_adc[j],primary_xyz.u[3],xyz.u[3],first,sample.split);
        }
        if(memcmp(uv.f,expected.stq,16)) {if(lit_secondary_stats.uv++<3)for(int k=0;k<4;k++)ps2_log("ac4 lit second stq: lane%d %g/%g",k,expected.stq[k],uv.f[k]);}
        if(memcmp(color.s,expected.rgba,16))lit_secondary_stats.color++;
        if(memcmp(xyz.s,expected.xyzf,16))lit_secondary_stats.geometry++;
    }
}
void rn_ac4_model_audit_kick(const ps2_vu *v,u32 address) {
    if(rn_ac4_model_mesh_on) rn_ac4_mesh_end();
    terrain_kick(v,address);
    cockpit_memory_kick(v,address);
    cockpit_kick(v,address);
    if(v && cockpit.owner==v)for(u32 i=0;i<80;i++)cockpit.scratch[i]=read_qw(v,32+i);
    if(rn_ac4_model_plan_on) {unlit_plan_kick(v,address);unlit_clip_observe(v,address);lit_secondary_observe(v,address);lit_clip_observe(v,address);lit_memory_kick(v,address);}
    if(!sample.active || v!=sample.owner || !v->mem || v->mem_size!=16384)return;
    u32 first=sample.split ? v->vi[9] : 0;
    u32 count=sample.split ? 3 : sample.count;
    u32 parity=sample.split ? first : sample.count-3;
    if(!sample.split) sample.active=0;
    if(first+count>sample.count || (address&1023u)!=((sample.top+126+((parity&1u)?104:0))&1023u)) {
        stats.wrong_kick++;return;
    }
    u32 header=sample.lit?3:1;
    ps2_vf tag=read_qw(v,address+header-1);
    /* vi10 marks the clipping-generated scratch polygon. Empty/cull kicks
     * don't have the original input-to-output vertex correspondence either. */
    if(v->vi[10]==1 || (tag.u[0]&0x7fffu)!=count) {
        stats.clipped++;
        if(stats.clipped<=3) ps2_log("ac4 model audit skipped packet: clip=%u loops=%u expected=%u first=%u tag=%08x/%08x/%08x/%08x",
            v->vi[10],tag.u[0]&0x7fff,count,first,tag.u[0],tag.u[1],tag.u[2],tag.u[3]);
        return;
    }
    /* Clipping can produce three vertices too, so packet size alone is not
     * proof that they are the original three. Require every input inside the
     * homogeneous clip volume before comparing the direct model kernel. */
    for(u32 i=first;i<first+count;i++) {
        if(!vertex_safe(i,0)) {stats.clipped++;return;}
    }
    rn_ac4_model_vertex verified[3];
    int match=count==3;
    stats.kicks++;
    for(u32 i=0;i<count;i++) {
        rn_ac4_model_vertex predicted;
        ps2_vf uv=read_qw(v,address+header+i*3), color=read_qw(v,address+header+1+i*3), xyz=read_qw(v,address+header+2+i*3);
        if(!rn_ac4_model_audit_on) {
            /* Use the original VU output for draw matching. Do not repeat its
             * lighting/UV/fog calculation solely to compare it with itself.
             * Keep projection eligibility checks: GPU float-to-int conversion
             * is undefined for NaN/overflow, unlike the VU conversion helper. */
            if(count!=3) {match=0;break;}
            int valid=vertex_safe(first+i,1);
            for(int k=0;k<3;k++) valid &= isfinite(uv.f[k]);
            if(!valid) {stats.rejected++;match=0;continue;}
            memcpy(verified[i].stq,uv.f,16);
            memcpy(verified[i].rgba,color.s,16);
            memcpy(verified[i].xyzf,xyz.s,16);
            verified[i].xyzf[3]&=0x7fff; /* ADC is not fog. */
            stats.forwarded++;
            continue;
        }
        if(!rn_ac4_model_eval(sample.constants,sample.records[first+i],&predicted)) {stats.rejected++;match=0;continue;}
        int bx=0,bf=0,bu=0,bc=0;
        for(int k=0;k<3;k++) { bx|=xyz.s[k]!=predicted.xyzf[k]; bu|=uv.f[k]!=predicted.stq[k]; }
        for(int k=0;k<4;k++) bc|=color.s[k]!=predicted.rgba[k];
        /* Bit 15 in the fourth lane carries ADC/backface suppression. */
        bf=(xyz.u[3]&0x7fffu)!=(u32)predicted.xyzf[3];
        if(bx||bf||bu||bc) match=0;
        if(count==3) verified[i]=predicted;
        stats.vertices++;
        if(sample.lit) stats.lit_vertices++; else stats.unlit_vertices++;
        stats.bad_xyz+=bx;stats.bad_fog+=bf;stats.bad_uv+=bu;stats.bad_color+=bc;
        if(!(bx||bf||bu||bc)) stats.exact++;
        else if(stats.vertices-stats.exact<=3)
            ps2_log("ac4 model audit mismatch vertex %u: xyz %d/%d %d/%d %d/%d, fog %u/%d, color %d/%d, uv %.9g/%.9g",i,
                    xyz.s[0],predicted.xyzf[0],xyz.s[1],predicted.xyzf[1],xyz.s[2],predicted.xyzf[2],xyz.u[3]&0x7fff,predicted.xyzf[3],
                    color.s[0],predicted.rgba[0],uv.f[0],predicted.stq[0]);
    }
    if(match && rn_ac4_model_mesh_on && (tag.u[1]&0x4000u)
       && ((tag.u[1]>>28)&15u)==3 && (tag.u[2]&0xfffu)==0x412u)
    {
        rn_ac4_mesh_prepare(sample.constants,sample.records+first,verified,(tag.u[1]>>15)&0x7ffu);
        if(sample.lit && !(tag.u[0]&0x8000u)) {
            ps2_vf second_tag=read_qw(v,address+5+3*count);
            if((second_tag.u[0]&0x7fff)==3 && (second_tag.u[1]&0x4000u)
               && ((second_tag.u[1]>>28)&15u)==3 && (second_tag.u[2]&0xfffu)==0x412u) {
                rn_ac4_model_vertex second[3];int valid=1;
                for(int j=0;j<3;j++)valid&=rn_ac4_lit_secondary(sample.constants,sample.records[first+j],second+j);
                if(valid)rn_ac4_mesh_prepare_secondary(sample.constants,sample.records+first,second,(second_tag.u[1]>>15)&0x7ffu);
            }
        }
    }
}
void rn_ac4_model_audit_report(void) {
    if(cockpit_native_on)ps2_log("ac4 cockpit native: %llu activations, %llu packets, %llu original instructions replaced",(unsigned long long)cockpit_native_runs,(unsigned long long)cockpit_native_packets,(unsigned long long)cockpit_native_cycles);
    if(cockpit_audit_on>0 && rn_ac4_model_plan_on)ps2_log("ac4 cockpit flags: %llu ended, %llu mismatches",(unsigned long long)cockpit_flag_stats.ended,(unsigned long long)cockpit_flag_stats.bad);
    if(cockpit_audit_on>0 && rn_ac4_model_plan_on)ps2_log("ac4 cockpit cycles: %llu ended, %llu mismatches, %llu declined",
        (unsigned long long)cockpit_cycle_stats.ended,(unsigned long long)cockpit_cycle_stats.bad,(unsigned long long)cockpit_cycle_stats.declined);
    if(cockpit_audit_on>0 && rn_ac4_model_plan_on)ps2_log("ac4 cockpit vectors: %llu packets, %llu ended, %llu kick / %llu end mismatches, %llu declined",
        (unsigned long long)cockpit_vector_stats.packets,(unsigned long long)cockpit_vector_stats.ended,
        (unsigned long long)cockpit_vector_stats.kick_bad,(unsigned long long)cockpit_vector_stats.end_bad,
        (unsigned long long)cockpit_vector_stats.declined);
    if(cockpit_audit_on>0 && rn_ac4_model_plan_on)ps2_log("ac4 cockpit scalars: %llu packets, %llu ended, %llu kick / %llu end mismatches, %llu declined",
        (unsigned long long)cockpit_scalar_stats.packets,(unsigned long long)cockpit_scalar_stats.ended,
        (unsigned long long)cockpit_scalar_stats.kick_bad,(unsigned long long)cockpit_scalar_stats.end_bad,
        (unsigned long long)cockpit_scalar_stats.declined);
    if(cockpit_audit_on>0 && rn_ac4_model_plan_on)ps2_log("ac4 cockpit integers: %llu packets, %llu ended, %llu kick / %llu end mismatches, %llu declined",
        (unsigned long long)cockpit_integer_stats.packets,(unsigned long long)cockpit_integer_stats.ended,
        (unsigned long long)cockpit_integer_stats.kick_bad,(unsigned long long)cockpit_integer_stats.end_bad,
        (unsigned long long)cockpit_integer_stats.declined);
    if(cockpit_audit_on>0 && rn_ac4_model_plan_on)ps2_log("ac4 cockpit memory: %llu begun, %llu ended, %llu packets, %llu kick / %llu end mismatches, %llu sequence failures, %llu declined",
        (unsigned long long)cockpit_memory_stats.begun,(unsigned long long)cockpit_memory_stats.ended,
        (unsigned long long)cockpit_memory_stats.packets,(unsigned long long)cockpit_memory_stats.kick_bad,
        (unsigned long long)cockpit_memory_stats.end_bad,(unsigned long long)cockpit_memory_stats.sequence_bad,
        (unsigned long long)cockpit_memory_stats.declined);
    if(cockpit_audit_on>0)ps2_log("ac4 cockpit clipping: %llu packets, %llu exact, %llu packet / %llu scratch mismatches, %llu declined, %llu empty",
        (unsigned long long)cockpit_clip_stats.packets,(unsigned long long)cockpit_clip_stats.exact,
        (unsigned long long)cockpit_clip_stats.bad,(unsigned long long)cockpit_clip_stats.scratch_bad,
        (unsigned long long)cockpit_clip_stats.declined,(unsigned long long)cockpit_clip_stats.empty);
    if(cockpit_audit_on>0)ps2_log("ac4 cockpit secondary: %llu vertices, %llu exact, %llu mismatches, %llu header failures, %llu rejected",
        (unsigned long long)cockpit_second_stats.vertices,(unsigned long long)cockpit_second_stats.exact,
        (unsigned long long)cockpit_second_stats.bad,(unsigned long long)cockpit_second_stats.headers,
        (unsigned long long)cockpit_second_stats.rejected);
    if(cockpit_audit_on>0)ps2_log("ac4 cockpit audit: %llu activations, %llu packets, %llu vertices, %llu exact, %llu xyz / %llu fog / %llu uv / %llu color mismatches, %llu skipped, %llu rejected",
        (unsigned long long)cockpit_stats.activations,(unsigned long long)cockpit_stats.packets,
        (unsigned long long)cockpit_stats.vertices,(unsigned long long)cockpit_stats.exact,
        (unsigned long long)cockpit_stats.xyz,(unsigned long long)cockpit_stats.fog,
        (unsigned long long)cockpit_stats.uv,(unsigned long long)cockpit_stats.color,
        (unsigned long long)cockpit_stats.skipped,(unsigned long long)cockpit_stats.rejected);
    rn_ac4_mesh_report();
    if(rn_ac4_model_plan_on) {
        ps2_log("ac4 terrain preparation: %llu seen, %llu compared, %llu vertices, %llu mismatches, %llu declined",(unsigned long long)terrain_stats.seen,(unsigned long long)terrain_stats.compared,(unsigned long long)terrain_stats.vertices,(unsigned long long)terrain_stats.bad,(unsigned long long)terrain_stats.declined);
        ps2_log("ac4 terrain packets: %llu compared, %llu packet / %llu bank mismatches, %llu declined",(unsigned long long)terrain_stats.packets,(unsigned long long)terrain_stats.packet_bad,(unsigned long long)terrain_stats.bank_bad,(unsigned long long)terrain_stats.packet_declined);
        ps2_log("ac4 terrain clipped topology: %llu packets, %llu triangles, %llu declined",(unsigned long long)terrain_clipped_topologies,(unsigned long long)terrain_clipped_triangles,(unsigned long long)terrain_clipped_topology_declined);
        ps2_log("ac4 terrain submissions: %llu packets, %llu memory mismatches, %llu sequence mismatches",(unsigned long long)terrain_submission_stats.packets,(unsigned long long)terrain_submission_stats.bad,(unsigned long long)terrain_submission_stats.sequence);
        ps2_log("ac4 terrain continuation: %llu built, %llu ended, %llu packets, %llu instructions, %llu mismatches, %llu declined",(unsigned long long)terrain_execution_stats.built,(unsigned long long)terrain_execution_stats.ended,(unsigned long long)terrain_execution_stats.packets,(unsigned long long)terrain_execution_stats.cycles,(unsigned long long)terrain_execution_stats.bad,(unsigned long long)terrain_execution_stats.declined);
        ps2_log("ac4 terrain flags: %llu built, %llu ended, %llu mismatches, %llu excluded",(unsigned long long)terrain_flag_stats.built,(unsigned long long)terrain_flag_stats.ended,(unsigned long long)terrain_flag_stats.bad,(unsigned long long)terrain_flag_stats.excluded);
        ps2_log("ac4 terrain vectors: %llu built, %llu ended, %llu kicks, %llu end / %llu kick mismatches, %llu excluded",(unsigned long long)terrain_vector_stats.built,(unsigned long long)terrain_vector_stats.ended,(unsigned long long)terrain_vector_stats.kicks,(unsigned long long)terrain_vector_stats.bad,(unsigned long long)terrain_vector_stats.kick_bad,(unsigned long long)terrain_vector_stats.excluded);
        ps2_log("ac4 terrain instructions: %llu built, %llu ended, %llu mismatches, %llu excluded, %llu predicted / %llu observed",(unsigned long long)terrain_cycle_stats.built,(unsigned long long)terrain_cycle_stats.ended,(unsigned long long)terrain_cycle_stats.bad,(unsigned long long)terrain_cycle_stats.excluded,(unsigned long long)terrain_cycle_stats.predicted,(unsigned long long)terrain_cycle_stats.observed);
        ps2_log("ac4 terrain scalars: %llu built, %llu ended, %llu kicks, %llu end / %llu kick / %llu sequence mismatches, %llu declined",(unsigned long long)terrain_scalar_stats.built,(unsigned long long)terrain_scalar_stats.ended,(unsigned long long)terrain_scalar_stats.kicks,(unsigned long long)terrain_scalar_stats.bad,(unsigned long long)terrain_scalar_stats.kick_bad,(unsigned long long)terrain_scalar_stats.sequence,(unsigned long long)terrain_scalar_stats.declined);
        ps2_log("ac4 terrain integers: %llu built, %llu ended, %llu kicks, %llu end / %llu kick / %llu sequence mismatches, %llu declined",(unsigned long long)terrain_integer_stats.built,(unsigned long long)terrain_integer_stats.ended,(unsigned long long)terrain_integer_stats.kicks,(unsigned long long)terrain_integer_stats.bad,(unsigned long long)terrain_integer_stats.kick_bad,(unsigned long long)terrain_integer_stats.sequence,(unsigned long long)terrain_integer_stats.declined);
        ps2_log("ac4 terrain memory: %llu built, %llu ended, %llu mismatches, %llu declined",(unsigned long long)terrain_memory_stats.built,(unsigned long long)terrain_memory_stats.ended,(unsigned long long)terrain_memory_stats.bad,(unsigned long long)terrain_memory_stats.declined);
        ps2_log("ac4 terrain polygons: %llu built, %llu declined, %llu packets, %llu mismatches, %llu sequence mismatches",(unsigned long long)terrain_polygon_stats.built,(unsigned long long)terrain_polygon_stats.declined,(unsigned long long)terrain_polygon_stats.packets,(unsigned long long)terrain_polygon_stats.bad,(unsigned long long)terrain_polygon_stats.sequence);
        ps2_log("ac4 terrain culling: %llu seen, %llu ended, %llu predicted empty, %llu observed empty, %llu mismatches, %llu partial, %llu invalid",(unsigned long long)terrain_clip_stats.seen,(unsigned long long)terrain_clip_stats.ended,(unsigned long long)terrain_clip_stats.predicted_empty,(unsigned long long)terrain_clip_stats.observed_empty,(unsigned long long)terrain_clip_stats.mismatch,(unsigned long long)terrain_clip_stats.partial,(unsigned long long)terrain_clip_stats.invalid);
        ps2_log("ac4 terrain topology: %llu patches, %llu triangles, %llu declined",(unsigned long long)terrain_stats.topologies,(unsigned long long)terrain_stats.triangles,(unsigned long long)terrain_stats.topology_declined);
        for(int j=0;j<24;j++)if(terrain_stats.counts[j])ps2_log("ac4 terrain count %d: %llu",j,(unsigned long long)terrain_stats.counts[j]);
        for(int j=0;j<8;j++)if(terrain_stats.modes[j])ps2_log("ac4 terrain mode %d: %llu",j,(unsigned long long)terrain_stats.modes[j]);
    }
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit instructions: %llu predicted",(unsigned long long)lit_cycle_stats.total);
    if(terrain_native_on)ps2_log("ac4 terrain native: %llu activations, %llu packets, %llu original instructions replaced",(unsigned long long)terrain_native_runs,(unsigned long long)terrain_native_packets,(unsigned long long)terrain_native_cycles);
    if(lit_native_on)ps2_log("ac4 lit native: %llu activations, %llu packets, %llu original instructions replaced",(unsigned long long)lit_native_runs,(unsigned long long)lit_native_packets,(unsigned long long)lit_native_cycles);
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit flags: %llu activations, %llu mismatches, %llu declined",(unsigned long long)lit_flag_stats.ended,(unsigned long long)lit_flag_stats.bad,(unsigned long long)lit_flag_stats.declined);
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit cycles: %llu activations, %llu mismatches, %llu declined",(unsigned long long)lit_cycle_stats.ended,(unsigned long long)lit_cycle_stats.bad,(unsigned long long)lit_cycle_stats.declined);
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit vectors: %llu packets, %llu activations, %llu kick / %llu continuation mismatches, %llu skipped packets",(unsigned long long)lit_vector_stats.packets,(unsigned long long)lit_vector_stats.ended,(unsigned long long)lit_vector_stats.bad,(unsigned long long)lit_vector_stats.end_bad,(unsigned long long)lit_vector_stats.skipped);
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit scalar: %llu packets, %llu activations, %llu kick / %llu continuation mismatches, %llu declined",(unsigned long long)lit_scalar_stats.packets,(unsigned long long)lit_scalar_stats.ended,(unsigned long long)lit_scalar_stats.bad,(unsigned long long)lit_scalar_stats.end_bad,(unsigned long long)lit_scalar_stats.declined);
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit integers: %llu packets, %llu activations, %llu kick / %llu continuation mismatches, %llu declined",(unsigned long long)lit_integer_stats.packets,(unsigned long long)lit_integer_stats.ended,(unsigned long long)lit_integer_stats.bad,(unsigned long long)lit_integer_stats.end_bad,(unsigned long long)lit_integer_stats.declined);
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit memory: %llu activations, %llu packets, %llu memory / %llu sequence mismatches, %llu declined, %llu packet mismatches",(unsigned long long)lit_memory_stats.ended,(unsigned long long)lit_memory_stats.packets,(unsigned long long)lit_memory_stats.memory_bad,(unsigned long long)lit_memory_stats.sequence_bad,(unsigned long long)lit_memory_stats.declined,(unsigned long long)lit_memory_stats.packet_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit plan: %llu built, %llu declined, %llu packets, %llu packet mismatches, %llu ended, %llu memory mismatches, %llu missing sequences, %llu control mismatches",
        (unsigned long long)plan_stats.built,(unsigned long long)plan_stats.declined,
        (unsigned long long)plan_stats.packets,(unsigned long long)plan_stats.packet_bad,
        (unsigned long long)plan_stats.ended,(unsigned long long)plan_stats.memory_bad,
        (unsigned long long)plan_stats.missing,(unsigned long long)plan_stats.control_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit state: %llu register mismatches (vector/integer/accumulator/Q/P/I/clip/random/pending/continuation)",(unsigned long long)plan_stats.register_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit flags: %llu pipeline mismatches",(unsigned long long)plan_stats.flags_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit cycles: %llu instruction-count mismatches",(unsigned long long)plan_stats.cycles_bad);
    if(rn_ac4_model_native_on)ps2_log("ac4 unlit native: %llu activations, %llu packets, %llu original instructions replaced",
        (unsigned long long)native_runs,(unsigned long long)native_packets,(unsigned long long)native_cycles);
    if(rn_ac4_model_plan_on || rn_ac4_model_native_on)ps2_log("ac4 unlit culling: %llu planned activations, %llu executed natively",
        (unsigned long long)plan_stats.culled,(unsigned long long)native_culled);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit clip: %llu packets compared, %llu exact, %llu mismatched, %llu declined",
        (unsigned long long)clip_stats.compared,(unsigned long long)clip_stats.exact,
        (unsigned long long)clip_stats.mismatch,(unsigned long long)clip_stats.declined);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit clip scratch: %llu mismatched packets",(unsigned long long)clip_stats.scratch_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit clip control: %llu integer-state mismatches",(unsigned long long)clip_stats.integer_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit clipping: %llu packets, %llu empty, %llu packet / %llu scratch mismatches, %llu declined",(unsigned long long)lit_clip_stats.packets,(unsigned long long)lit_clip_stats.empty,(unsigned long long)lit_clip_stats.bad,(unsigned long long)lit_clip_stats.scratch_bad,(unsigned long long)lit_clip_stats.declined);
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit packets: %llu compared, %llu mismatched",(unsigned long long)lit_secondary_stats.packets,(unsigned long long)lit_secondary_stats.packet_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit ADC: %llu marked vertices, %llu mismatches",(unsigned long long)lit_secondary_stats.adc_set,(unsigned long long)lit_secondary_stats.adc_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 lit second: %llu vertices, %llu header / %llu stq / %llu colour / %llu geometry mismatches, %llu rejected; %llu clipped submissions skipped",(unsigned long long)lit_secondary_stats.vertices,(unsigned long long)lit_secondary_stats.headers,(unsigned long long)lit_secondary_stats.uv,(unsigned long long)lit_secondary_stats.color,(unsigned long long)lit_secondary_stats.geometry,(unsigned long long)lit_secondary_stats.rejected,(unsigned long long)lit_secondary_stats.skipped);
    if(rn_ac4_model_plan_on)ps2_log("ac4 mixed eligible: %llu activations, %llu packets, %llu instructions",(unsigned long long)clip_stats.ready_runs,(unsigned long long)clip_stats.ready_packets,(unsigned long long)clip_stats.ready_cycles);
    if(rn_ac4_model_native_on)ps2_log("ac4 mixed native: %llu activations, %llu packets, %llu original instructions replaced",(unsigned long long)mixed_native_runs,(unsigned long long)mixed_native_packets,(unsigned long long)mixed_native_cycles);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit mixed registers: %llu state mismatches",(unsigned long long)clip_stats.register_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit mixed memory: %llu mismatched activations, %llu packet mismatches, %llu sequence mismatches",(unsigned long long)clip_stats.memory_bad,(unsigned long long)clip_stats.ordered_bad,(unsigned long long)clip_stats.sequence_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit clip flags: %llu state mismatches",(unsigned long long)clip_stats.flags_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit clip cycles: %llu activations, %llu mismatches",(unsigned long long)clip_stats.cycle_runs,(unsigned long long)clip_stats.cycle_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit clip scalar: %llu state mismatches; %llu drain mismatches",(unsigned long long)clip_stats.scalar_bad,(unsigned long long)clip_stats.drain_bad);
    if(rn_ac4_model_plan_on)ps2_log("ac4 unlit clip vector: %llu state mismatches",(unsigned long long)clip_stats.vector_bad);
    if(!stats.seen)return;
    ps2_log("ac4 model safety: %llu transforms evaluated, %llu repeated transforms avoided",
        (unsigned long long)stats.safety_checks,(unsigned long long)stats.safety_reused);
    ps2_log("ac4 model forwarding: %llu vertices without CPU reference evaluation; audit %s",
        (unsigned long long)stats.forwarded,rn_ac4_model_audit_on?"on":"off");
    ps2_log("ac4 model audit: %llu activations, %llu unknown, %llu entry skips, %llu clipping skips, %llu layout skips, %llu eligible, %llu kicks, %llu wrong kick",
        (unsigned long long)stats.seen,(unsigned long long)stats.unknown,(unsigned long long)stats.entry,(unsigned long long)stats.clipped,
        (unsigned long long)stats.layout,(unsigned long long)stats.eligible,(unsigned long long)stats.kicks,(unsigned long long)stats.wrong_kick);
    ps2_log("ac4 model audit coverage: %llu lit, %llu unlit vertex outputs",
        (unsigned long long)stats.lit_vertices,(unsigned long long)stats.unlit_vertices);
    ps2_log("ac4 model audit: %llu vertices, %llu exact, %llu xyz / %llu fog / %llu uv / %llu color mismatches, %llu rejected",
        (unsigned long long)stats.vertices,(unsigned long long)stats.exact,(unsigned long long)stats.bad_xyz,(unsigned long long)stats.bad_fog,
        (unsigned long long)stats.bad_uv,(unsigned long long)stats.bad_color,(unsigned long long)stats.rejected);
}
