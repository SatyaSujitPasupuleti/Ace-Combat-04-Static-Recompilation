#ifndef RN_AC4_MODEL_H
#define RN_AC4_MODEL_H
#include "ps2_runtime.h"
/* AC4's unclipped, four-qword model record; addresses are relative to TOP. */
typedef struct {
    float stq[4];
    s32 rgba[4];
    s32 xyzf[4];
} rn_ac4_model_vertex;
int rn_ac4_model_eval(const ps2_vf constants[32], const ps2_vf record[4],
                      rn_ac4_model_vertex *out);
int rn_ac4_lit_secondary(const ps2_vf constants[32], const ps2_vf record[4], rn_ac4_model_vertex *out);
/* split: 0 original strip, 1 split triangle, 2 clipped polygon (fixed winding). */
int rn_ac4_lit_packet(const ps2_vf constants[32], const ps2_vf records[][4], u32 count, u32 parity, int split, ps2_vf output[144], u32 *qwords);
extern int rn_ac4_model_audit_on, rn_ac4_model_mesh_on, rn_ac4_model_capture_on, rn_ac4_model_material_on;
extern int rn_ac4_model_texture_on, rn_ac4_terrain_mesh_on;
int rn_ac4_unlit_clip_packet(const ps2_vf constants[32], const ps2_vf input[3][4],
                            ps2_vf output[70], u32 *qwords);
/* Independent unlit packet plan, built solely from activation input. */
typedef struct {
    ps2_vf tag;
    rn_ac4_model_vertex vertices[23];
    u32 count, top, split, packets, culled;
    float clip[23][4];
    u32 clip_bits[23];
    u8 cull_plane[21];
} rn_ac4_unlit_plan;
int rn_ac4_unlit_plan_build(const ps2_vf constants[32], const ps2_vf records[][4],
                           u32 top, rn_ac4_unlit_plan *out);
u32 rn_ac4_unlit_plan_packet(const rn_ac4_unlit_plan *plan, u32 index,
                            ps2_vf output[70], u32 *address);
extern int rn_ac4_model_plan_on, rn_ac4_model_native_on;
int rn_ac4_model_try_native(ps2_vu *vu, u32 limit, void (*kick)(ps2_vu *,u32));
void rn_ac4_model_audit_end(const ps2_vu *vu,u32 cycles);
void rn_ac4_model_init(void);
void rn_ac4_model_audit_begin(const ps2_vu *vu, u32 entry, u64 signature);
void rn_ac4_model_audit_kick(const ps2_vu *vu, u32 address);
void rn_ac4_model_audit_report(void);
#endif
