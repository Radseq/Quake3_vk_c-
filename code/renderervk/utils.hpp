#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstddef> // For std::size_t

template <typename T, std::size_t N>
constexpr std::size_t arrayLen(const T(&)[N]) noexcept {
    return N;
}

#endif // UTILS_HPP
