// Unified MD3 vertex shader: supports both fog and non-fog pipelines.
// Non-fog fragment shaders simply ignore fog_tex_coord.

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
layout(location = 2) in vec2 in_tex_coord0;
layout(location = 4) in uint in_old_normal_packed;
layout(location = 5) in uint in_new_normal_packed;

layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 4) out vec2 fog_tex_coord;


float ApplyGpuWave(float phase, int func)
{
    const float twoPi = 6.28318530717958647692;
    const float t = fract(phase);

    if (func == 1)
    {
        return sin(phase * twoPi);
    }
    if (func == 2)
    {
        return t < 0.5 ? 1.0 : -1.0;
    }
    if (func == 3)
    {
        return t < 0.5 ? (4.0 * t - 1.0) : (3.0 - 4.0 * t);
    }
    if (func == 4)
    {
        return t;
    }
    if (func == 5)
    {
        return 1.0 - t;
    }

    return 0.0;
}

vec3 ApplyGpuDeform(vec3 position, vec3 normal, vec2 baseSt)
{
    const int mode = int(ubo.deform0.x + 0.5);

    if (mode == 1)
    {
        float phaseNow = ubo.deform0.w;
        if (ubo.deform1.w > 0.5)
        {
            phaseNow += (position.x + position.y + position.z) * ubo.deform0.y;
        }

        const int func = int(ubo.deform1.z + 0.5);
        const float scale = ubo.deform1.x + ApplyGpuWave(phaseNow, func) * ubo.deform1.y;
        return position + normal * scale;
    }

    if (mode == 2)
    {
        const float scale = sin(baseSt.x * ubo.deform0.y + ubo.deform0.w) * ubo.deform0.z;
        return position + normal * scale;
    }

    if (mode == 3)
    {
        const int func = int(ubo.deform1.z + 0.5);
        const float scale = ubo.deform1.x + ApplyGpuWave(ubo.deform1.w, func) * ubo.deform1.y;
        return position + ubo.deform0.yzw * scale;
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


vec2 calc_fog_tc(const vec4 pos4)
{
    float s = dot(pos4, ubo.fogDistanceVector);
    float t = dot(pos4, ubo.fogDepthVector);

    if (ubo.fogEyeT.y == 1.0)
    {
        if (t < 0.0)
        {
            t = 1.0 / 32.0;
        }
        else
        {
            t = 31.0 / 32.0;
        }
    }
    else
    {
        if (t < 1.0)
        {
            t = 1.0 / 32.0;
        }
        else
        {
            t = 1.0 / 32.0 + (30.0 / 32.0 * t) / (t - ubo.fogEyeT.x);
        }
    }

    return vec2(s, t);
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
    frag_tex_coord0 = ApplyGpuTcMods(position, in_tex_coord0);
    fog_tex_coord = calc_fog_tc(pos4);
}
