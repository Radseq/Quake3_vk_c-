#include "string_operations.hpp"
#include "utils.hpp"
#include "q_shared.hpp"
#include <cctype>

int Q_stricmp_cpp(std::string_view s1, std::string_view s2)
{
    if (s1.empty())
    {
        if (s2.empty())
            return 0;
        else
            return -1;
    }
    else if (s2.empty())
        return 1;

    auto it1 = s1.begin();
    auto it2 = s2.begin();

    while (it1 != s1.end() && it2 != s2.end())
    {
        unsigned char c1 = *it1++;
        unsigned char c2 = *it2++;

        if (c1 != c2)
        {
            if (c1 <= 'Z' && c1 >= 'A')
                c1 += ('a' - 'A');

            if (c2 <= 'Z' && c2 >= 'A')
                c2 += ('a' - 'A');

            if (c1 != c2)
                return c1 < c2 ? -1 : 1;
        }
    }

    if (it1 != s1.end())
        return 1;
    if (it2 != s2.end())
        return -1;

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

std::string_view SkipWhitespace(std::string_view data, bool &hasNewLines)
{
    hasNewLines = false;
    size_t i = 0;
    while (i < data.size() && std::isspace(data[i]))
    {
        if (data[i] == '\n')
        {
            hasNewLines = true;
            com_lines++;
        }
        i++;
    }
    return data.substr(i);
}

std::string_view COM_ParseExt_cpp(std::string_view &data, bool allowLineBreaks)
{
    int len = 0;
    bool hasNewLines = false;
    com_token[0] = '\0';
    com_tokenline = 0;

    // make sure incoming data is valid
    if (data.empty())
    {
        data = {};
        return std::string_view(com_token);
    }

    while (true)
    {
        // skip whitespace
        data = SkipWhitespace(data, hasNewLines);
        if (data.empty())
        {
            return std::string_view(com_token);
        }
        if (hasNewLines && !allowLineBreaks)
        {
            return std::string_view(com_token);
        }

        char c = data.front();

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
            while (!data.empty() && !(data.front() == '*' && data.size() > 1 && data[1] == '/'))
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
    if (data.front() == '"')
    {
        data.remove_prefix(1);
        while (true)
        {
            char c = data.front();
            if (c == '"' || c == '\0')
            {
                if (c == '"')
                {
                    data.remove_prefix(1);
                }
                com_token[len] = '\0';
                std::string_view(com_token);
            }
            if (c == '\n')
            {
                com_lines++;
            }
            if (len < MAX_TOKEN_CHARS - 1)
            {
                com_token[len++] = c;
            }
            data.remove_prefix(1);
        }
    }

    // parse a regular word
    while (!data.empty() && data.front() > ' ')
    {
        if (len < MAX_TOKEN_CHARS - 1)
        {
            com_token[len++] = data.front();
        }
        data.remove_prefix(1);
    }

    com_token[len] = '\0';
    return std::string_view(com_token);
}