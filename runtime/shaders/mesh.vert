#version 450

layout(location = 0) in vec4  in_pos;
layout(location = 1) in ivec4 in_nrm;
layout(location = 2) in ivec4 in_col;
layout(location = 3) in vec4  in_uv;

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec3 v_stq;
layout(location = 2) out float v_fog;
layout(location = 3) flat out uint v_round_uv;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[1];
};

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
    uint  mesh_block;
    uint  mesh_pad;
} pc;

struct Block {
    vec4  m[4];
    vec4  c[4];
    vec4  l[3];
    vec4  r[4];
    vec4  e[3];
    float fog0, fog1, zscale, zmax;
    float xoff, yoff, spec_alpha, pad0;
    uint  mode, pass, clip, pad1;
    uint  prog, pad2, pad3, pad4;
    vec4  p[4];
};

layout(std430, set = 2, binding = 0) readonly buffer Blocks { Block b[]; };

// Construct correctly rounded fog/255 without an approximate GPU reciprocal.
// For bytes 1..254 the significand numerator fits uint32, including rounding.
float ac4_fog_normalized(uint value) {
    if (value == 0u) return 0.0;
    if (value == 255u) return 1.0;
    uint exponent = uint(findMSB(value));
    uint significand = ((value << (31u - exponent)) + 127u) / 255u;
    return uintBitsToFloat(((exponent + 119u) << 23u) | (significand & 0x7fffffu));
}

