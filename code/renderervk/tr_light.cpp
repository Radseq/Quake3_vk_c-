/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/ort modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
ort (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY ort FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_light.hpp"
#include "math.hpp"
#include <assert.h>
#include "tr_soa_frame.hpp"
#include "tr_soa_stage2.hpp"

constexpr int DLIGHT_AT_RADIUS = 16;
// at the edge of a dlight's influence, this amount of light will be added

constexpr int DLIGHT_MINIMUM_RADIUS = 16;
// never calculate a range less than this to prevent huge light numbers

extern cvar_t *r_ambientScale;
extern cvar_t *r_directedScale;
extern cvar_t *r_debugLight;

/*
===============
R_TransformDlights

Transforms the origins of an array of dlights.
Used by both the front end (for DlightBmodel) and
the back end (before doing the lighting calculation)
===============
*/
void R_TransformDlights(const int count, dlight_t *dl, const orientationr_t &ort)
{
    if (count == 0) return;
#if defined(USE_AoS_to_SoA_SIMD) 
    auto& soa = trsoa::GetFrameSoA();
    if (trsoa::SoA_ValidThisFrame(soa))
    {
        // view dlights (PMLIGHT / world recursion)
        if (dl == tr.viewParms.dlights && count == soa.dlights_view.count)
        {
            trsoa::TransformDlights_SoA_Writeback(soa.dlights_view, dl, ort);
            return;
        }

        // refdef dlights (entity lighting / DlightBmodel)
        if (dl == tr.refdef.dlights && count == soa.dlights_refdef.count)
        {
            trsoa::TransformDlights_SoA_Writeback(soa.dlights_refdef, dl, ort);
            return;
        }
    }
#endif
    vec3_t temp{}, temp2{};

    for (int i = 0; i < count; i++, dl++)
    {
        VectorSubtract(dl->origin, ort.origin, temp);
        dl->transformed[0] = DotProduct(temp, ort.axis[0]);
        dl->transformed[1] = DotProduct(temp, ort.axis[1]);
        dl->transformed[2] = DotProduct(temp, ort.axis[2]);
        if (dl->linear)
        {
            VectorSubtract(dl->origin2, ort.origin, temp2);
            dl->transformed2[0] = DotProduct(temp2, ort.axis[0]);
            dl->transformed2[1] = DotProduct(temp2, ort.axis[1]);
            dl->transformed2[2] = DotProduct(temp2, ort.axis[2]);
        }
    }
}

#ifdef USE_LEGACY_DLIGHTS
/*
=============
R_DlightBmodel

Determine which dynamic lights may effect this bmodel
=============
*/
void R_DlightBmodel(bmodel_t &bmodel)
{
    int j;
    int mask;
    msurface_t *surf;

    // transform all the lights
    R_TransformDlights(tr.refdef.num_dlights, tr.refdef.dlights, tr.ort);

    mask = 0;
    for (uint32_t i = 0; i < tr.refdef.num_dlights; i++)
    {
        const dlight_t &dl = tr.refdef.dlights[i];

        // see if the point is close enough to the bounds to matter
        for (j = 0; j < 3; j++)
        {
            if (dl.transformed[j] - bmodel.bounds[1][j] > dl.radius)
            {
                break;
            }
            if (bmodel.bounds[0][j] - dl.transformed[j] > dl.radius)
            {
                break;
            }
        }
        if (j < 3)
        {
            continue;
        }

        // we need to check this light
        mask |= 1 << i;
    }

    tr.currentEntity->needDlights = (mask != 0) ? 1 : 0;

    // set the dlight bits in all the surfaces
    for (auto i = 0; i < bmodel.numSurfaces; i++)
    {
        surf = bmodel.firstSurface + i;

        if (*surf->data == surfaceType_t::SF_FACE)
        {
            ((srfSurfaceFace_t *)surf->data)->dlightBits = mask;
        }
        else if (*surf->data == surfaceType_t::SF_GRID)
        {
            ((srfGridMesh_t *)surf->data)->dlightBits = mask;
        }
        else if (*surf->data == surfaceType_t::SF_TRIANGLES)
        {
            ((srfTriangles_t *)surf->data)->dlightBits = mask;
        }
    }
}
#endif // USE_LEGACY_DLIGHTS

static void R_SetupEntityLightingGrid(trRefEntity_t &ent)
{
    vec3_t lightOrigin{};
    int pos[3]{};
    int i, j;
    byte *gridData;
    float frac[3]{};
    vec3_t direction {};
    float totalFactor;

    if (ent.e.renderfx & RF_LIGHTING_ORIGIN)
    {
        // separate lightOrigins are needed so an object that is
        // sinking into the ground can still be lit, and so
        // multi-part models can be lit identically
        VectorCopy(ent.e.lightingOrigin, lightOrigin);
    }
    else
    {
        VectorCopy(ent.e.origin, lightOrigin);
    }

    VectorSubtract(lightOrigin, tr.world->lightGridOrigin, lightOrigin);
    for (i = 0; i < 3; i++)
    {
        float v = lightOrigin[i] * tr.world->lightGridInverseSize[i];
        pos[i] = floor(v);
        frac[i] = v - pos[i];
        if (pos[i] < 0)
        {
            pos[i] = 0;
        }
        else if (pos[i] > tr.world->lightGridBounds[i] - 1)
        {
            pos[i] = tr.world->lightGridBounds[i] - 1;
        }
    }

    VectorClear(ent.ambientLight);
    VectorClear(ent.directedLight);

    assert(tr.world->lightGridData); // NULL with -nolight maps

    // trilerp the light value
    int gridStep[3] = {
        8,
        8 * tr.world->lightGridBounds[0],
        8 * tr.world->lightGridBounds[0] * tr.world->lightGridBounds[1]
    };
    gridData = tr.world->lightGridData + pos[0] * gridStep[0] + pos[1] * gridStep[1] + pos[2] * gridStep[2];

    totalFactor = 0;
    for (i = 0; i < 8; i++)
    {
        float factor;
        byte *data;
        int lat, lng;
        factor = 1.0;
        data = gridData;
        for (j = 0; j < 3; j++)
        {
            if (i & (1 << j))
            {
                if (pos[j] + 1 > tr.world->lightGridBounds[j] - 1)
                {
                    break; // ignore values outside lightgrid
                }
                factor *= frac[j];
                data += gridStep[j];
            }
            else
            {
                factor *= (1.0f - frac[j]);
            }
        }

        if (j != 3)
        {
            continue;
        }

        if (!(data[0] + data[1] + data[2]))
        {
            continue; // ignore samples in walls
        }
        totalFactor += factor;

        ent.ambientLight[0] += factor * data[0];
        ent.ambientLight[1] += factor * data[1];
        ent.ambientLight[2] += factor * data[2];

        ent.directedLight[0] += factor * data[3];
        ent.directedLight[1] += factor * data[4];
        ent.directedLight[2] += factor * data[5];

        lat = data[7];
        lng = data[6];
        lat *= (FUNCTABLE_SIZE / 256);
        lng *= (FUNCTABLE_SIZE / 256);

        // decode X as cos( lat ) * sin( long )
        // decode Y as sin( lat ) * sin( long )
        // decode Z as cos( long )

        vec3_t normal = {
            tr.sinTable[(lat + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK] * tr.sinTable[lng],
            tr.sinTable[lat] * tr.sinTable[lng],
            tr.sinTable[(lng + (FUNCTABLE_SIZE / 4)) & FUNCTABLE_MASK]
        };

        VectorMA(direction, factor, normal, direction);
    }

    if (totalFactor > 0 && totalFactor < 0.99)
    {
        totalFactor = 1.0f / totalFactor;
        VectorScale(ent.ambientLight, totalFactor, ent.ambientLight);
        VectorScale(ent.directedLight, totalFactor, ent.directedLight);
    }

    VectorScale(ent.ambientLight, r_ambientScale->value, ent.ambientLight);
    VectorScale(ent.directedLight, r_directedScale->value, ent.directedLight);

    VectorNormalize2(direction, ent.lightDir);
}

int R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir)
{
    trRefEntity_t ent;

    if (tr.world->lightGridData == NULL)
        return false;

    Com_Memset(&ent, 0, sizeof(ent));
    VectorCopy(point, ent.e.origin);
    R_SetupEntityLightingGrid(ent);
    VectorCopy(ent.ambientLight, ambientLight);
    VectorCopy(ent.directedLight, directedLight);
    VectorCopy(ent.lightDir, lightDir);

    return true;
}

//static void LogLight(const trRefEntity_t &ent)
//{
//    int max1, max2;
//
//    if (!(ent.e.renderfx & RF_FIRST_PERSON))
//    {
//        return;
//    }
//
//    max1 = ent.ambientLight[0];
//    if (ent.ambientLight[1] > max1)
//    {
//        max1 = ent.ambientLight[1];
//    }
//    else if (ent.ambientLight[2] > max1)
//    {
//        max1 = ent.ambientLight[2];
//    }
//
//    max2 = ent.directedLight[0];
//    if (ent.directedLight[1] > max2)
//    {
//        max2 = ent.directedLight[1];
//    }
//    else if (ent.directedLight[2] > max2)
//    {
//        max2 = ent.directedLight[2];
//    }
//
//    ri.Printf(PRINT_ALL, "amb:%i  dir:%i\n", max1, max2);
//}

static void LogLight(const trRefEntity_t& ent)
{
    if (!(ent.e.renderfx & RF_FIRST_PERSON)) return;

    int max1 = static_cast<int>(ent.ambientLight[0]);
    if (ent.ambientLight[1] > max1) max1 = static_cast<int>(ent.ambientLight[1]);
    if (ent.ambientLight[2] > max1) max1 = static_cast<int>(ent.ambientLight[2]);

    int max2 = static_cast<int>(ent.directedLight[0]);
    if (ent.directedLight[1] > max2) max2 = static_cast<int>(ent.directedLight[1]);
    if (ent.directedLight[2] > max2) max2 = static_cast<int>(ent.directedLight[2]);

    ri.Printf(PRINT_ALL, "amb:%i  dir:%i\n", max1, max2);
}

/*
=================
R_SetupEntityLighting

Calculates all the lighting values that will be used
by the Calc_* functions
=================
*/
void R_SetupEntityLighting(const trRefdef_t &refdef, trRefEntity_t &ent)
{
    uint32_t i;
    float power;
    vec3_t dir{};
    float d;
    vec3_t lightDir{};
    vec3_t lightOrigin{};
#ifdef USE_PMLIGHT
    vec3_t shadowLightDir{};
#endif

    // lighting calculations
    if (ent.lightingCalculated)
    {
        return;
    }
    ent.lightingCalculated = true;

    //
    // trace a sample point down to find ambient light
    //
    if (ent.e.renderfx & RF_LIGHTING_ORIGIN)
    {
        // separate lightOrigins are needed so an object that is
        // sinking into the ground can still be lit, and so
        // multi-part models can be lit identically
        VectorCopy(ent.e.lightingOrigin, lightOrigin);
    }
    else
    {
        VectorCopy(ent.e.origin, lightOrigin);
    }

    // if NOWORLDMODEL, only use dynamic lights (menu system, etc)
    if (!(refdef.rdflags & RDF_NOWORLDMODEL) && tr.world->lightGridData)
    {
        R_SetupEntityLightingGrid(ent);
    }
    else
    {
        ent.ambientLight[0] = ent.ambientLight[1] =
            ent.ambientLight[2] = tr.identityLight * 150;
        ent.directedLight[0] = ent.directedLight[1] =
            ent.directedLight[2] = tr.identityLight * 150;
        VectorCopy(tr.sunDirection, ent.lightDir);
    }

    // bonus items and view weapons have a fixed minimum add
    if (1 /* ent->e.renderfx & RF_MINLIGHT */)
    {
        // give everything a minimum light add
        ent.ambientLight[0] += tr.identityLight * 32;
        ent.ambientLight[1] += tr.identityLight * 32;
        ent.ambientLight[2] += tr.identityLight * 32;
    }

    //
    // modify the light by dynamic lights
    //
    d = VectorLength(ent.directedLight);
    VectorScale(ent.lightDir, d, lightDir);
#ifdef USE_PMLIGHT
    if (r_dlightMode->integer == 2)
    {
        // only direct lights
        // but we need to deal with shadow light direction
        VectorCopy(lightDir, shadowLightDir);
        if (r_shadows->integer == 2)
        {
            for (i = 0; i < refdef.num_dlights; i++)
            {
                const dlight_t &dl = refdef.dlights[i];
                if (dl.linear) // no support for linear lights atm
                    continue;
                VectorSubtract(dl.origin, lightOrigin, dir);
                d = VectorNormalize(dir);
                power = DLIGHT_AT_RADIUS * (dl.radius * dl.radius);
                if (d < DLIGHT_MINIMUM_RADIUS)
                {
                    d = DLIGHT_MINIMUM_RADIUS;
                }
                d = power / (d * d);
                VectorMA(shadowLightDir, d, dir, shadowLightDir);
            }
        } // if ( r_shadows->integer == 2 )
    } // if ( r_dlightMode->integer == 2 )
    else
#endif
        for (i = 0; i < refdef.num_dlights; i++)
        {
            const dlight_t &dl = refdef.dlights[i];
            VectorSubtract(dl.origin, lightOrigin, dir);
            d = VectorNormalize(dir);

            power = DLIGHT_AT_RADIUS * (dl.radius * dl.radius);
            if (d < DLIGHT_MINIMUM_RADIUS)
            {
                d = DLIGHT_MINIMUM_RADIUS;
            }
            d = power / (d * d);

            VectorMA(ent.directedLight, d, dl.color, ent.directedLight);
            VectorMA(lightDir, d, dir, lightDir);
        }

    // clamp ambient
    for (i = 0; i < 3; i++)
    {
        if (ent.ambientLight[i] > tr.identityLightByte)
        {
            ent.ambientLight[i] = tr.identityLightByte;
        }
    }

    if (r_debugLight->integer)
    {
        [[unlikely]] LogLight(ent);
    }

    // save out the byte packet version
    //((byte *)&ent.ambientLightInt)[0] = myftol(ent.ambientLight[0]); // -EC-: don't use ri.ftol to avoid precision losses
    //((byte *)&ent.ambientLightInt)[1] = myftol(ent.ambientLight[1]);
    //((byte *)&ent.ambientLightInt)[2] = myftol(ent.ambientLight[2]);
    //((byte *)&ent.ambientLightInt)[3] = 0xff;

    static_assert(sizeof(ent.ambientLightInt) == 4);
    static_assert(CHAR_BIT == 8);

    const std::array<std::uint8_t, 4> amb = {
         static_cast<std::uint8_t>(myftol(ent.ambientLight[0])),
        static_cast<std::uint8_t>(myftol(ent.ambientLight[1])),
        static_cast<std::uint8_t>(myftol(ent.ambientLight[2])),
        0xFFu
    };

    std::memcpy(&ent.ambientLightInt, amb.data(), amb.size());


    //-10790053

    // transform the direction to local space
    VectorNormalize(lightDir);
    ent.lightDir[0] = DotProduct(lightDir, ent.e.axis[0]);
    ent.lightDir[1] = DotProduct(lightDir, ent.e.axis[1]);
    ent.lightDir[2] = DotProduct(lightDir, ent.e.axis[2]);

#ifdef USE_PMLIGHT
    if (r_shadows->integer == 2 && r_dlightMode->integer == 2)
    {
        VectorNormalize(shadowLightDir);
        ent.shadowLightDir[0] = DotProduct(shadowLightDir, ent.e.axis[0]);
        ent.shadowLightDir[1] = DotProduct(shadowLightDir, ent.e.axis[1]);
        ent.shadowLightDir[2] = DotProduct(shadowLightDir, ent.e.axis[2]);
    }
#endif
}

#if defined(USE_AoS_to_SoA_SIMD)

static inline float Vec3Len3(const float x, const float y, const float z) noexcept
{
    return std::sqrt(x * x + y * y + z * z);
}

static inline void Vec3Normalize3(float& x, float& y, float& z) noexcept
{
    const float len2 = x * x + y * y + z * z;
    if (len2 <= 0.0f) { x = y = z = 0.0f; return; }
    const float inv = 1.0f / std::sqrt(len2);
    x *= inv; y *= inv; z *= inv;
}

struct LightGridCtx final
{
    const byte* gridData{};
    vec3_t gridOrigin{};
    vec3_t invSize{};
    int bounds[3]{};
    int step[3]{};

    float ambientScale{};
    float directedScale{};
    std::span<const float> sinTable;
};

static TR_FORCEINLINE void SetupEntityLightingGrid_SoA_One(
    const LightGridCtx& c,
    const float lightOrgX, const float lightOrgY, const float lightOrgZ,
    float& outAmb0, float& outAmb1, float& outAmb2,
    float& outDir0, float& outDir1, float& outDir2,
    float& outWDirX, float& outWDirY, float& outWDirZ) noexcept
{
    // default
    outAmb0 = outAmb1 = outAmb2 = 0.0f;
    outDir0 = outDir1 = outDir2 = 0.0f;
    outWDirX = outWDirY = outWDirZ = 0.0f;

    int pos0, pos1, pos2;
    float frac0, frac1, frac2;

    // world -> grid space
    {
        const float v0 = (lightOrgX - c.gridOrigin[0]) * c.invSize[0];
        const float v1 = (lightOrgY - c.gridOrigin[1]) * c.invSize[1];
        const float v2 = (lightOrgZ - c.gridOrigin[2]) * c.invSize[2];

        pos0 = static_cast<int>(std::floor(v0)); frac0 = v0 - static_cast<float>(pos0);
        pos1 = static_cast<int>(std::floor(v1)); frac1 = v1 - static_cast<float>(pos1);
        pos2 = static_cast<int>(std::floor(v2)); frac2 = v2 - static_cast<float>(pos2);

        // clamp (jak upstream)
        if (pos0 < 0) pos0 = 0; else if (pos0 > c.bounds[0] - 1) pos0 = c.bounds[0] - 1;
        if (pos1 < 0) pos1 = 0; else if (pos1 > c.bounds[1] - 1) pos1 = c.bounds[1] - 1;
        if (pos2 < 0) pos2 = 0; else if (pos2 > c.bounds[2] - 1) pos2 = c.bounds[2] - 1;
    }

    const bool canX = (pos0 < c.bounds[0] - 1);
    const bool canY = (pos1 < c.bounds[1] - 1);
    const bool canZ = (pos2 < c.bounds[2] - 1);

    const float om0 = 1.0f - frac0;
    const float om1 = 1.0f - frac1;
    const float om2 = 1.0f - frac2;

    // 8 corners factors
    const float f000 = om0 * om1 * om2;
    const float f100 = frac0 * om1 * om2;
    const float f010 = om0 * frac1 * om2;
    const float f110 = frac0 * frac1 * om2;
    const float f001 = om0 * om1 * frac2;
    const float f101 = frac0 * om1 * frac2;
    const float f011 = om0 * frac1 * frac2;
    const float f111 = frac0 * frac1 * frac2;

    auto Sample = [&](int dx, int dy, int dz, float factor) noexcept -> bool
        {
            if (factor <= 0.0f) return false;
            if ((dx && !canX) || (dy && !canY) || (dz && !canZ)) return false;

            const int ix = pos0 + dx;
            const int iy = pos1 + dy;
            const int iz = pos2 + dz;

            const byte* data = c.gridData + ((ix * c.step[0] + iy * c.step[1] + iz * c.step[2]) << 3);

            if ((data[0] + data[1] + data[2]) == 0) return false;

            outAmb0 += factor * float(data[0]);
            outAmb1 += factor * float(data[1]);
            outAmb2 += factor * float(data[2]);

            outDir0 += factor * float(data[3]);
            outDir1 += factor * float(data[4]);
            outDir2 += factor * float(data[5]);

            const int lat = int(data[7]) * (FUNCTABLE_SIZE / 256);
            const int lng = int(data[6]) * (FUNCTABLE_SIZE / 256);

            const float sinLat = c.sinTable[lat];
            const float cosLat = c.sinTable[(lat + FUNCTABLE_SIZE / 4) & FUNCTABLE_MASK];
            const float sinLng = c.sinTable[lng];
            const float cosLng = c.sinTable[(lng + FUNCTABLE_SIZE / 4) & FUNCTABLE_MASK];

            const float nx = cosLat * sinLng;
            const float ny = sinLat * sinLng;
            const float nz = cosLng;

            outWDirX += factor * nx;
            outWDirY += factor * ny;
            outWDirZ += factor * nz;

            return true;
        };

    float totalFactor = 0.0f;

    const bool energyLoss = false;//(r_soaGridEnergyLoss && r_soaGridEnergyLoss->integer != 0);

    auto SampleAcc = [&](int dx, int dy, int dz, float f) noexcept
        {
            const bool used = Sample(dx, dy, dz, f);
            if (energyLoss)
            {
                // "efekt": licz wagę zawsze, nawet jak próbka odpadła
                totalFactor += f;
            }
            else
            {
                // AoS: licz wagę tylko dla faktycznie użytej próbki
                if (used) totalFactor += f;
            }
        };

    SampleAcc(0, 0, 0, f000);
    SampleAcc(1, 0, 0, f100);
    SampleAcc(0, 1, 0, f010);
    SampleAcc(1, 1, 0, f110);
    SampleAcc(0, 0, 1, f001);
    SampleAcc(1, 0, 1, f101);
    SampleAcc(0, 1, 1, f011);
    SampleAcc(1, 1, 1, f111);

    if (!energyLoss && totalFactor > 0.0f && totalFactor < 0.99f)
    {
        const float inv = 1.0f / totalFactor;
        outAmb0 *= inv; outAmb1 *= inv; outAmb2 *= inv;
        outDir0 *= inv; outDir1 *= inv; outDir2 *= inv;
        outWDirX *= inv; outWDirY *= inv; outWDirZ *= inv;
    }

    // scales
    outAmb0 *= c.ambientScale; outAmb1 *= c.ambientScale; outAmb2 *= c.ambientScale;
    outDir0 *= c.directedScale; outDir1 *= c.directedScale; outDir2 *= c.directedScale;

    // normalize direction with AoS-like fallback
    {
        const float len2 = outWDirX * outWDirX + outWDirY * outWDirY + outWDirZ * outWDirZ;
        if (len2 > 0.0f)
        {
            const float inv = 1.0f / std::sqrt(len2);
            outWDirX *= inv; outWDirY *= inv; outWDirZ *= inv;
        }
        else
        {
            outWDirX = 0.0f; outWDirY = 0.0f; outWDirZ = 1.0f;
        }
    }
}

void R_SetupEntityLighting_SoA_Batch(const trRefdef_t& refdef, trsoa::FrameSoA& soa) noexcept
{
    const int visCount = soa.visibleModelCount;
    if (visCount <= 0) return;

    auto& T = soa.models.transform;
    auto& L = soa.models.lighting;

    // scratch (world-space accumulated dir * intensity)
    static thread_local std::array<float, trsoa::kMaxRefEntitiesPadded> acc_x;
    static thread_local std::array<float, trsoa::kMaxRefEntitiesPadded> acc_y;
    static thread_local std::array<float, trsoa::kMaxRefEntitiesPadded> acc_z;

    const bool useGrid =
        !(refdef.rdflags & RDF_NOWORLDMODEL) &&
        tr.world && tr.world->lightGridData;

    LightGridCtx ctx{};
    if (useGrid)
    {
        ctx.gridData = tr.world->lightGridData;
        VectorCopy(tr.world->lightGridOrigin, ctx.gridOrigin);
        VectorCopy(tr.world->lightGridInverseSize, ctx.invSize);
        ctx.bounds[0] = tr.world->lightGridBounds[0];
        ctx.bounds[1] = tr.world->lightGridBounds[1];
        ctx.bounds[2] = tr.world->lightGridBounds[2];

/*        ctx.step[0] = tr.world->lightGridBounds[1] * tr.world->lightGridBounds[2];
        ctx.step[1] = tr.world->lightGridBounds[2];
        ctx.step[2] = 1*/;

        const int bx = ctx.bounds[0];
        const int by = ctx.bounds[1];

        ctx.step[0] = 1;
        ctx.step[1] = bx;
        ctx.step[2] = bx * by;

        ctx.ambientScale = r_ambientScale->value;
        ctx.directedScale = r_directedScale->value;
        ctx.sinTable = tr.sinTable;
    }

    // A) base lighting (grid / fallback) + minlight + init acc
    trsoa::ForEachConsecutiveRun(soa.visibleModelSlots.data(), visCount,
        [&](int base, int len) noexcept
        {
            const int end = base + len;
            for (int slot = base; slot < end; ++slot)
            {
                float amb0, amb1, amb2, dir0, dir1, dir2, wdx, wdy, wdz;

                if (useGrid)
                {
                    SetupEntityLightingGrid_SoA_One(
                        ctx,
                        T.lightOrg_x[slot], T.lightOrg_y[slot], T.lightOrg_z[slot],
                        amb0, amb1, amb2,
                        dir0, dir1, dir2,
                        wdx, wdy, wdz);
                }
                else
                {
                    const float v = tr.identityLight * 150.0f;
                    amb0 = amb1 = amb2 = v;
                    dir0 = dir1 = dir2 = v;
                    wdx = tr.sunDirection[0];
                    wdy = tr.sunDirection[1];
                    wdz = tr.sunDirection[2];
                }

                // minlight (Twoja logika)
                const float minL = tr.identityLight * 32.0f;
                amb0 += minL; amb1 += minL; amb2 += minL;

                // write SoA
                L.ambient_x[slot] = amb0; L.ambient_y[slot] = amb1; L.ambient_z[slot] = amb2;
                L.directed_x[slot] = dir0; L.directed_y[slot] = dir1; L.directed_z[slot] = dir2;

                // init acc = wdir * |directed|
                const float dl = Vec3Len3(dir0, dir1, dir2);
                acc_x[slot] = wdx * dl;
                acc_y[slot] = wdy * dl;
                acc_z[slot] = wdz * dl;
            }
        });

    // B) dynamic lights (SIMD tylko tu, jeśli są)
    if (refdef.num_dlights > 0)
    {
        constexpr float kMinR = 16.0f;
        constexpr float kAtRadius = 16.0f;

#if TR_HAS_XSIMD
        using batch = xsimd::batch<float>;
        constexpr int W = static_cast<int>(batch::size);

        trsoa::ForEachConsecutiveRun(soa.visibleModelSlots.data(), visCount,
            [&](int base, int len) noexcept
            {
                const int end = base + len;

                for (int i = base; i < end; i += W)
                {
                    const int lanes = std::min(W, end - i);

                    batch ox = xsimd::load_aligned(&T.lightOrg_x[i]);
                    batch oy = xsimd::load_aligned(&T.lightOrg_y[i]);
                    batch oz = xsimd::load_aligned(&T.lightOrg_z[i]);

                    batch dxL = xsimd::load_aligned(&L.directed_x[i]);
                    batch dyL = xsimd::load_aligned(&L.directed_y[i]);
                    batch dzL = xsimd::load_aligned(&L.directed_z[i]);

                    batch ax = xsimd::load_aligned(&acc_x[i]);
                    batch ay = xsimd::load_aligned(&acc_y[i]);
                    batch az = xsimd::load_aligned(&acc_z[i]);

                    // lane mask tylko dla ogona
                    batch laneMask(1.0f);
                    if (lanes != W)
                    {
                        alignas(64) float m[W];
                        for (int l = 0;l < W;++l) m[l] = (l < lanes) ? 1.0f : 0.0f;
                        laneMask = xsimd::load_aligned(m);
                    }

                    for (int di = 0; di < refdef.num_dlights; ++di)
                    {
                        const dlight_t& dl = refdef.dlights[di];
#ifdef USE_PMLIGHT
                        if (dl.linear) continue;
#endif
                        const float power = kAtRadius * (dl.radius * dl.radius);

                        const batch lx = batch(dl.origin[0]) - ox;
                        const batch ly = batch(dl.origin[1]) - oy;
                        const batch lz = batch(dl.origin[2]) - oz;

                        batch dist2 = lx * lx + ly * ly + lz * lz;
                        batch dist = xsimd::sqrt(dist2);
                        dist = xsimd::max(dist, batch(kMinR));

                        const batch invDist = batch(1.0f) / dist;
                        const batch nx = lx * invDist;
                        const batch ny = ly * invDist;
                        const batch nz = lz * invDist;

                        const batch atten = (batch(power) / (dist * dist)) * laneMask;

                        dxL = dxL + atten * batch(dl.color[0]);
                        dyL = dyL + atten * batch(dl.color[1]);
                        dzL = dzL + atten * batch(dl.color[2]);

                        ax = ax + atten * nx;
                        ay = ay + atten * ny;
                        az = az + atten * nz;
                    }

                    xsimd::store_aligned(&L.directed_x[i], dxL);
                    xsimd::store_aligned(&L.directed_y[i], dyL);
                    xsimd::store_aligned(&L.directed_z[i], dzL);

                    xsimd::store_aligned(&acc_x[i], ax);
                    xsimd::store_aligned(&acc_y[i], ay);
                    xsimd::store_aligned(&acc_z[i], az);
                }
            });
#else
        // scalar dlights (nadal SoA, bez AoS calls)
        trsoa::ForEachConsecutiveRun(soa.visibleModelSlots.data(), visCount,
            [&](int base, int len) noexcept
            {
                const int end = base + len;
                for (int slot = base; slot < end; ++slot)
                {
                    float ox = T.lightOrg_x[slot], oy = T.lightOrg_y[slot], oz = T.lightOrg_z[slot];
                    float dxL = L.directed_x[slot], dyL = L.directed_y[slot], dzL = L.directed_z[slot];
                    float ax = acc_x[slot], ay = acc_y[slot], az = acc_z[slot];

                    for (int di = 0; di < refdef.num_dlights; ++di)
                    {
                        const dlight_t& dl = refdef.dlights[di];
#ifdef USE_PMLIGHT
                        if (dl.linear) continue;
#endif
                        float vx = dl.origin[0] - ox;
                        float vy = dl.origin[1] - oy;
                        float vz = dl.origin[2] - oz;

                        float dist = std::sqrt(vx * vx + vy * vy + vz * vz);
                        if (dist < kMinR) dist = kMinR;

                        const float inv = 1.0f / dist;
                        vx *= inv; vy *= inv; vz *= inv;

                        const float power = kAtRadius * (dl.radius * dl.radius);
                        const float atten = power / (dist * dist);

                        dxL += atten * dl.color[0];
                        dyL += atten * dl.color[1];
                        dzL += atten * dl.color[2];

                        ax += atten * vx;
                        ay += atten * vy;
                        az += atten * vz;
                    }

                    L.directed_x[slot] = dxL; L.directed_y[slot] = dyL; L.directed_z[slot] = dzL;
                    acc_x[slot] = ax; acc_y[slot] = ay; acc_z[slot] = az;
                }
            });
#endif
    }

    // C) clamp + pack + normalize(acc) + world->local + commit do AoS
    trsoa::ForEachConsecutiveRun(soa.visibleModelSlots.data(), visCount,
        [&](int base, int len) noexcept
        {
            const int end = base + len;

            for (int slot = base; slot < end; ++slot)
            {
                float amb0 = L.ambient_x[slot];
                float amb1 = L.ambient_y[slot];
                float amb2 = L.ambient_z[slot];

                if (amb0 > tr.identityLightByte) amb0 = tr.identityLightByte;
                if (amb1 > tr.identityLightByte) amb1 = tr.identityLightByte;
                if (amb2 > tr.identityLightByte) amb2 = tr.identityLightByte;

                // normalize accumulated world dir
                float wx = acc_x[slot], wy = acc_y[slot], wz = acc_z[slot];
                Vec3Normalize3(wx, wy, wz);

                // world->local
                const float lx = wx * T.ax0_x[slot] + wy * T.ax0_y[slot] + wz * T.ax0_z[slot];
                const float ly = wx * T.ax1_x[slot] + wy * T.ax1_y[slot] + wz * T.ax1_z[slot];
                const float lz = wx * T.ax2_x[slot] + wy * T.ax2_y[slot] + wz * T.ax2_z[slot];

                L.lightDir_x[slot] = lx;
                L.lightDir_y[slot] = ly;
                L.lightDir_z[slot] = lz;

                // pack ambient
                const std::uint32_t a0 = static_cast<std::uint32_t>(myftol(amb0)) & 0xFFu;
                const std::uint32_t a1 = static_cast<std::uint32_t>(myftol(amb1)) & 0xFFu;
                const std::uint32_t a2 = static_cast<std::uint32_t>(myftol(amb2)) & 0xFFu;
                const std::uint32_t packed = (a0) | (a1 << 8) | (a2 << 16) | (0xFFu << 24);
                L.ambientPacked[slot] = packed;

                // commit do AoS (tylko tu dotykasz ent)
                const int entNum = static_cast<int>(soa.models.render.entNum[slot]);
                trRefEntity_t& ent = tr.refdef.entities[entNum];

                ent.lightingCalculated = true;

                ent.ambientLight[0] = amb0; ent.ambientLight[1] = amb1; ent.ambientLight[2] = amb2;
                ent.ambientLightInt = packed;

                ent.directedLight[0] = L.directed_x[slot];
                ent.directedLight[1] = L.directed_y[slot];
                ent.directedLight[2] = L.directed_z[slot];

                ent.lightDir[0] = lx; ent.lightDir[1] = ly; ent.lightDir[2] = lz;
            }
        });
}

#endif // USE_AoS_to_SoA_SIMD
