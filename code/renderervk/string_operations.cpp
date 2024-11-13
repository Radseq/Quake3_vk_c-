#include "string_operations.hpp"
#include "utils.hpp"
#include <cctype>
#include <charconv>

std::string_view COM_GetExtension_cpp(std::string_view name)
{
    auto dot = name.find_last_of('.');
    auto slash = name.find_last_of('/');

    if (dot != std::string_view::npos && (slash == std::string_view::npos || slash < dot))
        return name.substr(dot + 1);
    else
        return "";
}

void COM_StripExtension_cpp(std::string_view in, std::string_view &out)
{
    auto dot = in.find_last_of('.');
    auto slash = in.find_last_of('/');

    size_t destsize = in.size(); // Initialize destsize with size of 'in'

    if (dot != std::string_view::npos && (slash == std::string_view::npos || slash < dot))
    {
        destsize = dot; // Update destsize to strip extension
    }

    if (in.data() == out.data() && destsize > 1)
    {
        out = in.substr(0, destsize);
    }
    else
    {
        out = in; // Fallback if out is not the same as in
    }
}

static int com_tokenline;
static int com_lines;
static char com_token[MAX_TOKEN_CHARS];

static inline bool isWhitespace(char c)
{
    return std::isspace(static_cast<unsigned char>(c));
}

std::string_view SkipWhitespace_cpp(std::string_view data, bool &hasNewLines)
{
    hasNewLines = false;
    while (!data.empty() && isWhitespace(data.front()))
    {
        if (data.front() == '\n')
        {
            hasNewLines = true;
        }
        data.remove_prefix(1);
    }
    return data;
}

std::string_view COM_ParseExt_cpp(const char **text, bool allowLineBreaks)
{
    std::string_view data(*text);
    int c = 0, len = 0;
    bool hasNewLines = false;

    com_token[0] = '\0';
    com_tokenline = 0;

    // make sure incoming data is valid
    if (data.empty())
    {
        *text = data.data();
        return std::string_view(com_token, len);
    }

    while (true)
    {
        // skip whitespace
        data = SkipWhitespace_cpp(data, hasNewLines);
        if (data.empty())
        {
            *text = data.data();
            return std::string_view(com_token, len);
        }
        if (hasNewLines && !allowLineBreaks)
        {
            *text = data.data();
            return std::string_view(com_token, len);
        }

        c = data.front();

        // skip double slash comments
        if (c == '/' && data.size() > 1 && data[1] == '/')
        {
            data.remove_prefix(2);
            while (!data.empty() && data.front() != '\n')
            {
                data.remove_prefix(1);
            }
        }
        // skip /* */ comments
        else if (c == '/' && data.size() > 1 && data[1] == '*')
        {
            data.remove_prefix(2);
            while (!data.empty() && (data.front() != '*' || (data.size() > 1 && data[1] != '/')))
            {
                if (data.front() == '\n')
                {
                    com_lines++;
                }
                data.remove_prefix(1);
            }
            if (!data.empty())
            {
                data.remove_prefix(2);
            }
        }
        else
        {
            break;
        }
    }

    // token starts on this line
    com_tokenline = com_lines;

    // handle quoted strings
    if (c == '"')
    {
        data.remove_prefix(1);
        while (true)
        {
            if (data.empty() || data.front() == '"')
            {
                if (!data.empty())
                {
                    data.remove_prefix(1);
                }
                com_token[len] = '\0';
                *text = data.data();
                return std::string_view(com_token, len);
            }
            if (data.front() == '\n')
            {
                com_lines++;
            }
            if (len < MAX_TOKEN_CHARS - 1)
            {
                com_token[len] = data.front();
                len++;
            }
            data.remove_prefix(1);
        }
    }

    // parse a regular word
    while (!data.empty() && data.front() > ' ')
    {
        if (len < MAX_TOKEN_CHARS - 1)
        {
            com_token[len] = data.front();
            len++;
        }
        data.remove_prefix(1);
    }

    com_token[len] = '\0';

    *text = data.data();
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

bool Q_isfinite(float f)
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