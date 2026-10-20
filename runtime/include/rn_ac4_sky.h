#ifndef RN_AC4_SKY_H
#define RN_AC4_SKY_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* AC4's EE sky band writer consumes two closed, projected 32-segment
 * rings. This preparatory encoder is not enabled in the renderer yet.
 * xy is packed GS 12.4 X/Y; clip is the guest projection reject mask.
 * Element 32 closes the ring. Depth payloads retain all caller bits. */
typedef struct { uint32_t xy, clip; } ac4_sky_point;
typedef struct { uint64_t lo, hi; } ac4_sky_qword;
#define AC4_SKY_BAND_QWORDS 129u

/* Return 1 on success (including an empty band), 0 on invalid arguments
 * or insufficient capacity. On failure neither output nor count changes.
 * The caller emits PRIM separately, as in the original EE producer. */
static inline int ac4_sky_band_build(
    const ac4_sky_point a[33], const ac4_sky_point b[33],
    uint64_t visible, uint32_t color_a, uint32_t color_b,
    uint64_t depth_a, uint64_t depth_b,
    ac4_sky_qword *out, size_t capacity, size_t *count)
{
    ac4_sky_qword packet[AC4_SKY_BAND_QWORDS];
    size_t n = 1, quads = 0;
    if (!a || !b || !count) return 0;
    for (unsigned i = 0; i < 32; ++i) {
        if (((visible >> i) & 3u) != 3u) continue;
        if (a[i].clip & a[i+1].clip & b[i].clip & b[i+1].clip) continue;
        packet[n++] = (ac4_sky_qword){color_a, depth_a | a[i].xy};
        packet[n++] = (ac4_sky_qword){color_a, depth_a | a[i+1].xy};
        packet[n++] = (ac4_sky_qword){color_b, depth_b | b[i].xy};
        packet[n++] = (ac4_sky_qword){color_b, depth_b | b[i+1].xy};
        ++quads;
    }
    if (!quads) { *count = 0; return 1; }
    if (!out || capacity < n) return 0;
    /* REGLIST: RGBA/XYZ3, RGBA/XYZ3, RGBA/XYZ2, RGBA/XYZ2.
     * XYZ3 retains strip vertices without drawing the first two. */
    packet[0] = (ac4_sky_qword){UINT64_C(0x8400000000008000) | quads,
                                      UINT64_C(0x5151d1d1)};
    memcpy(out, packet, n * sizeof *out);
    *count = n;
    return 1;
}
#endif
