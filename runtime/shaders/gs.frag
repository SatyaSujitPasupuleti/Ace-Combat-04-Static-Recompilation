#version 450
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec3 v_stq;
layout(location = 2) in float v_fog;
layout(location = 3) flat in uint v_round_uv;

layout(location = 0, index = 0) out vec4 out_color;
layout(location = 0, index = 1) out vec4 out_blend;

layout(set = 0, binding = 0) uniform sampler2D tex;
layout(set = 1, binding = 0) uniform sampler2D dsttex;

layout(push_constant) uniform PC {
    vec2  inv_size;
    vec2  tex_size;
    uvec4 flags;
    vec4  fogcol;
    uvec4 clamp_uv;
    vec2  img_size;
    float scale;
    float point_size;
    float tex_scale;
    float pad0;
} pc;

const uint RUV_SPRITE = 1u << 29;

float gs_wrap(float t, uint mode, float lo, float hi, float size, bool own) {
    if (mode == 0u) return own ? mod(t, max(size, 1.0)) : t;
    if (mode == 1u) return clamp(t, 0.0, max(size - 1.0, 0.0));
    if (mode == 2u) return clamp(t, lo, max(hi, lo));
    return float((int(floor(t)) & int(lo)) | int(hi));
}

float sprite_round(float t, uint mode, int origin, int pixel) {
    if (mode == 0u) return t + 1.0 / 1024.0;
    float halftexel = floor(t * 2.0 + 0.5) * 0.5;
    if (abs(t - halftexel) > 1.0 / 64.0) return t;
    return halftexel + ((mode == 1u || pixel == origin) ? 1.0 : -1.0) / 64.0;
}

vec4 sample_region_repeat(vec2 tc) {
    vec2 lo = floor(tc), hi = lo + 1.0, w = fract(tc);
    ivec2 a, b, size = textureSize(tex, 0);
    a.x = int(gs_wrap(lo.x, pc.flags.z & 3u, float(pc.clamp_uv.x), float(pc.clamp_uv.y), pc.tex_size.x, true));
    b.x = int(gs_wrap(hi.x, pc.flags.z & 3u, float(pc.clamp_uv.x), float(pc.clamp_uv.y), pc.tex_size.x, true));
    a.y = int(gs_wrap(lo.y, (pc.flags.z >> 2) & 3u, float(pc.clamp_uv.z), float(pc.clamp_uv.w), pc.tex_size.y, true));
    b.y = int(gs_wrap(hi.y, (pc.flags.z >> 2) & 3u, float(pc.clamp_uv.z), float(pc.clamp_uv.w), pc.tex_size.y, true));
    a = ((a % size) + size) % size;
    b = ((b % size) + size) % size;
    return mix(mix(texelFetch(tex, a, 0), texelFetch(tex, ivec2(b.x, a.y), 0), w.x),
               mix(texelFetch(tex, ivec2(a.x, b.y), 0), texelFetch(tex, b, 0), w.x), w.y);
}

int wrap_up(int k, uint mode, int lo, int hi, int size, int s) {
    if (mode == 0u) {
        int period = max(size, 1) * s;
        return k - period * int(floor(float(k) / float(period)));
    }
    if (mode == 1u) return clamp(k, 0, max(size, 1) * s - 1);
    if (mode == 2u) return clamp(k, lo * s, max(hi, lo) * s + s - 1);
    int g = int(floor(float(k) / float(s)));
    return ((g & lo) | hi) * s + (k - g * s);
}

vec4 sample_region_repeat_up(vec2 tc) {
    int s = int(pc.tex_scale + 0.5);
    vec2 c = (tc + 0.5) * pc.tex_scale - 0.5;
    vec2 lo = floor(c), w = c - lo;
    ivec2 size = textureSize(tex, 0);
    uint mu = pc.flags.z & 3u, mv = (pc.flags.z >> 2) & 3u;
    int tw = int(pc.tex_size.x), th = int(pc.tex_size.y);
    int x0 = int(lo.x), y0 = int(lo.y);
    ivec2 a = ivec2(wrap_up(x0, mu, int(pc.clamp_uv.x), int(pc.clamp_uv.y), tw, s),
                    wrap_up(y0, mv, int(pc.clamp_uv.z), int(pc.clamp_uv.w), th, s));
    ivec2 b = ivec2(wrap_up(x0 + 1, mu, int(pc.clamp_uv.x), int(pc.clamp_uv.y), tw, s),
                    wrap_up(y0 + 1, mv, int(pc.clamp_uv.z), int(pc.clamp_uv.w), th, s));
    a -= size * ivec2(floor(vec2(a) / vec2(size)));
    b -= size * ivec2(floor(vec2(b) / vec2(size)));
    return mix(mix(texelFetch(tex, a, 0), texelFetch(tex, ivec2(b.x, a.y), 0), w.x),
               mix(texelFetch(tex, ivec2(a.x, b.y), 0), texelFetch(tex, b, 0), w.x), w.y);
}

