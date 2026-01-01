#ifndef TR_SOA_FRAME_HPP
#define TR_SOA_FRAME_HPP

#include "tr_local.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if __has_include(<xsimd/xsimd.hpp>)
#include <xsimd/xsimd.hpp>
#define TR_HAS_XSIMD 1
#else
#define TR_HAS_XSIMD 0
#endif

namespace trsoa
{
	static constexpr std::size_t kMaxRefEntities = static_cast<std::size_t>(MAX_REFENTITIES);
	static_assert(MAX_REFENTITIES <= 0xFFFF);
	using ent_index_t = std::uint16_t;

	// MAX_DLIGHTS jest w upstream headers.
	static constexpr std::size_t kMaxDlights = static_cast<std::size_t>(MAX_DLIGHTS);

	[[nodiscard]] static inline float VectorLength_Exact(const vec3_t v) noexcept
	{
		return (float)std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	}

	[[nodiscard]] static inline bool Skip_FirstPersonWeapon_InPortal(const trRefEntity_t& ent, const viewParms_t& vp) noexcept
	{
		return (ent.e.renderfx & RF_FIRST_PERSON) && (vp.portalView != portalView_t::PV_NONE);
	}

	[[nodiscard]] static inline bool Skip_ThirdPersonFx_InPrimaryView(const trRefEntity_t& ent, const viewParms_t& vp) noexcept
	{
		return (ent.e.renderfx & RF_THIRD_PERSON) && (vp.portalView == portalView_t::PV_NONE);
	}

	// -------------------------------------------------------------------------
	// SoA “systems”
	// -------------------------------------------------------------------------

	struct EntityTransformSoA final
	{
		alignas(64) std::array<float, kMaxRefEntities> origin_x{};
		alignas(64) std::array<float, kMaxRefEntities> origin_y{};
		alignas(64) std::array<float, kMaxRefEntities> origin_z{};

		// axis[0..2] rows (AoS: ent.e.axis[3][3])
		alignas(64) std::array<float, kMaxRefEntities> ax0_x{};
		alignas(64) std::array<float, kMaxRefEntities> ax0_y{};
		alignas(64) std::array<float, kMaxRefEntities> ax0_z{};

		alignas(64) std::array<float, kMaxRefEntities> ax1_x{};
		alignas(64) std::array<float, kMaxRefEntities> ax1_y{};
		alignas(64) std::array<float, kMaxRefEntities> ax1_z{};

		alignas(64) std::array<float, kMaxRefEntities> ax2_x{};
		alignas(64) std::array<float, kMaxRefEntities> ax2_y{};
		alignas(64) std::array<float, kMaxRefEntities> ax2_z{};

		// If nonNormalizedAxes: axisLenInv = 1/|axis0|, else 1.0f
		alignas(64) std::array<float, kMaxRefEntities> axisLenInv{};

		// Lighting origin in worldspace; equals origin unless RF_LIGHTING_ORIGIN.
		alignas(64) std::array<float, kMaxRefEntities> lightOrg_x{};
		alignas(64) std::array<float, kMaxRefEntities> lightOrg_y{};
		alignas(64) std::array<float, kMaxRefEntities> lightOrg_z{};

		alignas(64) std::array<std::uint8_t, kMaxRefEntities> nonNormalizedAxes{};
		alignas(64) std::array<std::uint8_t, kMaxRefEntities> hasLightingOrigin{};
	};

	struct EntityRenderSoA final
	{
		std::array<ent_index_t, kMaxRefEntities> entNum{}; // original tr.refdef.entities index

		// 32-bit jak upstream (ABI/znaki)
		std::array<std::int32_t, kMaxRefEntities> reType{};
		std::array<std::int32_t, kMaxRefEntities> renderfx{};
		std::array<std::int32_t, kMaxRefEntities> hModel{};
		std::array<std::int32_t, kMaxRefEntities> customShader{};
	};

