#ifndef STRING_OPERATIONS_HPP
#define STRING_OPERATIONS_HPP

#include <string_view>
#include <array>

#include "tr_local.hpp"

int Q_stricmp_cpp(const std::string_view s1, const std::string_view s2);
std::string_view COM_GetExtension_cpp(std::string_view name);
void COM_StripExtension_cpp(std::string_view in, std::string_view &out);
std::string_view COM_ParseExt_cpp(const char **text, bool allowLineBreaks);
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