// Indexed texels must be palette-expanded before bilinear interpolation.
vec4 gpu_clut_texel(vec2 tc) {
    ivec2 size = textureSize(tex, 0);
    ivec2 p;
    p.x = int(gs_wrap(tc.x, pc.flags.z & 3u, float(pc.clamp_uv.x), float(pc.clamp_uv.y), pc.tex_size.x, true));
    p.y = int(gs_wrap(tc.y, (pc.flags.z >> 2) & 3u, float(pc.clamp_uv.z), float(pc.clamp_uv.w), pc.tex_size.y, true));
    p = ((p % size) + size) % size;
    uint index = uint(round(texelFetch(tex, p, 0).r * 255.0));
    uint entry = (index & ~24u) | ((index & 8u) << 1) | ((index & 16u) >> 1);
    ivec2 cp = ivec2(vec2(entry % 16u, entry / 16u) * pc.scale + pc.scale * 0.5);
    return texelFetch(dsttex, cp, 0);
}

vec4 mip_texel(ivec2 p, uint level) {
    ivec2 size = max(ivec2(pc.tex_size) >> int(level), ivec2(1));
    uvec4 bounds = pc.clamp_uv >> level;
    p.x = int(gs_wrap(float(p.x), pc.flags.z & 3u, float(bounds.x), float(bounds.y), float(size.x), true));
    p.y = int(gs_wrap(float(p.y), (pc.flags.z >> 2) & 3u, float(bounds.z), float(bounds.w), float(size.y), true));
    p = ((p % size) + size) % size;
    if (level > 0u) {
        int yoff = 0;
        for (uint i = 1u; i < level; ++i) yoff += max(int(pc.tex_size.y) >> int(i), 1);
        p += ivec2(int(pc.tex_size.x), yoff);
    }
    return texelFetch(tex, p, 0);
}
vec4 mip_sample(vec2 uv, uint level, bool linear_filter) {
    vec2 tc = uv / exp2(float(level));
    if (!linear_filter) return mip_texel(ivec2(floor(tc)), level);
    tc -= 0.5;
    ivec2 p = ivec2(floor(tc)); vec2 w = fract(tc);
    return mix(mix(mip_texel(p,level), mip_texel(p+ivec2(1,0),level),w.x),
               mix(mip_texel(p+ivec2(0,1),level), mip_texel(p+ivec2(1,1),level),w.x),w.y);
}
vec4 sample_gs_mips(uint params) {
    int k = int((params >> 8) & 4095u); if (k >= 2048) k -= 4096;
    float lod = -log2(max(abs(v_stq.z), 1e-30)) * exp2(float((params >> 6) & 3u)) + float(k)/16.0;
    uint mxl = params & 7u, mmin = (params >> 3) & 7u;
    vec2 uv = (v_stq.xy / v_stq.z) * pc.tex_size;
    if (lod <= 0.0) return mip_sample(uv, 0u, (params & (1u << 21)) != 0u);
    lod = clamp(lod, 0.0, float(mxl));
    if ((mmin & 1u) == 0u) return mip_sample(uv, uint(floor(lod+0.5)), mmin >= 4u);
    uint lo = uint(floor(lod)), hi = min(lo+1u, mxl);
    return mix(mip_sample(uv,lo,mmin >= 4u), mip_sample(uv,hi,mmin >= 4u), fract(lod));
}

