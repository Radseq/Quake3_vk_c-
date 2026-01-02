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
		return std::sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
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
		// Lighting origin (raw) from refEntity_t (RF_LIGHTING_ORIGIN).
		// NOTE: refEntity_t is typically zero-initialized by the caller;
		//       if you ever submit partially-initialized entities, ensure lightingOrigin is valid.
		alignas(64) std::array<float, kMaxRefEntities> lightingOrigin_x{};
		alignas(64) std::array<float, kMaxRefEntities> lightingOrigin_y{};
		alignas(64) std::array<float, kMaxRefEntities> lightingOrigin_z{};

		// Final lighting origin in worldspace; equals origin unless RF_LIGHTING_ORIGIN.
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

	struct EntityModelAnimSoA final
	{
		std::array<std::int32_t, kMaxRefEntities> frame{};
		std::array<std::int32_t, kMaxRefEntities> oldframe{};
		alignas(64) std::array<float, kMaxRefEntities> backlerp{};
	};

	struct EntityShaderSoA final
	{
		alignas(64) std::array<float, kMaxRefEntities> shaderTC0{};
		alignas(64) std::array<float, kMaxRefEntities> shaderTC1{};
		std::array<std::uint32_t, kMaxRefEntities> shaderColorPacked{}; // memcpy z color4ub_t
		std::array<std::uint32_t, kMaxRefEntities> shaderTimeRaw{};     // memcpy z floatint_t
	};


	// Bucket = jedna spójna pula indeksów (brak branchy w pętli SIMD)
	struct EntityBucketSoA final
	{
		int count{};

		EntityRenderSoA render{};
		EntityTransformSoA transform{};
		EntityLightingSoA lighting{};
		EntityModelAnimSoA anim{}; // tylko RT_MODEL (dla FX nieużywane)
		EntityShaderSoA shader{};

		inline void clear() noexcept { count = 0; }
	};

	struct FxSpriteSoA final
	{
		int count{};

		std::array<ent_index_t, kMaxRefEntities> entNum{};
		std::array<std::int32_t, kMaxRefEntities> renderfx{};
		std::array<qhandle_t, kMaxRefEntities> customShader{};
		std::array<shader_t*, kMaxRefEntities> shaderPtr{}; // resolved per frame
		std::array<std::int16_t, kMaxRefEntities> fogNum{};  // 0..numfogs-1


		alignas(64) std::array<float, kMaxRefEntities> origin_x{};
		alignas(64) std::array<float, kMaxRefEntities> origin_y{};
		alignas(64) std::array<float, kMaxRefEntities> origin_z{};

		alignas(64) std::array<float, kMaxRefEntities> radius{};
		alignas(64) std::array<float, kMaxRefEntities> rotation{};
		alignas(64) std::array<float, kMaxRefEntities> st0{};
		alignas(64) std::array<float, kMaxRefEntities> st1{};

		std::array<std::uint32_t, kMaxRefEntities> shaderPacked{}; // memcpy z color4ub_t

		inline void clear() noexcept { count = 0; }
	};

	struct FxBeamSoA final
	{
		int count{};

		std::array<ent_index_t, kMaxRefEntities> entNum{};
		std::array<std::int32_t, kMaxRefEntities> renderfx{};
		std::array<qhandle_t, kMaxRefEntities> customShader{};
		std::array<shader_t*, kMaxRefEntities> shaderPtr{}; // resolved per frame
		std::array<std::int16_t, kMaxRefEntities> fogNum{};  // 0..numfogs-1


		alignas(64) std::array<float, kMaxRefEntities> from_x{};
		alignas(64) std::array<float, kMaxRefEntities> from_y{};
		alignas(64) std::array<float, kMaxRefEntities> from_z{};

		alignas(64) std::array<float, kMaxRefEntities> to_x{};
		alignas(64) std::array<float, kMaxRefEntities> to_y{};
		alignas(64) std::array<float, kMaxRefEntities> to_z{};

		std::array<std::int32_t, kMaxRefEntities> frameOrDiameter{}; // ent.e.frame (np. MODEL_BEAM)

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

		// sphere-cull prepass (tylko gdy !nonNormalizedAxes && frame==oldframe)
		alignas(64) std::array<float, kMaxRefEntities> cullLocal_x{};
		alignas(64) std::array<float, kMaxRefEntities> cullLocal_y{};
		alignas(64) std::array<float, kMaxRefEntities> cullLocal_z{};
		alignas(64) std::array<float, kMaxRefEntities> cullRadius{};
		alignas(64) std::array<std::uint8_t, kMaxRefEntities> cullResult{}; // CULL_*

		[[nodiscard]] inline float* mat_ptr(const int i) noexcept { return modelMatrix.data() + (i * 16); }
		[[nodiscard]] inline const float* mat_ptr(const int i) const noexcept { return modelMatrix.data() + (i * 16); }

		// cached per-slot model lookup (set in ComputeModelLODs_Batch)
		alignas(64) std::array<std::uint8_t, kMaxRefEntities> modelType{}; // modtype_t or 0xFF
		alignas(64) std::array<model_t*, kMaxRefEntities> modelPtr{};      // nullptr if invalid

		alignas(64) std::array<std::int16_t, kMaxRefEntities> fogNum{}; // 0..numfogs-1

		inline void clear() noexcept
		{
			// intencjonalnie puste: pola są nadpisywane dla [0..models.count) co klatkę
			// brak kosztu runtime
		}
	};

	// Per-frame SoA view
	struct FrameSoA final
	{
		//int frameStamp{ -1 };
		int viewStamp{ -1 }; // zamiast frameStamp

		EntityBucketSoA models{};

		FxSpriteSoA sprites{};
		FxBeamSoA beams{};
		FxBeamSoA lightning{};
		FxBeamSoA rail_core{};
		FxBeamSoA rail_rings{};

		DlightSoA dlights_refdef{};
		DlightSoA dlights_view{};

		ModelDerivedSoA modelDerived{};

		// visible & typed slot lists (built after cull)
		int visibleModelCount{};
		alignas(64) std::array<std::uint16_t, kMaxRefEntities> visibleModelSlots{};

		int md3Count{};
		alignas(64) std::array<std::uint16_t, kMaxRefEntities> md3Slots{};

		int mdrCount{};
		alignas(64) std::array<std::uint16_t, kMaxRefEntities> mdrSlots{};

		int iqmCount{};
		alignas(64) std::array<std::uint16_t, kMaxRefEntities> iqmSlots{};

		int brushCount{};
		alignas(64) std::array<std::uint16_t, kMaxRefEntities> brushSlots{};

		int otherCount{};
		alignas(64) std::array<std::uint16_t, kMaxRefEntities> otherSlots{};


		alignas(64) std::array<std::int16_t, kMaxRefEntities> modelSlotOfEnt{};
		alignas(64) std::array<std::int16_t, kMaxRefEntities> spriteSlotOfEnt{};
		alignas(64) std::array<std::int16_t, kMaxRefEntities> beamSlotOfEnt{};
		alignas(64) std::array<std::int16_t, kMaxRefEntities> lightningSlotOfEnt{};
		alignas(64) std::array<std::int16_t, kMaxRefEntities> railCoreSlotOfEnt{};
		alignas(64) std::array<std::int16_t, kMaxRefEntities> railRingsSlotOfEnt{};

		inline void clear() noexcept
		{
			models.clear();

			sprites.clear();
			beams.clear();
			lightning.clear();
			rail_core.clear();
			rail_rings.clear();

			dlights_refdef.clear();
			dlights_view.clear();

			modelDerived.clear();

			modelSlotOfEnt.fill(-1);
			spriteSlotOfEnt.fill(-1);
			beamSlotOfEnt.fill(-1);
			lightningSlotOfEnt.fill(-1);
			railCoreSlotOfEnt.fill(-1);
			railRingsSlotOfEnt.fill(-1);

			visibleModelCount = 0;
			md3Count = 0;
			mdrCount = 0;
			iqmCount = 0;
			brushCount = 0;
			otherCount = 0;

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

	struct SoAContextTLS final
	{
		int modelSlot{ -1 };
	};

	[[nodiscard]] static inline SoAContextTLS& GetSoAContextTLS() noexcept
	{
		static thread_local SoAContextTLS c{};
		return c;
	}

	static inline void SetCurrentModelSlot(const int slot) noexcept
	{
		GetSoAContextTLS().modelSlot = slot;
	}

	[[nodiscard]] static inline int CurrentModelSlot() noexcept
	{
		return GetSoAContextTLS().modelSlot;
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

			const refEntityType_t reType = ent.e.reType;

			// ------------------------------------------------------------
			// RT_MODEL -> models bucket
			// ------------------------------------------------------------
			if (reType == RT_MODEL)
			{
				auto& b = out.models;

				if (b.count >= static_cast<int>(kMaxRefEntities))
				{
					break;
				}

				const int i = b.count++;
				out.modelSlotOfEnt[entNum] = static_cast<std::int16_t>(i);

				b.render.entNum[i] = static_cast<ent_index_t>(entNum);
				b.render.reType[i] = reType;
				b.render.renderfx[i] = ent.e.renderfx;
				b.render.hModel[i] = ent.e.hModel;
				b.render.customShader[i] = ent.e.customShader;

				// transform
				b.transform.origin_x[i] = ent.e.origin[0];
				b.transform.origin_y[i] = ent.e.origin[1];
				b.transform.origin_z[i] = ent.e.origin[2];

				b.transform.ax0_x[i] = ent.e.axis[0][0];
				b.transform.ax0_y[i] = ent.e.axis[0][1];
				b.transform.ax0_z[i] = ent.e.axis[0][2];

				b.transform.ax1_x[i] = ent.e.axis[1][0];
				b.transform.ax1_y[i] = ent.e.axis[1][1];
				b.transform.ax1_z[i] = ent.e.axis[1][2];

				b.transform.ax2_x[i] = ent.e.axis[2][0];
				b.transform.ax2_y[i] = ent.e.axis[2][1];
				b.transform.ax2_z[i] = ent.e.axis[2][2];

				const bool nonNorm = !!ent.e.nonNormalizedAxes;
				b.transform.nonNormalizedAxes[i] = static_cast<std::uint8_t>(nonNorm);

				if (nonNorm)
				{
					const float len = VectorLength_Exact(ent.e.axis[0]);
					b.transform.axisLenInv[i] = (len != 0.0f) ? (1.0f / len) : 0.0f;
				}
				else
				{
					b.transform.axisLenInv[i] = 1.0f;
				}

				const bool hasLO = (ent.e.renderfx & RF_LIGHTING_ORIGIN) != 0;
				b.transform.hasLightingOrigin[i] = static_cast<std::uint8_t>(hasLO);
				if (hasLO)
				{
					b.transform.lightOrg_x[i] = ent.e.lightingOrigin[0];
					b.transform.lightOrg_y[i] = ent.e.lightingOrigin[1];
					b.transform.lightOrg_z[i] = ent.e.lightingOrigin[2];
				}
				else
				{
					b.transform.lightOrg_x[i] = ent.e.origin[0];
					b.transform.lightOrg_y[i] = ent.e.origin[1];
					b.transform.lightOrg_z[i] = ent.e.origin[2];
				}

				// lighting mirror
				b.lighting.lightingCalculated[i] = static_cast<std::uint8_t>(ent.lightingCalculated);
				b.lighting.intShaderTime[i] = static_cast<std::uint8_t>(ent.intShaderTime);

				b.lighting.lightDir_x[i] = ent.lightDir[0];
				b.lighting.lightDir_y[i] = ent.lightDir[1];
				b.lighting.lightDir_z[i] = ent.lightDir[2];

				b.lighting.ambient_x[i] = ent.ambientLight[0];
				b.lighting.ambient_y[i] = ent.ambientLight[1];
				b.lighting.ambient_z[i] = ent.ambientLight[2];
				b.lighting.ambientPacked[i] = ent.ambientLightInt;

				b.lighting.directed_x[i] = ent.directedLight[0];
				b.lighting.directed_y[i] = ent.directedLight[1];
				b.lighting.directed_z[i] = ent.directedLight[2];

				// anim hot fields (dla sphere-cull / lod)
				b.anim.frame[i] = ent.e.frame;
				b.anim.oldframe[i] = ent.e.oldframe;
				b.anim.backlerp[i] = ent.e.backlerp;

#ifdef USE_LEGACY_DLIGHTS
				ent.needDlights = 0;
#endif
				continue;
			}

			// ------------------------------------------------------------
			// FX -> osobne buckety
			// ------------------------------------------------------------
			switch (reType)
			{
			case RT_SPRITE:
			{
				if (Skip_ThirdPersonFx_InPrimaryView(ent, vp)) { break; }

				auto& bucket = out.sprites;
				if (bucket.count >= static_cast<int>(kMaxRefEntities)) { break; }

				const int i = bucket.count++;
				out.spriteSlotOfEnt[entNum] = static_cast<std::int16_t>(i);

				bucket.entNum[i] = static_cast<ent_index_t>(entNum);
				bucket.renderfx[i] = ent.e.renderfx;
				bucket.customShader[i] = ent.e.customShader;

				bucket.origin_x[i] = ent.e.origin[0];
				bucket.origin_y[i] = ent.e.origin[1];
				bucket.origin_z[i] = ent.e.origin[2];

				bucket.radius[i] = ent.e.radius;
				bucket.rotation[i] = ent.e.rotation;

				bucket.st0[i] = ent.e.shaderTexCoord[0];
				bucket.st1[i] = ent.e.shaderTexCoord[1];

				std::uint32_t packed{};
				std::memcpy(&packed, &ent.e.shader, sizeof(packed));
				bucket.shaderPacked[i] = packed;
				break;
			}

			case RT_BEAM:
			{
				if (Skip_ThirdPersonFx_InPrimaryView(ent, vp)) { break; }

				auto& bucket = out.beams;
				if (bucket.count >= static_cast<int>(kMaxRefEntities)) { break; }

				const int i = bucket.count++;
				out.beamSlotOfEnt[entNum] = static_cast<std::int16_t>(i);

				bucket.entNum[i] = static_cast<ent_index_t>(entNum);
				bucket.renderfx[i] = ent.e.renderfx;
				bucket.customShader[i] = ent.e.customShader;

				bucket.from_x[i] = ent.e.origin[0];
				bucket.from_y[i] = ent.e.origin[1];
				bucket.from_z[i] = ent.e.origin[2];

				bucket.to_x[i] = ent.e.oldorigin[0];
				bucket.to_y[i] = ent.e.oldorigin[1];
				bucket.to_z[i] = ent.e.oldorigin[2];

				bucket.frameOrDiameter[i] = ent.e.frame;
				break;
			}

			case RT_LIGHTNING:
			{
				if (Skip_ThirdPersonFx_InPrimaryView(ent, vp)) { break; }

				auto& bucket = out.lightning;
				if (bucket.count >= static_cast<int>(kMaxRefEntities)) { break; }

				const int i = bucket.count++;
				out.lightningSlotOfEnt[entNum] = static_cast<std::int16_t>(i);

				bucket.entNum[i] = static_cast<ent_index_t>(entNum);
				bucket.renderfx[i] = ent.e.renderfx;
				bucket.customShader[i] = ent.e.customShader;

				bucket.from_x[i] = ent.e.origin[0];
				bucket.from_y[i] = ent.e.origin[1];
				bucket.from_z[i] = ent.e.origin[2];

				bucket.to_x[i] = ent.e.oldorigin[0];
				bucket.to_y[i] = ent.e.oldorigin[1];
				bucket.to_z[i] = ent.e.oldorigin[2];

				bucket.frameOrDiameter[i] = ent.e.frame;
				break;
			}

			case RT_RAIL_CORE:
			{
				if (Skip_ThirdPersonFx_InPrimaryView(ent, vp)) { break; }

				auto& bucket = out.rail_core;
				if (bucket.count >= static_cast<int>(kMaxRefEntities)) { break; }

				const int i = bucket.count++;
				out.railCoreSlotOfEnt[entNum] = static_cast<std::int16_t>(i);

				bucket.entNum[i] = static_cast<ent_index_t>(entNum);
				bucket.renderfx[i] = ent.e.renderfx;
				bucket.customShader[i] = ent.e.customShader;

				bucket.from_x[i] = ent.e.origin[0];
				bucket.from_y[i] = ent.e.origin[1];
				bucket.from_z[i] = ent.e.origin[2];

				bucket.to_x[i] = ent.e.oldorigin[0];
				bucket.to_y[i] = ent.e.oldorigin[1];
				bucket.to_z[i] = ent.e.oldorigin[2];

				bucket.frameOrDiameter[i] = ent.e.frame;
				break;
			}

			case RT_RAIL_RINGS:
			{
				if (Skip_ThirdPersonFx_InPrimaryView(ent, vp)) { break; }

				auto& bucket = out.rail_rings;
				if (bucket.count >= static_cast<int>(kMaxRefEntities)) { break; }

				const int i = bucket.count++;
				out.railRingsSlotOfEnt[entNum] = static_cast<std::int16_t>(i);

				bucket.entNum[i] = static_cast<ent_index_t>(entNum);
				bucket.renderfx[i] = ent.e.renderfx;
				bucket.customShader[i] = ent.e.customShader;

				bucket.from_x[i] = ent.e.origin[0];
				bucket.from_y[i] = ent.e.origin[1];
				bucket.from_z[i] = ent.e.origin[2];

				bucket.to_x[i] = ent.e.oldorigin[0];
				bucket.to_y[i] = ent.e.oldorigin[1];
				bucket.to_z[i] = ent.e.oldorigin[2];

				bucket.frameOrDiameter[i] = ent.e.frame;
				break;
			}

			default:
				// Portalsurface itd. zostają na AoS
				break;
			}

#ifdef USE_LEGACY_DLIGHTS
			ent.needDlights = 0;
#endif
		}
	}

	static inline void PostCompute_ModelTransform(EntityTransformSoA& t, const int count) noexcept
	{
		if (count <= 0) return;

#if TR_HAS_XSIMD
		using batch = xsimd::batch<float>;
		constexpr int W = static_cast<int>(batch::size);

		alignas(64) float nnLane[W];
		alignas(64) float loLane[W];

		int base = 0;
		for (; base + W <= count; base += W)
		{
			unsigned anyNN = 0;
			unsigned anyLO = 0;
			for (int l = 0; l < W; ++l)
			{
				const int s = base + l;
				const unsigned nn = static_cast<unsigned>(t.nonNormalizedAxes[s] != 0);
				const unsigned lo = static_cast<unsigned>(t.hasLightingOrigin[s] != 0);
				nnLane[l] = nn ? 1.0f : 0.0f;
				loLane[l] = lo ? 1.0f : 0.0f;
				anyNN |= nn;
				anyLO |= lo;
			}

			// axisLenInv
			if (!anyNN)
			{
				// common case: normalized axes
				batch(1.0f).store_aligned(t.axisLenInv.data() + base);
			}
			else
			{
				const batch nn = xsimd::load_aligned(nnLane);
				const auto nnMask = (nn != batch(0.0f));

				const batch ax0x = batch::load_aligned(t.ax0_x.data() + base);
				const batch ax0y = batch::load_aligned(t.ax0_y.data() + base);
				const batch ax0z = batch::load_aligned(t.ax0_z.data() + base);

				const batch len2 = ax0x * ax0x + ax0y * ax0y + ax0z * ax0z;
				const batch len = xsimd::sqrt(len2);

				batch inv = batch(1.0f) / len;
				inv = xsimd::select((len != batch(0.0f)), inv, batch(0.0f)); // len==0 => 0
				inv = xsimd::select(nnMask, inv, batch(1.0f));              // !nonNorm => 1

				inv.store_aligned(t.axisLenInv.data() + base);
			}

			// lightOrg
			if (!anyLO)
			{
				// common case: no RF_LIGHTING_ORIGIN
				const batch ox = batch::load_aligned(t.origin_x.data() + base);
				const batch oy = batch::load_aligned(t.origin_y.data() + base);
				const batch oz = batch::load_aligned(t.origin_z.data() + base);
				ox.store_aligned(t.lightOrg_x.data() + base);
				oy.store_aligned(t.lightOrg_y.data() + base);
				oz.store_aligned(t.lightOrg_z.data() + base);
			}
			else
			{
				const batch hasLO = xsimd::load_aligned(loLane);
				const auto loMask = (hasLO != batch(0.0f));

				const batch ox = batch::load_aligned(t.origin_x.data() + base);
				const batch oy = batch::load_aligned(t.origin_y.data() + base);
				const batch oz = batch::load_aligned(t.origin_z.data() + base);

				const batch lx = batch::load_aligned(t.lightingOrigin_x.data() + base);
				const batch ly = batch::load_aligned(t.lightingOrigin_y.data() + base);
				const batch lz = batch::load_aligned(t.lightingOrigin_z.data() + base);

				const batch outX = xsimd::select(loMask, lx, ox);
				const batch outY = xsimd::select(loMask, ly, oy);
				const batch outZ = xsimd::select(loMask, lz, oz);

				outX.store_aligned(t.lightOrg_x.data() + base);
				outY.store_aligned(t.lightOrg_y.data() + base);
				outZ.store_aligned(t.lightOrg_z.data() + base);
			}
		}

		// tail
		for (int i = base; i < count; ++i)
		{
			if (t.nonNormalizedAxes[i])
			{
				const float x = t.ax0_x[i];
				const float y = t.ax0_y[i];
				const float z = t.ax0_z[i];
				const float len = std::sqrtf(x * x + y * y + z * z);
				t.axisLenInv[i] = (len != 0.0f) ? (1.0f / len) : 0.0f;
			}
			else
			{
				t.axisLenInv[i] = 1.0f;
			}

			if (t.hasLightingOrigin[i])
			{
				t.lightOrg_x[i] = t.lightingOrigin_x[i];
				t.lightOrg_y[i] = t.lightingOrigin_y[i];
				t.lightOrg_z[i] = t.lightingOrigin_z[i];
			}
			else
			{
				t.lightOrg_x[i] = t.origin_x[i];
				t.lightOrg_y[i] = t.origin_y[i];
				t.lightOrg_z[i] = t.origin_z[i];
			}
		}
#else
		// scalar fallback ...
#endif
	}

	static inline void BuildFrameSoA(trRefdef_t& refdef, const viewParms_t& vp, FrameSoA& out) noexcept
	{
		out.viewStamp = tr.viewCount;

		BuildEntitiesSoA(refdef, vp, out);
		PostCompute_ModelTransform(out.models.transform, out.models.count);

		BuildDlightsSoA_FromAoS(static_cast<int>(refdef.num_dlights), refdef.dlights, out.dlights_refdef);
		BuildDlightsSoA_FromAoS(static_cast<int>(vp.num_dlights), vp.dlights, out.dlights_view);
	}
} // namespace trsoa

#endif // TR_SOA_FRAME_HPP
