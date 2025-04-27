#ifndef COMPILER_DEFINES_HPP
#define COMPILER_DEFINES_HPP

#if defined(__GNUC__) || defined(__clang__)
#   define LIKELY(x)   __builtin_expect(!!(x), 1)
#   define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#   define LIKELY(x)   (x)
#   define UNLIKELY(x) (x)
#endif

#endif // COMPILER_DEFINES_HPP