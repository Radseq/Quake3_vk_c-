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
vec4 tc1Mod0;
vec4 tc1Mod1;
vec4 tc1GenVector0;
vec4 tc1GenVector1;
vec4 tc2Mod0;
vec4 tc2Mod1;
vec4 tc2GenVector0;
vec4 tc2GenVector1;
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
layout(location = 2) in vec4  in_color0;
layout(location = 3) in vec2  in_tex_coord0;
layout(location = 4) in vec2  in_tex_coord1;
layout(location = 5) in vec2  in_tex_coord2;
layout(location = 6) in uint  in_old_normal_packed;
layout(location = 7) in uint  in_new_normal_packed;

layout(location = 0) out vec4 frag_color0;
layout(location = 1) out vec2 frag_tex_coord0;
layout(location = 2) out vec2 frag_tex_coord1;
layout(location = 3) out vec2 frag_tex_coord2;
layout(location = 4) out vec2 fog_tex_coord;

out gl_PerVertex
{
    vec4 gl_Position;
};


vec2 calc_env_tc_regular(vec3 position, vec3 normal)
{
    const vec3 viewer = safe_normalize(ubo.eyePos.xyz - position);
    const float d = dot(normal, viewer);
    const vec2 reflected = normal.yz * (2.0 * d) - viewer.yz;
    return vec2(0.5 + reflected.x * 0.5, 0.5 - reflected.y * 0.5);
}

vec2 calc_env_tc_fp(vec3 position, vec3 normal)
{
    const vec3 why    = safe_normalize(ubo.lightColor.xyz - position);
    const vec3 who    = safe_normalize(ubo.lightVector.xyz - position);
    const vec3 where  = safe_normalize(ubo.lightPos.xyz - position);
    const vec3 viewer = safe_normalize(ubo.eyePos.xyz - position);

    const float d = dot(normal, viewer);

    vec2 reflected;
    reflected.x = normal.y * (2.0 * d) - viewer.y - (where.y * 5.0) + (why.y * 4.0);
    reflected.y = normal.z * (2.0 * d) - viewer.z - (where.z * 5.0) + (who.z * 4.0);

    return vec2(0.33 + reflected.x * 0.33, 0.33 - reflected.y * 0.33);
}

