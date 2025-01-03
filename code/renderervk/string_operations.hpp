#ifndef STRING_OPERATIONS_HPP
#define STRING_OPERATIONS_HPP

#include <string_view>
#include <array>

#include "tr_local.hpp"
#include <string>

inline constexpr bool strrchr_sv(std::string_view sv, char character) {
	// Find the last '.' character
	auto dotPos = sv.find_last_of(character);

	// Check if a dot exists and is after the start of the string
	if (dotPos != std::string_view::npos && dotPos > 0)
	{
		// Equivalent to "if (strrchr(name, '.') > name)" when name is a string_view
		return true;
	}
	return false;
}

inline constexpr int Q_stricmp_cpp(std::string_view s1, std::string_view s2) {
	// Compare lengths first for a quick check
	if (s1.size() != s2.size()) return s1.size() < s2.size() ? -1 : 1;

	// Compare characters one by one
	for (size_t i = 0; i < s1.size(); ++i) {
		char c1 = s1[i];
		char c2 = s2[i];

		// Convert to lowercase if uppercase
		if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
		if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';

		// Compare characters
		if (c1 != c2) return c1 < c2 ? -1 : 1;
	}
	return 0;  // Strings are equal
}

std::string_view COM_GetExtension_cpp(std::string_view name);
std::string COM_StripExtension_cpp(std::string_view in);
std::string_view COM_ParseExt_cpp(const char** text, bool allowLineBreaks);
int Q_stricmpn_cpp(std::string_view s1, std::string_view s2, int n);
float Q_atof_cpp(std::string_view str);
int atoi_from_view(std::string_view sv);

template <int N>
constexpr auto intToStringView() {
	std::array<char, 12> buffer{}; // Enough to hold -2^31 to 2^31-1 and the null terminator
	char* p = buffer.data() + buffer.size() - 1;
	*p = '\0';
	int value = N;

	do {
		*--p = '0' + (value % 10);
		value /= 10;
	} while (value);

	return std::string_view(p);
}

#endif // STRING_OPERATIONS_HPP
