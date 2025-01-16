#include "string_operations.hpp"
#include "utils.hpp"
#include <cctype>
#include <charconv>
#include "tr_local.hpp"

/*
==================
COM_GenerateHashValue

used in renderer and filesystem
==================
*/
// ASCII lowcase conversion table with '\\' turned to '/' and '.' to '\0'
static constexpr byte hash_locase[256] =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x00, 0x2f,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
        0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
        0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7a, 0x5b, 0x2f, 0x5d, 0x5e, 0x5f,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
        0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
        0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
        0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
        0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
        0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
        0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
        0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
        0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
        0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
        0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
        0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

unsigned long Com_GenerateHashValue_cpp(std::string_view fname, const unsigned int size)
{
    const unsigned char *s = reinterpret_cast<const unsigned char *>(fname.data());
    unsigned long hash = 0;
    int c;

    while ((c = hash_locase[(byte)*s++]) != '\0')
    {
        hash = hash * 101 + static_cast<unsigned char>(c);
    }

    hash ^= (hash >> 10);
    hash ^= (hash >> 20);
    hash &= (size - 1);

    return hash;
}

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

static const char *SkipWhitespace(const char *data, bool &hasNewLines)
{
    while (char c = *data)
    {
        if (c > ' ')
        {
            return data;
        }
        if (c == '\n')
        {
            ++com_lines;
            hasNewLines = true;
        }
        ++data;
    }
    return nullptr;
}

std::string_view COM_ParseExt_cpp(const char **text, bool allowLineBreaks)
{
    const char *data = *text;
    bool hasNewLines = false;
    int len = 0;

    com_token[0] = '\0';
    com_tokenline = 0;

    if (!data)
    {
        *text = nullptr;
        return std::string_view(com_token);
    }

    while (true)
    {
        data = SkipWhitespace(data, hasNewLines);
        if (!data)
        {
            *text = nullptr;
            return std::string_view(com_token);
        }

        if (hasNewLines && !allowLineBreaks)
        {
            *text = data;
            return std::string_view(com_token);
        }

        char c = *data;

        if (c == '/' && data[1] == '/')
        {
            data += 2;
            while (*data && *data != '\n')
            {
                ++data;
            }
        }
        else if (c == '/' && data[1] == '*')
        {
            data += 2;
            while (*data && (*data != '*' || data[1] != '/'))
            {
                if (*data == '\n')
                {
                    ++com_lines;
                }
                ++data;
            }
            if (*data)
            {
                data += 2;
            }
        }
        else
        {
            break;
        }
    }

    com_tokenline = com_lines;

    if (*data == '"')
    {
        ++data;
        while (char c = *data)
        {
            if (c == '"' || c == '\0')
            {
                if (c == '"')
                {
                    ++data;
                }
                com_token[len] = '\0';
                *text = data;
                return std::string_view(com_token, len);
            }
            if (c == '\n')
            {
                ++com_lines;
            }
            if (len < MAX_TOKEN_CHARS - 1)
            {
                com_token[len++] = c;
            }
            ++data;
        }
    }

    while (char c = *data)
    {
        if (c <= ' ')
        {
            break;
        }
        if (len < MAX_TOKEN_CHARS - 1)
        {
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

static inline bool Q_isfinite(float f)
{
    return !(std::isinf(f) || std::isnan(f));
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