vec2 calc_env_tc_fpscr(vec3 position, vec3 normal)
{
    const vec3 viewer = safe_normalize(ubo.eyePos.xyz - position);
    const float d = dot(normal, viewer);

    vec2 reflected;
    reflected.x = normal.y * (2.0 * d) - viewer.y;
    reflected.y = normal.z * (2.0 * d) - viewer.z;

    return vec2(0.5 - reflected.x * 0.5, 0.5 + reflected.y * 0.5);
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

vec2 ApplyGpuSecondaryTc(
    vec3 position,
    vec3 normal,
    vec2 rawTc,
    vec2 baseTc,
    vec4 mod0,
    vec4 mod1,
    vec4 gen0,
    vec4 gen1)
{
    const int flags = int(mod0.w + 0.5);
    const bool enabled        = (flags & 4) != 0;
    const bool useVectorTcGen = (flags & 1) != 0;
    const bool useTurbulent   = (flags & 2) != 0;
    const bool useEnvRegular  = (flags & 8) != 0;
    const bool useEnvFp       = (flags & 16) != 0;
    const bool useEnvFpScr    = (flags & 32) != 0;

    if (!enabled)
    {
        return rawTc;
    }

    vec2 tc = baseTc;

    if (useEnvFpScr)
    {
        tc = calc_env_tc_fpscr(position, normal);
    }
    else if (useEnvFp)
    {
        tc = calc_env_tc_fp(position, normal);
    }
    else if (useEnvRegular)
    {
        tc = calc_env_tc_regular(position, normal);
    }
    else if (useVectorTcGen)
    {
        tc = vec2(
            dot(position, gen0.xyz) + gen0.w,
            dot(position, gen1.xyz) + gen1.w
        );
    }

    tc = vec2(
        tc.x * mod0.x + tc.y * mod0.y + mod0.z,
        tc.x * mod1.x + tc.y * mod1.y + mod1.z
    );

    if (useTurbulent)
    {
        const float now = gen0.w;
        const float amplitude = mod1.w;
        const float twoPi = 6.28318530717958647692;

        tc.x += sin((((position.x + position.z) * (1.0 / 1024.0)) + now) * twoPi) * amplitude;
        tc.y += sin(((position.y * (1.0 / 1024.0)) + now) * twoPi) * amplitude;

        tc = vec2(
            tc.x * gen0.x + tc.y * gen0.y + gen0.z,
            tc.x * gen1.x + tc.y * gen1.y + gen1.z
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



const uint GPU_MD3_COLOR_UNIFORM_DIFFUSE_RGB    = 1u << 0;
const uint GPU_MD3_COLOR_UNIFORM_SPECULAR_ALPHA = 1u << 1;
const uint GPU_MD3_COLOR_UNIFORM_SOLID_RGBA     = 1u << 2;
const uint GPU_MD3_COLOR_VERTEX_RGB             = 1u << 3;
const uint GPU_MD3_COLOR_ONE_MINUS_VERTEX_RGB   = 1u << 4;
const uint GPU_MD3_COLOR_EXACT_VERTEX_RGB       = 1u << 5;
const uint GPU_MD3_COLOR_ONE_MINUS_VERTEX_ALPHA = 1u << 6;
const uint GPU_MD3_COLOR_UNIFORM_ALPHA          = 1u << 7;
const uint GPU_MD3_COLOR_VERTEX_ALPHA           = 1u << 8;
const uint GPU_MD3_COLOR_PORTAL_ALPHA           = 1u << 9;

float CalcGpuSpecularAlpha(vec3 position, vec3 normal)
{
    const vec3 lightOrigin = vec3(-960.0, 1980.0, 96.0);
    const vec3 lightDir = safe_normalize(lightOrigin - position);
    const float d = dot(normal, lightDir);
    const vec3 reflected = normal * (2.0 * d) - lightDir;
    const vec3 viewer = ubo.eyePos.xyz - position;
    const float viewerLen2 = dot(viewer, viewer);

    if (viewerLen2 <= 1e-20)
    {
        return 0.0;
    }

    float l = dot(reflected, viewer) * inversesqrt(viewerLen2);
    if (l < 0.0)
    {
        return 0.0;
    }

    l *= l;
    l *= l;
    return clamp(l, 0.0, 1.0);
}

vec4 BuildGpuMd3Color(vec3 position, vec3 normal, vec4 fallbackColor)
{
    const uint colorMode = uint(ubo.lightVector.w + 0.5);
    const bool useGpuDiffuseRgb        = (colorMode & GPU_MD3_COLOR_UNIFORM_DIFFUSE_RGB) != 0u;
    const bool useGpuSpecularAlpha     = (colorMode & GPU_MD3_COLOR_UNIFORM_SPECULAR_ALPHA) != 0u;
    const bool useGpuSolidRgba         = (colorMode & GPU_MD3_COLOR_UNIFORM_SOLID_RGBA) != 0u;
    const bool useGpuVertexRgb         = (colorMode & GPU_MD3_COLOR_VERTEX_RGB) != 0u;
    const bool useGpuOneMinusVertexRgb = (colorMode & GPU_MD3_COLOR_ONE_MINUS_VERTEX_RGB) != 0u;
    const bool useGpuExactVertexRgb    = (colorMode & GPU_MD3_COLOR_EXACT_VERTEX_RGB) != 0u;
    const bool useGpuOneMinusVertexAlpha = (colorMode & GPU_MD3_COLOR_ONE_MINUS_VERTEX_ALPHA) != 0u;
    const bool useGpuUniformAlpha      = (colorMode & GPU_MD3_COLOR_UNIFORM_ALPHA) != 0u;
    const bool useGpuVertexAlpha       = (colorMode & GPU_MD3_COLOR_VERTEX_ALPHA) != 0u;
    const bool useGpuPortalAlpha       = (colorMode & GPU_MD3_COLOR_PORTAL_ALPHA) != 0u;

    vec4 color = fallbackColor;

    if (useGpuSolidRgba)
    {
        color.rgb = ubo.lightPos.xyz;
        color.a = ubo.lightColor.w;
    }
    else if (useGpuDiffuseRgb)
    {
        const float incoming = max(dot(normal, ubo.lightVector.xyz), 0.0);
        const vec3 rgb = clamp(ubo.lightPos.xyz + incoming * ubo.lightColor.xyz, 0.0, 1.0);
        color = vec4(rgb, 1.0);
    }
    else if (useGpuExactVertexRgb)
    {
        color.rgb = fallbackColor.rgb;
    }
    else if (useGpuVertexRgb)
    {
        color.rgb = fallbackColor.rgb * pc.md3Anim.z;
    }
    else if (useGpuOneMinusVertexRgb)
    {
        color.rgb = (vec3(1.0) - fallbackColor.rgb) * pc.md3Anim.z;
    }

    if (useGpuSpecularAlpha)
    {
        color.a = CalcGpuSpecularAlpha(position, normal);
    }
    else if (useGpuPortalAlpha)
    {
        color.a = clamp(length(ubo.eyePos.xyz - position) * ubo.fogEyeT.z, 0.0, 1.0);
    }
    else if (useGpuOneMinusVertexAlpha)
    {
        color.a = 1.0 - fallbackColor.a;
    }
    else if (useGpuVertexAlpha)
    {
        color.a = fallbackColor.a;
    }
    else if (useGpuUniformAlpha)
    {
        color.a = ubo.lightColor.w;
    }

    return clamp(color, 0.0, 1.0);
}

void main()
{
    const vec3 oldPosition = decode_md3_position(in_old_position_packed);
    const vec3 newPosition = decode_md3_position(in_new_position_packed);
    const vec3 oldNormal = decode_md3_normal(in_old_normal_packed);
    const vec3 newNormal = decode_md3_normal(in_new_normal_packed);

    vec3 normal = safe_normalize(oldNormal * pc.md3Anim.y + newNormal * pc.md3Anim.x);
    vec3 position = oldPosition * pc.md3Anim.y + newPosition * pc.md3Anim.x;
    normal = ApplyGpuNormalDeform(normal, position);
    position = ApplyGpuDeform(position, normal, in_tex_coord0);

    const vec4 pos4 = vec4(position, 1.0);
    gl_Position = pc.mvp * pos4;
    frag_color0 = BuildGpuMd3Color(position, normal, in_color0);

    if (ubo.eyePos.w > 0.5)
    {
        if (ubo.lightPos.w > 0.5)
        {
            frag_tex_coord0 = ApplyGpuTcMods(position, calc_env_tc_fpscr(position, normal));
        }
        else
        {
            frag_tex_coord0 = ApplyGpuTcMods(position, calc_env_tc_fp(position, normal));
        }
    }
    else
    {
        frag_tex_coord0 = ApplyGpuTcMods(position, calc_env_tc_regular(position, normal));
    }
    frag_tex_coord1 = ApplyGpuSecondaryTc(position, normal, in_tex_coord1, in_tex_coord0, ubo.tc1Mod0, ubo.tc1Mod1, ubo.tc1GenVector0, ubo.tc1GenVector1);
    frag_tex_coord2 = ApplyGpuSecondaryTc(position, normal, in_tex_coord2, in_tex_coord0, ubo.tc2Mod0, ubo.tc2Mod1, ubo.tc2GenVector0, ubo.tc2GenVector1);
    fog_tex_coord = calc_fog_tc(pos4);
}
