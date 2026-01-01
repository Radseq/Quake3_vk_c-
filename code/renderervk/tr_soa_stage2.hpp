#pragma once

#include "tr_soa_frame.hpp"
#include "math.hpp"
#include "tr_model.hpp"

#if defined(_MSC_VER)
#  define TR_RESTRICT __restrict
#  define TR_FORCEINLINE __forceinline
#else
#  define TR_RESTRICT __restrict__
#  define TR_FORCEINLINE inline __attribute__((always_inline))
#endif

extern void myGlMultMatrix_SIMD(const float* a, const float* b, float* out);

static void myGlMultMatrix_SIMD_test(const float* a, const float* b, float* out)
{
	__m128 rowA;

	for (int i = 0; i < 4; ++i)
	{
		// Compute each row of the output matrix
		__m128 colB0 = _mm_loadu_ps(&b[0]);	 // Load first column of B
		__m128 colB1 = _mm_loadu_ps(&b[4]);	 // Load second column of B
		__m128 colB2 = _mm_loadu_ps(&b[8]);	 // Load third column of B
		__m128 colB3 = _mm_loadu_ps(&b[12]); // Load fourth column of B

		rowA = _mm_set1_ps(a[i * 4 + 0]); // Broadcast element a[i][0]
		__m128 res0 = _mm_mul_ps(rowA, colB0);

		rowA = _mm_set1_ps(a[i * 4 + 1]); // Broadcast element a[i][1]
		__m128 res1 = _mm_mul_ps(rowA, colB1);

		rowA = _mm_set1_ps(a[i * 4 + 2]); // Broadcast element a[i][2]
		__m128 res2 = _mm_mul_ps(rowA, colB2);

		rowA = _mm_set1_ps(a[i * 4 + 3]); // Broadcast element a[i][3]
		__m128 res3 = _mm_mul_ps(rowA, colB3);

		// Accumulate all partial results
		__m128 result = _mm_add_ps(_mm_add_ps(res0, res1), _mm_add_ps(res2, res3));

		// Store the computed row in the output matrix
		_mm_storeu_ps(&out[i * 4], result);
	}
}
// cvars (są używane w R_ComputeLOD)
extern cvar_t* r_lodscale;
extern cvar_t* r_lodbias;

namespace trsoa
{

#if defined(XSIMD_WITH_SSE2) && XSIMD_WITH_SSE2
	using b4 = xsimd::batch<float, xsimd::sse2>;
#else
	using b4 = xsimd::batch<float>;
#endif

