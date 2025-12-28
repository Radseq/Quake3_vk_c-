#ifndef TR_SOA_REFENTS_HPP
#define TR_SOA_REFENTS_HPP

#include <array>
#include <cstdint>
#include <span>

#include "tr_local.hpp" // trRefdef_t, trRefEntity_t, refEntity_t, model_t, shader_t, viewParms_t, qhandle_t, vec3_t, etc.

namespace trsoa
{
	// ---------------------------------------------
	// Fixed-capacity index bucket (no heap)
	// ---------------------------------------------
	template <std::size_t Capacity, typename IndexT = std::uint16_t>
	struct IndexBucket
	{
		std::array<IndexT, Capacity> data{};
		std::uint32_t count = 0;

		inline void reset() noexcept { count = 0; }

		inline void push(IndexT v) noexcept
		{
			// Release: zakładamy, że MAX_REFENTITIES i call-site nie przepełnią.
			data[count++] = v;
		}

		[[nodiscard]] inline std::span<const IndexT> span() const noexcept
		{
			return { data.data(), static_cast<std::size_t>(count) };
		}

		[[nodiscard]] inline IndexT size() const noexcept { return static_cast<IndexT>(count); }
		[[nodiscard]] inline const IndexT* begin() const noexcept { return data.data(); }
		[[nodiscard]] inline const IndexT* end() const noexcept { return data.data() + count; }
		[[nodiscard]] inline IndexT operator[](IndexT i) const noexcept { return data[i]; }
	};

	// ---------------------------------------------
	// Buckets per-frame (indices into tr.refdef.entities[])
	// ---------------------------------------------
	struct FrameEntityBuckets
	{
		IndexBucket<MAX_REFENTITIES> models;
		IndexBucket<MAX_REFENTITIES> generated;      // RT_SPRITE/BEAM/LIGHTNING/RAIL_*
		IndexBucket<MAX_REFENTITIES> portalSurfaces; // RT_PORTALSURFACE
		IndexBucket<MAX_REFENTITIES> polys;          // RT_POLY (jeśli używasz)
		IndexBucket<MAX_REFENTITIES> unknown;        // inne reType (opcjonalnie do debug)

		inline void reset() noexcept
		{
			models.reset();
			generated.reset();
			portalSurfaces.reset();
			polys.reset();
			unknown.reset();
		}
	};

	// ---------------------------------------------
	// SoA scratch: "packed" view of entities for this frame
	// (nie zastępuje AoS, to tylko cache do przyszłego SIMD / mniej derefów)
	// ---------------------------------------------
	struct alignas(64) FrameEntitiesSoA
	{
		std::uint16_t count = 0;

		// map packed slot -> entity index in tr.refdef.entities[]
		std::array<std::uint16_t, MAX_REFENTITIES> entIndex{};

		// Hot fields (minimal set, łatwo rozszerzyć)
		std::array<std::uint8_t,  MAX_REFENTITIES> reType{};     // refEntityType_t fits in u8
		std::array<std::uint8_t,  MAX_REFENTITIES> nnAxes{};     // nonNormalizedAxes 0/1
		std::array<std::uint16_t, MAX_REFENTITIES> frame{};
		std::array<std::uint16_t, MAX_REFENTITIES> oldframe{};

		std::array<std::uint32_t, MAX_REFENTITIES> renderfx{};
		std::array<qhandle_t,     MAX_REFENTITIES> hModel{};
		std::array<qhandle_t,     MAX_REFENTITIES> customShader{};
		std::array<qhandle_t,     MAX_REFENTITIES> customSkin{};
		std::array<std::uint16_t, MAX_REFENTITIES> skinNum{};

		// origin (SoA – pod SIMD później)
		std::array<float, MAX_REFENTITIES> ox{};
		std::array<float, MAX_REFENTITIES> oy{};
		std::array<float, MAX_REFENTITIES> oz{};

		inline void clear() noexcept { count = 0; }
	};

