#ifndef COMPILER_DEFINES_HPP
#define COMPILER_DEFINES_HPP

/*
Probability True	Should You Use UNLIKELY()?
< 1%	            ✅ Absolutely
~5%	                ✅ Usually
10–20%	            ⚠️ Depends
25–30%	            ❌ Probably not
> 30%	            ❌ No benefit
*/

#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

// OPT05: SIMD acceleration for dense renderer hot loops.
// Set Q3VK_OPT05_SIMD=0 at compile time to get the exact scalar baseline.
#ifndef Q3VK_OPT05_SIMD
#define Q3VK_OPT05_SIMD 1
#endif

// Individual switches make A/B testing each hot loop possible without
// maintaining separate source trees.
#ifndef Q3VK_OPT05_SIMD_DEFORM
#define Q3VK_OPT05_SIMD_DEFORM Q3VK_OPT05_SIMD
#endif

#ifndef Q3VK_OPT05_SIMD_MOVE
#define Q3VK_OPT05_SIMD_MOVE Q3VK_OPT05_SIMD
#endif

#ifndef Q3VK_OPT05_SIMD_TEX_TRANSFORM
#define Q3VK_OPT05_SIMD_TEX_TRANSFORM Q3VK_OPT05_SIMD
#endif

// Tiny batches are intentionally left scalar: function dispatch/setup can cost
// more than the saved arithmetic. Override during benchmarking if desired.
#ifndef Q3VK_OPT05_SIMD_MIN_VERTS
#define Q3VK_OPT05_SIMD_MIN_VERTS 8
#endif

// AVX2 is emitted only for explicitly targeted functions on GCC/Clang x86.
// Runtime dispatch keeps the binary compatible with x86 CPUs without AVX2.
#ifndef Q3VK_OPT05_SIMD_AVX2
#define Q3VK_OPT05_SIMD_AVX2 Q3VK_OPT05_SIMD
#endif

#if Q3VK_OPT05_SIMD_AVX2 && (defined(__GNUC__) || defined(__clang__)) && \
    (defined(__x86_64__) || defined(__i386__))
#define Q3VK_OPT05_HAVE_AVX2_TARGET 1
#define Q3VK_OPT05_AVX2_TARGET __attribute__((target("avx2")))
#else
#define Q3VK_OPT05_HAVE_AVX2_TARGET 0
#define Q3VK_OPT05_AVX2_TARGET
#endif

// OPT09: collapse multiple static-world VBO indexed draws that already share
// pipeline/descriptor/index-buffer state into one vkCmdDrawIndexedIndirect.
// This is deliberately CPU-generated MDI only: culling/order/batching semantics
// stay exactly as in the existing VBO path.
#ifndef Q3VK_OPT09_VBO_INDIRECT
#define Q3VK_OPT09_VBO_INDIRECT 1
#endif

// One indirect command is not useful: keep the existing direct draw for tiny
// batches. 2 is the conservative default and can be tuned by A/B testing.
#ifndef Q3VK_OPT09_VBO_INDIRECT_MIN_DRAWS
#define Q3VK_OPT09_VBO_INDIRECT_MIN_DRAWS 2
#endif

// Candidate-only diagnostics. Enable for one validation run if you want a
// first-use console message; keep 0 for performance measurements.
#ifndef Q3VK_OPT09_VBO_INDIRECT_DIAGNOSTICS
#define Q3VK_OPT09_VBO_INDIRECT_DIAGNOSTICS 0
#endif

#endif // COMPILER_DEFINES_HPP
