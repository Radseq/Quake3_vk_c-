#version 450

layout(push_constant) uniform Transform
{
    mat4 mvp;
    vec4 md3Anim; // x = backlerp, y = frontlerp
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

layout(location = 0) in vec4 in_old_position;
layout(location = 1) in vec4 in_new_position;
layout(location = 2) in vec4 in_color0;
layout(location = 3) in vec2 in_tex_coord0;

layout(location = 0) out vec4 frag_color0;
layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 4) out vec2 fog_tex_coord;

void main()
{
    vec4 pos = mix(in_new_position, in_old_position, pc.md3Anim.x);
    gl_Position = pc.mvp * pos;
    frag_color0 = in_color0;
    frag_tex_coord0 = in_tex_coord0;

    float s = dot(pos, ubo.fogDistanceVector);
    float t = dot(pos, ubo.fogDepthVector);
    fog_tex_coord = vec2(s, t);
}