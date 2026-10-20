#version 450
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec3 v_stq;
layout(location = 2) in float v_fog;
layout(location = 3) flat in uint v_round_uv;

layout(location = 0, index = 0) out vec4 out_color;
layout(location = 0, index = 1) out vec4 out_blend;

layout(set = 0, binding = 0) uniform sampler2D tex;

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

// Preserve AC4 supplied mip artwork and per-level region clamps.
float gs_wrap(float t, uint mode, float lo, float hi, float size, bool own) {
    if (mode == 0u) return own ? mod(t, max(size, 1.0)) : t;
    if (mode == 1u) return clamp(t, 0.0, max(size - 1.0, 0.0));
    if (mode == 2u) return clamp(t, lo, max(hi, lo));
    return float((int(floor(t)) & int(lo)) | int(hi));
}

vec4 mip_texel(ivec2 p, uint level) {
    ivec2 size = max(ivec2(pc.tex_size) >> int(level), ivec2(1));
    uvec4 bounds = pc.clamp_uv >> level;
    p.x = int(gs_wrap(float(p.x), pc.flags.z & 3u, float(bounds.x), float(bounds.y), float(size.x), true));
    p.y = int(gs_wrap(float(p.y), (pc.flags.z >> 2) & 3u, float(bounds.z), float(bounds.w), float(size.y), true));
    p = ((p % size) + size) % size;
    if ((pc.flags.z & 0x200u) != 0u) {
        ivec2 origin=ivec2(0);
        if ((pc.flags.z & 3u)==2u) origin.x=int(bounds.x);
        if (((pc.flags.z >> 2) & 3u)==2u) origin.y=int(bounds.z);
        return texelFetch(tex,p-origin,int(level));
    }
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
    vec4 c = v_color;
    uint f = pc.flags.x;
    uint tfx = f & 3u;
    bool ate = ((f >> 3) & 1u) != 0u;
    uint atst = (f >> 4) & 7u;
    float aref = float((f >> 8) & 0xFFu) / 128.0;
    bool fge = ((f >> 16) & 1u) != 0u;
    bool tcc = ((f >> 17) & 1u) != 0u;
    bool afkeep = ((f >> 18) & 1u) != 0u;
    bool point = ((f >> 19) & 1u) != 0u;
    bool afonly = ((f >> 20) & 1u) != 0u;
    bool fba = ((f >> 21) & 1u) != 0u;

    if (pc.flags.y != 0u) {
        vec2 tc = (v_stq.xy / max(v_stq.z, 1e-9)) * pc.tex_size;
        uint wms = pc.flags.z & 3u, wmt = (pc.flags.z >> 2) & 3u;
        bool region_image = ((pc.flags.z >> 8) & 1u) != 0u;
        vec2 org = vec2(0.0);
        if (region_image) {
            if (wms == 2u) org.x = float(pc.clamp_uv.x);
            if (wmt == 2u) org.y = float(pc.clamp_uv.z);
        }
        vec2 uv = (tc - org) / pc.img_size;
        // Region clamping changes sample position, not the surface footprint.
        // Derivatives of clamped coordinates can incorrectly select level 0.
        vec2 uv_dx = dFdx(uv), uv_dy = dFdy(uv);
        vec2 lo = 0.5 / pc.img_size;
        vec2 hi = (pc.tex_size - 0.5) / pc.img_size;
        if (wms == 2u) {
            lo.x = (float(pc.clamp_uv.x) - org.x + 0.5) / pc.img_size.x;
            hi.x = (float(pc.clamp_uv.y) - org.x + 0.5) / pc.img_size.x;
        }
        if (wmt == 2u) {
            lo.y = (float(pc.clamp_uv.z) - org.y + 0.5) / pc.img_size.y;
            hi.y = (float(pc.clamp_uv.w) - org.y + 0.5) / pc.img_size.y;
        }
        if (wms == 1u || wms == 2u) uv.x = clamp(uv.x, lo.x, hi.x);
        if (wmt == 1u || wmt == 2u) uv.y = clamp(uv.y, lo.y, hi.y);
        vec4 t;
        if ((pc.flags.z & 0x600u)==0x600u) {
            uint params=uint(pc.pad0);
            int k=int((params>>8)&4095u);if(k>=2048)k-=4096;
            float lod=clamp(-log2(max(abs(v_stq.z),1e-30))*exp2(float((params>>6)&3u))
                +float(k)/16.0,0.0,float(params&7u));
            if ((pc.flags.z & 0x800u)!=0u) {
                // Preserve the game's LOD bias, retaining the projected footprint
                // direction and aspect for the native anisotropic sampler.
                vec2 dx=uv_dx*pc.img_size,dy=uv_dy*pc.img_size;
                float a=dot(dx,dx),b=dot(dx,dy),c=dot(dy,dy);
                float disc=sqrt(max((a-c)*(a-c)+4.0*b*b,0.0));
                float major=sqrt(max(0.5*(a+c+disc),0.0));
                float minor=sqrt(max(0.5*(a+c-disc),0.0));
                // Anisotropic samplers choose detail from the minor axis.
                // Scale that axis to GS LOD; scaling the major axis instead
                // selects finer artwork and exposes mismatched terrain tiles.
                float footprint=max(minor,major/16.0);
                float factor=exp2(lod)/max(footprint,1e-8);
                t=textureGrad(tex,uv,uv_dx*factor,uv_dy*factor);
            } else t=textureLod(tex,uv,lod);
        } else if (pc.pad0 >= 1048576.0) {
            t = sample_gs_mips(uint(pc.pad0));
        } else if (point) {
            ivec2 size = textureSize(tex, 0);
            ivec2 p = ivec2(floor(uv * vec2(size)));
            p = ((p % size) + size) % size;
            t = texelFetch(tex, p, 0);
        } else {
            t = textureQueryLevels(tex) > 1
              ? textureGrad(tex, uv, uv_dx, uv_dy) : texture(tex, uv);
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