	struct EntityLightingSoA final
	{
		alignas(64) std::array<float, kMaxRefEntities> lightDir_x{};
		alignas(64) std::array<float, kMaxRefEntities> lightDir_y{};
		alignas(64) std::array<float, kMaxRefEntities> lightDir_z{};

		alignas(64) std::array<float, kMaxRefEntities> ambient_x{};
		alignas(64) std::array<float, kMaxRefEntities> ambient_y{};
		alignas(64) std::array<float, kMaxRefEntities> ambient_z{};

		alignas(64) std::array<float, kMaxRefEntities> directed_x{};
		alignas(64) std::array<float, kMaxRefEntities> directed_y{};
		alignas(64) std::array<float, kMaxRefEntities> directed_z{};

		std::array<std::uint32_t, kMaxRefEntities> ambientPacked{};
		alignas(64) std::array<std::uint8_t, kMaxRefEntities> lightingCalculated{};
		alignas(64) std::array<std::uint8_t, kMaxRefEntities> intShaderTime{};
	};

	// Bucket = jedna spójna pula indeksów (brak branchy w pętli SIMD)
	struct EntityBucketSoA final
	{
		int count{};

		EntityRenderSoA render{};
		EntityTransformSoA transform{};
		EntityLightingSoA lighting{};

		inline void clear() noexcept { count = 0; }
	};

	struct DlightSoA final
	{
		int count{};

		// original index in tr.refdef.dlights
		std::array<std::uint16_t, kMaxDlights> lightNum{};

		alignas(64) std::array<float, kMaxDlights> origin_x{};
		alignas(64) std::array<float, kMaxDlights> origin_y{};
		alignas(64) std::array<float, kMaxDlights> origin_z{};

		alignas(64) std::array<float, kMaxDlights> origin2_x{};
		alignas(64) std::array<float, kMaxDlights> origin2_y{};
		alignas(64) std::array<float, kMaxDlights> origin2_z{};

		alignas(64) std::array<float, kMaxDlights> color_r{};
		alignas(64) std::array<float, kMaxDlights> color_g{};
		alignas(64) std::array<float, kMaxDlights> color_b{};
		alignas(64) std::array<float, kMaxDlights> radius{};

		alignas(64) std::array<std::uint8_t, kMaxDlights> additive{};
		alignas(64) std::array<std::uint8_t, kMaxDlights> linear{};

		inline void clear() noexcept { count = 0; }
	};

	struct ModelDerivedSoA final
	{
		// 16 floats per entity, contiguous: [i*16 + 0..15]
		alignas(64) std::array<float, 16 * kMaxRefEntities> modelMatrix{};
		alignas(64) std::array<float, kMaxRefEntities> viewOrigin_x{};
		alignas(64) std::array<float, kMaxRefEntities> viewOrigin_y{};
		alignas(64) std::array<float, kMaxRefEntities> viewOrigin_z{};

		alignas(64) std::array<float, kMaxRefEntities> boundRadius{}; // for LOD batch
		alignas(64) std::array<std::uint8_t, kMaxRefEntities> lod{};  // final LOD per model slot

		[[nodiscard]] inline float* mat_ptr(const int i) noexcept { return modelMatrix.data() + (i * 16); }
		[[nodiscard]] inline const float* mat_ptr(const int i) const noexcept { return modelMatrix.data() + (i * 16); }
	};

	// Per-frame SoA view
	struct FrameSoA final
	{
		//int frameStamp{ -1 };
		int viewStamp{ -1 }; // zamiast frameStamp

		// RT_MODEL: Rotate/LOD/Cull/Lighting/TransformDlights
		EntityBucketSoA models{};

		// RT_SPRITE/BEAM/etc
		EntityBucketSoA fx{};

		// split dlights:
		// - refdef: used by lighting accumulation (R_SetupEntityLighting)
		// - view: used by PMLIGHT (bounds/face cull per entity)
		DlightSoA dlights_refdef{}; // refdef.dlights (lighting)
		DlightSoA dlights_view{};   // viewParms.dlights (pm light + cull bounds)

