#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstddef> // For std::size_t

template <typename T, std::size_t N>
constexpr std::size_t arrayLen(const T (&)[N]) noexcept
{
    return N;
}

static inline unsigned int log2pad_plus(unsigned int v, int roundup)
{
    unsigned int x = 1;

    while (x < v)
        x <<= 1;

    if (roundup == 0)
    {
        if (x > v)
        {
            x >>= 1;
        }
    }

    return x;
}

#endif // UTILS_HPP
