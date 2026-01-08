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

	// -------------------------------------------------------------------------
	// SIMD padding helpers (remove tail loops in hot SIMD paths)
	// -------------------------------------------------------------------------
#if TR_HAS_XSIMD
	using simd_f32_batch = xsimd::batch<float>;
	static constexpr int kSimdW_F32 = static_cast<int>(simd_f32_batch::size);
#else
	static constexpr int kSimdW_F32 = 4;
#endif

	[[nodiscard]] static constexpr std::size_t RoundUpTo(std::size_t n, std::size_t m) noexcept
	{
		return (m > 0) ? ((n + m - 1) / m) * m : n;
	}

	static constexpr std::size_t kMaxRefEntitiesPadded =
		RoundUpTo(kMaxRefEntities, static_cast<std::size_t>(kSimdW_F32));

	[[nodiscard]] static inline int RoundUpToSimdW_F32(const int n) noexcept
	{
		return static_cast<int>(RoundUpTo(static_cast<std::size_t>(n), static_cast<std::size_t>(kSimdW_F32)));
	}

	// MAX_DLIGHTS jest w upstream headers.
	static constexpr std::size_t kMaxDlights = static_cast<std::size_t>(MAX_DLIGHTS);

	[[nodiscard]] static inline float VectorLength_Exact(const vec3_t v) noexcept
	{
		return std::sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	}

	[[nodiscard]] static inline bool Skip_FirstPerson_InPortalView(const trRefEntity_t& ent, const viewParms_t& vp) noexcept
	{
		return (ent.e.renderfx & RF_FIRST_PERSON) && (vp.portalView != portalView_t::PV_NONE);
	}

	[[nodiscard]] static inline bool Skip_ThirdPersonFx_InPrimaryView(const trRefEntity_t& ent, const viewParms_t& vp) noexcept
	{
		return (ent.e.renderfx & RF_THIRD_PERSON) && (vp.portalView == portalView_t::PV_NONE);
	}

	[[nodiscard]] static inline bool Skip_FirstPerson_InPortalViewFx(const int renderfx, const viewParms_t& vp) noexcept
	{
		return (renderfx & RF_FIRST_PERSON) && (vp.portalView != portalView_t::PV_NONE);
	}

	[[nodiscard]] static inline bool Skip_ThirdPersonFx_InPrimaryViewFx(const int renderfx, const viewParms_t& vp) noexcept
	{
		return (renderfx & RF_THIRD_PERSON) && (vp.portalView == portalView_t::PV_NONE);
	}

	// -------------------------------------------------------------------------
	// SoA “systems”
	// -------------------------------------------------------------------------

	struct EntityTransformSoA final
	{
		alignas(64) std::array<float, kMaxRefEntitiesPadded> origin_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> origin_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> origin_z{};

		// axis[0..2] rows (AoS: ent.e.axis[3][3])
		alignas(64) std::array<float, kMaxRefEntitiesPadded> ax0_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> ax0_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> ax0_z{};

		alignas(64) std::array<float, kMaxRefEntitiesPadded> ax1_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> ax1_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> ax1_z{};

		alignas(64) std::array<float, kMaxRefEntitiesPadded> ax2_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> ax2_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> ax2_z{};

		// If nonNormalizedAxes: axisLenInv = 1/|axis0|, else 1.0f
		alignas(64) std::array<float, kMaxRefEntitiesPadded> axisLenInv{};

		// Lighting origin in worldspace; equals origin unless RF_LIGHTING_ORIGIN.
		// Lighting origin (raw) from refEntity_t (RF_LIGHTING_ORIGIN).
		// NOTE: refEntity_t is typically zero-initialized by the caller;
		//       if you ever submit partially-initialized entities, ensure lightingOrigin is valid.
		alignas(64) std::array<float, kMaxRefEntitiesPadded> lightingOrigin_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> lightingOrigin_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> lightingOrigin_z{};

		// Final lighting origin in worldspace; equals origin unless RF_LIGHTING_ORIGIN.
		alignas(64) std::array<float, kMaxRefEntitiesPadded> lightOrg_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> lightOrg_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> lightOrg_z{};

		alignas(64) std::array<std::uint8_t, kMaxRefEntitiesPadded> nonNormalizedAxes{};
		alignas(64) std::array<std::uint8_t, kMaxRefEntitiesPadded> hasLightingOrigin{};
	};

	struct EntityRenderSoA final
	{
		std::array<ent_index_t, kMaxRefEntitiesPadded> entNum{}; // original tr.refdef.entities index

		// 32-bit jak upstream (ABI/znaki)
		std::array<std::int32_t, kMaxRefEntitiesPadded> reType{};
		std::array<std::int32_t, kMaxRefEntitiesPadded> renderfx{};
		std::array<std::int32_t, kMaxRefEntitiesPadded> hModel{};
		std::array<std::int32_t, kMaxRefEntitiesPadded> customShader{};
	};

	struct EntityLightingSoA final
	{
		alignas(64) std::array<float, kMaxRefEntitiesPadded> lightDir_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> lightDir_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> lightDir_z{};

		alignas(64) std::array<float, kMaxRefEntitiesPadded> ambient_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> ambient_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> ambient_z{};

		alignas(64) std::array<float, kMaxRefEntitiesPadded> directed_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> directed_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> directed_z{};

		std::array<std::uint32_t, kMaxRefEntitiesPadded> ambientPacked{};
		alignas(64) std::array<std::uint8_t, kMaxRefEntitiesPadded> lightingCalculated{};
		alignas(64) std::array<std::uint8_t, kMaxRefEntitiesPadded> intShaderTime{};
	};

	struct EntityModelAnimSoA final
	{
		std::array<std::int32_t, kMaxRefEntitiesPadded> frame{};
		std::array<std::int32_t, kMaxRefEntitiesPadded> oldframe{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> backlerp{};
	};

	struct EntityShaderSoA final
	{
		alignas(64) std::array<float, kMaxRefEntitiesPadded> shaderTC0{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> shaderTC1{};
		std::array<std::uint32_t, kMaxRefEntitiesPadded> shaderColorPacked{}; // memcpy z color4ub_t
		std::array<std::uint32_t, kMaxRefEntitiesPadded> shaderTimeRaw{};     // memcpy z floatint_t
	};

	// Bucket = jedna spójna pula indeksów (brak branchy w pętli SIMD)
	struct EntityBucketSoA final
	{
		int count{};
		int countPadded{};

		EntityRenderSoA render{};
		EntityTransformSoA transform{};
		EntityLightingSoA lighting{};
		EntityModelAnimSoA anim{}; // tylko RT_MODEL (dla FX nieużywane)
		EntityShaderSoA shader{};

		inline void clear() noexcept { count = 0; countPadded = 0; }
	};

	struct FxSpriteSoA final
	{
		int count{};
		int countPadded{};

		std::array<ent_index_t, kMaxRefEntitiesPadded> entNum{};
		std::array<std::int32_t, kMaxRefEntitiesPadded> renderfx{};
		std::array<qhandle_t, kMaxRefEntitiesPadded> customShader{};
		std::array<shader_t*, kMaxRefEntitiesPadded> shaderPtr{};
		std::array<std::int16_t, kMaxRefEntitiesPadded> fogNum{};

		alignas(64) std::array<float, kMaxRefEntitiesPadded> origin_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> origin_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> origin_z{};

		alignas(64) std::array<float, kMaxRefEntitiesPadded> radius{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> rotation{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> st0{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> st1{};

		std::array<std::uint32_t, kMaxRefEntitiesPadded> shaderPacked{};

		inline void clear() noexcept { count = 0; countPadded = 0; }
	};


	struct FxBeamSoA final
	{
		int count{};
		int countPadded{};

		std::array<ent_index_t, kMaxRefEntitiesPadded> entNum{};
		std::array<std::int32_t, kMaxRefEntitiesPadded> renderfx{};
		std::array<qhandle_t, kMaxRefEntitiesPadded> customShader{};
		std::array<shader_t*, kMaxRefEntitiesPadded> shaderPtr{};
		std::array<std::int16_t, kMaxRefEntitiesPadded> fogNum{};

		alignas(64) std::array<float, kMaxRefEntitiesPadded> from_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> from_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> from_z{};

		alignas(64) std::array<float, kMaxRefEntitiesPadded> to_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> to_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> to_z{};

		std::array<std::int32_t, kMaxRefEntitiesPadded> frameOrDiameter{};

		inline void clear() noexcept { count = 0; countPadded = 0; }
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
		alignas(64) std::array<float, 16 * kMaxRefEntitiesPadded> modelMatrix{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> viewOrigin_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> viewOrigin_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> viewOrigin_z{};

		alignas(64) std::array<float, kMaxRefEntitiesPadded> boundRadius{}; // for LOD batch
		alignas(64) std::array<std::uint8_t, kMaxRefEntitiesPadded> lod{};  // final LOD per model slot

		// sphere-cull prepass (tylko gdy !nonNormalizedAxes && frame==oldframe)
		alignas(64) std::array<float, kMaxRefEntitiesPadded> cullLocal_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> cullLocal_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> cullLocal_z{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> cullRadius{};
		alignas(64) std::array<std::uint8_t, kMaxRefEntitiesPadded> cullResult{}; // CULL_*

		// merged local AABB (dla poprawności i box-cull ścieżek AoS)
		// valid=1: prawdziwe boundsy z modelu (MD3/MDR/IQM z bounds / BRUSH)
		// valid=0: brak bounds (np. IQM bez bounds) -> NIE WOLNO robić OUT na podstawie prepassu
		alignas(64) std::array<float, kMaxRefEntitiesPadded> aabbMin_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> aabbMin_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> aabbMin_z{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> aabbMax_x{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> aabbMax_y{};
		alignas(64) std::array<float, kMaxRefEntitiesPadded> aabbMax_z{};
		alignas(64) std::array<std::uint8_t, kMaxRefEntitiesPadded> aabbValid{}; // 0/1

		[[nodiscard]] inline float* mat_ptr(const int i) noexcept { return modelMatrix.data() + (i * 16); }
		[[nodiscard]] inline const float* mat_ptr(const int i) const noexcept { return modelMatrix.data() + (i * 16); }

		// cached per-slot model lookup (set in ComputeModelLODs_Batch)
		alignas(64) std::array<std::uint8_t, kMaxRefEntitiesPadded> modelType{}; // modtype_t or 0xFF
		alignas(64) std::array<model_t*, kMaxRefEntitiesPadded> modelPtr{};      // nullptr if invalid

		alignas(64) std::array<std::int16_t, kMaxRefEntitiesPadded> fogNum{}; // 0..numfogs-1

		inline void clear() noexcept
		{
			// intencjonalnie puste: pola są nadpisywane dla [0..models.count) co klatkę
			// brak kosztu runtime
		}
	};

	// -------------------------------------------------------------------------
	// Sparse mapping: entNum -> slot without per-view fill(-1)
	//
	// Previous code did multiple fill(-1) passes over kMaxRefEntities every view.
	// That is pure bandwidth cost (scales with MAX_REFENTITIES even for small N).
	//
	// Stamping eliminates the clears:
	// - reset_for_new_build() increments a stamp counter
	// - set() writes slot + current stamp
	// - get() returns -1 if stamp mismatches
	//
	// Wrap-around is handled by a one-time full reset.
	// -------------------------------------------------------------------------
	struct EntToSlotMap16 final
	{
		alignas(64) std::array<std::int16_t, kMaxRefEntities> slot{};   // last written slot
		alignas(64) std::array<std::uint32_t, kMaxRefEntities> stamp{}; // stamp per ent
		std::uint32_t currentStamp{ 1u };                               // 0 reserved as "invalid"

		inline void reset_for_new_build() noexcept
		{
			++currentStamp;
			if (currentStamp == 0u)
			{
				slot.fill(static_cast<std::int16_t>(-1));
				stamp.fill(0u);
				currentStamp = 1u;
			}
		}

		inline void set(const int entNum, const std::int16_t s) noexcept
		{
			slot[entNum] = s;
			stamp[entNum] = currentStamp;
		}

		[[nodiscard]] inline std::int16_t get(const int entNum) const noexcept
		{
			return (stamp[entNum] == currentStamp) ? slot[entNum] : static_cast<std::int16_t>(-1);
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

		// sparse slot lists (built during AoS->SoA)
		int modelNonNormCount{};
		alignas(64) std::array<std::uint16_t, kMaxRefEntities> modelNonNormSlots{};

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

		EntToSlotMap16 modelSlotOfEnt{};
		EntToSlotMap16 spriteSlotOfEnt{};
		EntToSlotMap16 beamSlotOfEnt{};
		EntToSlotMap16 lightningSlotOfEnt{};
		EntToSlotMap16 railCoreSlotOfEnt{};
		EntToSlotMap16 railRingsSlotOfEnt{};

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
			modelSlotOfEnt.reset_for_new_build();
			spriteSlotOfEnt.reset_for_new_build();
			beamSlotOfEnt.reset_for_new_build();
			lightningSlotOfEnt.reset_for_new_build();
			railCoreSlotOfEnt.reset_for_new_build();
			railRingsSlotOfEnt.reset_for_new_build();

			visibleModelCount = 0;
			md3Count = 0;
			mdrCount = 0;
			iqmCount = 0;
			brushCount = 0;
			otherCount = 0;
			modelNonNormCount = 0;
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
			if (Skip_FirstPerson_InPortalView(ent, vp))
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
				out.modelSlotOfEnt.set(entNum, static_cast<std::int16_t>(i));

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
					out.modelNonNormSlots[out.modelNonNormCount++] = static_cast<std::uint16_t>(i);
				}

				// axisLenInv + lightOrg are computed in PostCompute_ModelTransform (SIMD bulk + sparse fixups)
				b.transform.axisLenInv[i] = 1.0f;

				// Always write lightingOrigin_* to a valid value (effective lighting origin).
				// This avoids reading uninitialized data in PostCompute and enables branch-free bulk copy.
				const bool hasLO = (ent.e.renderfx & RF_LIGHTING_ORIGIN) != 0;
				b.transform.hasLightingOrigin[i] = static_cast<std::uint8_t>(hasLO);
				if (hasLO)
				{
					b.transform.lightingOrigin_x[i] = ent.e.lightingOrigin[0];
					b.transform.lightingOrigin_y[i] = ent.e.lightingOrigin[1];
					b.transform.lightingOrigin_z[i] = ent.e.lightingOrigin[2];
				}
				else
				{
					b.transform.lightingOrigin_x[i] = ent.e.origin[0];
					b.transform.lightingOrigin_y[i] = ent.e.origin[1];
					b.transform.lightingOrigin_z[i] = ent.e.origin[2];
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
				out.spriteSlotOfEnt.set(entNum, static_cast<std::int16_t>(i));

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
				out.beamSlotOfEnt.set(entNum, static_cast<std::int16_t>(i));

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
				out.lightningSlotOfEnt.set(entNum, static_cast<std::int16_t>(i));

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
				out.railCoreSlotOfEnt.set(entNum, static_cast<std::int16_t>(i));

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
				out.railRingsSlotOfEnt.set(entNum, static_cast<std::int16_t>(i));

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

	static inline void PostCompute_ModelTransform(
		EntityTransformSoA& t,
		const std::uint16_t* nonNormSlots,
		const int nonNormCount,
		const int countPadded) noexcept
	{
		if (countPadded <= 0) return;

#if TR_HAS_XSIMD
		using batch = xsimd::batch<float>;
		constexpr int W = static_cast<int>(batch::size);

		// 1) axisLenInv = 1.0f (bulk)
		{
			for (int i = 0; i < countPadded; i += W)
			{
				batch(1.0f).store_aligned(t.axisLenInv.data() + i);
			}
		}

		// 2) sparse fixups for nonNormalizedAxes (avoid per-lane staging / selects)
		for (int k = 0; k < nonNormCount; ++k)
		{
			const int i = static_cast<int>(nonNormSlots[k]);

			const float x = t.ax0_x[i];
			const float y = t.ax0_y[i];
			const float z = t.ax0_z[i];
			const float len = std::sqrtf(x * x + y * y + z * z);
			t.axisLenInv[i] = (len != 0.0f) ? (1.0f / len) : 0.0f;
		}

		// 3) lightOrg = lightingOrigin (bulk, branch-free)
		{
			for (int i = 0; i < countPadded; i += W)
			{
				const batch lx = batch::load_aligned(t.lightingOrigin_x.data() + i);
				const batch ly = batch::load_aligned(t.lightingOrigin_y.data() + i);
				const batch lz = batch::load_aligned(t.lightingOrigin_z.data() + i);

				lx.store_aligned(t.lightOrg_x.data() + i);
				ly.store_aligned(t.lightOrg_y.data() + i);
				lz.store_aligned(t.lightOrg_z.data() + i);
			}
		}
#else
		// scalar fallback
		for (int i = 0; i < countPadded; ++i)
			t.axisLenInv[i] = 1.0f;

		for (int k = 0; k < nonNormCount; ++k)
		{
			const int i = static_cast<int>(nonNormSlots[k]);

			const float x = t.ax0_x[i];
			const float y = t.ax0_y[i];
			const float z = t.ax0_z[i];
			const float len = std::sqrtf(x * x + y * y + z * z);
			t.axisLenInv[i] = (len != 0.0f) ? (1.0f / len) : 0.0f;
		}

		for (int i = 0; i < countPadded; ++i)
		{
			t.lightOrg_x[i] = t.lightingOrigin_x[i];
			t.lightOrg_y[i] = t.lightingOrigin_y[i];
			t.lightOrg_z[i] = t.lightingOrigin_z[i];
		}
#endif
	}

	static inline void PadOneFxSprite(FxSpriteSoA& fx) noexcept
	{
		fx.countPadded = RoundUpToSimdW_F32(fx.count);
		const int c = fx.count;
		const int p = fx.countPadded;
		if (p <= c) return;

		for (int i = c; i < p; ++i)
		{
			fx.entNum[i] = 0;
			fx.renderfx[i] = RF_CROSSHAIR; // => fog always 0
			fx.customShader[i] = 0;
			fx.shaderPtr[i] = nullptr;
			fx.fogNum[i] = 0;

			fx.origin_x[i] = 0.0f; fx.origin_y[i] = 0.0f; fx.origin_z[i] = 0.0f;
			fx.radius[i] = 0.0f;
			fx.rotation[i] = 0.0f;
			fx.st0[i] = 0.0f; fx.st1[i] = 0.0f;

			fx.shaderPacked[i] = 0;
		}
	}

	static inline void PadOneFxBeam(FxBeamSoA& fx) noexcept
	{
		fx.countPadded = RoundUpToSimdW_F32(fx.count);
		const int c = fx.count;
		const int p = fx.countPadded;
		if (p <= c) return;

		for (int i = c; i < p; ++i)
		{
			fx.entNum[i] = 0;
			fx.renderfx[i] = RF_CROSSHAIR; // => fog always 0
			fx.customShader[i] = 0;
			fx.shaderPtr[i] = nullptr;
			fx.fogNum[i] = 0;

			fx.from_x[i] = 0.0f; fx.from_y[i] = 0.0f; fx.from_z[i] = 0.0f;
			fx.to_x[i] = 0.0f;   fx.to_y[i] = 0.0f;   fx.to_z[i] = 0.0f;

			fx.frameOrDiameter[i] = 0;
		}
	}

	static inline void PadFxForSIMD(FrameSoA& out) noexcept
	{
		PadOneFxSprite(out.sprites);
		PadOneFxBeam(out.beams);
		PadOneFxBeam(out.lightning);
		PadOneFxBeam(out.rail_core);
		PadOneFxBeam(out.rail_rings);
	}


	static inline void PadModelsForSIMD(FrameSoA& out) noexcept
	{
		auto& b = out.models;
		b.countPadded = RoundUpToSimdW_F32(b.count);

		const int count = b.count;
		const int pad = b.countPadded;
		if (pad <= count) return;

		// Models bucket tail padding (neutral defaults)
		for (int i = count; i < pad; ++i)
		{
			// render
			b.render.entNum[i] = 0;
			b.render.reType[i] = 0;
			b.render.renderfx[i] = 0;
			b.render.hModel[i] = 0;
			b.render.customShader[i] = 0;

			// transform (identity-ish, but neutral)
			b.transform.origin_x[i] = 0.0f;
			b.transform.origin_y[i] = 0.0f;
			b.transform.origin_z[i] = 0.0f;

			b.transform.ax0_x[i] = 1.0f; b.transform.ax0_y[i] = 0.0f; b.transform.ax0_z[i] = 0.0f;
			b.transform.ax1_x[i] = 0.0f; b.transform.ax1_y[i] = 1.0f; b.transform.ax1_z[i] = 0.0f;
			b.transform.ax2_x[i] = 0.0f; b.transform.ax2_y[i] = 0.0f; b.transform.ax2_z[i] = 1.0f;

			b.transform.axisLenInv[i] = 1.0f;

			// effective lighting origin already (always valid)
			b.transform.lightingOrigin_x[i] = 0.0f;
			b.transform.lightingOrigin_y[i] = 0.0f;
			b.transform.lightingOrigin_z[i] = 0.0f;

			b.transform.lightOrg_x[i] = 0.0f;
			b.transform.lightOrg_y[i] = 0.0f;
			b.transform.lightOrg_z[i] = 0.0f;

			b.transform.nonNormalizedAxes[i] = 0;
			b.transform.hasLightingOrigin[i] = 0;

			// anim
			b.anim.frame[i] = 0;
			b.anim.oldframe[i] = 0;
			b.anim.backlerp[i] = 0.0f;

			// lighting
			b.lighting.lightDir_x[i] = 0.0f;
			b.lighting.lightDir_y[i] = 0.0f;
			b.lighting.lightDir_z[i] = 0.0f;
			b.lighting.ambient_x[i] = 0.0f;
			b.lighting.ambient_y[i] = 0.0f;
			b.lighting.ambient_z[i] = 0.0f;
			b.lighting.directed_x[i] = 0.0f;
			b.lighting.directed_y[i] = 0.0f;
			b.lighting.directed_z[i] = 0.0f;
			b.lighting.ambientPacked[i] = 0;
			b.lighting.lightingCalculated[i] = 0;
			b.lighting.intShaderTime[i] = 0;

			// shader
			b.shader.shaderTC0[i] = 0.0f;
			b.shader.shaderTC1[i] = 0.0f;
			b.shader.shaderColorPacked[i] = 0;
			b.shader.shaderTimeRaw[i] = 0;
		}

		// Derived tail padding (so hot SIMD loops can run to countPadded)
		auto& md = out.modelDerived;
		for (int i = count; i < pad; ++i)
		{
			md.viewOrigin_x[i] = 0.0f;
			md.viewOrigin_y[i] = 0.0f;
			md.viewOrigin_z[i] = 0.0f;

			md.boundRadius[i] = 0.0f;
			md.lod[i] = 0;

			md.cullLocal_x[i] = 0.0f;
			md.cullLocal_y[i] = 0.0f;
			md.cullLocal_z[i] = 0.0f;
			md.cullRadius[i] = 0.0f;
			md.cullResult[i] = CULL_OUT;

			md.modelType[i] = 0xFF;
			md.modelPtr[i] = nullptr;
			md.fogNum[i] = 0;

			float* m = md.mat_ptr(i);
			for (int k = 0; k < 16; ++k) m[k] = 0.0f;
		}
	}

	static inline void BuildFrameSoA(trRefdef_t& refdef, const viewParms_t& vp, FrameSoA& out) noexcept
	{
		out.viewStamp = tr.viewCount;

		BuildEntitiesSoA(refdef, vp, out);
		PadModelsForSIMD(out);
		PadFxForSIMD(out);
		PostCompute_ModelTransform(
			out.models.transform,
			out.modelNonNormSlots.data(),
			out.modelNonNormCount,
			out.models.countPadded);

		BuildDlightsSoA_FromAoS(static_cast<int>(refdef.num_dlights), refdef.dlights, out.dlights_refdef);
		BuildDlightsSoA_FromAoS(static_cast<int>(vp.num_dlights), vp.dlights, out.dlights_view);
	}
} // namespace trsoa

#endif // TR_SOA_FRAME_HPP