void main() {
    out_blend = vec4(1.0);
    vec2 tdx = vec2(0.0), tdy = vec2(0.0);
    if (pc.scale > 1.0 && pc.flags.y != 0u) {
        vec2 tcd = ((pc.flags.x >> 2) & 1u) != 0u
                 ? v_stq.xy : (v_stq.xy / max(v_stq.z, 1e-9)) * pc.tex_size;
        tdx = dFdx(tcd);
        tdy = dFdy(tcd);
    }
    if (pc.flags.w == 1u) { out_color = vec4(1.0, 0.0, 1.0, 1.0); return; }
    if (pc.flags.w == 2u) { out_color = vec4(v_color.rgb, 1.0); return; }
    vec4 c = v_color;
    uint f = pc.flags.x;
    if ((f & (1u << 24)) != 0u) {
        vec2 rg = texelFetch(tex, ivec2(gl_FragCoord.xy), 0).rg;
        uint green = uint(round(rg.y * 255.0));
        uint ta = (f >> ((green & 128u) != 0u ? 27u : 26u)) & 1u;
        if ((f & (1u << 28)) != 0u && uint(round(rg.x * 255.0)) == 0u && (green & 127u) == 0u)
            ta = 0u;
        green = (green & 127u) | (ta << 7);
        out_color = vec4(rg.x, float(green) / 255.0, rg.x, float(green) / 128.0);
        out_blend = vec4(1.0);
        return;
    }
    uint tfx  = f & 3u;
    bool fst  = ((f >> 2) & 1u) != 0u;
    bool ate  = ((f >> 3) & 1u) != 0u;
    uint atst = (f >> 4) & 7u;
    float aref = float((f >> 8) & 0xFFu) / 128.0;
    bool fge  = ((f >> 16) & 1u) != 0u;
    bool tcc  = ((f >> 17) & 1u) != 0u;
    bool afkeep = ((f >> 18) & 1u) != 0u;
    bool point = ((f >> 19) & 1u) != 0u;
    bool point_rows = point && ((f >> 29) & 1u) != 0u;
    bool afonly = ((f >> 20) & 1u) != 0u;
    bool fba   = ((f >> 21) & 1u) != 0u;
    bool date  = ((f >> 22) & 1u) != 0u;
    bool datm  = ((f >> 23) & 1u) != 0u;
    if (date) {
        float ad = texelFetch(dsttex, ivec2(gl_FragCoord.xy), 0).a;
        if ((ad >= 1.0) != datm) discard;
    }

    if (pc.flags.y != 0u) {
        vec2 tc;
        if (fst) tc = v_stq.xy;
        else     tc = (v_stq.xy / max(v_stq.z, 1e-9)) * pc.tex_size;
        uint ruv = v_round_uv & (RUV_SPRITE - 1u);
        bool sprite = (v_round_uv & RUV_SPRITE) != 0u;
        bool rt_up = pc.tex_scale > 1.0;
        ivec2 pix = ivec2(gl_FragCoord.xy);
        vec2 sub = vec2(0.0);
        if (pc.scale > 1.0) {
            vec2 nat = floor(gl_FragCoord.xy / pc.scale);
            sub = gl_FragCoord.xy - 0.5 - nat * pc.scale;
            pix = ivec2(nat);
            if (point && (sprite || rt_up))
                tc += (point_rows ? vec2(0.0) : tdx * ((nat.x + 0.5) * pc.scale - gl_FragCoord.x))
                    + tdy * ((nat.y + 0.5) * pc.scale - gl_FragCoord.y);
        }
        if (point && ruv != 0u) {
            if (!point_rows)
                tc.x = sprite_round(tc.x, ruv & 3u,
                    int((ruv >> 4) & 4095u) - 2048, pix.x);
            tc.y = sprite_round(tc.y, (ruv >> 2) & 3u,
                int((ruv >> 16) & 4095u) - 2048, pix.y);
        }
        if (!point && (f & (1u << 25)) != 0u) tc -= 0.5;
        vec2 unwrapped_tc = tc;
        bool own = any(notEqual(pc.img_size, pc.tex_size));
        tc.x = gs_wrap(tc.x, pc.flags.z & 3u,
                       float(pc.clamp_uv.x), float(pc.clamp_uv.y),
                       pc.tex_size.x, own);
        tc.y = gs_wrap(tc.y, (pc.flags.z >> 2) & 3u,
                       float(pc.clamp_uv.z), float(pc.clamp_uv.w),
                       pc.tex_size.y, own);
        vec4 t;
        if (pc.pad0 >= 1048576.0) {
            t = sample_gs_mips(uint(pc.pad0));
        } else if ((f & (1u << 31)) != 0u) {
            if (point) t = gpu_clut_texel(floor(unwrapped_tc + 1.0 / 32.0));
            else {
                vec2 lo = floor(unwrapped_tc), w = fract(unwrapped_tc);
                t = mix(mix(gpu_clut_texel(lo), gpu_clut_texel(lo + vec2(1,0)), w.x),
                        mix(gpu_clut_texel(lo + vec2(0,1)), gpu_clut_texel(lo + vec2(1,1)), w.x), w.y);
            }
        } else if ((f & (1u << 30)) != 0u) {
            // GS target alpha is represented as alpha / 128 by the renderer.
            // Sample the source pixel before looking its byte up in the CLUT.
            ivec2 p = ivec2(floor(tc + 1.0 / 32.0) * pc.scale);
            p += ivec2(clamp(sub + 0.5, vec2(0.0), vec2(pc.scale - 1.0)));
            p = clamp(p, ivec2(0), textureSize(dsttex, 0) - 1);
            int index = clamp(int(round(texelFetch(dsttex, p, 0).a * 128.0)), 0, 255);
            t = texelFetch(tex, ivec2(index, 0), 0);
        } else if (point) {
            ivec2 size = textureSize(tex, 0);
            ivec2 p = ivec2(floor(tc + (ruv == 0u ? 1.0 / 32.0 : 0.0)));
            if (rt_up) {
                int s = int(pc.tex_scale + 0.5);
                ivec2 k = clamp(ivec2(sub + 0.5), ivec2(0), ivec2(s - 1));
                if (tdx.x < 0.0) k.x = s - 1 - k.x;
                if (tdy.y < 0.0) k.y = s - 1 - k.y;
                p = p * s + k;
            }
            p = ((p % size) + size) % size;
            t = texelFetch(tex, p, 0);
        } else if ((pc.flags.z & 3u) == 3u || ((pc.flags.z >> 2) & 3u) == 3u
                   || (own && ((pc.flags.z & 3u) == 0u
                               || ((pc.flags.z >> 2) & 3u) == 0u))) {
            if (rt_up) t = sample_region_repeat_up(unwrapped_tc);
            else       t = sample_region_repeat(unwrapped_tc);
        } else {
            t = texture(tex, (tc + 0.5) / pc.img_size);
        }
        float af = c.a;
        vec3 rgb;
        float a;
        if (tfx == 1u) {
            rgb = t.rgb;
            a   = tcc ? t.a : af;
        } else if (tfx == 0u) {
            rgb = t.rgb * c.rgb;
            a   = tcc ? t.a * af : af;
        } else if (tfx == 2u) {
            rgb = t.rgb * c.rgb + af * (128.0 / 255.0);
            a   = tcc ? t.a + af : af;
        } else {
            rgb = t.rgb * c.rgb + af * (128.0 / 255.0);
            a   = tcc ? t.a : af;
        }
        c = vec4(rgb, a);
    } else {
        c.rgb *= 128.0 / 255.0;
    }

    if (fge) c.rgb = mix(pc.fogcol.rgb, c.rgb, clamp(v_fog, 0.0, 1.0));

    if (ate) {
        bool pass;
        if      (atst == 0u) pass = false;
        else if (atst == 1u) pass = true;
        else if (atst == 2u) pass = c.a <  aref;
        else if (atst == 3u) pass = c.a <= aref;
        else if (atst == 4u) pass = abs(c.a - aref) < (0.5 / 128.0);
        else if (atst == 5u) pass = c.a >= aref;
        else if (atst == 6u) pass = c.a >  aref;
        else                 pass = abs(c.a - aref) >= (0.5 / 128.0);
        if (afonly ? pass : (!pass && afkeep)) discard;
    }

    out_blend = vec4(0.0, 0.0, 0.0, clamp(c.a, 0.0, 1.0));
    if (fba) c.a = max(c.a, 1.0);
    out_color = clamp(c, vec4(0.0), vec4(2.0));
}
