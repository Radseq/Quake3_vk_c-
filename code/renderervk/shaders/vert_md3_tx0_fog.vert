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
} ubo;

layout(location = 0) in ivec4 in_old_position_packed;
layout(location = 1) in ivec4 in_new_position_packed;
layout(location = 2) in vec4 in_color0;
layout(location = 3) in vec2 in_tex_coord0;

layout(location = 0) out vec4 frag_color0;
layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 4) out vec2 fog_tex_coord;

const float kMd3PositionScale = 1.0 / 64.0;

vec4 decode_md3_position(const ivec4 p)
{
    return vec4(vec3(p.xyz) * kMd3PositionScale, 1.0);
}




vec2 ApplyGpuTcMods(vec3 position, vec2 st)
{
    vec2 tc = st;

    if (ubo.tcMod0.w > 0.5)
    {
        tc = vec2(
            dot(position, ubo.tcGenVector0.xyz) + ubo.tcGenVector0.w,
            dot(position, ubo.tcGenVector1.xyz) + ubo.tcGenVector1.w
        );
    }

    return vec2(
        tc.x * ubo.tcMod0.x + tc.y * ubo.tcMod0.y + ubo.tcMod0.z,
        tc.x * ubo.tcMod1.x + tc.y * ubo.tcMod1.y + ubo.tcMod1.z
    );
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
    vec4 oldPos = decode_md3_position(in_old_position_packed);
    vec4 newPos = decode_md3_position(in_new_position_packed);

    vec4 pos = oldPos * pc.md3Anim.y + newPos * pc.md3Anim.x;

    gl_Position = pc.mvp * pos;
    frag_color0 = in_color0;
    frag_tex_coord0 = ApplyGpuTcMods(pos.xyz, in_tex_coord0);

    fog_tex_coord = calc_fog_tc(pos);
}