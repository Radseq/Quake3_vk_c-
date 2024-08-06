#ifndef Q_COMMON_HPP
#define Q_COMMON_HPP

#include <string_view>

int Q_stricmp_cpp(std::string_view s1, std::string_view s2);
std::string_view COM_GetExtension_cpp(std::string_view name);
void COM_StripExtension_cpp(std::string_view in, std::string_view& out);
std::string_view COM_ParseExt_cpp(const char** text, bool allowLineBreaks);
int Q_stricmpn_cpp(std::string_view s1, std::string_view s2, int n);
float Q_atof_cpp(std::string_view str);
int atoi_from_view(std::string_view sv);
std::string_view COM_GetExtension_plus(std::string_view name);

#endif // Q_COMMON_HPP
