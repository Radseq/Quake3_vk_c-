#version 450

layout(push_constant) uniform Transform
{
    mat4 mvp;
    vec4 md3Anim; // x = frontlerp, y = backlerp
} pc;

layout(set = 0, binding = 0, std140) uniform UBO
{
    vec4 eyePos;
    vec4 lightPos;
    vec4 lightColor;
    vec4 lightVector;
    vec4 fogDistanceVector;
    vec4 fogDepthVector;
    vec4 fogEyeT;
    vec4 fogColor;
vec4 tcMod0;
vec4 tcMod1;
vec4 tcGenVector0;
vec4 tcGenVector1;
vec4 deform0;
vec4 deform1;
} ubo;

const float kMd3PositionScale = 1.0 / 64.0;
const float kMd3AngleScale = 6.28318530717958647692 / 256.0;

vec3 decode_md3_position(const ivec4 packed)
{
    return vec3(packed.xyz) * kMd3PositionScale;
}

vec3 decode_md3_normal(const uint packed)
{
    const float lat = float((packed >> 8u) & 0xFFu) * kMd3AngleScale;
    const float lng = float(packed & 0xFFu) * kMd3AngleScale;
    const float sinLng = sin(lng);
    return vec3(cos(lat) * sinLng, sin(lat) * sinLng, cos(lng));
}

vec3 safe_normalize(vec3 v)
{
    const float len2 = dot(v, v);
    if (len2 <= 1e-20)
    {
        return vec3(0.0, 0.0, 1.0);
    }
    return v * inversesqrt(len2);
}

layout(location = 0) in ivec4 in_old_position_packed;
layout(location = 1) in ivec4 in_new_position_packed;
layout(location = 2) in vec4  in_color0;
layout(location = 3) in vec2  in_tex_coord0;
layout(location = 4) in uint  in_old_normal_packed;
layout(location = 5) in uint  in_new_normal_packed;

layout(location = 0) out vec4 frag_color0;
layout(location = 1) out vec2 frag_tex_coord0;

out gl_PerVertex
{
    vec4 gl_Position;
};


vec2 calc_env_tc_regular(vec3 position, vec3 normal)
{
    const vec3 viewer = safe_normalize(ubo.eyePos.xyz - position);
    const float d = dot(normal, viewer);
    const vec2 reflected = normal.yz * (2.0 * d) - viewer.yz;
    return vec2(0.5 + reflected.x * 0.5, 0.5 - reflected.y * 0.5);
}

vec2 calc_env_tc_fp(vec3 position, vec3 normal)
{
    const vec3 why    = safe_normalize(ubo.lightColor.xyz - position);
    const vec3 who    = safe_normalize(ubo.lightVector.xyz - position);
    const vec3 where  = safe_normalize(ubo.lightPos.xyz - position);
    const vec3 viewer = safe_normalize(ubo.eyePos.xyz - position);

    const float d = dot(normal, viewer);

    vec2 reflected;
    reflected.x = normal.y * (2.0 * d) - viewer.y - (where.y * 5.0) + (why.y * 4.0);
    reflected.y = normal.z * (2.0 * d) - viewer.z - (where.z * 5.0) + (who.z * 4.0);

    return vec2(0.33 + reflected.x * 0.33, 0.33 - reflected.y * 0.33);
}

vec2 calc_env_tc_fpscr(vec3 position, vec3 normal)
{
    const vec3 viewer = safe_normalize(ubo.eyePos.xyz - position);
    const float d = dot(normal, viewer);

    vec2 reflected;
    reflected.x = normal.y * (2.0 * d) - viewer.y;
    reflected.y = normal.z * (2.0 * d) - viewer.z;

    return vec2(0.5 - reflected.x * 0.5, 0.5 + reflected.y * 0.5);
}


vec3 ApplyGpuDeform(vec3 position, vec3 normal, vec2 baseSt)
{
    const int mode = int(ubo.deform0.x + 0.5);

    if (mode == 1)
    {
        float phaseNow = ubo.deform0.w;
        if (ubo.deform1.z > 0.5)
        {
            phaseNow += (position.x + position.y + position.z) * ubo.deform0.y;
        }

        const float scale = ubo.deform1.x + sin(phaseNow * 6.28318530717958647692) * ubo.deform1.y;
        return position + normal * scale;
    }

    if (mode == 2)
    {
        const float scale = sin(baseSt.x * ubo.deform0.y + ubo.deform0.w) * ubo.deform0.z;
        return position + normal * scale;
    }

    return position;
}

vec2 ApplyGpuTcMods(vec3 position, vec2 st)
{
    const int flags = int(ubo.tcMod0.w + 0.5);
    const bool useVectorTcGen = (flags & 1) != 0;
    const bool useTurbulent   = (flags & 2) != 0;

    vec2 tc = st;

    if (useVectorTcGen)
    {
        tc = vec2(
            dot(position, ubo.tcGenVector0.xyz) + ubo.tcGenVector0.w,
            dot(position, ubo.tcGenVector1.xyz) + ubo.tcGenVector1.w
        );
    }

    tc = vec2(
        tc.x * ubo.tcMod0.x + tc.y * ubo.tcMod0.y + ubo.tcMod0.z,
        tc.x * ubo.tcMod1.x + tc.y * ubo.tcMod1.y + ubo.tcMod1.z
    );

    if (useTurbulent)
    {
        const float now = ubo.tcGenVector0.w;
        const float amplitude = ubo.tcMod1.w;
        const float twoPi = 6.28318530717958647692;

        tc.x += sin((((position.x + position.z) * (1.0 / 1024.0)) + now) * twoPi) * amplitude;
        tc.y += sin(((position.y * (1.0 / 1024.0)) + now) * twoPi) * amplitude;

        tc = vec2(
            tc.x * ubo.tcGenVector0.x + tc.y * ubo.tcGenVector0.y + ubo.tcGenVector0.z,
            tc.x * ubo.tcGenVector1.x + tc.y * ubo.tcGenVector1.y + ubo.tcGenVector1.z
        );
    }

    return tc;
}


void main()
{
    const vec3 oldPosition = decode_md3_position(in_old_position_packed);
    const vec3 newPosition = decode_md3_position(in_new_position_packed);
    const vec3 oldNormal = decode_md3_normal(in_old_normal_packed);
    const vec3 newNormal = decode_md3_normal(in_new_normal_packed);

    const vec3 normal = safe_normalize(oldNormal * pc.md3Anim.y + newNormal * pc.md3Anim.x);
    vec3 position = oldPosition * pc.md3Anim.y + newPosition * pc.md3Anim.x;
    position = ApplyGpuDeform(position, normal, in_tex_coord0);

    const vec4 pos4 = vec4(position, 1.0);
    gl_Position = pc.mvp * pos4;
    frag_color0 = in_color0;

    if (ubo.eyePos.w > 0.5)
    {
        if (ubo.lightPos.w > 0.5)
        {
            frag_tex_coord0 = ApplyGpuTcMods(position, calc_env_tc_fpscr(position, normal));
        }
        else
        {
            frag_tex_coord0 = ApplyGpuTcMods(position, calc_env_tc_fp(position, normal));
        }
    }
    else
    {
        frag_tex_coord0 = ApplyGpuTcMods(position, calc_env_tc_regular(position, normal));
    }
}
