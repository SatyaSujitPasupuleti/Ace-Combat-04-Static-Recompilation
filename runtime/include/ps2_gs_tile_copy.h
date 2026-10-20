#ifndef PS2_GS_TILE_COPY_H
#define PS2_GS_TILE_COPY_H
#include "ps2_runtime.h"
#include <string.h>

/* Separate atlas storage: preserve wrapped source pixels, copy contiguous runs. */
static inline void ps2_copy_tiled_rgba(u8 *dst, size_t stride, const u8 *src,
                                      u32 sw, u32 sh, u32 w, u32 h) {
    if (!sw || !sh) return;
    for (u32 y = 0; y < h; ++y) {
        const u8 *row = src + (size_t)(y % sh) * sw * 4u;
        u8 *out = dst + (size_t)y * stride;
        for (u32 x = 0; x < w;) {
            u32 run = w - x;
            if (run > sw) run = sw;
            memcpy(out + (size_t)x * 4u, row, (size_t)run * 4u);
            x += run;
        }
    }
}
#endif
