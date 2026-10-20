#ifndef RN_AC4_CLOUD_PACKET_H
#define RN_AC4_CLOUD_PACKET_H
#include "rn_ac4_cloud_project.h"
typedef struct {ac4_cloud_point_input input;ac4_cloud_point_output output;} ac4_cloud_record;
typedef struct {ac4_cloud_record point;uint32_t rgba;} ac4_cloud_mesh_vertex;
/* Map one complete AC4 REGLIST quad to verified raw inputs. Require exact
 * XYZ/ST/Q and reject ambiguous raw inputs. Never partially publish indices. */
static inline int ac4_cloud_quad_map(const void *packet,size_t bytes,
    const ac4_cloud_record *records,size_t count,uint32_t indices[4]) {
    if(!packet || bytes!=128 || !records || !indices)return 0;
    uint64_t words[16];memcpy(words,packet,sizeof words);
    if(words[0]!=UINT64_C(0xe400000000008001) || words[1]!=UINT64_C(0x00512512512512f0)
       || words[2]!=0x5c)return 0;
    uint32_t found[4];
    for(unsigned k=0;k<4;++k){
        size_t match=count;uint32_t q=(uint32_t)(words[5+3*k]>>32);
        for(size_t i=0;i<count;++i){
            const ac4_cloud_record *r=records+i;
            if(r->output.clip==UINT32_MAX || !r->input.write_st || !r->input.write_q
               || r->output.xyz!=words[6+3*k] || memcmp(r->output.st,words+4+3*k,8)
               || memcmp(&r->output.q,&q,4))continue;
            if(match<count && memcmp(&records[match].input,&r->input,sizeof r->input))return 0;
            match=i;
        }
        if(match==count || match>UINT32_MAX)return 0;
        found[k]=(uint32_t)match;
    }
    memcpy(indices,found,sizeof found);return 1;
}
#endif
