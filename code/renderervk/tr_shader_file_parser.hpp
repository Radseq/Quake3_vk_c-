/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// Job-local lexer for shader-file structural validation.
// Tokenization intentionally matches COM_ParseExt_cpp in string_operations.cpp.
#pragma once

#include "../qcommon/q_shared.h"
#include <string_view>

class ShaderFileParser
{
    int com_tokenline = 0;
    int com_lines = 1;
    char com_token[MAX_TOKEN_CHARS]{};

    const char *SkipWhitespace(const char *data, bool &hasNewLines)
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

    std::string_view Parse(const char **text, bool allowLineBreaks)
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

public:
    std::string_view Next(const char** text) { return Parse(text, true); }
    int Line() const { return com_tokenline ? com_tokenline : com_lines; }

    bool SkipBlock(const char** text, int depth)
    {
        do
        {
            const auto token = Next(text);
            if (token.size() == 1)
            {
                if (token[0] == '{') ++depth;
                else if (token[0] == '}') --depth;
            }
        } while (depth && *text);
        return depth == 0;
    }

    void SkipLine(const char** text)
    {
        const char* p = *text;
        while (*p)
        {
            if (*p++ == '\n') { ++com_lines; break; }
        }
        *text = p;
    }
};
