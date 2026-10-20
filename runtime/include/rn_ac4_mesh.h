#ifndef RN_AC4_MESH_H
#define RN_AC4_MESH_H
#include "rn_ac4_model.h"
#include "ps2_vk.h"
void rn_ac4_mesh_prepare(const ps2_vf constants[32],const ps2_vf records[3][4],
                         const rn_ac4_model_vertex expected[3],u32 prim);
void rn_ac4_mesh_prepare_secondary(const ps2_vf constants[32],const ps2_vf records[3][4],
                                   const rn_ac4_model_vertex expected[3],u32 prim);
void rn_ac4_mesh_prepare_cockpit(const ps2_vf constants[32],const ps2_vf records[3][4],
                               const rn_ac4_model_vertex expected[3],u32 prim);
void rn_ac4_mesh_cockpit_append(const ps2_vf constants[32],const ps2_vf records[3][4],
                              const rn_ac4_model_vertex expected[3],int secondary);
void rn_ac4_mesh_terrain_append(const ps2_vf constants[32],const ps2_vf records[3][4],
                                const rn_ac4_model_vertex expected[3],int secondary,int clipped);
int rn_ac4_mesh_claim(const ps2_vk_state *st,int kind,const ps2_vk_vertex *v,int n);
void rn_ac4_mesh_end(void);
int rn_ac4_mesh_pending(void);
int rn_ac4_mesh_cockpit_pending(void);
void rn_ac4_mesh_report(void);
#endif
