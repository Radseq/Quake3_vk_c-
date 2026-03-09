#version 450

layout(push_constant) uniform Transform
{
    mat4 mvp;
    vec4 md3Anim; // x = backlerp, y = frontlerp
} pc;

layout(location = 0) in vec4 in_old_position;
layout(location = 1) in vec4 in_new_position;
layout(location = 2) in vec2 in_tex_coord0;

layout(location = 1) out vec2 frag_tex_coord0;

void main()
{
    vec4 pos = mix(in_new_position, in_old_position, pc.md3Anim.x);
    gl_Position = pc.mvp * pos;
    frag_tex_coord0 = in_tex_coord0;
}