void main() {
    Block k = b[pc.mesh_block];
    if (k.prog == 10u) {
        // AC4 sprite world projection, clipping and horizontal row subdivision.
        precise vec4 h=k.m[0]*in_pos.x;
        h=h+k.m[1]*in_pos.y;h=h+k.m[2]*in_pos.z;h=h+k.m[3]*in_pos.w;
        precise float q=1.0/h.w;
        q=fma(q,fma(-h.w,q,1.0),q);
        precise vec2 center=h.xy*q;
        precise float size=k.p[0].x*q;
        precise vec2 extent=k.c[0].xy*size;
        precise vec2 lo=center-extent,hi=center+extent;
        if(lo.x<k.c[3].x)lo.x=k.c[1].x;
        if(lo.y<k.c[3].y)lo.y=k.c[1].y;
        if(hi.x>k.c[3].z)hi.x=k.c[2].x;
        if(hi.y>k.c[3].w)hi.y=k.c[2].y;
        uvec2 a=uvec2(ivec2(lo*16.0))&uvec2(65535u);
        uvec2 z=uvec2(ivec2(hi*16.0))&uvec2(65535u);
        uint edge=(a.x&~1023u)+uint(in_nrm.z)*1024u;
        float x0=float(max(a.x,edge))/16.0-k.xoff+0.5;
        float x1=float(min(z.x,edge+1024u))/16.0-k.xoff+0.5;
        float p0=ceil(x0-0.5),p1=ceil(x1-0.5);
        if(x1>x0 && p1>p0){x0=p0;x1=p1;}
        float x=in_nrm.x!=0?x1:x0;
        float y0=float(a.y)/16.0-k.yoff+0.5,y1=float(z.y)/16.0-k.yoff+0.5;
        // Ordinary GS sprites receive this same correction at vertex upload.
        bool snap_rows=pc.scale>1.0 && (uint(in_nrm.w)&(1u<<29u))!=0u;
        float sy0=ceil(y0-0.5),sy1=ceil(y1-0.5);
        if(snap_rows && y1>y0 && sy1>sy0){y0=sy0;y1=sy1;}
        float y=in_nrm.y!=0?y1:y0;
        precise float depth=k.p[0].y/k.zmax;
        depth=min(fma(fma(-depth,k.zmax,k.p[0].y),1.0/k.zmax,depth),1.0);
        gl_Position=vec4(vec2(x,y)*pc.inv_size-1.0,depth,1.0);
        gl_ClipDistance[0]=1.0;gl_PointSize=pc.point_size;
        v_stq=vec3(in_uv.x,snap_rows?in_uv.z:in_uv.y,1.0);v_color=vec4(in_col&ivec4(255))/128.0;
        v_fog=1.0;v_round_uv=uint(in_nrm.w);return;
    }
    if (k.prog == 9u) {
        // AC4 cloud planes: raw world projection and original affine STQ.
        precise vec4 h = k.m[0] * in_pos.x;
        h = h + k.m[1] * in_pos.y;
        h = h + k.m[2] * in_pos.z;
        h = h + k.m[3] * in_pos.w;
        precise float reciprocal = 1.0 / h.w;
        reciprocal = fma(reciprocal, fma(-h.w,reciprocal,1.0),reciprocal);
        precise vec2 screen = h.xy * reciprocal;
        screen = screen * 16.0;
        uvec2 packed = uvec2(ivec2(screen)) & uvec2(65535u);
        vec2 xy = vec2(packed)/16.0 + vec2(0.5-k.xoff,0.5-k.yoff);
        float depth = float(uint(in_nrm.y));
        precise float z = depth/k.zmax;
        z = min(fma(fma(-z,k.zmax,depth),1.0/k.zmax,z),1.0);
        gl_Position = vec4(xy*pc.inv_size-1.0,z,1.0);
        gl_ClipDistance[0] = 1.0;
        gl_PointSize = pc.point_size;
        precise float q = in_uv.z * reciprocal;
        precise vec2 st = in_uv.xy * q;
        v_stq = vec3(st,q);
        v_color = vec4(in_col & ivec4(255))/128.0;
        v_fog = 1.0;
        v_round_uv = 0u;
        return;
    }
    if (k.prog == 8u) {
        // AC4 dome: scale columns before projection, preserving EE roundings.
        precise vec4 c0 = k.m[0] * in_uv.x;
        precise vec4 c1 = k.m[1] * in_uv.y;
        precise vec4 c2 = k.m[2] * in_uv.x;
        precise vec4 h = c0 * in_pos.x;
        h = h + c1 * in_pos.y;
        h = h + c2 * in_pos.z;
        h = h + k.m[3] * in_pos.w;
        uvec2 packed;
        if (in_nrm.z != 0) {
            // Preserve the stale closing endpoint selected by the EE mask.
            packed = uvec2(uint(in_nrm.x) & 65535u, uint(in_nrm.x) >> 16u);
        } else {
            precise float q = 1.0 / h.w;
            q = fma(q, fma(-h.w, q, 1.0), q);
            precise vec2 xy = h.xy * q;
            xy = xy * 16.0;
            packed = uvec2(ivec2(xy)) & uvec2(65535u);
        }
        vec2 xy = vec2(packed) / 16.0 + vec2(0.5-k.xoff, 0.5-k.yoff);
        float depth = float(uint(in_nrm.y));
        precise float z = depth / k.zmax;
        z = min(fma(fma(-z,k.zmax,depth),1.0/k.zmax,z),1.0);
        gl_Position = vec4(xy * pc.inv_size - 1.0,z,1.0);
        gl_ClipDistance[0] = 1.0;
        gl_PointSize = pc.point_size;
        v_stq = vec3(0.0,0.0,1.0);
        v_color = vec4(in_col & ivec4(255)) / 128.0;
        v_fog = 1.0;
        v_round_uv = 0u;
        return;
    }
    if (k.prog == 6u || k.prog == 7u) {
        // AC4 raw model records. Keep VU multiply/add boundaries and GS packing.
        precise vec4 h = k.m[0] * in_pos.x;
        h = h + k.m[1] * in_pos.y;
        h = h + k.m[2] * in_pos.z;
        h = h + k.m[3] * in_pos.w;
        precise float q = 1.0 / h.w;
        // Vulkan division can use an approximate reciprocal. Refine its residual:
        // a one-ULP error can move GS fixed-point coordinates across a boundary.
        q = fma(q, fma(-h.w, q, 1.0), q);
        precise vec3 screen = h.xyz * q;
        if (k.prog == 6u && (k.mode & 4u) != 0u) screen.z = screen.z * 16.0;
        screen = screen * 16.0;
        uvec3 packed = uvec3(ivec3(screen));
        vec2 xy = vec2(packed.xy & uvec2(65535u)) / 16.0;
        xy = xy + vec2(0.5 - k.xoff, 0.5 - k.yoff);
        float depth = float((packed.z >> 4u) & 0xffffffu);
        precise float z = depth / k.zmax;
        // Match coplanar GS fallback passes at normalized depth boundaries.
        z = min(fma(fma(-z, k.zmax, depth), 1.0 / k.zmax, z), 1.0);
        gl_Position = vec4(xy * pc.inv_size - 1.0, z, 1.0);
        gl_ClipDistance[0] = 1.0;
        gl_PointSize = pc.point_size;
        precise vec3 uvq = in_uv.xyz * q;
        v_stq = uvq;
        precise float fog = -h.w + k.fog0;
        fog = fog * k.fog1;
        uint f = uint(clamp(fog, 0.0, 255.0) * 16.0);
        v_fog = ac4_fog_normalized((f >> 4u) & 255u);
        v_round_uv = 0u;
        ivec4 col = in_col;
        if (k.prog == 7u) {
            precise vec4 source = intBitsToFloat(in_nrm);
            col = ivec4(source);
            if (k.mode != 0u) {
                precise float alpha = source.w * uintBitsToFloat(0x3b808080u);
                alpha = alpha * clamp(fog, 0.0, 255.0);
                col = ivec4(128, 128, 128, int(alpha));
            }
        } else if ((k.mode & 3u) != 0u) {
            vec3 normal = vec3(in_nrm.xyz) / 4096.0;
            precise vec4 n = k.c[0] * normal.x;
            n = n + k.c[1] * normal.y;
            n = n + k.c[2] * normal.z;
            if ((k.mode & 3u) == 2u) {
                precise float u = (0.0 - n.x) + 1.0;
                precise float v = (0.0 + n.z) + 1.0;
                u = u * 0.5; v = v * 0.5;
                v_stq = vec3(u * q, v * q, q);
                col = n.y < 0.0 ? ivec4(0, 0, 0, 128) : ivec4(128);
            } else {
            precise vec4 light = max(k.l[0] * n.x, vec4(0.0));
            light = light + max(k.l[1] * n.y, vec4(0.0));
            light = light + max(k.l[2] * n.z, vec4(0.0));
            light = light + max(k.p[0] * n.w, vec4(0.0));
            precise vec4 color = k.r[0] * light.x;
            color = color + k.r[1] * light.y;
            color = color + k.r[2] * light.z;
            color = color + k.r[3] * light.w;
            precise vec3 rgb = max(color.xyz, vec3(0.0)) + k.e[0].xyz;
            rgb = rgb * vec3(in_col.xyz);
            col.xyz = ivec3(clamp(rgb, 0.0, 255.0));
            }
        }
        v_color = vec4(col & ivec4(255)) / 128.0;
        return;
    }
    vec3 p = in_pos.xyz;
    vec4 s = k.m[0] * p.x + k.m[1] * p.y + k.m[2] * p.z + k.m[3];
    float w = s.w;

    float X = (s.x + (0.5 - k.xoff) * w) * pc.inv_size.x - w;
    float Y = (s.y + (0.5 - k.yoff) * w) * pc.inv_size.y - w;
    float zlim = min(16777215.0, k.zmax);
    float Z = min(max(s.z * k.zscale, 0.0), zlim * max(w, 0.0)) / k.zmax;
    gl_Position = vec4(X, Y, Z, w);

    vec4 cc = k.c[0] * p.x + k.c[1] * p.y + k.c[2] * p.z + k.c[3];
    gl_ClipDistance[0] = k.clip != 0u ? cc.z + cc.w : 1.0;

    float fog = clamp(k.fog0 + w * k.fog1, 0.0, 255.0);
    v_fog = fog / 255.0;
    v_round_uv = 0u;
    gl_PointSize = pc.point_size;

    if (k.prog == 5u) {
        gl_Position = vec4(in_pos.x * pc.inv_size.x - 1.0,
                           in_pos.y * pc.inv_size.y - 1.0, in_pos.z, 1.0);
        gl_ClipDistance[0] = 1.0;
        v_stq = in_uv.xyz;
        v_color = vec4(in_col) / 128.0;
        v_fog = in_uv.w;
        v_round_uv = uint(in_nrm.x);
        return;
    }

    if (k.prog == 3u) {
        float iw = 1.0 / w;
        vec3 c = s.xyz * iw;
        vec2 corner = c.xy + vec2(in_nrm.xy) * k.p[0].xy * in_uv.z * iw;
        float zg = clamp(floor(floor(c.z * 16.0) / 8.0), k.p[0].z, zlim);
        gl_Position = c.z > 0.0
            ? vec4((corner.x + 0.5 - k.xoff) * pc.inv_size.x - 1.0,
                   (corner.y + 0.5 - k.yoff) * pc.inv_size.y - 1.0, zg / k.zmax, 1.0)
            : vec4(2.0, 2.0, 2.0, 1.0);
        gl_ClipDistance[0] = 1.0;
        v_stq = vec3(in_uv.xy, 1.0);
        v_color = vec4(in_col & 255) / 128.0;
        v_fog = 1.0;
        return;
    }

    if (k.prog == 4u) {
        v_stq = in_uv.xyz;
        v_color = vec4(in_col & 255) / 128.0;
        return;
    }

    if (k.prog == 2u) {
        if (k.pass == 0u) {
            v_stq = vec3(in_uv.xy, 1.0);
            v_color = vec4(in_col & 255) / 128.0;
        } else {
            v_stq = vec3(in_uv.xy * 2.0, 1.0);
            v_color = vec4(vec3(clamp((k.p[0].z - cc.w) * k.p[0].w * 128.0, 0.0, 128.0)),
                           128.0) / 128.0;
        }
        return;
    }

    if (k.prog == 1u) {
        vec2 uv;
        vec4 col;
        if (k.pass == 0u) {
            uv = in_uv.xy + in_uv.zw;
            col = vec4(in_col & 255);
        } else if (k.pass == 1u) {
            uv = in_uv.xy * 16.0;
            col = vec4(vec3(clamp((k.p[0].z - cc.w) * k.p[0].w * 128.0, 0.0, 128.0)),
                       k.p[3].x);
        } else if (k.pass == 2u) {
            uv = in_uv.xy * 16.0 + k.p[1].xy;
            col = vec4(vec3(clamp((k.p[1].z - cc.w) * k.p[1].w * 128.0, 0.0, 128.0)),
                       fog * 0.5);
        } else {
            uv = (p.xz - k.p[2].xz) * k.p[2].w;
            col = vec4(fog * 0.5);
        }
        v_stq = vec3(uv, 1.0);
        v_color = col / 128.0;
        return;
    }

    if (k.pass == 0u) {
        v_stq = vec3(in_uv.xy, 1.0);
        if (k.mode == 0u) {
            v_color = vec4(in_col & 255) / 128.0;
        } else {
            vec3 n = vec3(in_nrm.xyz) / 4096.0;
            vec4 l = vec4(k.l[0].xyz * n.x + k.l[1].xyz * n.y + k.l[2].xyz * n.z, 1.0);
            l = l * l * l;
            l.xyz *= 2.0;
            l = max(l, 0.0);
            vec4 li = max(k.r[0] * l.x + k.r[1] * l.y + k.r[2] * l.z + k.r[3] * l.w, 0.0);
            vec3 c = min(vec3(in_col.xyz) * li.xyz, vec3(255.0));
            v_color = vec4(c, 128.0) / 128.0;
        }
    } else {
        vec3 n = vec3(in_nrm.xyz) / 4096.0;
        vec3 nb = k.e[0].xyz * n.x + k.e[1].xyz * n.y + k.e[2].xyz * n.z;
        v_stq = vec3((nb.x + 1.0) * 0.5, (nb.z + 1.0) * 0.5, 1.0);
        float spec = min(max(nb.y, 0.0) * 1280.0, 128.0);
        v_color = vec4(vec3(spec), k.spec_alpha) / 128.0;
    }
}
