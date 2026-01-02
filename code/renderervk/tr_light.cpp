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

void R_SetupEntityLighting_SoA_Batch(const trRefdef_t& refdef, trsoa::FrameSoA& soa) noexcept
{
    const int count = soa.models.count;
    if (count <= 0) return;

#ifdef USE_PMLIGHT
    // tryby specjalne (shadowLightDir / direct-only) -> nie dotykamy, zostaw scalar
    if (r_dlightMode->integer == 2)
    {
        for (int i = 0; i < soa.visibleModelCount; ++i)
        {
            const int slot = static_cast<int>(soa.visibleModelSlots[i]);
            const int entNum = static_cast<int>(soa.models.render.entNum[slot]);
            trRefEntity_t& ent = tr.refdef.entities[entNum];
            R_SetupEntityLighting(refdef, ent);
        }
        return;
    }
#endif

    // małe sceny -> scalar (szybciej niż setup SIMD)
    if (refdef.num_dlights < 2 || count < 16)
    {
        for (int i = 0; i < soa.visibleModelCount; ++i)
        {
            const int slot = static_cast<int>(soa.visibleModelSlots[i]);
            const int entNum = static_cast<int>(soa.models.render.entNum[slot]);
            trRefEntity_t& ent = tr.refdef.entities[entNum];
            R_SetupEntityLighting(refdef, ent);
        }
        return;
    }

    // scratch (world-space accumulated light direction)
    static thread_local std::array<float, trsoa::kMaxRefEntities> acc_x;
    static thread_local std::array<float, trsoa::kMaxRefEntities> acc_y;
    static thread_local std::array<float, trsoa::kMaxRefEntities> acc_z;
    static thread_local std::array<std::uint8_t, trsoa::kMaxRefEntities> need{};
    need.fill(0);

    auto& L = soa.models.lighting;
    const auto& T = soa.models.transform;

    // -------------------------
    // A) baza: grid lub fallback + minlight + init acc
    // -------------------------
    for (int i = 0; i < soa.visibleModelCount; ++i)
    {
        const int slot = static_cast<int>(soa.visibleModelSlots[i]);
        const int entNum = static_cast<int>(soa.models.render.entNum[slot]);
        trRefEntity_t& ent = tr.refdef.entities[entNum];

        if (ent.lightingCalculated) { continue; }
        ent.lightingCalculated = true;

        if (!(refdef.rdflags & RDF_NOWORLDMODEL) && tr.world->lightGridData)
        {
            R_SetupEntityLightingGrid(ent);
        }
        else
        {
            ent.ambientLight[0] = ent.ambientLight[1] = ent.ambientLight[2] = tr.identityLight * 150;
            ent.directedLight[0] = ent.directedLight[1] = ent.directedLight[2] = tr.identityLight * 150;
            VectorCopy(tr.sunDirection, ent.lightDir);
        }

        // minlight (u ciebie zawsze ON, jak w kodzie)
        ent.ambientLight[0] += tr.identityLight * 32;
        ent.ambientLight[1] += tr.identityLight * 32;
        ent.ambientLight[2] += tr.identityLight * 32;

        // init acc = ent.lightDir * |directedLight|
        const float dl = Vec3Len3(ent.directedLight[0], ent.directedLight[1], ent.directedLight[2]);
        acc_x[slot] = ent.lightDir[0] * dl;
        acc_y[slot] = ent.lightDir[1] * dl;
        acc_z[slot] = ent.lightDir[2] * dl;

        // mirror do SoA (dla SIMD)
        L.directed_x[slot] = ent.directedLight[0];
        L.directed_y[slot] = ent.directedLight[1];
        L.directed_z[slot] = ent.directedLight[2];

        L.ambient_x[slot] = ent.ambientLight[0];
        L.ambient_y[slot] = ent.ambientLight[1];
        L.ambient_z[slot] = ent.ambientLight[2];

        need[slot] = 1u;
    }

    // jeżeli nic do roboty
    bool anyNeed = false;
    for (int slot = 0; slot < count; ++slot) { if (need[slot]) { anyNeed = true; break; } }
    if (!anyNeed) return;

    // -------------------------
    // B) dynamic lights SIMD per-dlight
    // -------------------------
    constexpr float kMinR = 16.0f;        // DLIGHT_MINIMUM_RADIUS
    constexpr float kAtRadius = 16.0f;    // DLIGHT_AT_RADIUS

#if TR_HAS_XSIMD
    using batch = xsimd::batch<float>;
    constexpr int W = static_cast<int>(batch::size);

    alignas(64) float activeLane[W];

    for (int base = 0; base < count; base += W)
    {
        const int lanes = std::min(W, count - base);

        bool any = false;
        for (int l = 0; l < lanes; ++l)
        {
            const int s = base + l;
            const bool act = (need[s] != 0u) && (soa.modelDerived.cullResult[s] != CULL_OUT);
            activeLane[l] = act ? 1.0f : 0.0f;
            any |= act;
        }
        for (int l = lanes; l < W; ++l) activeLane[l] = 0.0f;
        if (!any) continue;

        const batch active = xsimd::load_aligned(activeLane);

        batch ox = xsimd::load_unaligned(&T.lightOrg_x[base]);
        batch oy = xsimd::load_unaligned(&T.lightOrg_y[base]);
        batch oz = xsimd::load_unaligned(&T.lightOrg_z[base]);

        batch dxL = xsimd::load_unaligned(&L.directed_x[base]);
        batch dyL = xsimd::load_unaligned(&L.directed_y[base]);
        batch dzL = xsimd::load_unaligned(&L.directed_z[base]);

        batch ax = xsimd::load_unaligned(&acc_x[base]);
        batch ay = xsimd::load_unaligned(&acc_y[base]);
        batch az = xsimd::load_unaligned(&acc_z[base]);

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

            const batch dist2 = lx * lx + ly * ly + lz * lz;
            batch dist = xsimd::sqrt(dist2);

            // clamp dist
            dist = xsimd::max(dist, batch(kMinR));

            const batch invDist = batch(1.0f) / dist;

            // normalized dir
            const batch nx = lx * invDist;
            const batch ny = ly * invDist;
            const batch nz = lz * invDist;

            // atten = power / (dist*dist)
            const batch atten = (batch(power) / (dist * dist)) * active;

            // directed += atten * dl.color
            dxL = dxL + atten * batch(dl.color[0]);
            dyL = dyL + atten * batch(dl.color[1]);
            dzL = dzL + atten * batch(dl.color[2]);

            // acc += atten * normalized_dir
            ax = ax + atten * nx;
            ay = ay + atten * ny;
            az = az + atten * nz;
        }

        xsimd::store_unaligned(&L.directed_x[base], dxL);
        xsimd::store_unaligned(&L.directed_y[base], dyL);
        xsimd::store_unaligned(&L.directed_z[base], dzL);

        xsimd::store_unaligned(&acc_x[base], ax);
        xsimd::store_unaligned(&acc_y[base], ay);
        xsimd::store_unaligned(&acc_z[base], az);
    }