	TR_FORCEINLINE void MultAxisOriginWithWorldMatrix_sse_preB(
		const __m128 B0, const __m128 B1, const __m128 B2, const __m128 B3,
		const float ax0x, const float ax0y, const float ax0z,
		const float ax1x, const float ax1y, const float ax1z,
		const float ax2x, const float ax2y, const float ax2z,
		const float ox, const float oy, const float oz,
		float* __restrict outM // 16 floats, aligned 16
	) noexcept
	{
		// Zakładamy alignment (masz alignas(64) na modelMatrix, i*16*4=64).
		outM = static_cast<float*>(__builtin_assume_aligned(outM, 16));

		// row0 = ax0x*B0 + ax1x*B1 + ax2x*B2 + ox*B3
		{
			__m128 r = _mm_mul_ps(_mm_set1_ps(ax0x), B0);
			r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(ax1x), B1));
			r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(ax2x), B2));
			r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(ox), B3));
			_mm_store_ps(outM + 0, r);
		}
		// row1
		{
			__m128 r = _mm_mul_ps(_mm_set1_ps(ax0y), B0);
			r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(ax1y), B1));
			r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(ax2y), B2));
			r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(oy), B3));
			_mm_store_ps(outM + 4, r);
		}
		// row2
		{
			__m128 r = _mm_mul_ps(_mm_set1_ps(ax0z), B0);
			r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(ax1z), B1));
			r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(ax2z), B2));
			r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(oz), B3));
			_mm_store_ps(outM + 8, r);
		}

		// row3 = [0 0 0 1] * B => B3
		_mm_store_ps(outM + 12, B3);
	}


	TR_FORCEINLINE void myGlMultMatrix_xsimd_fast(
		const float* TR_RESTRICT a,
		const float* TR_RESTRICT b,
		float* TR_RESTRICT out) noexcept
	{
		using arch = xsimd::sse2;
		using batch4 = xsimd::batch<float, arch>;
		static_assert(batch4::size == 4);

		// Jeśli nie masz 100% gwarancji 16B alignmentu: użyj load_unaligned/store_unaligned.
		const batch4 b0 = batch4::load_aligned(b + 0);
		const batch4 b1 = batch4::load_aligned(b + 4);
		const batch4 b2 = batch4::load_aligned(b + 8);
		const batch4 b3 = batch4::load_aligned(b + 12);

		for (int i = 0; i < 4; ++i)
		{
			const batch4 a0(a[i * 4 + 0]);
			const batch4 a1(a[i * 4 + 1]);
			const batch4 a2(a[i * 4 + 2]);
			const batch4 a3(a[i * 4 + 3]);

			// Struktura jak u Ciebie: (res0+res1) + (res2+res3)
			const batch4 r01 = a0 * b0 + a1 * b1;
			const batch4 r23 = a2 * b2 + a3 * b3;
			const batch4 r = r01 + r23;

			r.store_aligned(out + i * 4);
		}
	}

	// 4-float batch (SSE-klasa) - idealne do macierzy 4x4
	//using mat_batch4 = xsimd::batch<float, xsimd::sse2>;
	// slower than myGlMultMatrix_SIMD why?????????
	//TR_FORCEINLINE void myGlMultMatrix_NEW(const float* a, const float* b, float* out) noexcept
	//{
	//	// Hoist loadów B poza pętlę
	//	const mat_batch4 b0 = mat_batch4::load_aligned(b + 0);
	//	const mat_batch4 b1 = mat_batch4::load_aligned(b + 4);
	//	const mat_batch4 b2 = mat_batch4::load_aligned(b + 8);
	//	const mat_batch4 b3 = mat_batch4::load_aligned(b + 12);

	//	// i = 0..3 (kolumny w układzie Q3/OpenGL-style)
	//	for (int i = 0; i < 4; ++i)
	//	{
	//		const mat_batch4 a0(a[i * 4 + 0]);
	//		const mat_batch4 a1(a[i * 4 + 1]);
	//		const mat_batch4 a2(a[i * 4 + 2]);
	//		const mat_batch4 a3(a[i * 4 + 3]);

	//		// FMA jeśli dostępne w arch; xsimd::fma wybierze najlepszą implementację
	//		mat_batch4 r = a0 * b0;
	//		r = xsimd::fma(a1, b1, r);
	//		r = xsimd::fma(a2, b2, r);
	//		r = xsimd::fma(a3, b3, r);

	//		r.store_unaligned(out + i * 4);
	//	}
	//}

	// ---------------------------------------------------------------------
	// 2.1 Precompute modelMatrix + viewOrigin (SIMD viewOrigin)
	// ---------------------------------------------------------------------
	static inline void PrecomputeModelDerived(const EntityBucketSoA& in, const viewParms_t& vp, ModelDerivedSoA& out) noexcept
	{
		const int count = in.count;
		if (count <= 0) return;

		//const float* const worldM = vp.world.modelMatrix;

		// Preload kolumn/wierszy B tylko raz
		//const __m128 B0 = _mm_load_ps(worldM + 0);
		//const __m128 B1 = _mm_load_ps(worldM + 4);
		//const __m128 B2 = _mm_load_ps(worldM + 8);
		//const __m128 B3 = _mm_load_ps(worldM + 12);

		for (int i = 0; i < count; ++i) { 
			alignas(16) float glMatrix[16]; 
			const float ox = in.transform.origin_x[i]; 
			const float oy = in.transform.origin_y[i]; 
			const float oz = in.transform.origin_z[i]; 
			glMatrix[0] = in.transform.ax0_x[i]; 
			glMatrix[4] = in.transform.ax1_x[i]; 
			glMatrix[8] = in.transform.ax2_x[i]; 
			glMatrix[12] = ox; 
			glMatrix[1] = in.transform.ax0_y[i]; 
			glMatrix[5] = in.transform.ax1_y[i]; 
			glMatrix[9] = in.transform.ax2_y[i]; 
			glMatrix[13] = oy; 
			glMatrix[2] = in.transform.ax0_z[i]; 
			glMatrix[6] = in.transform.ax1_z[i]; 
			glMatrix[10] = in.transform.ax2_z[i]; 
			glMatrix[14] = oz; 
			glMatrix[3] = 0.0f;
			glMatrix[7] = 0.0f;
			glMatrix[11] = 0.0f; 
			glMatrix[15] = 1.0f; 
			myGlMultMatrix_SIMD(glMatrix, vp.world.modelMatrix, out.mat_ptr(i));
		}

		//const float* const worldM = vp.world.modelMatrix;

		//for (int i = 0; i < count; ++i)
		//{
		//	float* const outM = out.mat_ptr(i);
		//	trsoa::MultAxisOriginWithWorldMatrix_xsimd4(
		//		in.transform.ax0_x[i], in.transform.ax0_y[i], in.transform.ax0_z[i],
		//		in.transform.ax1_x[i], in.transform.ax1_y[i], in.transform.ax1_z[i],
		//		in.transform.ax2_x[i], in.transform.ax2_y[i], in.transform.ax2_z[i],
		//		in.transform.origin_x[i], in.transform.origin_y[i], in.transform.origin_z[i],
		//		worldM,
		//		outM
		//	);
		//}

		const float camX = vp.ort.origin[0];
		const float camY = vp.ort.origin[1];
		const float camZ = vp.ort.origin[2];

#if TR_HAS_XSIMD
		using batch = xsimd::batch<float>;
		constexpr int W = static_cast<int>(batch::size);

		int i = 0;
		for (; i + W <= count; i += W)
		{
			const batch ox = batch::load_aligned(in.transform.origin_x.data() + i);
			const batch oy = batch::load_aligned(in.transform.origin_y.data() + i);
			const batch oz = batch::load_aligned(in.transform.origin_z.data() + i);

			const batch dx = batch(camX) - ox;
			const batch dy = batch(camY) - oy;
			const batch dz = batch(camZ) - oz;

			const batch ax0x = batch::load_aligned(in.transform.ax0_x.data() + i);
			const batch ax0y = batch::load_aligned(in.transform.ax0_y.data() + i);
			const batch ax0z = batch::load_aligned(in.transform.ax0_z.data() + i);

			const batch ax1x = batch::load_aligned(in.transform.ax1_x.data() + i);
			const batch ax1y = batch::load_aligned(in.transform.ax1_y.data() + i);
			const batch ax1z = batch::load_aligned(in.transform.ax1_z.data() + i);

			const batch ax2x = batch::load_aligned(in.transform.ax2_x.data() + i);
			const batch ax2y = batch::load_aligned(in.transform.ax2_y.data() + i);
			const batch ax2z = batch::load_aligned(in.transform.ax2_z.data() + i);

			const batch invLen = batch::load_aligned(in.transform.axisLenInv.data() + i);

			const batch v0 = (dx * ax0x + dy * ax0y + dz * ax0z) * invLen;
			const batch v1 = (dx * ax1x + dy * ax1y + dz * ax1z) * invLen;
			const batch v2 = (dx * ax2x + dy * ax2y + dz * ax2z) * invLen;

			v0.store_aligned(out.viewOrigin_x.data() + i);
			v1.store_aligned(out.viewOrigin_y.data() + i);
			v2.store_aligned(out.viewOrigin_z.data() + i);
		}

		for (; i < count; ++i)
		{
			const float dx = camX - in.transform.origin_x[i];
			const float dy = camY - in.transform.origin_y[i];
			const float dz = camZ - in.transform.origin_z[i];

			const float invLen = in.transform.axisLenInv[i];

			out.viewOrigin_x[i] = (dx * in.transform.ax0_x[i] + dy * in.transform.ax0_y[i] + dz * in.transform.ax0_z[i]) * invLen;
			out.viewOrigin_y[i] = (dx * in.transform.ax1_x[i] + dy * in.transform.ax1_y[i] + dz * in.transform.ax1_z[i]) * invLen;
			out.viewOrigin_z[i] = (dx * in.transform.ax2_x[i] + dy * in.transform.ax2_y[i] + dz * in.transform.ax2_z[i]) * invLen;
		}
#else
		for (int i = 0; i < count; ++i)
		{
			const float dx = camX - in.transform.origin_x[i];
			const float dy = camY - in.transform.origin_y[i];
			const float dz = camZ - in.transform.origin_z[i];

			const float invLen = in.transform.axisLenInv[i];

			out.viewOrigin_x[i] = (dx * in.transform.ax0_x[i] + dy * in.transform.ax0_y[i] + dz * in.transform.ax0_z[i]) * invLen;
			out.viewOrigin_y[i] = (dx * in.transform.ax1_x[i] + dy * in.transform.ax1_y[i] + dz * in.transform.ax1_z[i]) * invLen;
			out.viewOrigin_z[i] = (dx * in.transform.ax2_x[i] + dy * in.transform.ax2_y[i] + dz * in.transform.ax2_z[i]) * invLen;
		}
#endif
	}

	static inline void ApplyModelOrientationFromSoA(const FrameSoA& soa, const int modelSlot, orientationr_t& ort) noexcept
	{
		const auto& t = soa.models.transform;
		const auto& d = soa.modelDerived;

		ort.origin[0] = t.origin_x[modelSlot];
		ort.origin[1] = t.origin_y[modelSlot];
		ort.origin[2] = t.origin_z[modelSlot];

		ort.axis[0][0] = t.ax0_x[modelSlot];
		ort.axis[0][1] = t.ax0_y[modelSlot];
		ort.axis[0][2] = t.ax0_z[modelSlot];

		ort.axis[1][0] = t.ax1_x[modelSlot];
		ort.axis[1][1] = t.ax1_y[modelSlot];
		ort.axis[1][2] = t.ax1_z[modelSlot];

		ort.axis[2][0] = t.ax2_x[modelSlot];
		ort.axis[2][1] = t.ax2_y[modelSlot];
		ort.axis[2][2] = t.ax2_z[modelSlot];

		std::memcpy(ort.modelMatrix, d.mat_ptr(modelSlot), 16 * sizeof(float));

		ort.viewOrigin[0] = d.viewOrigin_x[modelSlot];
		ort.viewOrigin[1] = d.viewOrigin_y[modelSlot];
		ort.viewOrigin[2] = d.viewOrigin_z[modelSlot];
	}

	// ---------------------------------------------------------------------
	// 2.2 Batch LOD (SIMD ProjectRadius part, scalar radius fetch)
	// ---------------------------------------------------------------------
	[[nodiscard]] static inline float ProjectRadiusFast(
		const float r, const float dist,
		const float pm5, const float pm9, const float pm13,
		const float pm7, const float pm11, const float pm15) noexcept
	{
#ifndef NDEBUG
		// RadiusFromBounds -> VectorLength => r powinno być >= 0
		assert(r >= 0.0f);
#endif

		const float pr1 = (r * pm5) + (-dist * pm9) + pm13;
		const float pr3 = (r * pm7) + (-dist * pm11) + pm15;

		float pr = pr1 / pr3;

		// Oryginał clampował tylko górę; zostawiamy tak samo, by nie zmieniać zachowania.
		if (pr > 1.0f) pr = 1.0f;
		return pr;
	}



	static inline void ComputeModelLODs_Batch(FrameSoA& soa, const trRefdef_t& refdef) noexcept
	{
		const int count = soa.models.count;
		if (count <= 0) return;

		// stałe z ProjectRadius
		const vec3_t& ax0 = tr.viewParms.ort.axis[0];
		const float c = DotProduct(ax0, tr.viewParms.ort.origin);

		float lodscale = r_lodscale->value;
		if (lodscale > 20.0f) lodscale = 20.0f;
		const int bias = r_lodbias->integer;

		const float pm5 = tr.viewParms.projectionMatrix[5];
		const float pm9 = tr.viewParms.projectionMatrix[9];
		const float pm13 = tr.viewParms.projectionMatrix[13];
		const float pm7 = tr.viewParms.projectionMatrix[7];
		const float pm11 = tr.viewParms.projectionMatrix[11];
		const float pm15 = tr.viewParms.projectionMatrix[15];

#if TR_HAS_XSIMD
		using batch = xsimd::batch<float>;
		constexpr int W = static_cast<int>(batch::size);

		alignas(64) float distTmp[W]{};

		int i = 0;
		for (; i + W <= count; i += W)
		{
			const batch ox = batch::load_aligned(soa.models.transform.origin_x.data() + i);
			const batch oy = batch::load_aligned(soa.models.transform.origin_y.data() + i);
			const batch oz = batch::load_aligned(soa.models.transform.origin_z.data() + i);

			const batch dist = (ox * batch(ax0[0])) + (oy * batch(ax0[1])) + (oz * batch(ax0[2])) - batch(c);
			dist.store_aligned(distTmp);

			for (int lane = 0; lane < W; ++lane)
			{
				const int slot = i + lane;
				const int entNum = static_cast<int>(soa.models.render.entNum[slot]);
				trRefEntity_t& ent = const_cast<trRefEntity_t&>(refdef.entities[entNum]);

				model_t* m = R_GetModelByHandle(ent.e.hModel);
				if (!m || m->numLods < 2)
				{
					soa.modelDerived.lod[slot] = static_cast<std::uint8_t>(0);
					soa.modelDerived.boundRadius[slot] = 0.0f;
					continue;
				}

				float radius = 0.0f;
				if (m->type == modtype_t::MOD_MDR)
				{
					mdrHeader_t* mdr = (mdrHeader_t*)m->modelData;
					const int frameSize = (int)(size_t)(&((mdrFrame_t*)0)->bones[mdr->numBones]);
					mdrFrame_t* f = (mdrFrame_t*)((byte*)mdr + mdr->ofsFrames + frameSize * ent.e.frame);
					radius = RadiusFromBounds(f->bounds[0], f->bounds[1]);
				}
				else
				{
					md3Frame_t* frame = (md3Frame_t*)(((unsigned char*)m->md3[0]) + m->md3[0]->ofsFrames);
					frame += ent.e.frame;
					radius = RadiusFromBounds(frame->bounds[0], frame->bounds[1]);
				}

				soa.modelDerived.boundRadius[slot] = radius;

				const float d = distTmp[lane];
				int lod = 0;

				if (d <= 0.0f)
				{
					lod = 0;
				}
				else
				{
					const float pr = ProjectRadiusFast(radius, d, pm5, pm9, pm13, pm7, pm11, pm15);
					float flod = 1.0f - pr * lodscale;
					flod *= (float)m->numLods;
					lod = myftol(flod);

					if (lod < 0) lod = 0;
					else if (lod >= m->numLods) lod = m->numLods - 1;
				}

				lod += bias;
				if (lod >= m->numLods) lod = m->numLods - 1;
				if (lod < 0) lod = 0;

				soa.modelDerived.lod[slot] = static_cast<std::uint8_t>(lod);
			}
		}

		for (; i < count; ++i)
#else
		for (int i = 0; i < count; ++i)
#endif
		{
#if !TR_HAS_XSIMD
			const int entNum = static_cast<int>(soa.models.render.entNum[i]);
			trRefEntity_t& ent = const_cast<trRefEntity_t&>(refdef.entities[entNum]);
#else
			const int entNum = static_cast<int>(soa.models.render.entNum[i]);
			trRefEntity_t& ent = const_cast<trRefEntity_t&>(refdef.entities[entNum]);
#endif
			model_t* m = R_GetModelByHandle(ent.e.hModel);
			if (!m || m->numLods < 2)
			{
				soa.modelDerived.lod[i] = 0;
				soa.modelDerived.boundRadius[i] = 0.0f;
				continue;
			}

			float radius = 0.0f;
			if (m->type == modtype_t::MOD_MDR)
			{
				mdrHeader_t* mdr = (mdrHeader_t*)m->modelData;
				const int frameSize = (int)(size_t)(&((mdrFrame_t*)0)->bones[mdr->numBones]);
				mdrFrame_t* f = (mdrFrame_t*)((byte*)mdr + mdr->ofsFrames + frameSize * ent.e.frame);
				radius = RadiusFromBounds(f->bounds[0], f->bounds[1]);
			}
			else
			{
				md3Frame_t* frame = (md3Frame_t*)(((unsigned char*)m->md3[0]) + m->md3[0]->ofsFrames);
				frame += ent.e.frame;
				radius = RadiusFromBounds(frame->bounds[0], frame->bounds[1]);
			}

			soa.modelDerived.boundRadius[i] = radius;

			const float dist = DotProduct(tr.viewParms.ort.axis[0], ent.e.origin) - c;

			int lod = 0;
			if (dist <= 0.0f)
			{
				lod = 0;
			}
			else
			{
				const float pr = ProjectRadiusFast(radius, dist, pm5, pm9, pm13, pm7, pm11, pm15);
				float flod = 1.0f - pr * lodscale;
				flod *= (float)m->numLods;
				lod = myftol(flod);

				if (lod < 0) lod = 0;
				else if (lod >= m->numLods) lod = m->numLods - 1;
			}

			lod += bias;
			if (lod >= m->numLods) lod = m->numLods - 1;
			if (lod < 0) lod = 0;

			soa.modelDerived.lod[i] = static_cast<std::uint8_t>(lod);
		}
	}

	// ---------------------------------------------------------------------
	// 2.3 SIMD R_TransformDlights (SoA -> AoS writeback)
	// ---------------------------------------------------------------------
	static inline void TransformDlights_SoA_Writeback(const DlightSoA& src, dlight_t* dstAoS, const orientationr_t& ort) noexcept
	{
		const int n = src.count;
		if (n <= 0 || !dstAoS) return;

		const float ox = ort.origin[0];
		const float oy = ort.origin[1];
		const float oz = ort.origin[2];

		const float ax0x = ort.axis[0][0], ax0y = ort.axis[0][1], ax0z = ort.axis[0][2];
		const float ax1x = ort.axis[1][0], ax1y = ort.axis[1][1], ax1z = ort.axis[1][2];
		const float ax2x = ort.axis[2][0], ax2y = ort.axis[2][1], ax2z = ort.axis[2][2];

#if TR_HAS_XSIMD
		using batch = xsimd::batch<float>;
		constexpr int W = static_cast<int>(batch::size);

		int i = 0;
		for (; i + W <= n; i += W)
		{
			const batch lx = batch::load_aligned(src.origin_x.data() + i) - batch(ox);
			const batch ly = batch::load_aligned(src.origin_y.data() + i) - batch(oy);
			const batch lz = batch::load_aligned(src.origin_z.data() + i) - batch(oz);

			const batch tx = lx * batch(ax0x) + ly * batch(ax0y) + lz * batch(ax0z);
			const batch ty = lx * batch(ax1x) + ly * batch(ax1y) + lz * batch(ax1z);
			const batch tz = lx * batch(ax2x) + ly * batch(ax2y) + lz * batch(ax2z);

			alignas(64) float outx[W], outy[W], outz[W];
			tx.store_aligned(outx);
			ty.store_aligned(outy);
			tz.store_aligned(outz);

			for (int lane = 0; lane < W; ++lane)
			{
				dlight_t& dl = dstAoS[i + lane];
				dl.transformed[0] = outx[lane];
				dl.transformed[1] = outy[lane];
				dl.transformed[2] = outz[lane];
			}

			// transformed2 for linear
			alignas(64) float out2x[W], out2y[W], out2z[W];
			const batch l2x = batch::load_aligned(src.origin2_x.data() + i) - batch(ox);
			const batch l2y = batch::load_aligned(src.origin2_y.data() + i) - batch(oy);
			const batch l2z = batch::load_aligned(src.origin2_z.data() + i) - batch(oz);

			const batch t2x = l2x * batch(ax0x) + l2y * batch(ax0y) + l2z * batch(ax0z);
			const batch t2y = l2x * batch(ax1x) + l2y * batch(ax1y) + l2z * batch(ax1z);
			const batch t2z = l2x * batch(ax2x) + l2y * batch(ax2y) + l2z * batch(ax2z);

			t2x.store_aligned(out2x);
			t2y.store_aligned(out2y);
			t2z.store_aligned(out2z);

			for (int lane = 0; lane < W; ++lane)
			{
				if (src.linear[i + lane])
				{
					dlight_t& dl = dstAoS[i + lane];
					dl.transformed2[0] = out2x[lane];
					dl.transformed2[1] = out2y[lane];
					dl.transformed2[2] = out2z[lane];
				}
			}
		}

		for (; i < n; ++i)
#else
		for (int i = 0; i < n; ++i)
#endif
		{
			dlight_t& dl = dstAoS[i];

			const float dx = src.origin_x[i] - ox;
			const float dy = src.origin_y[i] - oy;
			const float dz = src.origin_z[i] - oz;

			dl.transformed[0] = dx * ax0x + dy * ax0y + dz * ax0z;
			dl.transformed[1] = dx * ax1x + dy * ax1y + dz * ax1z;
			dl.transformed[2] = dx * ax2x + dy * ax2y + dz * ax2z;

			if (src.linear[i])
			{
				const float d2x = src.origin2_x[i] - ox;
				const float d2y = src.origin2_y[i] - oy;
				const float d2z = src.origin2_z[i] - oz;

				dl.transformed2[0] = d2x * ax0x + d2y * ax0y + d2z * ax0z;
				dl.transformed2[1] = d2x * ax1x + d2y * ax1y + d2z * ax1z;
				dl.transformed2[2] = d2x * ax2x + d2y * ax2y + d2z * ax2z;
			}
		}
	}

	// ---------------------------------------------------------------------
	// 2.4 SIMD/batch: gather affecting PMLIGHT dlights for entity bounds
	//      (transform + cull) -> returns pointer list (dlights[])
	// ---------------------------------------------------------------------
	static inline int GatherAffectingViewDlights_PMLIGHT(
		FrameSoA& soa,
		const orientationr_t& ort,
		const vec3_t& mins,
		const vec3_t& maxs,
		dlight_t** outPtrs,
		const int outCap) noexcept
	{
		if (!SoA_ValidThisFrame(soa)) return 0;

		const int n = soa.dlights_view.count;
		if (n <= 0 || outCap <= 0) return 0;

		dlight_t* const aos = tr.viewParms.dlights;

		const float ox = ort.origin[0], oy = ort.origin[1], oz = ort.origin[2];

		const float ax0x = ort.axis[0][0], ax0y = ort.axis[0][1], ax0z = ort.axis[0][2];
		const float ax1x = ort.axis[1][0], ax1y = ort.axis[1][1], ax1z = ort.axis[1][2];
		const float ax2x = ort.axis[2][0], ax2y = ort.axis[2][1], ax2z = ort.axis[2][2];

#if TR_HAS_XSIMD
		using batch = xsimd::batch<float>;
		using bmask = typename batch::batch_bool_type;
		constexpr int W = static_cast<int>(batch::size);

		const batch bMinX(mins[0]), bMinY(mins[1]), bMinZ(mins[2]);
		const batch bMaxX(maxs[0]), bMaxY(maxs[1]), bMaxZ(maxs[2]);

		alignas(64) float tx[W], ty[W], tz[W];
		alignas(64) float t2x[W], t2y[W], t2z[W];
		alignas(64) float rads[W];

		int outN = 0;
		int i = 0;

		for (; i + W <= n; i += W)
		{
			// --- Transform: SoA -> local (SIMD) ---
			const batch lx = batch::load_aligned(soa.dlights_view.origin_x.data() + i) - batch(ox);
			const batch ly = batch::load_aligned(soa.dlights_view.origin_y.data() + i) - batch(oy);
			const batch lz = batch::load_aligned(soa.dlights_view.origin_z.data() + i) - batch(oz);

			const batch btx = lx * batch(ax0x) + ly * batch(ax0y) + lz * batch(ax0z);
			const batch bty = lx * batch(ax1x) + ly * batch(ax1y) + lz * batch(ax1z);
			const batch btz = lx * batch(ax2x) + ly * batch(ax2y) + lz * batch(ax2z);

			btx.store_aligned(tx);
			bty.store_aligned(ty);
			btz.store_aligned(tz);

			// endpoint 2 zawsze liczymy SIMD (prościej; koszt mały vs gałęzie)
			const batch l2x = batch::load_aligned(soa.dlights_view.origin2_x.data() + i) - batch(ox);
			const batch l2y = batch::load_aligned(soa.dlights_view.origin2_y.data() + i) - batch(oy);
			const batch l2z = batch::load_aligned(soa.dlights_view.origin2_z.data() + i) - batch(oz);

			const batch bt2x = l2x * batch(ax0x) + l2y * batch(ax0y) + l2z * batch(ax0z);
			const batch bt2y = l2x * batch(ax1x) + l2y * batch(ax1y) + l2z * batch(ax1z);
			const batch bt2z = l2x * batch(ax2x) + l2y * batch(ax2y) + l2z * batch(ax2z);

			bt2x.store_aligned(t2x);
			bt2y.store_aligned(t2y);
			bt2z.store_aligned(t2z);

			const batch br = batch::load_aligned(soa.dlights_view.radius.data() + i);
			br.store_aligned(rads);

			// --- Cull (SIMD) dla punktu 0 ---
			const bmask culX0 = (btx - br > bMaxX) | (btx + br < bMinX);
			const bmask culY0 = (bty - br > bMaxY) | (bty + br < bMinY);
			const bmask culZ0 = (btz - br > bMaxZ) | (btz + br < bMinZ);
			const bmask cul0 = culX0 | culY0 | culZ0;

			// --- Cull (SIMD) dla punktu 1 (linear endpoint) ---
			const bmask culX1 = (bt2x - br > bMaxX) | (bt2x + br < bMinX);
			const bmask culY1 = (bt2y - br > bMaxY) | (bt2y + br < bMinY);
			const bmask culZ1 = (bt2z - br > bMaxZ) | (bt2z + br < bMinZ);
			const bmask cul1 = culX1 | culY1 | culZ1;


			// bit i = 1 => lane i jest culled
			const std::uint64_t m0 = static_cast<std::uint64_t>(cul0.mask());
			const std::uint64_t m1 = static_cast<std::uint64_t>(cul1.mask());

			for (int lane = 0; lane < W; ++lane)
			{
				const int idx = i + lane;
				const bool isLinear = (soa.dlights_view.linear[idx] != 0);

				bool culled = ((m0 >> lane) & 1) != 0;
				if (isLinear)
					culled = culled && (((m1 >> lane) & 1) != 0);

				if (culled) continue;

				dlight_t& dl = aos[idx];
				dl.transformed[0] = tx[lane];
				dl.transformed[1] = ty[lane];
				dl.transformed[2] = tz[lane];

				if (isLinear)
				{
					dl.transformed2[0] = t2x[lane];
					dl.transformed2[1] = t2y[lane];
					dl.transformed2[2] = t2z[lane];
				}

				if (outN < outCap) outPtrs[outN++] = &dl;
			}
		}

		// tail scalar
		for (; i < n; ++i)
		{
			const float dx = soa.dlights_view.origin_x[i] - ox;
			const float dy = soa.dlights_view.origin_y[i] - oy;
			const float dz = soa.dlights_view.origin_z[i] - oz;

			const float txs = dx * ax0x + dy * ax0y + dz * ax0z;
			const float tys = dx * ax1x + dy * ax1y + dz * ax1z;
			const float tzs = dx * ax2x + dy * ax2y + dz * ax2z;

			const float r = soa.dlights_view.radius[i];

			bool culled0 =
				(txs - r > maxs[0]) || (txs + r < mins[0]) ||
				(tys - r > maxs[1]) || (tys + r < mins[1]) ||
				(tzs - r > maxs[2]) || (tzs + r < mins[2]);

			const bool isLinear = (soa.dlights_view.linear[i] != 0);
			bool culled = culled0;

			float t2xs = 0.f, t2ys = 0.f, t2zs = 0.f;
			if (isLinear)
			{
				const float d2x = soa.dlights_view.origin2_x[i] - ox;
				const float d2y = soa.dlights_view.origin2_y[i] - oy;
				const float d2z = soa.dlights_view.origin2_z[i] - oz;

				t2xs = d2x * ax0x + d2y * ax0y + d2z * ax0z;
				t2ys = d2x * ax1x + d2y * ax1y + d2z * ax1z;
				t2zs = d2x * ax2x + d2y * ax2y + d2z * ax2z;

				bool culled1 =
					(t2xs - r > maxs[0]) || (t2xs + r < mins[0]) ||
					(t2ys - r > maxs[1]) || (t2ys + r < mins[1]) ||
					(t2zs - r > maxs[2]) || (t2zs + r < mins[2]);

				culled = culled0 && culled1;
			}

			if (culled) continue;

			dlight_t& dl = aos[i];
			dl.transformed[0] = txs;
			dl.transformed[1] = tys;
			dl.transformed[2] = tzs;

			if (isLinear)
			{
				dl.transformed2[0] = t2xs;
				dl.transformed2[1] = t2ys;
				dl.transformed2[2] = t2zs;
			}

			if (outN < outCap) outPtrs[outN++] = &dl;
		}

		return outN;
#else
		// scalar fallback (bez xsimd)
		int outN = 0;
		for (int i = 0; i < n; ++i)
		{
			const float dx = soa.dlights_view.origin_x[i] - ox;
			const float dy = soa.dlights_view.origin_y[i] - oy;
			const float dz = soa.dlights_view.origin_z[i] - oz;

			const float txs = dx * ax0x + dy * ax0y + dz * ax0z;
			const float tys = dx * ax1x + dy * ax1y + dz * ax1z;
			const float tzs = dx * ax2x + dy * ax2y + dz * ax2z;

			const float r = soa.dlights_view.radius[i];

			bool culled0 =
				(txs - r > maxs[0]) || (txs + r < mins[0]) ||
				(tys - r > maxs[1]) || (tys + r < mins[1]) ||
				(tzs - r > maxs[2]) || (tzs + r < mins[2]);

			const bool isLinear = (soa.dlights_view.linear[i] != 0);
			bool culled = culled0;

			float t2xs = 0.f, t2ys = 0.f, t2zs = 0.f;
			if (isLinear)
			{
				const float d2x = soa.dlights_view.origin2_x[i] - ox;
				const float d2y = soa.dlights_view.origin2_y[i] - oy;
				const float d2z = soa.dlights_view.origin2_z[i] - oz;

				t2xs = d2x * ax0x + d2y * ax0y + d2z * ax0z;
				t2ys = d2x * ax1x + d2y * ax1y + d2z * ax1z;
				t2zs = d2x * ax2x + d2y * ax2y + d2z * ax2z;

				bool culled1 =
					(t2xs - r > maxs[0]) || (t2xs + r < mins[0]) ||
					(t2ys - r > maxs[1]) || (t2ys + r < mins[1]) ||
					(t2zs - r > maxs[2]) || (t2zs + r < mins[2]);

				culled = culled0 && culled1;
			}

			if (culled) continue;

			dlight_t& dl = tr.viewParms.dlights[i];
			dl.transformed[0] = txs;
			dl.transformed[1] = tys;
			dl.transformed[2] = tzs;

			if (isLinear)
			{
				dl.transformed2[0] = t2xs;
				dl.transformed2[1] = t2ys;
				dl.transformed2[2] = t2zs;
			}

			if (outN < outCap) outPtrs[outN++] = &dl;
		}
		return outN;
#endif
	}

} // namespace trsoa