	// ---------------------------------------------
	// Model-only SoA (packed separately) + resolved model_t*
	// ---------------------------------------------
	struct alignas(64) ModelsSoA
	{
		std::uint16_t count = 0;

		std::array<std::uint16_t, MAX_REFENTITIES> entIndex{}; // index into tr.refdef.entities[]
		std::array<model_t*,      MAX_REFENTITIES> modelPtr{}; // resolved once per frame

		std::array<std::uint16_t, MAX_REFENTITIES> frame{};
		std::array<std::uint16_t, MAX_REFENTITIES> oldframe{};
		std::array<std::uint32_t, MAX_REFENTITIES> renderfx{};
		std::array<std::uint8_t,  MAX_REFENTITIES> nnAxes{};

		std::array<float, MAX_REFENTITIES> ox{};
		std::array<float, MAX_REFENTITIES> oy{};
		std::array<float, MAX_REFENTITIES> oz{};

		inline void clear() noexcept { count = 0; }
	};

	// ---------------------------------------------
	// Frame gather = buckets + SoA
	// ---------------------------------------------
	struct FrameGather
	{
		FrameEntityBuckets buckets{};
		FrameEntitiesSoA    all{};
		ModelsSoA           models{};

		inline void reset() noexcept
		{
			buckets.reset();
			all.clear();
			models.clear();
		}
	};

	// ---------------------------------------------
	// Build: AoS -> SoA + buckets in one pass
	// - zachowuje Twoją semantykę: RF_FIRST_PERSON nie trafia do portal view
	// ---------------------------------------------
	void BuildFrameGather(
		FrameGather& out,
		const trRefdef_t& refdef,
		const viewParms_t& view,
		model_t* (*GetModelByHandle)(qhandle_t)
	) noexcept;

	// ---------------------------------------------
	// Model type buckets (po resolved modelPtr)
	// ---------------------------------------------
	struct ModelTypeBuckets
	{
		IndexBucket<MAX_REFENTITIES> md3;
		IndexBucket<MAX_REFENTITIES> mdr;
		IndexBucket<MAX_REFENTITIES> iqm;
		IndexBucket<MAX_REFENTITIES> brush;
		IndexBucket<MAX_REFENTITIES> bad;
		IndexBucket<MAX_REFENTITIES> unknown;

		inline void reset() noexcept
		{
			md3.reset(); mdr.reset(); iqm.reset(); brush.reset(); bad.reset(); unknown.reset();
		}
	};

	void BucketModelsByType(
		ModelTypeBuckets& out,
		const ModelsSoA& models
	) noexcept;

	// ---------------------------------------------
	// High-level: drop-in replacement for R_AddEntitySurfaces
	// (dalej używa Twoich istniejących funkcji renderujących)
	// ---------------------------------------------
	struct AddEntitySurfacesApi
	{
		trGlobals_t* tr = nullptr;

		// required engine functions
		shader_t* (*GetShaderByHandle)(qhandle_t) = nullptr;
		model_t*  (*GetModelByHandle)(qhandle_t) = nullptr;

		// generated entities
		int (*SpriteFogNum)(const trRefEntity_t&) = nullptr;
		void (*AddDrawSurf)(surfaceType_t&, shader_t&, int fogIndex, int dlightMap) = nullptr;

		// model pipeline
		void (*RotateForEntity)(const trRefEntity_t&, const viewParms_t&, orientationr_t&) = nullptr;

		void (*AddMD3)(trRefEntity_t&) = nullptr;
		void (*AddMDR)(trRefEntity_t&) = nullptr;
		void (*AddIQM)(trRefEntity_t&) = nullptr;
		void (*AddBrush)(trRefEntity_t&) = nullptr;
	};

	// entitySurface to zwykle globalny "entitySurface" jak w Q3 (SF_ENTITY)
	void R_AddEntitySurfaces_SoA(
		const AddEntitySurfacesApi& api,
		surfaceType_t& entitySurface
	);

} // namespace trsoa

#endif // TR_SOA_REFENTS_HPP
