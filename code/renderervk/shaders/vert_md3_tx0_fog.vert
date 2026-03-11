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

void main()
{
    vec4 oldPos = decode_md3_position(in_old_position_packed);
    vec4 newPos = decode_md3_position(in_new_position_packed);

    vec4 pos = oldPos * pc.md3Anim.y + newPos * pc.md3Anim.x;

    gl_Position = pc.mvp * pos;
    frag_color0 = in_color0;
    frag_tex_coord0 = in_tex_coord0;

    float s = dot(pos, ubo.fogDistanceVector);
    float t = dot(pos, ubo.fogDepthVector);
    fog_tex_coord = vec2(s, t);
}