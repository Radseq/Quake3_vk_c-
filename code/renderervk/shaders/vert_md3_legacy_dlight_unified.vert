// Legacy dynamic-light blob projection for GPU-MD3.
// This removes CPU-side texcoord/color generation and triangle filtering
// from ProjectDlightTexture() when r_gpuAnim=1.

#version 450

layout(push_constant) uniform Transform
{
    mat4 mvp;
    vec4 md3Anim; // x = frontlerp, y = backlerp
} pc;

layout(set = 0, binding = 0, std140) uniform UBO
{
    vec4 eyePos;
    vec4 lightPos;             // xyz = light origin in local/object space, w = 1/radius
    vec4 lightColor;           // rgb = light color
    vec4 lightVector;          // x = radius, y = allowBacks, z = halfRadius
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

float GpuHash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float GpuNoise1(float x)
{
    const float i = floor(x);
    const float f = fract(x);
    const float u = f * f * (3.0 - 2.0 * f);
    return mix(GpuHash11(i), GpuHash11(i + 1.0), u) * 2.0 - 1.0;
}

float ApplyGpuWave(float phase, int func)
{
    const float twoPi = 6.28318530717958647692;
    const float t = fract(phase);

    switch (func)
    {
    case 1:
        return sin(phase * twoPi);
    case 2:
        return t < 0.5 ? 1.0 : -1.0;
    case 3:
        return t < 0.5 ? (4.0 * t - 1.0) : (3.0 - 4.0 * t);
    case 4:
        return t;
    case 5:
        return 1.0 - t;
    case 6:
        return GpuNoise1(phase * 256.0);
    default:
        return 0.0;
    }
}

float GpuNoise4(vec4 p)
{
    return GpuNoise1(dot(p, vec4(1.0, 57.0, 113.0, 271.0)));
}

vec3 ApplyGpuNormalDeform(vec3 normal, vec3 position)
{
    const int mode = int(ubo.deform0.x + 0.5);
    if (mode != 4)
    {
        return normal;
    }

    const float amp = ubo.deform0.y;
    const float now = ubo.deform0.w;
    const float scale = 0.98;

    vec3 n = normal;
    n.x += amp * GpuNoise4(vec4(position.x * scale, position.y * scale, position.z * scale, now));
    n.y += amp * GpuNoise4(vec4(100.0 + position.x * scale, position.y * scale, position.z * scale, now));
    n.z += amp * GpuNoise4(vec4(200.0 + position.x * scale, position.y * scale, position.z * scale, now));
    return safe_normalize(n);
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

layout(location = 0) in ivec4 in_old_position_packed;
layout(location = 1) in ivec4 in_new_position_packed;
layout(location = 2) in vec2  in_tex_coord0;
layout(location = 3) in uint  in_old_normal_packed;
layout(location = 4) in uint  in_new_normal_packed;

layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out vec4 frag_color;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    const vec3 oldPosition = decode_md3_position(in_old_position_packed);
    const vec3 newPosition = decode_md3_position(in_new_position_packed);
    const vec3 oldNormal = decode_md3_normal(in_old_normal_packed);
    const vec3 newNormal = decode_md3_normal(in_new_normal_packed);

    vec3 normal = safe_normalize(oldNormal * pc.md3Anim.y + newNormal * pc.md3Anim.x);
    const vec3 position = oldPosition * pc.md3Anim.y + newPosition * pc.md3Anim.x;
    normal = ApplyGpuNormalDeform(normal, position);
    const vec3 deformedPosition = ApplyGpuDeform(position, normal, in_tex_coord0);

    gl_Position = pc.mvp * vec4(deformedPosition, 1.0);

    const vec3 dist = ubo.lightPos.xyz - deformedPosition;
    frag_tex_coord = vec2(0.5 + dist.x * ubo.lightPos.w,
                          0.5 + dist.y * ubo.lightPos.w);

    float modulate = 0.0;
    const bool allowBacks = (ubo.lightVector.y > 0.5);

    if ((allowBacks || dot(dist, normal) >= 0.0) &&
        frag_tex_coord.x >= 0.0 && frag_tex_coord.x <= 1.0 &&
        frag_tex_coord.y >= 0.0 && frag_tex_coord.y <= 1.0)
    {
        const float absZ = abs(dist.z);
        const float radius = ubo.lightVector.x;
        const float halfRadius = ubo.lightVector.z;

        if (absZ <= radius)
        {
            if (absZ < halfRadius)
            {
                modulate = 1.0;
            }
            else
            {
                modulate = clamp(2.0 * (radius - absZ) * ubo.lightPos.w, 0.0, 1.0);
            }
        }
    }

    frag_color = vec4(ubo.lightColor.rgb * modulate, modulate);
}
