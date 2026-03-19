#version 450

layout(set = 1, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

void main()
{
    if (frag_color.a <= 0.0)
    {
        discard;
    }

    if (frag_tex_coord.x < 0.0 || frag_tex_coord.x > 1.0 ||
        frag_tex_coord.y < 0.0 || frag_tex_coord.y > 1.0)
    {
        discard;
    }

    out_color = texture(texture0, frag_tex_coord) * vec4(frag_color.rgb, 1.0);
}