#else
    // fallback scalar (ale nadal per-SoA, bez wywołań na entity)
    for (int slot = 0; slot < count; ++slot)
    {
        if (!need[slot]) continue;
        if (soa.modelDerived.cullResult[slot] == CULL_OUT) continue;

        const float ox = T.lightOrg_x[slot];
        const float oy = T.lightOrg_y[slot];
        const float oz = T.lightOrg_z[slot];

        float dxL = L.directed_x[slot];
        float dyL = L.directed_y[slot];
        float dzL = L.directed_z[slot];

        float ax = acc_x[slot];
        float ay = acc_y[slot];
        float az = acc_z[slot];

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

        L.directed_x[slot] = dxL;
        L.directed_y[slot] = dyL;
        L.directed_z[slot] = dzL;

        acc_x[slot] = ax;
        acc_y[slot] = ay;
        acc_z[slot] = az;
    }
#endif

    // -------------------------
    // C) clamp ambient + pack + normalize(acc) + world->local (dot(axis))
    // -------------------------
    for (int i = 0; i < soa.visibleModelCount; ++i)
    {
        const int slot = static_cast<int>(soa.visibleModelSlots[i]);
        if (!need[slot]) continue;
        if (soa.modelDerived.cullResult[slot] == CULL_OUT) continue;

        const int entNum = static_cast<int>(soa.models.render.entNum[slot]);
        trRefEntity_t& ent = tr.refdef.entities[entNum];

        // clamp ambient
        float amb0 = L.ambient_x[slot];
        float amb1 = L.ambient_y[slot];
        float amb2 = L.ambient_z[slot];

        if (amb0 > tr.identityLightByte) amb0 = tr.identityLightByte;
        if (amb1 > tr.identityLightByte) amb1 = tr.identityLightByte;
        if (amb2 > tr.identityLightByte) amb2 = tr.identityLightByte;

        // write AoS ambient + pack
        ent.ambientLight[0] = amb0;
        ent.ambientLight[1] = amb1;
        ent.ambientLight[2] = amb2;

        const std::array<std::uint8_t, 4> packed = {
            static_cast<std::uint8_t>(myftol(amb0)),
            static_cast<std::uint8_t>(myftol(amb1)),
            static_cast<std::uint8_t>(myftol(amb2)),
            0xFFu
        };
        std::memcpy(&ent.ambientLightInt, packed.data(), 4);
        L.ambientPacked[slot] = ent.ambientLightInt;

        // directed -> AoS
        ent.directedLight[0] = L.directed_x[slot];
        ent.directedLight[1] = L.directed_y[slot];
        ent.directedLight[2] = L.directed_z[slot];

        // normalize accumulated world light dir
        float wx = acc_x[slot];
        float wy = acc_y[slot];
        float wz = acc_z[slot];
        Vec3Normalize3(wx, wy, wz);

        // world->local (dot with axis rows)
        const float lx =
            wx * T.ax0_x[slot] + wy * T.ax0_y[slot] + wz * T.ax0_z[slot];
        const float ly =
            wx * T.ax1_x[slot] + wy * T.ax1_y[slot] + wz * T.ax1_z[slot];
        const float lz =
            wx * T.ax2_x[slot] + wy * T.ax2_y[slot] + wz * T.ax2_z[slot];

        ent.lightDir[0] = lx;
        ent.lightDir[1] = ly;
        ent.lightDir[2] = lz;

        L.lightDir_x[slot] = lx;
        L.lightDir_y[slot] = ly;
        L.lightDir_z[slot] = lz;
    }
}

#endif // USE_AoS_to_SoA_SIMD
