#include "string_operations.hpp"
#include "utils.hpp"
#include <cctype>
#include <charconv>
#include "tr_local.hpp"

int Q_strncmp_cpp(std::string_view s1, std::string_view s2, size_t n)
{
    size_t min_length = std::min(n, std::min(s1.size(), s2.size()));

    for (size_t i = 0; i < min_length; ++i)
    {
        if (s1[i] != s2[i])
        {
            return s1[i] < s2[i] ? -1 : 1;
        }
    }

    // If we compared `n` characters or reached the end of either string, they're equal up to this point
    return 0;
}

std::string_view COM_GetExtension_cpp(std::string_view name)
{
    auto dot = name.find_last_of('.');
    auto slash = name.find_last_of('/');

    if (dot != std::string_view::npos && (slash == std::string_view::npos || slash < dot))
        return name.substr(dot + 1);
    else
        return "";
}

constexpr int MAX_TOKEN_CHARS_CPP = 1024;

static int com_tokenline;
static int com_lines;
static char com_token[MAX_TOKEN_CHARS_CPP];

static const char* SkipWhitespace(const char* data, bool& hasNewLines) {
    while (char c = *data) {
        if (c > ' ') {
            return data;
        }
        if (c == '\n') {
            ++com_lines;
            hasNewLines = true;
        }
        ++data;
    }
    return nullptr;
}

std::string_view COM_ParseExt_cpp(const char **text, bool allowLineBreaks)
{
    const char* data = *text;
    bool hasNewLines = false;
    int len = 0;

    com_token[0] = '\0';
    com_tokenline = 0;

    if (!data) {
        *text = nullptr;
        return std::string_view(com_token);
    }

    while (true) {
        data = SkipWhitespace(data, hasNewLines);
        if (!data) {
            *text = nullptr;
            return std::string_view(com_token);
        }

        if (hasNewLines && !allowLineBreaks) {
            *text = data;
            return std::string_view(com_token);
        }

        char c = *data;

        if (c == '/' && data[1] == '/') {
            data += 2;
            while (*data && *data != '\n') {
                ++data;
            }
        }
        else if (c == '/' && data[1] == '*') {
            data += 2;
            while (*data && (*data != '*' || data[1] != '/')) {
                if (*data == '\n') {
                    ++com_lines;
                }
                ++data;
            }
            if (*data) {
                data += 2;
            }
        }
        else {
            break;
        }
    }

    com_tokenline = com_lines;

    if (*data == '"') {
        ++data;
        while (char c = *data) {
            if (c == '"' || c == '\0') {
                if (c == '"') {
                    ++data;
                }
                com_token[len] = '\0';
                *text = data;
                return std::string_view(com_token, len);
            }
            if (c == '\n') {
                ++com_lines;
            }
            if (len < MAX_TOKEN_CHARS - 1) {
                com_token[len++] = c;
            }
            ++data;
        }
    }

    while (char c = *data) {
        if (c <= ' ') {
            break;
        }
        if (len < MAX_TOKEN_CHARS - 1) {
            com_token[len++] = c;
        }
        ++data;
    }

    com_token[len] = '\0';
    *text = data;
    return std::string_view(com_token, len);
}

int Q_stricmpn_cpp(std::string_view s1, std::string_view s2, int n)
{
    // Ensure the comparison length does not exceed the actual string lengths
    n = std::min(n, static_cast<int>(std::min(s1.size(), s2.size())));

    for (int i = 0; i < n; ++i)
    {
        char c1 = s1[i];
        char c2 = s2[i];

        // Convert to uppercase if lowercase
        if (c1 >= 'a' && c1 <= 'z')
        {
            c1 -= ('a' - 'A');
        }
        if (c2 >= 'a' && c2 <= 'z')
        {
            c2 -= ('a' - 'A');
        }

        if (c1 != c2)
        {
            return c1 < c2 ? -1 : 1;
        }

        // If we've reached the end of either string, exit
        if (c1 == '\0')
        {
            break;
        }
    }

    return 0; // Strings are equal up to n characters
}

static bool Q_isfinite(float f)
{
    return std::isfinite(f);
}

float Q_atof_cpp(std::string_view str)
{
    float result = 0.0f;
    const char *begin = str.data();
    const char *end = str.data() + str.size();

    // Use from_chars for conversion, which doesn't throw exceptions
    auto [ptr, ec] = std::from_chars(begin, end, result);

    // If the conversion failed or the value is not finite, return 0.0f
    if (ec != std::errc() || !Q_isfinite(result))
    {
        return 0.0f;
    }

    return result;
}

int atoi_from_view(std::string_view sv)
{
    int result = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), result);
    return result;
}