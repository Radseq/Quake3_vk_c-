#ifndef TR_SOA_STAGE2_HPP
#define TR_SOA_STAGE2_HPP

#include "tr_soa_frame.hpp"
#include "math.hpp"
#include "tr_model.hpp"
#include "tr_shader.hpp"

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

	TR_FORCEINLINE void WrapClampFrame(int& f, const int numFrames, const int renderfx) noexcept
	{
		if (numFrames <= 0) { f = 0; return; }
		if (renderfx & RF_WRAP_FRAMES)
		{
			f %= numFrames;
			if (f < 0) f += numFrames;
		}
		if ((unsigned)f >= (unsigned)numFrames) f = 0;
	}

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


	TR_FORCEINLINE void MultAxisOriginWithWorldMatrix_sse_preB_rowmajor(
		const __m128 B0, const __m128 B1, const __m128 B2, const __m128 B3,
		float ax0x, float ax0y, float ax0z,
		float ax1x, float ax1y, float ax1z,
		float ax2x, float ax2y, float ax2z,
		float ox, float oy, float oz,
		float* __restrict outM) noexcept
	{
		// out row0 = ax0x*B0 + ax0y*B1 + ax0z*B2
		__m128 r0 = _mm_mul_ps(_mm_set1_ps(ax0x), B0);
		r0 = _mm_add_ps(r0, _mm_mul_ps(_mm_set1_ps(ax0y), B1));
		r0 = _mm_add_ps(r0, _mm_mul_ps(_mm_set1_ps(ax0z), B2));
		_mm_storeu_ps(outM + 0, r0);

		__m128 r1 = _mm_mul_ps(_mm_set1_ps(ax1x), B0);
		r1 = _mm_add_ps(r1, _mm_mul_ps(_mm_set1_ps(ax1y), B1));
		r1 = _mm_add_ps(r1, _mm_mul_ps(_mm_set1_ps(ax1z), B2));
		_mm_storeu_ps(outM + 4, r1);

		__m128 r2 = _mm_mul_ps(_mm_set1_ps(ax2x), B0);
		r2 = _mm_add_ps(r2, _mm_mul_ps(_mm_set1_ps(ax2y), B1));
		r2 = _mm_add_ps(r2, _mm_mul_ps(_mm_set1_ps(ax2z), B2));
		_mm_storeu_ps(outM + 8, r2);

		// row3 = ox*B0 + oy*B1 + oz*B2 + 1*B3
		__m128 r3 = _mm_mul_ps(_mm_set1_ps(ox), B0);
		r3 = _mm_add_ps(r3, _mm_mul_ps(_mm_set1_ps(oy), B1));
		r3 = _mm_add_ps(r3, _mm_mul_ps(_mm_set1_ps(oz), B2));
		r3 = _mm_add_ps(r3, B3);
		_mm_storeu_ps(outM + 12, r3);
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

		//for (int i = 0; i < count; ++i) { 
		//	alignas(16) float glMatrix[16]; 
		//	const float ox = in.transform.origin_x[i]; 
		//	const float oy = in.transform.origin_y[i]; 
		//	const float oz = in.transform.origin_z[i]; 
		//	glMatrix[0] = in.transform.ax0_x[i]; 
		//	glMatrix[4] = in.transform.ax1_x[i]; 
		//	glMatrix[8] = in.transform.ax2_x[i]; 
		//	glMatrix[12] = ox; 
		//	glMatrix[1] = in.transform.ax0_y[i]; 
		//	glMatrix[5] = in.transform.ax1_y[i]; 
		//	glMatrix[9] = in.transform.ax2_y[i]; 
		//	glMatrix[13] = oy; 
		//	glMatrix[2] = in.transform.ax0_z[i]; 
		//	glMatrix[6] = in.transform.ax1_z[i]; 
		//	glMatrix[10] = in.transform.ax2_z[i]; 
		//	glMatrix[14] = oz; 
		//	glMatrix[3] = 0.0f;
		//	glMatrix[7] = 0.0f;
		//	glMatrix[11] = 0.0f; 
		//	glMatrix[15] = 1.0f; 
		//	myGlMultMatrix_SIMD(glMatrix, vp.world.modelMatrix, out.mat_ptr(i));
		//}

		const float* worldM = vp.world.modelMatrix;
		const __m128 B0 = _mm_loadu_ps(worldM + 0);
		const __m128 B1 = _mm_loadu_ps(worldM + 4);
		const __m128 B2 = _mm_loadu_ps(worldM + 8);
		const __m128 B3 = _mm_loadu_ps(worldM + 12);

		for (int i = 0; i < count; ++i) {
			MultAxisOriginWithWorldMatrix_sse_preB_rowmajor(
				B0, B1, B2, B3,
				in.transform.ax0_x[i], in.transform.ax0_y[i], in.transform.ax0_z[i],
				in.transform.ax1_x[i], in.transform.ax1_y[i], in.transform.ax1_z[i],
				in.transform.ax2_x[i], in.transform.ax2_y[i], in.transform.ax2_z[i],
				in.transform.origin_x[i], in.transform.origin_y[i], in.transform.origin_z[i],
				out.mat_ptr(i)
			);
		}


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

	//	struct LodConsts final
	//	{
	//		float lodscale;
	//		int   bias;
	//		float c;
	//		float pm5, pm9, pm13, pm7, pm11, pm15;
	//		float ax0x, ax0y, ax0z;
	//	};
	//
	//	static inline LodConsts MakeLodConsts() noexcept
	//	{
	//		LodConsts lc{};
	//		lc.lodscale = r_lodscale->value;
	//		if (lc.lodscale > 20.0f) lc.lodscale = 20.0f;
	//		lc.bias = r_lodbias->integer;
	//
	//		const vec3_t& ax0 = tr.viewParms.ort.axis[0];
	//		lc.ax0x = ax0[0]; lc.ax0y = ax0[1]; lc.ax0z = ax0[2];
	//		lc.c = DotProduct(ax0, tr.viewParms.ort.origin);
	//
	//		lc.pm5 = tr.viewParms.projectionMatrix[5];
	//		lc.pm9 = tr.viewParms.projectionMatrix[9];
	//		lc.pm13 = tr.viewParms.projectionMatrix[13];
	//		lc.pm7 = tr.viewParms.projectionMatrix[7];
	//		lc.pm11 = tr.viewParms.projectionMatrix[11];
	//		lc.pm15 = tr.viewParms.projectionMatrix[15];
	//		return lc;
	//	}
	//
	//	static inline void ComputeLODsAndCullSphere_MD3(FrameSoA& soa, const LodConsts& lc) noexcept
	//	{
	//		auto& md = soa.modelDerived;
	//		const auto& t = soa.models.transform;
	//		const auto& a = soa.models.anim;
	//
	//		for (int i = 0; i < soa.md3Count; ++i)
	//		{
	//			const int slot = static_cast<int>(soa.md3Slots[i]);
	//			model_t* const m = md.modelPtr[slot];
	//			if (!m) continue; // safety
	//			// zakładamy: m->type == MOD_MESH
	//
	//			const int frame = a.frame[slot];
	//
	//			// radius/localOrigin z md3[0] (jak w Twoim switch-case)
	//			md3Frame_t* fr0 = (md3Frame_t*)(((unsigned char*)m->md3[0]) + m->md3[0]->ofsFrames);
	//			fr0 += frame;
	//
	//			const float radius0 = fr0->radius;
	//			md.boundRadius[slot] = radius0;
	//
	//			// wstępnie wpisz cull sphere (i tak nadpiszemy z md3[lod] po wyliczeniu lod)
	//			md.cullLocal_x[slot] = fr0->localOrigin[0];
	//			md.cullLocal_y[slot] = fr0->localOrigin[1];
	//			md.cullLocal_z[slot] = fr0->localOrigin[2];
	//			md.cullRadius[slot] = radius0;
	//
	//			int lod = 0;
	//			if (m->numLods >= 2)
	//			{
	//				const float ox = t.origin_x[slot];
	//				const float oy = t.origin_y[slot];
	//				const float oz = t.origin_z[slot];
	//				const float dist = (ox * lc.ax0x + oy * lc.ax0y + oz * lc.ax0z) - lc.c;
	//
	//				if (dist <= 0.0f)
	//				{
	//					lod = 0;
	//				}
	//				else
	//				{
	//					const float pr = ProjectRadiusFast(radius0, dist, lc.pm5, lc.pm9, lc.pm13, lc.pm7, lc.pm11, lc.pm15);
	//					float flod = 1.0f - pr * lc.lodscale;
	//					flod *= (float)m->numLods;
	//					lod = myftol(flod);
	//
	//					if (lod < 0) lod = 0;
	//					else if (lod >= m->numLods) lod = m->numLods - 1;
	//				}
	//
	//				lod += lc.bias;
	//				if (lod >= m->numLods) lod = m->numLods - 1;
	//				if (lod < 0) lod = 0;
	//			}
	//
	//			md.lod[slot] = static_cast<std::uint8_t>(lod);
	//
	//			// cull sphere musi być z md3[lod] (jak w Twoim kodzie)
	//			md3Header_t* hdr = m->md3[lod];
	//			md3Frame_t* fr = (md3Frame_t*)(((unsigned char*)hdr) + hdr->ofsFrames);
	//			fr += frame;
	//
	//			md.cullLocal_x[slot] = fr->localOrigin[0];
	//			md.cullLocal_y[slot] = fr->localOrigin[1];
	//			md.cullLocal_z[slot] = fr->localOrigin[2];
	//			md.cullRadius[slot] = fr->radius;
	//		}
	//	}
	//
	//	static inline void ComputeLODsAndCullSphere_MDR(FrameSoA& soa, const LodConsts& lc) noexcept
	//	{
	//		auto& md = soa.modelDerived;
	//		const auto& t = soa.models.transform;
	//		const auto& a = soa.models.anim;
	//
	//		for (int i = 0; i < soa.mdrCount; ++i)
	//		{
	//			const int slot = static_cast<int>(soa.mdrSlots[i]);
	//			model_t* const m = md.modelPtr[slot];
	//			if (!m) continue; // safety
	//			// zakładamy: m->type == MOD_MDR
	//
	//			const int frame = a.frame[slot];
	//
	//			mdrHeader_t* mdr = (mdrHeader_t*)m->modelData;
	//			const int frameSize = (int)(size_t)(&((mdrFrame_t*)0)->bones[mdr->numBones]);
	//			mdrFrame_t* f = (mdrFrame_t*)((byte*)mdr + mdr->ofsFrames + frameSize * frame);
	//
	//			const float radius = f->radius;
	//
	//			md.boundRadius[slot] = radius;
	//			md.cullLocal_x[slot] = f->localOrigin[0];
	//			md.cullLocal_y[slot] = f->localOrigin[1];
	//			md.cullLocal_z[slot] = f->localOrigin[2];
	//			md.cullRadius[slot] = radius;
	//
	//			int lod = 0;
	//			if (m->numLods >= 2)
	//			{
	//				const float ox = t.origin_x[slot];
	//				const float oy = t.origin_y[slot];
	//				const float oz = t.origin_z[slot];
	//				const float dist = (ox * lc.ax0x + oy * lc.ax0y + oz * lc.ax0z) - lc.c;
	//
	//				if (dist <= 0.0f)
	//				{
	//					lod = 0;
	//				}
	//				else
	//				{
	//					const float pr = ProjectRadiusFast(radius, dist, lc.pm5, lc.pm9, lc.pm13, lc.pm7, lc.pm11, lc.pm15);
	//					float flod = 1.0f - pr * lc.lodscale;
	//					flod *= (float)m->numLods;
	//					lod = myftol(flod);
	//
	//					if (lod < 0) lod = 0;
	//					else if (lod >= m->numLods) lod = m->numLods - 1;
	//				}
	//
	//				lod += lc.bias;
	//				if (lod >= m->numLods) lod = m->numLods - 1;
	//				if (lod < 0) lod = 0;
	//			}
	//
	//			md.lod[slot] = static_cast<std::uint8_t>(lod);
	//
	//			// MDR: cull sphere nie zależy od lod headera tak jak md3[lod] (zostaw jak masz)
	//		}
	//	}
	//
	//	static inline void ComputeCullFrustum_ForSlotRuns(FrameSoA& soa, const std::uint16_t* slots, int n, const viewParms_t& vp) noexcept
	//	{
	//		auto& md = soa.modelDerived;
	//		const auto& t = soa.models.transform;
	//		const auto& a = soa.models.anim;
	//
	//		if (n <= 0) return;
	//
	//		// zgodnie z Twoim ComputeModelCullSphere_Batch: nocull -> CLIP
	//		if (r_nocull->integer)
	//		{
	//			for (int i = 0; i < n; ++i)
	//				md.cullResult[static_cast<int>(slots[i])] = CULL_CLIP;
	//			return;
	//		}
	//
	//#if TR_HAS_XSIMD
	//		using batch = xsimd::batch<float>;
	//		constexpr int W = (int)batch::size;
	//
	//		alignas(64) float activeLane[W];
	//		alignas(64) float outLane[W];
	//
	//		const batch fOut(static_cast<float>(CULL_OUT));
	//		const batch fClip(static_cast<float>(CULL_CLIP));
	//		const batch fIn(static_cast<float>(CULL_IN));
	//
	//		ForEachConsecutiveRun(slots, n, [&](int base, int len) noexcept
	//			{
	//				// doprowadź base do alignmentu W
	//				while (len > 0 && (base & (W - 1)) != 0)
	//				{
	//					const bool ok =
	//						(t.nonNormalizedAxes[base] == 0) &&
	//						(a.frame[base] == a.oldframe[base]) &&
	//						(md.cullRadius[base] > 0.0f);
	//
	//					if (!ok) { md.cullResult[base] = CULL_CLIP; ++base; --len; continue; }
	//
	//					// scalar frustum test
	//					const float ox = t.origin_x[base], oy = t.origin_y[base], oz = t.origin_z[base];
	//					const float ax0x = t.ax0_x[base], ax0y = t.ax0_y[base], ax0z = t.ax0_z[base];
	//					const float ax1x = t.ax1_x[base], ax1y = t.ax1_y[base], ax1z = t.ax1_z[base];
	//					const float ax2x = t.ax2_x[base], ax2y = t.ax2_y[base], ax2z = t.ax2_z[base];
	//
	//					const float lx = md.cullLocal_x[base], ly = md.cullLocal_y[base], lz = md.cullLocal_z[base];
	//					const float r = md.cullRadius[base];
	//
	//					const float cx = ox + ax0x * lx + ax1x * ly + ax2x * lz;
	//					const float cy = oy + ax0y * lx + ax1y * ly + ax2y * lz;
	//					const float cz = oz + ax0z * lx + ax1z * ly + ax2z * lz;
	//
	//					std::uint8_t res = CULL_IN;
	//					for (int p = 0; p < 4; ++p)
	//					{
	//						const cplane_t& fr = vp.frustum[p];
	//						const float dist = cx * fr.normal[0] + cy * fr.normal[1] + cz * fr.normal[2] - fr.dist;
	//						if (dist < -r) { res = CULL_OUT; break; }
	//						if (dist < r) { res = CULL_CLIP; }
	//					}
	//					md.cullResult[base] = res;
	//
	//					++base; --len;
	//				}
	//
	//				int i = 0;
	//				for (; i + W <= len; i += W)
	//				{
	//					const int s = base + i;
	//
	//					bool anyActive = false;
	//					for (int l = 0; l < W; ++l)
	//					{
	//						const int slot = s + l;
	//						const bool ok =
	//							(t.nonNormalizedAxes[slot] == 0) &&
	//							(a.frame[slot] == a.oldframe[slot]) &&
	//							(md.cullRadius[slot] > 0.0f);
	//
	//						activeLane[l] = ok ? 1.0f : 0.0f;
	//						anyActive |= ok;
	//					}
	//
	//					if (!anyActive)
	//					{
	//						std::memset(md.cullResult.data() + s, CULL_CLIP, (std::size_t)W);
	//						continue;
	//					}
	//
	//					const batch active = batch::load_aligned(activeLane);
	//
	//					const batch ox = batch::load_aligned(t.origin_x.data() + s);
	//					const batch oy = batch::load_aligned(t.origin_y.data() + s);
	//					const batch oz = batch::load_aligned(t.origin_z.data() + s);
	//
	//					const batch ax0x = batch::load_aligned(t.ax0_x.data() + s);
	//					const batch ax0y = batch::load_aligned(t.ax0_y.data() + s);
	//					const batch ax0z = batch::load_aligned(t.ax0_z.data() + s);
	//
	//					const batch ax1x = batch::load_aligned(t.ax1_x.data() + s);
	//					const batch ax1y = batch::load_aligned(t.ax1_y.data() + s);
	//					const batch ax1z = batch::load_aligned(t.ax1_z.data() + s);
	//
	//					const batch ax2x = batch::load_aligned(t.ax2_x.data() + s);
	//					const batch ax2y = batch::load_aligned(t.ax2_y.data() + s);
	//					const batch ax2z = batch::load_aligned(t.ax2_z.data() + s);
	//
	//					const batch lx = batch::load_aligned(md.cullLocal_x.data() + s);
	//					const batch ly = batch::load_aligned(md.cullLocal_y.data() + s);
	//					const batch lz = batch::load_aligned(md.cullLocal_z.data() + s);
	//					const batch r = batch::load_aligned(md.cullRadius.data() + s);
	//
	//					// worldCenter = origin + axis0*lx + axis1*ly + axis2*lz
	//					const batch cx = ox + ax0x * lx + ax1x * ly + ax2x * lz;
	//					const batch cy = oy + ax0y * lx + ax1y * ly + ax2y * lz;
	//					const batch cz = oz + ax0z * lx + ax1z * ly + ax2z * lz;
	//
	//					auto activeMask = (active != batch(0.0f));
	//
	//					auto anyOut = xsimd::batch_bool<float>(false);
	//					auto anyClip = xsimd::batch_bool<float>(false);
	//
	//					for (int p = 0; p < 4; ++p)
	//					{
	//						const cplane_t& fr = vp.frustum[p];
	//						const batch nx(fr.normal[0]), ny(fr.normal[1]), nz(fr.normal[2]), dd(fr.dist);
	//
	//						const batch dist = cx * nx + cy * ny + cz * nz - dd;
	//
	//						anyOut = anyOut | (dist < (-r));
	//						anyClip = anyClip | (dist < (r));
	//					}
	//
	//					const auto outMask = activeMask & anyOut;
	//					const auto clipMask = activeMask & (!anyOut) && anyClip;
	//					const auto inMask = activeMask & (!anyOut) && (!anyClip);
	//
	//					batch code = xsimd::select(outMask, fOut, fClip);
	//					code = xsimd::select(inMask, fIn, code); // IN nadpisuje CLIP tylko dla inMask
	//
	//					code.store_aligned(outLane);
	//
	//					for (int l = 0; l < W; ++l)
	//						md.cullResult[s + l] = static_cast<std::uint8_t>(outLane[l]);
	//				}
	//
	//				// tail scalar
	//				for (; i < len; ++i)
	//				{
	//					const int slot = base + i;
	//
	//					const bool ok =
	//						(t.nonNormalizedAxes[slot] == 0) &&
	//						(a.frame[slot] == a.oldframe[slot]) &&
	//						(md.cullRadius[slot] > 0.0f);
	//
	//					if (!ok) { md.cullResult[slot] = CULL_CLIP; continue; }
	//
	//					const float ox = t.origin_x[slot], oy = t.origin_y[slot], oz = t.origin_z[slot];
	//					const float ax0x = t.ax0_x[slot], ax0y = t.ax0_y[slot], ax0z = t.ax0_z[slot];
	//					const float ax1x = t.ax1_x[slot], ax1y = t.ax1_y[slot], ax1z = t.ax1_z[slot];
	//					const float ax2x = t.ax2_x[slot], ax2y = t.ax2_y[slot], ax2z = t.ax2_z[slot];
	//
	//					const float lx = md.cullLocal_x[slot], ly = md.cullLocal_y[slot], lz = md.cullLocal_z[slot];
	//					const float r = md.cullRadius[slot];
	//
	//					const float cx = ox + ax0x * lx + ax1x * ly + ax2x * lz;
	//					const float cy = oy + ax0y * lx + ax1y * ly + ax2y * lz;
	//					const float cz = oz + ax0z * lx + ax1z * ly + ax2z * lz;
	//
	//					std::uint8_t res = CULL_IN;
	//					for (int p = 0; p < 4; ++p)
	//					{
	//						const cplane_t& fr = vp.frustum[p];
	//						const float dist = cx * fr.normal[0] + cy * fr.normal[1] + cz * fr.normal[2] - fr.dist;
	//						if (dist < -r) { res = CULL_OUT; break; }
	//						if (dist < r) { res = CULL_CLIP; }
	//					}
	//					md.cullResult[slot] = res;
	//				}
	//			});
	//#else
	//		// fallback scalar
	//		for (int i = 0; i < n; ++i)
	//			md.cullResult[static_cast<int>(slots[i])] = CULL_CLIP;
	//#endif
	//	}
	//

	static inline void ComputeModelLODs_Batch(FrameSoA& soa, const trRefdef_t& refdef) noexcept
	{
		const int count = soa.models.count;
		if (count <= 0) return;
		const int countPadded = soa.models.countPadded;

		// stałe z ProjectRadius / view axis
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

		auto& md = soa.modelDerived;

		const auto store_cull = [&](const int slot, const float lx, const float ly, const float lz, const float r) noexcept
			{
				md.cullLocal_x[slot] = lx;
				md.cullLocal_y[slot] = ly;
				md.cullLocal_z[slot] = lz;
				md.cullRadius[slot] = r;
				md.cullResult[slot] = CULL_CLIP; // prepass ustawi IN/OUT/CLIP
			};

		const auto clear_cull = [&](const int slot) noexcept
			{
				store_cull(slot, 0.0f, 0.0f, 0.0f, 0.0f);
			};

		const auto store_aabb = [&](const int slot,
			const float minx, const float miny, const float minz,
			const float maxx, const float maxy, const float maxz,
			const std::uint8_t valid) noexcept
			{
				md.aabbMin_x[slot] = minx; md.aabbMin_y[slot] = miny; md.aabbMin_z[slot] = minz;
				md.aabbMax_x[slot] = maxx; md.aabbMax_y[slot] = maxy; md.aabbMax_z[slot] = maxz;
				md.aabbValid[slot] = valid;
			};

		const auto clear_aabb = [&](const int slot) noexcept
			{
				store_aabb(slot, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0u);
			};

		const auto RadiusFromAabb = [&](const float minx, const float miny, const float minz,
			const float maxx, const float maxy, const float maxz) noexcept -> float
			{
				const float dx = maxx - minx;
				const float dy = maxy - miny;
				const float dz = maxz - minz;
				return 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
			};


		using batch = xsimd::batch<float>;
		constexpr int W = static_cast<int>(batch::size);

		alignas(64) float distTmp[W]{};

		int i = 0;
		for (; i < countPadded; i += W)
		{
			const batch ox = batch::load_aligned(soa.models.transform.origin_x.data() + i);
			const batch oy = batch::load_aligned(soa.models.transform.origin_y.data() + i);
			const batch oz = batch::load_aligned(soa.models.transform.origin_z.data() + i);

			const batch dist = (ox * batch(ax0[0])) + (oy * batch(ax0[1])) + (oz * batch(ax0[2])) - batch(c);
			dist.store_aligned(distTmp);

			for (int lane = 0; lane < W; ++lane)
			{
				const int slot = i + lane;

				// padded lanes: nie dotykamy refdef/entities i nie wołamy na śmieciach
				if (slot >= count)
				{
					md.modelPtr[slot] = nullptr;
					md.modelType[slot] = static_cast<std::uint8_t>(0xFF);
					md.lod[slot] = 0;
					md.boundRadius[slot] = 0.0f;
					clear_cull(slot);
					clear_aabb(slot);
					continue;
				}

				int frame = soa.models.anim.frame[slot];       // MUST be non-const
				int oldframe = soa.models.anim.oldframe[slot]; // MUST be non-const
				const int rfx = soa.models.render.renderfx[slot];
				model_t* m = R_GetModelByHandle(soa.models.render.hModel[slot]);
				md.modelPtr[slot] = m;
				md.modelType[slot] = m ? static_cast<std::uint8_t>(m->type) : static_cast<std::uint8_t>(0xFF);

				if (!m)
				{
					md.lod[slot] = 0;
					md.boundRadius[slot] = 0.0f;
					clear_cull(slot);
					continue;
				}

				float sphereR = 0.0f;
				float lodR = 0.0f;
				float lx = 0.0f;
				float ly = 0.0f;
				float lz = 0.0f;

				switch (m->type)
				{
				case modtype_t::MOD_MDR:
				{
					mdrHeader_t* mdr = (mdrHeader_t*)m->modelData;
					const int numFrames = mdr ? mdr->numFrames : 0;
					WrapClampFrame(frame, numFrames, rfx);
					WrapClampFrame(oldframe, numFrames, rfx);
					soa.models.anim.frame[slot] = frame;
					soa.models.anim.oldframe[slot] = oldframe;
					if (numFrames <= 0)
					{
						md.lod[slot] = 0;
						md.boundRadius[slot] = 0.0f;
						clear_cull(slot);
						clear_aabb(slot);
						continue;
					}

					const int frameSize = (int)(size_t)(&((mdrFrame_t*)0)->bones[mdr->numBones]);
					mdrFrame_t* f = (mdrFrame_t*)((byte*)mdr + mdr->ofsFrames + frameSize * frame);
					mdrFrame_t* fOld = (mdrFrame_t*)((byte*)mdr + mdr->ofsFrames + frameSize * oldframe);

					sphereR = f->radius;
					lx = f->localOrigin[0];
					ly = f->localOrigin[1];
					lz = f->localOrigin[2];

					lodR = RadiusFromAabb(
						f->bounds[0][0], f->bounds[0][1], f->bounds[0][2],
						f->bounds[1][0], f->bounds[1][1], f->bounds[1][2]);
					break;
				}
				case modtype_t::MOD_MESH: // MD3
				{
					md3Header_t* hdr0 = m->md3[0];
					const int numFrames = hdr0 ? hdr0->numFrames : 0;
					WrapClampFrame(frame, numFrames, rfx);
					WrapClampFrame(oldframe, numFrames, rfx);
					soa.models.anim.frame[slot] = frame;
					soa.models.anim.oldframe[slot] = oldframe;
					if (!hdr0 || numFrames <= 0)
					{
						md.lod[slot] = 0;
						md.boundRadius[slot] = 0.0f;
						clear_cull(slot);
						clear_aabb(slot);
						continue;
					}

					md3Frame_t* fr = (md3Frame_t*)(((unsigned char*)hdr0) + hdr0->ofsFrames);
					fr += frame;

					sphereR = fr->radius;
					lx = fr->localOrigin[0];
					ly = fr->localOrigin[1];
					lz = fr->localOrigin[2];

					// merged AABB(old/new) jak w AoS (R_CullModel)
					lodR = RadiusFromAabb(
						fr->bounds[0][0], fr->bounds[0][1], fr->bounds[0][2],
						fr->bounds[1][0], fr->bounds[1][1], fr->bounds[1][2]);
					break;
				}
				case modtype_t::MOD_IQM:
				{
					// IQM: bez LOD; bierzemy konserwatywną kulę obejmującą UNION(bounds(frame), bounds(oldframe))
					// dzięki temu możemy bezpiecznie cullować nawet gdy frame != oldframe.
					iqmData_t* iqm = (iqmData_t*)m->modelData;

					const int numFrames = iqm ? iqm->num_frames : 0;
					int f = frame;
					int of = oldframe;

					// naśladuj AoS: RF_WRAP_FRAMES
					if ((rfx & RF_WRAP_FRAMES) && numFrames > 0)
					{
						f %= numFrames;  if (f < 0)  f += numFrames;
						of %= numFrames; if (of < 0) of += numFrames;
					}

					// naśladuj AoS: walidacja (żeby nie zrobić OOB na bounds)
					if ((unsigned)f >= (unsigned)numFrames)  f = 0;
					if ((unsigned)of >= (unsigned)numFrames) of = 0;

					// utrzymaj spójność w SoA (dla kolejnych stage’ów)
					soa.models.anim.frame[slot] = f;
					soa.models.anim.oldframe[slot] = of;

					const bool hasBounds = (iqm && iqm->bounds);
					if (!hasBounds)
					{
						// AoS: R_CullIQM -> zawsze CLIP gdy brak bounds
						clear_cull(slot);
						clear_aabb(slot);
						break;
					}

					const vec_t* bF = iqm->bounds + 6 * f;
					const vec_t* bO = iqm->bounds + 6 * of;

					// union AABB
					const float minx = (bF[0] < bO[0]) ? bF[0] : bO[0];
					const float miny = (bF[1] < bO[1]) ? bF[1] : bO[1];
					const float minz = (bF[2] < bO[2]) ? bF[2] : bO[2];
					const float maxx = (bF[3] > bO[3]) ? bF[3] : bO[3];
					const float maxy = (bF[4] > bO[4]) ? bF[4] : bO[4];
					const float maxz = (bF[5] > bO[5]) ? bF[5] : bO[5];

					store_aabb(slot, minx, miny, minz, maxx, maxy, maxz, hasBounds ? 1u : 0u);

					const float dx = maxx - minx;
					const float dy = maxy - miny;
					const float dz = maxz - minz;

					// center w local space
					lx = minx + 0.5f * dx;
					ly = miny + 0.5f * dy;
					lz = minz + 0.5f * dz;

					// radius = 0.5 * |diag|
					sphereR = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
					lodR = sphereR;
					break;
				}
				case modtype_t::MOD_BRUSH:
				{
					// BRUSH: box-cull zawsze po bmodel.bounds w AoS
					if (m->bmodel)
					{
						const bmodel_t& bm = *m->bmodel;
						store_aabb(slot,
							bm.bounds[0][0], bm.bounds[0][1], bm.bounds[0][2],
							bm.bounds[1][0], bm.bounds[1][1], bm.bounds[1][2],
							1u);
					}
					else
					{
						clear_aabb(slot);
					}

					md.lod[slot] = 0;
					md.boundRadius[slot] = 0.0f;
					clear_cull(slot); // sphere prepass nie dotyczy brush
					continue;
				}
				default:
					md.lod[slot] = 0;
					md.boundRadius[slot] = 0.0f;
					clear_cull(slot);
					clear_aabb(slot);
					continue;
				}

				md.boundRadius[slot] = lodR;
				store_cull(slot, lx, ly, lz, sphereR);

				int lod = 0;
				if (m->numLods >= 2)
				{
					const float d = distTmp[lane];

					if (d <= 0.0f)
					{
						lod = 0;
					}
					else
					{
						const float pr = ProjectRadiusFast(lodR, d, pm5, pm9, pm13, pm7, pm11, pm15);
						float flod = 1.0f - pr * lodscale;
						flod *= (float)m->numLods;
						lod = myftol(flod);

						if (lod < 0) lod = 0;
						else if (lod >= m->numLods) lod = m->numLods - 1;
					}

					lod += bias;
					if (lod >= m->numLods) lod = m->numLods - 1;
					if (lod < 0) lod = 0;
				}
				else
				{
					lod = 0;
				}

				md.lod[slot] = static_cast<std::uint8_t>(lod);

				// cullLocal/cullRadius z aktualnego LOD tylko dla MDR/MD3.
				// IQM już ma poprawnie ustawione (UNION bounds).
				if (m->type == modtype_t::MOD_MDR)
				{
					mdrHeader_t* mdr = (mdrHeader_t*)m->modelData;
					const int frameSize = (int)(size_t)(&((mdrFrame_t*)0)->bones[mdr->numBones]);
					mdrFrame_t* fN = (mdrFrame_t*)((byte*)mdr + mdr->ofsFrames + frameSize * frame);
					mdrFrame_t* fO = (mdrFrame_t*)((byte*)mdr + mdr->ofsFrames + frameSize * oldframe);

					soa.modelDerived.cullLocal_x[slot] = fN->localOrigin[0];
					soa.modelDerived.cullLocal_y[slot] = fN->localOrigin[1];
					soa.modelDerived.cullLocal_z[slot] = fN->localOrigin[2];
					soa.modelDerived.cullRadius[slot] = fN->radius;

					store_aabb(slot,
						(fO->bounds[0][0] < fN->bounds[0][0]) ? fO->bounds[0][0] : fN->bounds[0][0],
						(fO->bounds[0][1] < fN->bounds[0][1]) ? fO->bounds[0][1] : fN->bounds[0][1],
						(fO->bounds[0][2] < fN->bounds[0][2]) ? fO->bounds[0][2] : fN->bounds[0][2],
						(fO->bounds[1][0] > fN->bounds[1][0]) ? fO->bounds[1][0] : fN->bounds[1][0],
						(fO->bounds[1][1] > fN->bounds[1][1]) ? fO->bounds[1][1] : fN->bounds[1][1],
						(fO->bounds[1][2] > fN->bounds[1][2]) ? fO->bounds[1][2] : fN->bounds[1][2],
						1u);
				}
				else if (m->type == modtype_t::MOD_MESH)
				{
					const int lodClamped = static_cast<int>(soa.modelDerived.lod[slot]);
					md3Header_t* hdr = m->md3[lodClamped];
					md3Frame_t* fN = (md3Frame_t*)(((unsigned char*)hdr) + hdr->ofsFrames) + frame;
					md3Frame_t* fO = (md3Frame_t*)(((unsigned char*)hdr) + hdr->ofsFrames) + oldframe;

					soa.modelDerived.cullLocal_x[slot] = fN->localOrigin[0];
					soa.modelDerived.cullLocal_y[slot] = fN->localOrigin[1];
					soa.modelDerived.cullLocal_z[slot] = fN->localOrigin[2];
					soa.modelDerived.cullRadius[slot] = fN->radius;

					store_aabb(slot,
						(fO->bounds[0][0] < fN->bounds[0][0]) ? fO->bounds[0][0] : fN->bounds[0][0],
						(fO->bounds[0][1] < fN->bounds[0][1]) ? fO->bounds[0][1] : fN->bounds[0][1],
						(fO->bounds[0][2] < fN->bounds[0][2]) ? fO->bounds[0][2] : fN->bounds[0][2],
						(fO->bounds[1][0] > fN->bounds[1][0]) ? fO->bounds[1][0] : fN->bounds[1][0],
						(fO->bounds[1][1] > fN->bounds[1][1]) ? fO->bounds[1][1] : fN->bounds[1][1],
						(fO->bounds[1][2] > fN->bounds[1][2]) ? fO->bounds[1][2] : fN->bounds[1][2],
						1u);
				}
				else
				{
					// IQM/BRUSH/UNKNOWN: nic – już ustawione albo wyczyszczone wcześniej.
				}
			}
		}
	}

	static inline void ComputeModelCullSphere_Batch(FrameSoA& soa, const viewParms_t& vp) noexcept
	{
		const int count = soa.models.count;
		if (count <= 0) return;
		const int countPadded = soa.models.countPadded;

		auto& md = soa.modelDerived;

		// zgodnie z oryginałem: gdy nocull -> traktuj jak CLIP
		if (r_nocull->integer)
		{
			std::memset(md.cullResult.data(), CULL_CLIP, static_cast<std::size_t>(countPadded));
			return;
		}

		const auto& t = soa.models.transform;
		const auto& a = soa.models.anim;

#if TR_HAS_XSIMD
		using batch = xsimd::batch<float>;
		constexpr int W = static_cast<int>(batch::size);

		alignas(64) float activeLane[W];
		alignas(64) float outLane[W];

		const batch fOut(static_cast<float>(CULL_OUT));
		const batch fClip(static_cast<float>(CULL_CLIP));
		const batch fIn(static_cast<float>(CULL_IN));

		// full batches only (no OOB loads)
		int base = 0;
		for (; base < countPadded; base += W)
		{
			bool anyActive = false;
			for (int l = 0; l < W; ++l)
			{
				const int s = base + l;

				// padded lanes nieaktywne
				if (s >= count)
				{
					activeLane[l] = 0.0f;
					continue;
				}

				const bool ok =
					(t.nonNormalizedAxes[s] == 0) &&
					(md.cullRadius[s] > 0.0f) &&
					(
						(a.frame[s] == a.oldframe[s]) ||
						((md.modelType[s] == static_cast<std::uint8_t>(modtype_t::MOD_IQM)) && (md.aabbValid[s] != 0u))
						);

				activeLane[l] = ok ? 1.0f : 0.0f;
				anyActive |= ok;
			}

			if (!anyActive)
			{
				std::memset(md.cullResult.data() + base, CULL_CLIP, static_cast<std::size_t>(W));
				continue;
			}

			const batch active = xsimd::load_aligned(activeLane);

			const batch ox = xsimd::load_aligned(&t.origin_x[base]);
			const batch oy = xsimd::load_aligned(&t.origin_y[base]);
			const batch oz = xsimd::load_aligned(&t.origin_z[base]);

			const batch ax0x = xsimd::load_aligned(&t.ax0_x[base]);
			const batch ax0y = xsimd::load_aligned(&t.ax0_y[base]);
			const batch ax0z = xsimd::load_aligned(&t.ax0_z[base]);

			const batch ax1x = xsimd::load_aligned(&t.ax1_x[base]);
			const batch ax1y = xsimd::load_aligned(&t.ax1_y[base]);
			const batch ax1z = xsimd::load_aligned(&t.ax1_z[base]);

			const batch ax2x = xsimd::load_aligned(&t.ax2_x[base]);
			const batch ax2y = xsimd::load_aligned(&t.ax2_y[base]);
			const batch ax2z = xsimd::load_aligned(&t.ax2_z[base]);

			const batch lx = xsimd::load_aligned(&md.cullLocal_x[base]);
			const batch ly = xsimd::load_aligned(&md.cullLocal_y[base]);
			const batch lz = xsimd::load_aligned(&md.cullLocal_z[base]);
			const batch r = xsimd::load_aligned(&md.cullRadius[base]);

			// worldCenter = origin + axis0*lx + axis1*ly + axis2*lz
			const batch cx = ox + ax0x * lx + ax1x * ly + ax2x * lz;
			const batch cy = oy + ax0y * lx + ax1y * ly + ax2y * lz;
			const batch cz = oz + ax0z * lx + ax1z * ly + ax2z * lz;

			// accumulate
			auto anyOut = xsimd::batch_bool<float>(false);
			auto anyClip = xsimd::batch_bool<float>(false);

			for (int p = 0; p < 4; ++p)
			{
				const cplane_t& fr = vp.frustum[p];
				const batch nx(fr.normal[0]);
				const batch ny(fr.normal[1]);
				const batch nz(fr.normal[2]);
				const batch dd(fr.dist);

				const batch dist = cx * nx + cy * ny + cz * nz - dd;

				anyOut = anyOut | (dist < (-r));
				anyClip = anyClip | (dist <= r);
			}

			// inactive lanes -> CLIP
			// active lanes:
			//   OUT if anyOut
			//   else CLIP if anyClip
			//   else IN
			const auto activeMask = (active != batch(0.0f));

			const auto outMask = activeMask & anyOut;
			const auto clipMask = activeMask & (!anyOut) & anyClip;
			const auto inMask = activeMask & (!anyOut) & (!anyClip);

			batch code = xsimd::select(outMask, fOut, fClip);
			code = xsimd::select(inMask, fIn, code); // IN overrides CLIP for inMask

			xsimd::store_aligned(outLane, code);

			for (int l = 0; l < W; ++l)
			{
				md.cullResult[base + l] = static_cast<std::uint8_t>(outLane[l]);
			}
		}
#else
		// scalar fallback (bez xsimd)
		for (int i = 0; i < count; ++i)
		{
			const bool isIqm = (md.modelType[i] == static_cast<std::uint8_t>(modtype_t::MOD_IQM));
			if (t.nonNormalizedAxes[i] != 0 || (a.frame[i] != a.oldframe[i] && !(isIqm && md.aabbValid[i])))
			{
				md.cullResult[i] = CULL_CLIP;
				continue;
			}

			const float lxS = md.cullLocal_x[i];
			const float lyS = md.cullLocal_y[i];
			const float lzS = md.cullLocal_z[i];
			const float rS = md.cullRadius[i];

			if (rS <= 0.0f)
			{
				md.cullResult[i] = CULL_CLIP;
				continue;
			}

			const float cxS =
				t.origin_x[i] +
				t.ax0_x[i] * lxS + t.ax1_x[i] * lyS + t.ax2_x[i] * lzS;

			const float cyS =
				t.origin_y[i] +
				t.ax0_y[i] * lxS + t.ax1_y[i] * lyS + t.ax2_y[i] * lzS;

			const float czS =
				t.origin_z[i] +
				t.ax0_z[i] * lxS + t.ax1_z[i] * lyS + t.ax2_z[i] * lzS;

			bool mightBeClipped = false;

			for (int p = 0; p < 4; ++p)
			{
				const cplane_t& fr = vp.frustum[p];
				const float dist = cxS * fr.normal[0] + cyS * fr.normal[1] + czS * fr.normal[2] - fr.dist;

				if (dist < -rS)
				{
					md.cullResult[i] = CULL_OUT;
					goto next_entity_scalar;
				}
				if (dist <= rS)
				{
					mightBeClipped = true;
				}
			}

			md.cullResult[i] = mightBeClipped ? CULL_CLIP : CULL_IN;

		next_entity_scalar:
			(void)0;
		}
#endif
	}

	// BRUSH (bmodel): dokładny test OBB vs frustum (szybszy niż 8 cornerów i usuwa redundancję AoS-culla).
	// Uwaga: ustawiamy cullRadius < 0 jako marker "brush cull policzony" (bo BRUSH nie używa sfery).
	static inline void ComputeBrushCullOBB_Batch(trsoa::FrameSoA& soa, const viewParms_t& vp) noexcept
	{
		auto& md = soa.modelDerived;
		const int count = soa.models.count;

		if (count <= 0)
			return;

		if (r_nocull->integer)
		{
			// Nie zmieniaj (albo ustaw CLIP) – byle stabilnie
			for (int slot = 0; slot < count; ++slot)
			{
				if (md.modelType[slot] == static_cast<std::uint8_t>(modtype_t::MOD_BRUSH))
					md.cullResult[slot] = CULL_CLIP;
			}
			return;
		}

		for (int slot = 0; slot < count; ++slot)
		{
			if (md.modelType[slot] != static_cast<std::uint8_t>(modtype_t::MOD_BRUSH))
				continue;

			// Jeśli już OUT (np. z wcześniejszego testu), nie licz drugi raz
			if (md.cullResult[slot] == CULL_OUT)
				continue;

			const model_t* const m = md.modelPtr[slot];
			const bmodel_t* const bm = (m ? m->bmodel : nullptr);
			if (!bm)
			{
				md.cullResult[slot] = CULL_CLIP;
				continue;
			}

			// local AABB
			const float minx = bm->bounds[0][0], miny = bm->bounds[0][1], minz = bm->bounds[0][2];
			const float maxx = bm->bounds[1][0], maxy = bm->bounds[1][1], maxz = bm->bounds[1][2];

			const float cx = 0.5f * (minx + maxx);
			const float cy = 0.5f * (miny + maxy);
			const float cz = 0.5f * (minz + maxz);

			const float ex = 0.5f * (maxx - minx);
			const float ey = 0.5f * (maxy - miny);
			const float ez = 0.5f * (maxz - minz);

			// entity transform (SoA)
			const float ox = soa.models.transform.origin_x[slot];
			const float oy = soa.models.transform.origin_y[slot];
			const float oz = soa.models.transform.origin_z[slot];

			const float ax0x = soa.models.transform.ax0_x[slot];
			const float ax0y = soa.models.transform.ax0_y[slot];
			const float ax0z = soa.models.transform.ax0_z[slot];

			const float ax1x = soa.models.transform.ax1_x[slot];
			const float ax1y = soa.models.transform.ax1_y[slot];
			const float ax1z = soa.models.transform.ax1_z[slot];

			const float ax2x = soa.models.transform.ax2_x[slot];
			const float ax2y = soa.models.transform.ax2_y[slot];
			const float ax2z = soa.models.transform.ax2_z[slot];

			// world center = origin + axis * localCenter
			const float wcx = ox + ax0x * cx + ax1x * cy + ax2x * cz;
			const float wcy = oy + ax0y * cx + ax1y * cy + ax2y * cz;
			const float wcz = oz + ax0z * cx + ax1z * cy + ax2z * cz;

			bool anyClip = false;
			int  res = CULL_IN;

			// Quake3 standardowo używa 4 planes
			for (int p = 0; p < 4; ++p)
			{
				const cplane_t& pl = vp.frustum[p];
				const float nx = pl.normal[0];
				const float ny = pl.normal[1];
				const float nz = pl.normal[2];

				const float dist = (wcx * nx + wcy * ny + wcz * nz) - pl.dist;

				// r = sum(|dot(n, axis_i)| * extent_i)
				const float d0 = std::fabs(nx * ax0x + ny * ax0y + nz * ax0z);
				const float d1 = std::fabs(nx * ax1x + ny * ax1y + nz * ax1z);
				const float d2 = std::fabs(nx * ax2x + ny * ax2y + nz * ax2z);

				const float rad = d0 * ex + d1 * ey + d2 * ez;

				if (dist < -rad)
				{
					res = CULL_OUT;
					break;
				}
				if (dist <= rad)
				{
					anyClip = true;
				}
			}

			if (res != CULL_OUT)
				res = anyClip ? CULL_CLIP : CULL_IN;

			md.cullResult[slot] = res;
		}
	}

	static inline void BuildVisibleModelLists(FrameSoA& soa, const viewParms_t& vp) noexcept
	{
		soa.visibleModelCount = 0;
		soa.md3Count = 0;
		soa.mdrCount = 0;
		soa.iqmCount = 0;
		soa.brushCount = 0;
		soa.otherCount = 0;

		const int count = soa.models.count;
		for (int slot = 0; slot < count; ++slot)
		{
			if (soa.modelDerived.cullResult[slot] == CULL_OUT)
				continue;

			// Early RF filtering (removes branches from model dispatch)
			const int entNum = static_cast<int>(soa.models.render.entNum[slot]);
			const int renderfx = soa.models.render.renderfx[slot];
			if (Skip_FirstPerson_InPortalViewFx(renderfx, vp))
				continue;
			if (Skip_ThirdPersonFx_InPrimaryViewFx(renderfx, vp))
				continue;

			soa.visibleModelSlots[soa.visibleModelCount++] = static_cast<std::uint16_t>(slot);

			const std::uint8_t t = soa.modelDerived.modelType[slot];
			switch (static_cast<modtype_t>(t))
			{
			case modtype_t::MOD_MESH:
				soa.md3Slots[soa.md3Count++] = static_cast<std::uint16_t>(slot);
				break;
			case modtype_t::MOD_MDR:
				soa.mdrSlots[soa.mdrCount++] = static_cast<std::uint16_t>(slot);
				break;
			case modtype_t::MOD_IQM:
				soa.iqmSlots[soa.iqmCount++] = static_cast<std::uint16_t>(slot);
				break;
			case modtype_t::MOD_BRUSH:
				soa.brushSlots[soa.brushCount++] = static_cast<std::uint16_t>(slot);
				break;
			default:
				soa.otherSlots[soa.otherCount++] = static_cast<std::uint16_t>(slot);
				break;
			}
		}
	}

	template <class Fn>
	TR_FORCEINLINE void ForEachConsecutiveRun(const std::uint16_t* slots, int n, Fn&& fn) noexcept
	{
		if (n <= 0) return;

		int runStart = static_cast<int>(slots[0]);
		int prev = runStart;
		int runLen = 1;

		for (int i = 1; i < n; ++i)
		{
			const int s = static_cast<int>(slots[i]);
			if (s == prev + 1)
			{
				++runLen;
				prev = s;
				continue;
			}

			fn(runStart, runLen);
			runStart = s;
			prev = s;
			runLen = 1;
		}

		fn(runStart, runLen);
	}


	static inline void ComputeModelFogNums_Batch(FrameSoA& soa, const trRefdef_t& refdef) noexcept
	{
		const int visCount = soa.visibleModelCount;
		if (visCount <= 0) return;

		auto& md = soa.modelDerived;

		// przypadki szybkie
		if (refdef.rdflags & RDF_NOWORLDMODEL)
		{
			ForEachConsecutiveRun(soa.visibleModelSlots.data(), visCount,
				[&](int base, int len) noexcept
				{
					for (int i = 0; i < len; ++i)
						md.fogNum[base + i] = 0;
				});
			return;
		}

		const int numFogs = (tr.world ? tr.world->numfogs : 0);
		if (numFogs <= 1)
		{
			ForEachConsecutiveRun(soa.visibleModelSlots.data(), visCount,
				[&](int base, int len) noexcept
				{
					for (int i = 0; i < len; ++i)
						md.fogNum[base + i] = 0;
				});
			return;
		}

#if TR_HAS_XSIMD
		using batch = xsimd::batch<float>;
		constexpr int W = static_cast<int>(batch::size);

		alignas(64) float fogTmp[W];

		ForEachConsecutiveRun(soa.visibleModelSlots.data(), visCount,
			[&](int base, int len) noexcept
			{
				const int end = base + len;

				// cap do countPadded (bezpieczne OOB)
				const int endCap = (end <= soa.models.countPadded) ? end : soa.models.countPadded;

				// scalar helper dla pojedynczego slotu (tylko VISIBLE run)
				auto fog_scalar_one = [&](int slot) noexcept
					{
						const float ox = soa.models.transform.origin_x[slot];
						const float oy = soa.models.transform.origin_y[slot];
						const float oz = soa.models.transform.origin_z[slot];

						const float cx = ox + md.cullLocal_x[slot];
						const float cy = oy + md.cullLocal_y[slot];
						const float cz = oz + md.cullLocal_z[slot];
						const float r = md.cullRadius[slot];

						int fogNum = 0;
						for (int fi = 1; fi < numFogs; ++fi)
						{
							const fog_t& fog = tr.world->fogs[fi];

							if (cx - r >= fog.bounds[1][0]) continue;
							if (cx + r <= fog.bounds[0][0]) continue;

							if (cy - r >= fog.bounds[1][1]) continue;
							if (cy + r <= fog.bounds[0][1]) continue;

							if (cz - r >= fog.bounds[1][2]) continue;
							if (cz + r <= fog.bounds[0][2]) continue;

							fogNum = fi;
							break;
						}

						md.fogNum[slot] = static_cast<std::int16_t>(fogNum);
					};

				int s = base;

				// --- prolog scalar: wyrównaj s do W (żeby load_aligned działało) ---
				while (s < endCap && (s & (W - 1)) != 0)
				{
					fog_scalar_one(s);
					++s;
				}

				// --- SIMD środek: tylko pełne batch'e i tylko gdy s jest aligned do W ---
				alignas(64) float fogTmp[W];

				for (; s + W <= endCap; s += W)
				{
					const batch ox = batch::load_aligned(soa.models.transform.origin_x.data() + s);
					const batch oy = batch::load_aligned(soa.models.transform.origin_y.data() + s);
					const batch oz = batch::load_aligned(soa.models.transform.origin_z.data() + s);

					const batch cx = ox + batch::load_aligned(md.cullLocal_x.data() + s);
					const batch cy = oy + batch::load_aligned(md.cullLocal_y.data() + s);
					const batch cz = oz + batch::load_aligned(md.cullLocal_z.data() + s);
					const batch r = batch::load_aligned(md.cullRadius.data() + s);

					batch fogNumF(0.0f);
					auto unresolved = xsimd::batch_bool<float>(true);

					for (int fi = 1; fi < numFogs; ++fi)
					{
						const fog_t& fog = tr.world->fogs[fi];

						const batch minX(fog.bounds[0][0]), minY(fog.bounds[0][1]), minZ(fog.bounds[0][2]);
						const batch maxX(fog.bounds[1][0]), maxY(fog.bounds[1][1]), maxZ(fog.bounds[1][2]);

						const batch cx_m = cx - r, cx_p = cx + r;
						const batch cy_m = cy - r, cy_p = cy + r;
						const batch cz_m = cz - r, cz_p = cz + r;

						// masks: bitwise ops, nie && / ||
						const auto ix = (cx_m < maxX) & (cx_p > minX);
						const auto iy = (cy_m < maxY) & (cy_p > minY);
						const auto iz = (cz_m < maxZ) & (cz_p > minZ);

						const auto intersects = ix & iy & iz;

						const auto hit = unresolved & intersects;
						fogNumF = xsimd::select(hit, batch(static_cast<float>(fi)), fogNumF);

						unresolved = unresolved & (!intersects);
					}

					fogNumF.store_aligned(fogTmp);

					// zapis tylko dla slotów w tym runie (s..end)
					for (int l = 0; l < W; ++l)
					{
						const int idx = s + l;
						if (idx >= end) break;
						md.fogNum[idx] = static_cast<std::int16_t>(fogTmp[l]);
					}
				}

				// --- tail scalar (reszta runu) ---
				for (; s < endCap; ++s)
				{
					fog_scalar_one(s);
				}
			});

#else
		for (int vi = 0; vi < visCount; ++vi)
		{
			const int slot = static_cast<int>(soa.visibleModelSlots[vi]);

			const float ox = soa.models.transform.origin_x[slot];
			const float oy = soa.models.transform.origin_y[slot];
			const float oz = soa.models.transform.origin_z[slot];

			const float cx = ox + md.cullLocal_x[slot];
			const float cy = oy + md.cullLocal_y[slot];
			const float cz = oz + md.cullLocal_z[slot];
			const float r = md.cullRadius[slot];

			int fogNum = 0;
			for (int fi = 1; fi < numFogs; ++fi)
			{
				const fog_t& fog = tr.world->fogs[fi];

				if (cx - r >= fog.bounds[1][0]) continue;
				if (cx + r <= fog.bounds[0][0]) continue;

				if (cy - r >= fog.bounds[1][1]) continue;
				if (cy + r <= fog.bounds[0][1]) continue;

				if (cz - r >= fog.bounds[1][2]) continue;
				if (cz + r <= fog.bounds[0][2]) continue;

				fogNum = fi;
				break;
			}

			md.fogNum[slot] = static_cast<std::int16_t>(fogNum);
		}
#endif
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

	template <std::size_t CachePow2>
	struct FxShaderCacheTLS final
	{
		static_assert((CachePow2& (CachePow2 - 1)) == 0, "CachePow2 must be power of two");

		std::array<std::int32_t, CachePow2> key{};
		std::array<shader_t*, CachePow2>    val{};

		std::int32_t lastKey = -1;
		shader_t* lastVal = nullptr;

		FxShaderCacheTLS() noexcept
		{
			key.fill(-1);
			val.fill(nullptr);
		}

		TR_FORCEINLINE shader_t* get(const std::int32_t h) noexcept
		{
			// super hot: last-handle
			if (h == lastKey) return lastVal;

			const std::uint32_t idx = static_cast<std::uint32_t>(h) & (CachePow2 - 1);

			if (key[idx] == h)
			{
				lastKey = h;
				lastVal = val[idx];
				return lastVal;
			}

			shader_t* const p = R_GetShaderByHandle(h);
			key[idx] = h;
			val[idx] = p;

			lastKey = h;
			lastVal = p;
			return p;
		}
	};

	static thread_local FxShaderCacheTLS<256> g_fxShaderCache;

	static inline void ResolveFxShaders(FrameSoA& soa) noexcept
	{
		auto& cache = g_fxShaderCache;

		for (int i = 0; i < soa.sprites.count; ++i)
			soa.sprites.shaderPtr[i] = cache.get(soa.sprites.customShader[i]);

		for (int i = 0; i < soa.beams.count; ++i)
			soa.beams.shaderPtr[i] = cache.get(soa.beams.customShader[i]);

		for (int i = 0; i < soa.lightning.count; ++i)
			soa.lightning.shaderPtr[i] = cache.get(soa.lightning.customShader[i]);

		for (int i = 0; i < soa.rail_core.count; ++i)
			soa.rail_core.shaderPtr[i] = cache.get(soa.rail_core.customShader[i]);

		for (int i = 0; i < soa.rail_rings.count; ++i)
			soa.rail_rings.shaderPtr[i] = cache.get(soa.rail_rings.customShader[i]);
	}

	static inline int SpriteFogNum_SoA(const float ox, const float oy, const float oz,
		const float radius, const int renderfx,
		const trRefdef_t& refdef) noexcept
	{
		if (refdef.rdflags & RDF_NOWORLDMODEL)
			return 0;

		if (renderfx & RF_CROSSHAIR)
			return 0;

		for (int i = 1; i < tr.world->numfogs; ++i)
		{
			const fog_t& fog = tr.world->fogs[i];

			// AABB test (jak oryginał)
			if (ox - radius >= fog.bounds[1][0]) continue;
			if (ox + radius <= fog.bounds[0][0]) continue;

			if (oy - radius >= fog.bounds[1][1]) continue;
			if (oy + radius <= fog.bounds[0][1]) continue;

			if (oz - radius >= fog.bounds[1][2]) continue;
			if (oz + radius <= fog.bounds[0][2]) continue;

			return i;
		}

		return 0;
	}

	static inline void ComputeFxFogNums(FrameSoA& soa, const trRefdef_t& refdef) noexcept
	{
		if (refdef.rdflags & RDF_NOWORLDMODEL)
		{
			for (int i = 0; i < soa.sprites.count; ++i)    soa.sprites.fogNum[i] = 0;
			for (int i = 0; i < soa.beams.count; ++i)      soa.beams.fogNum[i] = 0;
			for (int i = 0; i < soa.lightning.count; ++i)  soa.lightning.fogNum[i] = 0;
			for (int i = 0; i < soa.rail_core.count; ++i)  soa.rail_core.fogNum[i] = 0;
			for (int i = 0; i < soa.rail_rings.count; ++i) soa.rail_rings.fogNum[i] = 0;
			return;
		}

		const int numFogs = (tr.world ? tr.world->numfogs : 0);
		if (numFogs <= 1)
		{
			for (int i = 0; i < soa.sprites.count; ++i)    soa.sprites.fogNum[i] = 0;
			for (int i = 0; i < soa.beams.count; ++i)      soa.beams.fogNum[i] = 0;
			for (int i = 0; i < soa.lightning.count; ++i)  soa.lightning.fogNum[i] = 0;
			for (int i = 0; i < soa.rail_core.count; ++i)  soa.rail_core.fogNum[i] = 0;
			for (int i = 0; i < soa.rail_rings.count; ++i) soa.rail_rings.fogNum[i] = 0;
			return;
		}

#if TR_HAS_XSIMD
		using f32 = xsimd::batch<float>;
		using i32 = xsimd::batch<std::int32_t>;
		constexpr int W = static_cast<int>(f32::size);

		alignas(64) float outTmp[W];

		auto fog_batch = [&](const int countPadded,
			const float* ox, const float* oy, const float* oz,
			const float* radiusOrNull,
			const std::int32_t* renderfx,
			std::int16_t* outFog) noexcept
			{
				for (int i = 0; i < countPadded; i += W)
				{
					const f32 x = f32::load_unaligned(ox + i);
					const f32 y = f32::load_unaligned(oy + i);
					const f32 z = f32::load_unaligned(oz + i);

					const f32 r = radiusOrNull ? f32::load_unaligned(radiusOrNull + i) : f32(0.0f);

					f32 fogF(0.0f);
					auto unresolved = xsimd::batch_bool<float>(true);

					for (int fi = 1; fi < numFogs; ++fi)
					{
						const fog_t& fog = tr.world->fogs[fi];

						const f32 minX(fog.bounds[0][0]), minY(fog.bounds[0][1]), minZ(fog.bounds[0][2]);
						const f32 maxX(fog.bounds[1][0]), maxY(fog.bounds[1][1]), maxZ(fog.bounds[1][2]);

						const f32 xm = x - r, xp = x + r;
						const f32 ym = y - r, yp = y + r;
						const f32 zm = z - r, zp = z + r;

						// IMPORTANT: use bitwise ops on masks, not &&/||
						const auto ix = (xm < maxX) & (xp > minX);
						const auto iy = (ym < maxY) & (yp > minY);
						const auto iz = (zm < maxZ) & (zp > minZ);

						const auto intersects = ix & iy & iz;
						const auto hit = unresolved & intersects;

						fogF = xsimd::select(hit, f32(static_cast<float>(fi)), fogF);
						unresolved = unresolved & (!intersects);
					}

					fogF.store_unaligned(outTmp);

					// crosshair lanes => 0 (do it scalar to avoid mask type mismatch: int-mask vs float-mask)
					for (int l = 0; l < W; ++l)
					{
						const int idx = i + l;
						outFog[idx] = (renderfx[idx] & RF_CROSSHAIR) ? 0 : static_cast<std::int16_t>(outTmp[l]);
					}
				}
			};

		// sprites: origin + radius
		fog_batch(soa.sprites.countPadded,
			soa.sprites.origin_x.data(), soa.sprites.origin_y.data(), soa.sprites.origin_z.data(),
			soa.sprites.radius.data(),
			soa.sprites.renderfx.data(),
			soa.sprites.fogNum.data());

		// beams/lightning/rail: używamy from_xyz i radius=0
		fog_batch(soa.beams.countPadded,
			soa.beams.from_x.data(), soa.beams.from_y.data(), soa.beams.from_z.data(),
			nullptr,
			soa.beams.renderfx.data(),
			soa.beams.fogNum.data());

		fog_batch(soa.lightning.countPadded,
			soa.lightning.from_x.data(), soa.lightning.from_y.data(), soa.lightning.from_z.data(),
			nullptr,
			soa.lightning.renderfx.data(),
			soa.lightning.fogNum.data());

		fog_batch(soa.rail_core.countPadded,
			soa.rail_core.from_x.data(), soa.rail_core.from_y.data(), soa.rail_core.from_z.data(),
			nullptr,
			soa.rail_core.renderfx.data(),
			soa.rail_core.fogNum.data());

		fog_batch(soa.rail_rings.countPadded,
			soa.rail_rings.from_x.data(), soa.rail_rings.from_y.data(), soa.rail_rings.from_z.data(),
			nullptr,
			soa.rail_rings.renderfx.data(),
			soa.rail_rings.fogNum.data());

#else
		// scalar fallback (zostaw jak było)
		for (int i = 0; i < soa.sprites.count; ++i)
			soa.sprites.fogNum[i] = static_cast<std::int16_t>(
				SpriteFogNum_SoA(soa.sprites.origin_x[i], soa.sprites.origin_y[i], soa.sprites.origin_z[i],
					soa.sprites.radius[i], soa.sprites.renderfx[i], refdef));

		for (int i = 0; i < soa.beams.count; ++i)
			soa.beams.fogNum[i] = static_cast<std::int16_t>(
				SpriteFogNum_SoA(soa.beams.from_x[i], soa.beams.from_y[i], soa.beams.from_z[i],
					0.0f, soa.beams.renderfx[i], refdef));

		for (int i = 0; i < soa.lightning.count; ++i)
			soa.lightning.fogNum[i] = static_cast<std::int16_t>(
				SpriteFogNum_SoA(soa.lightning.from_x[i], soa.lightning.from_y[i], soa.lightning.from_z[i],
					0.0f, soa.lightning.renderfx[i], refdef));

		for (int i = 0; i < soa.rail_core.count; ++i)
			soa.rail_core.fogNum[i] = static_cast<std::int16_t>(
				SpriteFogNum_SoA(soa.rail_core.from_x[i], soa.rail_core.from_y[i], soa.rail_core.from_z[i],
					0.0f, soa.rail_core.renderfx[i], refdef));

		for (int i = 0; i < soa.rail_rings.count; ++i)
			soa.rail_rings.fogNum[i] = static_cast<std::int16_t>(
				SpriteFogNum_SoA(soa.rail_rings.from_x[i], soa.rail_rings.from_y[i], soa.rail_rings.from_z[i],
					0.0f, soa.rail_rings.renderfx[i], refdef));
#endif
	}


	TR_FORCEINLINE void TransformDlights_FromOr(const int count, dlight_t* TR_RESTRICT dl, const orientationr_t& ort) noexcept
	{
		if (count <= 0) return;

		// prefetch axis (dot(temp, axis[j]))
		const float ax00 = ort.axis[0][0], ax01 = ort.axis[0][1], ax02 = ort.axis[0][2];
		const float ax10 = ort.axis[1][0], ax11 = ort.axis[1][1], ax12 = ort.axis[1][2];
		const float ax20 = ort.axis[2][0], ax21 = ort.axis[2][1], ax22 = ort.axis[2][2];

		const float ox = ort.origin[0];
		const float oy = ort.origin[1];
		const float oz = ort.origin[2];

#if TR_HAS_XSIMD
		using batch = xsimd::batch<float>;
		constexpr int W = static_cast<int>(batch::size);

		// małe count -> scalar (pack lane arrays bywa wolniejsze)
		if (count >= (W * 2))
		{
			alignas(64) float dxL[W], dyL[W], dzL[W];
			alignas(64) float dx2L[W], dy2L[W], dz2L[W];
			alignas(64) float o0[W], o1[W], o2[W];

			int base = 0;
			for (; base + W <= count; base += W)
			{
				for (int l = 0; l < W; ++l)
				{
					const dlight_t& d = dl[base + l];

					dxL[l] = d.origin[0] - ox;
					dyL[l] = d.origin[1] - oy;
					dzL[l] = d.origin[2] - oz;

					dx2L[l] = d.origin2[0] - ox;
					dy2L[l] = d.origin2[1] - oy;
					dz2L[l] = d.origin2[2] - oz;
				}

				const batch dx = xsimd::load_aligned(dxL);
				const batch dy = xsimd::load_aligned(dyL);
				const batch dz = xsimd::load_aligned(dzL);

				const batch dx2 = xsimd::load_aligned(dx2L);
				const batch dy2 = xsimd::load_aligned(dy2L);
				const batch dz2 = xsimd::load_aligned(dz2L);

				// origin -> transformed
				const batch t0 = dx * batch(ax00) + dy * batch(ax01) + dz * batch(ax02);
				const batch t1 = dx * batch(ax10) + dy * batch(ax11) + dz * batch(ax12);
				const batch t2 = dx * batch(ax20) + dy * batch(ax21) + dz * batch(ax22);

				xsimd::store_aligned(o0, t0);
				xsimd::store_aligned(o1, t1);
				xsimd::store_aligned(o2, t2);

				for (int l = 0; l < W; ++l)
				{
					dlight_t& d = dl[base + l];
					d.transformed[0] = o0[l];
					d.transformed[1] = o1[l];
					d.transformed[2] = o2[l];
				}

				// origin2 -> transformed2 (liczymy zawsze: brak gałęzi)
				const batch t20 = dx2 * batch(ax00) + dy2 * batch(ax01) + dz2 * batch(ax02);
				const batch t21 = dx2 * batch(ax10) + dy2 * batch(ax11) + dz2 * batch(ax12);
				const batch t22 = dx2 * batch(ax20) + dy2 * batch(ax21) + dz2 * batch(ax22);

				xsimd::store_aligned(o0, t20);
				xsimd::store_aligned(o1, t21);
				xsimd::store_aligned(o2, t22);

				for (int l = 0; l < W; ++l)
				{
					dlight_t& d = dl[base + l];
					d.transformed2[0] = o0[l];
					d.transformed2[1] = o1[l];
					d.transformed2[2] = o2[l];
				}
			}

			// tail scalar
			for (int i = base; i < count; ++i)
			{
				dlight_t& d = dl[i];

				const float dxs = d.origin[0] - ox;
				const float dys = d.origin[1] - oy;
				const float dzs = d.origin[2] - oz;

				d.transformed[0] = dxs * ax00 + dys * ax01 + dzs * ax02;
				d.transformed[1] = dxs * ax10 + dys * ax11 + dzs * ax12;
				d.transformed[2] = dxs * ax20 + dys * ax21 + dzs * ax22;

				const float dxs2 = d.origin2[0] - ox;
				const float dys2 = d.origin2[1] - oy;
				const float dzs2 = d.origin2[2] - oz;

				d.transformed2[0] = dxs2 * ax00 + dys2 * ax01 + dzs2 * ax02;
				d.transformed2[1] = dxs2 * ax10 + dys2 * ax11 + dzs2 * ax12;
				d.transformed2[2] = dxs2 * ax20 + dys2 * ax21 + dzs2 * ax22;
			}
			return;
		}
#endif

		// scalar fast path
		for (int i = 0; i < count; ++i)
		{
			dlight_t& d = dl[i];

			const float dxs = d.origin[0] - ox;
			const float dys = d.origin[1] - oy;
			const float dzs = d.origin[2] - oz;

			d.transformed[0] = dxs * ax00 + dys * ax01 + dzs * ax02;
			d.transformed[1] = dxs * ax10 + dys * ax11 + dzs * ax12;
			d.transformed[2] = dxs * ax20 + dys * ax21 + dzs * ax22;

			const float dxs2 = d.origin2[0] - ox;
			const float dys2 = d.origin2[1] - oy;
			const float dzs2 = d.origin2[2] - oz;

			d.transformed2[0] = dxs2 * ax00 + dys2 * ax01 + dzs2 * ax02;
			d.transformed2[1] = dxs2 * ax10 + dys2 * ax11 + dzs2 * ax12;
			d.transformed2[2] = dxs2 * ax20 + dys2 * ax21 + dzs2 * ax22;
		}
	}

	static inline void ComputeViewOrigin_VisibleRuns(const FrameSoA& soa, const viewParms_t& vp) noexcept
	{
		const int visCount = soa.visibleModelCount;
		if (visCount <= 0) return;

		const auto& t = soa.models.transform;
		auto& d = const_cast<ModelDerivedSoA&>(soa.modelDerived);

		const float camX = vp.ort.origin[0];
		const float camY = vp.ort.origin[1];
		const float camZ = vp.ort.origin[2];

#if TR_HAS_XSIMD
		using batch = xsimd::batch<float>;
		constexpr int W = static_cast<int>(batch::size);

		ForEachConsecutiveRun(soa.visibleModelSlots.data(), visCount,
			[&](int base, int len) noexcept
			{
				const int end = base + len;
				const int endCap = (end <= soa.models.countPadded) ? end : soa.models.countPadded;

				auto view_scalar_one = [&](int slot) noexcept
					{
						const float dx = camX - t.origin_x[slot];
						const float dy = camY - t.origin_y[slot];
						const float dz = camZ - t.origin_z[slot];
						const float inv = t.axisLenInv[slot];

						d.viewOrigin_x[slot] = (dx * t.ax0_x[slot] + dy * t.ax0_y[slot] + dz * t.ax0_z[slot]) * inv;
						d.viewOrigin_y[slot] = (dx * t.ax1_x[slot] + dy * t.ax1_y[slot] + dz * t.ax1_z[slot]) * inv;
						d.viewOrigin_z[slot] = (dx * t.ax2_x[slot] + dy * t.ax2_y[slot] + dz * t.ax2_z[slot]) * inv;
					};

				int s = base;

				// prolog: align do W
				while (s < endCap && (s & (W - 1)) != 0)
				{
					view_scalar_one(s);
					++s;
				}

				for (; s + W <= endCap; s += W)
				{
					const batch ox = batch::load_aligned(t.origin_x.data() + s);
					const batch oy = batch::load_aligned(t.origin_y.data() + s);
					const batch oz = batch::load_aligned(t.origin_z.data() + s);

					const batch dx = batch(camX) - ox;
					const batch dy = batch(camY) - oy;
					const batch dz = batch(camZ) - oz;

					const batch ax0x = batch::load_aligned(t.ax0_x.data() + s);
					const batch ax0y = batch::load_aligned(t.ax0_y.data() + s);
					const batch ax0z = batch::load_aligned(t.ax0_z.data() + s);

					const batch ax1x = batch::load_aligned(t.ax1_x.data() + s);
					const batch ax1y = batch::load_aligned(t.ax1_y.data() + s);
					const batch ax1z = batch::load_aligned(t.ax1_z.data() + s);

					const batch ax2x = batch::load_aligned(t.ax2_x.data() + s);
					const batch ax2y = batch::load_aligned(t.ax2_y.data() + s);
					const batch ax2z = batch::load_aligned(t.ax2_z.data() + s);

					const batch inv = batch::load_aligned(t.axisLenInv.data() + s);

					const batch v0 = (dx * ax0x + dy * ax0y + dz * ax0z) * inv;
					const batch v1 = (dx * ax1x + dy * ax1y + dz * ax1z) * inv;
					const batch v2 = (dx * ax2x + dy * ax2y + dz * ax2z) * inv;

					v0.store_aligned(d.viewOrigin_x.data() + s);
					v1.store_aligned(d.viewOrigin_y.data() + s);
					v2.store_aligned(d.viewOrigin_z.data() + s);
				}

				// tail scalar
				for (; s < endCap; ++s)
				{
					view_scalar_one(s);
				}
			});
#else
		for (int vi = 0; vi < visCount; ++vi)
		{
			const int slot = static_cast<int>(soa.visibleModelSlots[vi]);
			const float dx = camX - t.origin_x[slot];
			const float dy = camY - t.origin_y[slot];
			const float dz = camZ - t.origin_z[slot];
			const float inv = t.axisLenInv[slot];

			d.viewOrigin_x[slot] = (dx * t.ax0_x[slot] + dy * t.ax0_y[slot] + dz * t.ax0_z[slot]) * inv;
			d.viewOrigin_y[slot] = (dx * t.ax1_x[slot] + dy * t.ax1_y[slot] + dz * t.ax1_z[slot]) * inv;
			d.viewOrigin_z[slot] = (dx * t.ax2_x[slot] + dy * t.ax2_y[slot] + dz * t.ax2_z[slot]) * inv;
		}
#endif
	}


	static inline void PrecomputeModelDerived_Visible(FrameSoA& soa, const viewParms_t& vp) noexcept
	{
		const int visCount = soa.visibleModelCount;
		if (visCount <= 0) return;

		const auto& t = soa.models.transform;
		auto& md = soa.modelDerived;

		// world matrix (vp.world.modelMatrix) – w Q3 jest column-major OpenGL styl
		const float* const worldM = vp.world.modelMatrix;

		// Jeżeli masz wersję "preB" (wstępnie załadowane kolumny/wiersze) – użyj jej.
		// Przykład dla SSE (dostosuj do swojej funkcji myGlMultMatrix_SIMD / MultAxisOriginWithWorldMatrix_*):
		const __m128 B0 = _mm_load_ps(worldM + 0);
		const __m128 B1 = _mm_load_ps(worldM + 4);
		const __m128 B2 = _mm_load_ps(worldM + 8);
		const __m128 B3 = _mm_load_ps(worldM + 12);

		ForEachConsecutiveRun(soa.visibleModelSlots.data(), visCount,
			[&](int base, int len) noexcept
			{
				for (int slot = base; slot < base + len; ++slot)
				{
					float* const outM = md.mat_ptr(slot);

					trsoa::MultAxisOriginWithWorldMatrix_sse_preB(
						B0, B1, B2, B3,
						t.ax0_x[slot], t.ax0_y[slot], t.ax0_z[slot],
						t.ax1_x[slot], t.ax1_y[slot], t.ax1_z[slot],
						t.ax2_x[slot], t.ax2_y[slot], t.ax2_z[slot],
						t.origin_x[slot], t.origin_y[slot], t.origin_z[slot],
						outM
					);
				}
			});


		// I TO JEST “WPIĘCIE” – po macierzach, bez żadnego mieszania w środku:
		ComputeViewOrigin_VisibleRuns(soa, vp);
	}

} // namespace trsoa
#endif // TR_SOA_STAGE2_HPP