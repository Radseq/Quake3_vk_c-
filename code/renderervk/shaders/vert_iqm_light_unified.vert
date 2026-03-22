// IQM GPU skinning unified vertex shader (single texture lighting path)

#version 450

layout(push_constant) uniform Transform
{
    mat4 mvp;
    vec4 md3Anim; // x = frontlerp, y = backlerp, z = identityLight
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
    vec4 iqmJointMat[128 * 3];
} ubo;

vec3 safe_normalize(vec3 v)
{
    const float len2 = dot(v, v);
    if (len2 <= 1e-20)
        return vec3(0.0, 0.0, 1.0);
    return v * inversesqrt(len2);
}

vec4 iqm_joint_row(uint jointIndex, int row)
{
    return ubo.iqmJointMat[int(jointIndex) * 3 + row];
}

vec3 skin_position(vec3 position, uvec4 jointIndex, vec4 jointWeight)
{
    vec3 result = vec3(0.0);
    for (int i = 0; i < 4; ++i)
    {
        const float w = jointWeight[i];
        if (w <= 0.0)
            continue;
        const vec4 r0 = iqm_joint_row(jointIndex[i], 0);
        const vec4 r1 = iqm_joint_row(jointIndex[i], 1);
        const vec4 r2 = iqm_joint_row(jointIndex[i], 2);
        result += vec3(dot(r0, vec4(position, 1.0)), dot(r1, vec4(position, 1.0)), dot(r2, vec4(position, 1.0))) * w;
    }
    return result;
}

vec3 skin_normal(vec3 normal, uvec4 jointIndex, vec4 jointWeight)
{
    vec3 result = vec3(0.0);
    for (int i = 0; i < 4; ++i)
    {
        const float w = jointWeight[i];
        if (w <= 0.0)
            continue;
        const vec4 r0 = iqm_joint_row(jointIndex[i], 0);
        const vec4 r1 = iqm_joint_row(jointIndex[i], 1);
        const vec4 r2 = iqm_joint_row(jointIndex[i], 2);
        result += vec3(dot(r0.xyz, normal), dot(r1.xyz, normal), dot(r2.xyz, normal)) * w;
    }
    return safe_normalize(result);
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
    case 1: return sin(phase * twoPi);
    case 2: return t < 0.5 ? 1.0 : -1.0;
    case 3: return t < 0.5 ? (4.0 * t - 1.0) : (3.0 - 4.0 * t);
    case 4: return t;
    case 5: return 1.0 - t;
    case 6: return GpuNoise1(phase * 256.0);
    default: return 0.0;
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
        return normal;
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
            phaseNow += (position.x + position.y + position.z) * ubo.deform0.y;
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
        tc = vec2(dot(position, ubo.tcGenVector0.xyz) + ubo.tcGenVector0.w,
                  dot(position, ubo.tcGenVector1.xyz) + ubo.tcGenVector1.w);
    tc = vec2(tc.x * ubo.tcMod0.x + tc.y * ubo.tcMod0.y + ubo.tcMod0.z,
              tc.x * ubo.tcMod1.x + tc.y * ubo.tcMod1.y + ubo.tcMod1.z);
    if (useTurbulent)
    {
        const float now = ubo.tcGenVector0.w;
        const float amplitude = ubo.tcMod1.w;
        const float twoPi = 6.28318530717958647692;
        tc.x += sin((((position.x + position.z) * (1.0 / 1024.0)) + now) * twoPi) * amplitude;
        tc.y += sin(((position.y * (1.0 / 1024.0)) + now) * twoPi) * amplitude;
        tc = vec2(tc.x * ubo.tcGenVector0.x + tc.y * ubo.tcGenVector0.y + ubo.tcGenVector0.z,
                  tc.x * ubo.tcGenVector1.x + tc.y * ubo.tcGenVector1.y + ubo.tcGenVector1.z);
    }
    return tc;
}

vec2 calc_fog_tc(const vec4 pos4)
{
    float s = dot(pos4, ubo.fogDistanceVector);
    float t = dot(pos4, ubo.fogDepthVector);
    if (ubo.fogEyeT.y == 1.0)
        t = (t < 0.0) ? (1.0 / 32.0) : (31.0 / 32.0);
    else if (t < 1.0)
        t = 1.0 / 32.0;
    else
        t = 1.0 / 32.0 + (30.0 / 32.0 * t) / (t - ubo.fogEyeT.x);
    return vec2(s, t);
}

layout(location = 0) in vec4 in_position;
layout(location = 2) in vec2 in_tex_coord0;
layout(location = 3) in vec4 in_normal;
layout(location = 4) in uvec4 in_joint_index;
layout(location = 5) in vec4 in_joint_weight;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out vec3 N;
layout(location = 2) out vec4 L;
layout(location = 3) out vec4 V;
layout(location = 4) out vec2 fog_tex_coord;

void main()
{
    vec3 normal = skin_normal(in_normal.xyz, in_joint_index, in_joint_weight);
    vec3 position = skin_position(in_position.xyz, in_joint_index, in_joint_weight);
    normal = ApplyGpuNormalDeform(normal, position);
    position = ApplyGpuDeform(position, normal, in_tex_coord0);
    const vec4 pos4 = vec4(position, 1.0);
    gl_Position = pc.mvp * pos4;
    frag_tex_coord = ApplyGpuTcMods(position, in_tex_coord0);
    N = normal;
    L = ubo.lightPos - pos4;
    V = ubo.eyePos - pos4;
    fog_tex_coord = calc_fog_tc(pos4);
}