		// mapowanie: original ent index -> slot w bucket
		alignas(64) std::array<std::int16_t, kMaxRefEntities> modelSlotOfEnt{};
		alignas(64) std::array<std::int16_t, kMaxRefEntities> fxSlotOfEnt{};

		ModelDerivedSoA modelDerived{};

		inline void clear() noexcept
		{
			models.clear();
			fx.clear();
			dlights_refdef.clear();
			dlights_view.clear();
			// -1 everywhere
			std::memset(modelSlotOfEnt.data(), 0xFF, sizeof(modelSlotOfEnt));
			std::memset(fxSlotOfEnt.data(), 0xFF, sizeof(fxSlotOfEnt));
		}
	};

	[[nodiscard]] static inline bool SoA_ValidThisFrame(const FrameSoA& s) noexcept
	{
		return s.viewStamp == tr.viewCount;
	}

	[[nodiscard]] static inline FrameSoA& GetFrameSoA() noexcept
	{
		static thread_local FrameSoA s{};
		return s;
	}

	static inline void BuildDlightsSoA_FromAoS(const int count, const dlight_t* dl, DlightSoA& out) noexcept
	{
		out.clear();
		if (count <= 0 || !dl) return;

		const int n = (count > static_cast<int>(kMaxDlights)) ? static_cast<int>(kMaxDlights) : count;
		for (int k = 0; k < n; ++k)
		{
			const dlight_t& d = dl[k];
			const int i = out.count++;

			out.lightNum[i] = static_cast<std::uint16_t>(k);

			out.origin_x[i] = d.origin[0];
			out.origin_y[i] = d.origin[1];
			out.origin_z[i] = d.origin[2];

			out.origin2_x[i] = d.origin2[0];
			out.origin2_y[i] = d.origin2[1];
			out.origin2_z[i] = d.origin2[2];

			out.color_r[i] = d.color[0];
			out.color_g[i] = d.color[1];
			out.color_b[i] = d.color[2];
			out.radius[i] = d.radius;

			out.additive[i] = static_cast<std::uint8_t>(d.additive != 0);
			out.linear[i] = static_cast<std::uint8_t>(d.linear);
		}
	}

	// -------------------------------------------------------------------------
	// Build (AoS -> SoA) per frame
	// -------------------------------------------------------------------------

