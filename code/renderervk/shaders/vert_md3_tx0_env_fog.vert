#version 450

layout(push_constant) uniform Transform
{
    mat4 mvp;
    vec4 md3Anim; // x = frontlerp, y = backlerp
} pc;

layout(set = 0, binding = 0, std140) uniform UBO
{
    vec4 eyePos;             // w = isFirstPerson
    vec4 lightPos;           // xyz = backEnd.ort.origin, w = isScreenMap
    vec4 lightColor;         // xyz = backEnd.ort.axis[1]
    vec4 lightVector;        // xyz = backEnd.ort.axis[2]
    vec4 fogDistanceVector;
    vec4 fogDepthVector;
    vec4 fogEyeT;
    vec4 fogColor;
} ubo;

layout(location = 0) in ivec4 in_old_position_packed;
layout(location = 1) in ivec4 in_new_position_packed;
layout(location = 2) in vec4  in_color0;
layout(location = 4) in uint  in_old_normal_packed;
layout(location = 5) in uint  in_new_normal_packed;

layout(location = 0) out vec4 frag_color0;
layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 4) out vec2 fog_tex_coord;

out gl_PerVertex
{
    vec4 gl_Position;
};

const float kMd3PositionScale = 1.0 / 64.0;
const float kMd3AngleScale = 6.28318530717958647692 / 256.0;

vec3 decode_md3_normal(const uint packed)
{
    const float lat = float((packed >> 8u) & 0xFFu) * kMd3AngleScale;
    const float lng = float(packed & 0xFFu) * kMd3AngleScale;
    const float sinLng = sin(lng);
    return vec3(cos(lat) * sinLng, sin(lat) * sinLng, cos(lng));
}

vec2 calc_env_tc_regular(vec3 position, vec3 normal)
{
    const vec3 viewer = normalize(ubo.eyePos.xyz - position);
    const float d = dot(normal, viewer);
    const vec2 reflected = normal.yz * (2.0 * d) - viewer.yz;
    return vec2(0.5 + reflected.x * 0.5, 0.5 - reflected.y * 0.5);
}

vec2 calc_env_tc_fp(vec3 position, vec3 normal)
{
    const vec3 why   = normalize(ubo.lightColor.xyz - position);
    const vec3 who   = normalize(ubo.lightVector.xyz - position);
    const vec3 where = normalize(ubo.lightPos.xyz - position);
    const vec3 viewer = normalize(ubo.eyePos.xyz - position);

    const float d = dot(normal, viewer);

    vec2 reflected;
    reflected.x = normal.y * (2.0 * d) - viewer.y - (where.y * 5.0) + (why.y * 4.0);
    reflected.y = normal.z * (2.0 * d) - viewer.z - (where.z * 5.0) + (who.z * 4.0);

    return vec2(0.33 + reflected.x * 0.33, 0.33 - reflected.y * 0.33);
}

vec2 calc_env_tc_fpscr(vec3 position, vec3 normal)
{
    const vec3 viewer = normalize(ubo.eyePos.xyz - position);
    const float d = dot(normal, viewer);

    vec2 reflected;
    reflected.x = normal.y * (2.0 * d) - viewer.y;
    reflected.y = normal.z * (2.0 * d) - viewer.z;

    return vec2(0.5 - reflected.x * 0.5, 0.5 + reflected.y * 0.5);
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
    const vec3 oldPosition = vec3(in_old_position_packed.xyz) * kMd3PositionScale;
    const vec3 newPosition = vec3(in_new_position_packed.xyz) * kMd3PositionScale;

    const vec3 position =
        oldPosition * pc.md3Anim.y +
        newPosition * pc.md3Anim.x;

    const vec3 oldNormal = decode_md3_normal(in_old_normal_packed);
    const vec3 newNormal = decode_md3_normal(in_new_normal_packed);
    const vec3 normal = normalize(oldNormal * pc.md3Anim.y + newNormal * pc.md3Anim.x);

    const vec4 pos4 = vec4(position, 1.0);

    gl_Position = pc.mvp * pos4;
    frag_color0 = in_color0;

    if (ubo.eyePos.w > 0.5)
    {
        if (ubo.lightPos.w > 0.5)
        {
            frag_tex_coord0 = calc_env_tc_fpscr(position, normal);
        }
        else
        {
            frag_tex_coord0 = calc_env_tc_fp(position, normal);
        }
    }
    else
    {
        frag_tex_coord0 = calc_env_tc_regular(position, normal);
    }

    fog_tex_coord = calc_fog_tc(pos4);
}