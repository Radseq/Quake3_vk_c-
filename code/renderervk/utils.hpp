#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstddef> // For std::size_t
#include <cassert>
#include <type_traits>
#include <chrono>
#include <cstdint>
#include <utility>

template <typename Func>
[[nodiscard]] double benchmark_ns(Func&& func, std::size_t iterations = 1)
{
	using clock = std::chrono::steady_clock;

	// Warm-up (important for branch prediction, caches, JIT-like effects)
	//func();

	auto start = clock::now();
	for (std::size_t i = 0; i < iterations; ++i)
		func();
	auto end = clock::now();

	const auto total_ns =
		std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

	return static_cast<double>(total_ns) / static_cast<double>(iterations);
	/*
	double ns = benchmark_ns([&] {
    // code you want to measure
		do_work();
	}, 1'000'000);

	std::cout << "do_work(): " << ns << " ns/call\n";
*/
}

constexpr std::array<std::uint8_t, 256> make_linear_gamma_table() {
	return[]<std::size_t... I>(std::index_sequence<I...>) {
		return std::array<std::uint8_t, 256>{ static_cast<std::uint8_t>(I)... };
	}(std::make_index_sequence<256>{});
}

template <typename T, std::size_t N>
constexpr std::size_t arrayLen(const T(&)[N]) noexcept
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

template <class T, class A>
constexpr T pad_up(T base, A alignment)
{
	using U = std::make_unsigned_t<T>;

	const U a = static_cast<U>(alignment);

#ifndef NDEBUG
	assert(a != 0);
	assert((a & (a - 1)) == 0); // power of two
#endif

	return static_cast<T>(
		(static_cast<U>(base) + (a - 1)) & ~(a - 1)
		);
}

// constexpr-only: no runtime assert; enforces power-of-two at compile time
// Usage requires alignment to be a non-type template parameter.
template <class T, auto Alignment>
constexpr T pad_up_ct(T base)
{
	static_assert(std::is_integral_v<decltype(Alignment)> || std::is_enum_v<decltype(Alignment)>,
		"Alignment must be an integral or enum constant");
	static_assert(Alignment > 0, "Alignment must be non-zero");
	static_assert((Alignment & (Alignment - 1)) == 0, "Alignment must be a power of two");

	using U = std::make_unsigned_t<T>;
	constexpr U a = static_cast<U>(Alignment);

	return static_cast<T>(
		(static_cast<U>(base) + (a - 1)) & ~(a - 1)
		);
}

#endif // UTILS_HPP
