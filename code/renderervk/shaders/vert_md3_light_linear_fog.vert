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
} ubo;

layout(location = 0) in ivec4 in_old_position_packed;
layout(location = 1) in ivec4 in_new_position_packed;
layout(location = 2) in vec2  in_tex_coord0;
layout(location = 3) in uint  in_old_normal_packed;
layout(location = 4) in uint  in_new_normal_packed;

layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out vec3 N;
layout(location = 2) out vec4 L;
layout(location = 3) out vec4 V;
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

    frag_tex_coord = in_tex_coord0;
    N = normal;
    L = ubo.lightPos - pos4;
    V = ubo.eyePos - pos4;

    fog_tex_coord = vec2(dot(pos4, ubo.fogDistanceVector), dot(pos4, ubo.fogDepthVector));
}