	static inline void BuildEntitiesSoA(trRefdef_t& refdef, const viewParms_t& vp, FrameSoA& out) noexcept
	{
		out.clear();

		const int n = refdef.num_entities;
		for (int entNum = 0; entNum < n; ++entNum)
		{
			trRefEntity_t& ent = refdef.entities[entNum];

			// mirror/portal weapon suppression (jak w AoS)
			if (Skip_FirstPersonWeapon_InPortal(ent, vp))
			{
				continue;
			}

			const int reType = ent.e.reType;

			std::int16_t* slotMap = nullptr;

			// Bucket selection: RT_MODEL vs FX.
			EntityBucketSoA* b = nullptr;
			if (reType == RT_MODEL)
			{
				b = &out.models;
				slotMap = out.modelSlotOfEnt.data();
			}
			else
			{
				switch (reType)
				{
				case RT_SPRITE:
				case RT_BEAM:
				case RT_LIGHTNING:
				case RT_RAIL_CORE:
				case RT_RAIL_RINGS:
					if (Skip_ThirdPersonFx_InPrimaryView(ent, vp))
					{
						continue;
					}
					b = &out.fx;
					slotMap = out.fxSlotOfEnt.data();
					break;

				default:
					// Portalsurface itd. zostają chwilowo na AoS.
					continue;
				}
			}

			if (b->count >= static_cast<int>(kMaxRefEntities)) { break; }
			const int i = b->count++;
			slotMap[entNum] = static_cast<std::int16_t>(i);

			b->render.entNum[i] = static_cast<ent_index_t>(entNum);
			b->render.reType[i] = reType;
			b->render.renderfx[i] = ent.e.renderfx;
			b->render.hModel[i] = ent.e.hModel;
			b->render.customShader[i] = ent.e.customShader;

			b->transform.origin_x[i] = ent.e.origin[0];
			b->transform.origin_y[i] = ent.e.origin[1];
			b->transform.origin_z[i] = ent.e.origin[2];

			b->transform.ax0_x[i] = ent.e.axis[0][0];
			b->transform.ax0_y[i] = ent.e.axis[0][1];
			b->transform.ax0_z[i] = ent.e.axis[0][2];

			b->transform.ax1_x[i] = ent.e.axis[1][0];
			b->transform.ax1_y[i] = ent.e.axis[1][1];
			b->transform.ax1_z[i] = ent.e.axis[1][2];

			b->transform.ax2_x[i] = ent.e.axis[2][0];
			b->transform.ax2_y[i] = ent.e.axis[2][1];
			b->transform.ax2_z[i] = ent.e.axis[2][2];

			const bool nonNorm = !!ent.e.nonNormalizedAxes;
			b->transform.nonNormalizedAxes[i] = static_cast<std::uint8_t>(nonNorm);
			if (nonNorm)
			{
				const float len = VectorLength_Exact(ent.e.axis[0]);
				b->transform.axisLenInv[i] = (len != 0.0f) ? (1.0f / len) : 0.0f;
			}
			else
			{
				b->transform.axisLenInv[i] = 1.0f;
			}

			const bool hasLO = (ent.e.renderfx & RF_LIGHTING_ORIGIN) != 0;
			b->transform.hasLightingOrigin[i] = static_cast<std::uint8_t>(hasLO);
			if (hasLO)
			{
				b->transform.lightOrg_x[i] = ent.e.lightingOrigin[0];
				b->transform.lightOrg_y[i] = ent.e.lightingOrigin[1];
				b->transform.lightOrg_z[i] = ent.e.lightingOrigin[2];
			}
			else
			{
				b->transform.lightOrg_x[i] = ent.e.origin[0];
				b->transform.lightOrg_y[i] = ent.e.origin[1];
				b->transform.lightOrg_z[i] = ent.e.origin[2];
			}

			b->lighting.lightingCalculated[i] = static_cast<std::uint8_t>(ent.lightingCalculated);
			b->lighting.intShaderTime[i] = static_cast<std::uint8_t>(ent.intShaderTime);

			b->lighting.lightDir_x[i] = ent.lightDir[0];
			b->lighting.lightDir_y[i] = ent.lightDir[1];
			b->lighting.lightDir_z[i] = ent.lightDir[2];

			b->lighting.ambient_x[i] = ent.ambientLight[0];
			b->lighting.ambient_y[i] = ent.ambientLight[1];
			b->lighting.ambient_z[i] = ent.ambientLight[2];
			b->lighting.ambientPacked[i] = ent.ambientLightInt;

			b->lighting.directed_x[i] = ent.directedLight[0];
			b->lighting.directed_y[i] = ent.directedLight[1];
			b->lighting.directed_z[i] = ent.directedLight[2];

#ifdef USE_LEGACY_DLIGHTS
			ent.needDlights = 0;
#endif
		}
	}

	static inline void BuildFrameSoA(trRefdef_t& refdef, const viewParms_t& vp, FrameSoA& out) noexcept
	{
		out.viewStamp = tr.viewCount;

		BuildEntitiesSoA(refdef, vp, out);

		BuildDlightsSoA_FromAoS(static_cast<int>(refdef.num_dlights), refdef.dlights, out.dlights_refdef);
		BuildDlightsSoA_FromAoS(static_cast<int>(vp.num_dlights), vp.dlights, out.dlights_view);
	}

} // namespace trsoa

#endif // TR_SOA_FRAME_HPP
