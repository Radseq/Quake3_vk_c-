#include "q_shared.hpp"

/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/


char *Q_strlwr(char *s1)
{
    char *s;

    s = s1;
    while (*s)
    {
        *s = locase[(unsigned char)*s];
        s++;
    }
    return s1;
}

const char *COM_GetExtension(const char *name)
{
    const char *dot = strrchr(name, '.'), *slash;
    if (dot && ((slash = strrchr(name, '/')) == nullptr || slash < dot))
        return dot + 1;
    else
        return "";
}

void COM_StripExtension(const char *in, char *out, int destsize)
{
    const char *dot = strrchr(in, '.'), *slash;

    if (dot && ((slash = strrchr(in, '/')) == nullptr || slash < dot))
        destsize = (destsize < dot - in + 1 ? destsize : dot - in + 1);

    if (in == out && destsize > 1)
        out[destsize - 1] = '\0';
    else
        Q_strncpyz(out, in, destsize);
}

/*
=============
Q_strncpyz

Safe strncpy that ensures a trailing zero
=============
*/
// void Q_strncpyz(char *dest, const char *src, int destsize)
// {
//     if (!dest)
//     {
//         Com_Error(ERR_FATAL, "Q_strncpyz: NULL dest");
//     }

//     if (!src)
//     {
//         Com_Error(ERR_FATAL, "Q_strncpyz: NULL src");
//     }

//     if (destsize < 1)
//     {
//         Com_Error(ERR_FATAL, "Q_strncpyz: destsize < 1");
//     }
// #if 1
//     // do not fill whole remaining buffer with zeros
//     // this is obvious behavior change but actually it may affect only buggy QVMs
//     // which passes overlapping or short buffers to cvar reading routines
//     // what is rather good than bad because it will no longer cause overwrites, maybe
//     while (--destsize > 0 && (*dest++ = *src++) != '\0')
//         ;
//     *dest = '\0';
// #else
//     strncpy(dest, src, destsize - 1);
//     dest[destsize - 1] = '\0';
// #endif
// }

static char com_token[MAX_TOKEN_CHARS];
static char com_parsename[MAX_TOKEN_CHARS];
static int com_tokenline;
static int com_lines;

// for complex parser

void COM_BeginParseSession(const char *name)
{
    com_lines = 1;
    com_tokenline = 0;
    Com_sprintf(com_parsename, sizeof(com_parsename), "%s", name);
}

int COM_GetCurrentParseLine(void)
{
    if (com_tokenline)
    {
        return com_tokenline;
    }

    return com_lines;
}

// int Q_stricmp( const char *s1, const char *s2 )
// {
// 	unsigned char c1, c2;

// 	if ( s1 == NULL )
// 	{
// 		if ( s2 == NULL )
// 			return 0;
// 		else
// 			return -1;
// 	}
// 	else if ( s2 == NULL )
// 		return 1;

// 	do
// 	{
// 		c1 = *s1++;
// 		c2 = *s2++;

// 		if ( c1 != c2 )
// 		{

// 			if ( c1 <= 'Z' && c1 >= 'A' )
// 				c1 += ('a' - 'A');

// 			if ( c2 <= 'Z' && c2 >= 'A' )
// 				c2 += ('a' - 'A');

// 			if ( c1 != c2 )
// 				return c1 < c2 ? -1 : 1;
// 		}
// 	}
// 	while ( c1 != '\0' );

// 	return 0;
// }

/*
==============
COM_Parse

Parse a token out of a string
Will never return NULL, just empty strings

If "allowLineBreaks" is true then an empty
string will be returned if the next token is
a newline.
==============
*/
static const char *SkipWhitespace(const char *data, bool *hasNewLines)
{
    int c;

    while ((c = *data) <= ' ')
    {
        if (!c)
        {
            return nullptr;
        }
        if (c == '\n')
        {
            com_lines++;
            *hasNewLines = true;
        }
        data++;
    }

    return data;
}

const char *COM_ParseExt(const char **data_p, bool allowLineBreaks)
{
    int c = 0, len;
    bool hasNewLines = false;
    const char *data;

    data = *data_p;
    len = 0;
    com_token[0] = '\0';
    com_tokenline = 0;

    // make sure incoming data is valid
    if (!data)
    {
        *data_p = nullptr;
        return com_token;
    }

    while (1)
    {
        // skip whitespace
        data = SkipWhitespace(data, &hasNewLines);
        if (!data)
        {
            *data_p = nullptr;
            return com_token;
        }
        if (hasNewLines && !allowLineBreaks)
        {
            *data_p = data;
            return com_token;
        }

        c = *data;

        // skip double slash comments
        if (c == '/' && data[1] == '/')
        {
            data += 2;
            while (*data && *data != '\n')
            {
                data++;
            }
        }
        // skip /* */ comments
        else if (c == '/' && data[1] == '*')
        {
            data += 2;
            while (*data && (*data != '*' || data[1] != '/'))
            {
                if (*data == '\n')
                {
                    com_lines++;
                }
                data++;
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

    // token starts on this line
    com_tokenline = com_lines;

    // handle quoted strings
    if (c == '"')
    {
        data++;
        while (1)
        {
            c = *data;
            if (c == '"' || c == '\0')
            {
                if (c == '"')
                    data++;
                com_token[len] = '\0';
                *data_p = data;
                return com_token;
            }
            data++;
            if (c == '\n')
            {
                com_lines++;
            }
            if (len < ARRAY_LEN(com_token) - 1)
            {
                com_token[len] = c;
                len++;
            }
        }
    }

    // parse a regular word
    do
    {
        if (len < ARRAY_LEN(com_token) - 1)
        {
            com_token[len] = c;
            len++;
        }
        data++;
        c = *data;
    } while (c > ' ');

    com_token[len] = '\0';

    *data_p = data;
    return com_token;
}

/*
=================
SkipBracedSection

The next token should be an open brace or set depth to 1 if already parsed it.
Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
bool SkipBracedSection(const char **program, int depth)
{
    const char *token;

    do
    {
        token = COM_ParseExt(program, true);
        if (token[1] == 0)
        {
            if (token[0] == '{')
            {
                depth++;
            }
            else if (token[0] == '}')
            {
                depth--;
            }
        }
    } while (depth && *program);

    return (depth == 0);
}

// never goes past bounds or leaves without a terminating 0
void Q_strcat(char *dest, int size, const char *src)
{
    int l1;

    l1 = strlen(dest);
    if (l1 >= size)
    {
        Com_Error(ERR_FATAL, "Q_strcat: already overflowed");
    }
    Q_strncpyz(dest + l1, src, size - l1);
}

// for complex parser

char *COM_ParseComplex(const char **data_p, bool allowLineBreaks)
{
    static const byte is_separator[256] =
        {
            // \0 . . . . . . .\b\t\n . .\r . .
            1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0,
            //  . . . . . . . . . . . . . . . .
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            //    ! " # $ % & ' ( ) * + , - . /
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, // excl. '-' '.' '/'
                                                            //  0 1 2 3 4 5 6 7 8 9 : ; < = > ?
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
            //  @ A B C D E F G H I J K L M N O
            1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            //  P Q R S T U V W X Y Z [ \ ] ^ _
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, // excl. '\\' '_'
                                                            //  ` a b c d e f g h i j k l m n o
            1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            //  p q r s t u v w x y z { | } ~
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1};

    int c, len, shift;
    const byte *str;

    str = (byte *)*data_p;
    len = 0;
    shift = 0; // token line shift relative to com_lines
    com_tokentype = TK_GENEGIC;

__reswitch:
    switch (*str)
    {
    case '\0':
        com_tokentype = TK_EOF;
        break;

    // whitespace
    case ' ':
    case '\t':
        str++;
        while ((c = *str) == ' ' || c == '\t')
            str++;
        goto __reswitch;

    // newlines
    case '\n':
    case '\r':
        com_lines++;
        if (*str == '\r' && str[1] == '\n')
            str += 2; // CR+LF
        else
            str++;
        if (!allowLineBreaks)
        {
            com_tokentype = TK_NEWLINE;
            break;
        }
        goto __reswitch;

    // comments, single slash
    case '/':
        // until end of line
        if (str[1] == '/')
        {
            str += 2;
            while ((c = *str) != '\0' && c != '\n' && c != '\r')
                str++;
            goto __reswitch;
        }

        // comment
        if (str[1] == '*')
        {
            str += 2;
            while ((c = *str) != '\0' && (c != '*' || str[1] != '/'))
            {
                if (c == '\n' || c == '\r')
                {
                    com_lines++;
                    if (c == '\r' && str[1] == '\n') // CR+LF?
                        str++;
                }
                str++;
            }
            if (c != '\0' && str[1] != '\0')
            {
                str += 2;
            }
            else
            {
                // FIXME: unterminated comment?
            }
            goto __reswitch;
        }

        // single slash
        com_token[len++] = *str++;
        break;

    // quoted string?
    case '"':
        str++; // skip leading '"'
        // com_tokenline = com_lines;
        while ((c = *str) != '\0' && c != '"')
        {
            if (c == '\n' || c == '\r')
            {
                com_lines++; // FIXME: unterminated quoted string?
                shift++;
            }
            if (len < MAX_TOKEN_CHARS - 1) // overflow check
                com_token[len++] = c;
            str++;
        }
        if (c != '\0')
        {
            str++; // skip ending '"'
        }
        else
        {
            // FIXME: unterminated quoted string?
        }
        com_tokentype = TK_QUOTED;
        break;

    // single tokens:
    case '+':
    case '`':
    /*case '*':*/ case '~':
    case '{':
    case '}':
    case '[':
    case ']':
    case '?':
    case ',':
    case ':':
    case ';':
    case '%':
    case '^':
        com_token[len++] = *str++;
        break;

    case '*':
        com_token[len++] = *str++;
        com_tokentype = TK_MATCH;
        break;

    case '(':
        com_token[len++] = *str++;
        com_tokentype = TK_SCOPE_OPEN;
        break;

    case ')':
        com_token[len++] = *str++;
        com_tokentype = TK_SCOPE_CLOSE;
        break;

    // !, !=
    case '!':
        com_token[len++] = *str++;
        if (*str == '=')
        {
            com_token[len++] = *str++;
            com_tokentype = TK_NEQ;
        }
        break;

    // =, ==
    case '=':
        com_token[len++] = *str++;
        if (*str == '=')
        {
            com_token[len++] = *str++;
            com_tokentype = TK_EQ;
        }
        break;

    // >, >=
    case '>':
        com_token[len++] = *str++;
        if (*str == '=')
        {
            com_token[len++] = *str++;
            com_tokentype = TK_GTE;
        }
        else
        {
            com_tokentype = TK_GT;
        }
        break;

    //  <, <=
    case '<':
        com_token[len++] = *str++;
        if (*str == '=')
        {
            com_token[len++] = *str++;
            com_tokentype = TK_LTE;
        }
        else
        {
            com_tokentype = TK_LT;
        }
        break;

    // |, ||
    case '|':
        com_token[len++] = *str++;
        if (*str == '|')
        {
            com_token[len++] = *str++;
            com_tokentype = TK_OR;
        }
        break;

    // &, &&
    case '&':
        com_token[len++] = *str++;
        if (*str == '&')
        {
            com_token[len++] = *str++;
            com_tokentype = TK_AND;
        }
        break;

    // rest of the charset
    default:
        com_token[len++] = *str++;
        while (!is_separator[(c = *str)])
        {
            if (len < MAX_TOKEN_CHARS - 1)
                com_token[len++] = c;
            str++;
        }
        com_tokentype = TK_STRING;
        break;

    } // switch ( *str )

    com_tokenline = com_lines - shift;
    com_token[len] = '\0';
    *data_p = (char *)str;
    return com_token;
}

char *Q_stradd(char *dst, const char *src)
{
    char c;
    while ((c = *src++) != '\0')
        *dst++ = c;
    *dst = '\0';
    return dst;
}

/*
==================
COM_GenerateHashValue

used in renderer and filesystem
==================
*/
// ASCII lowcase conversion table with '\\' turned to '/' and '.' to '\0'
static const byte hash_locase[256] =
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

unsigned long Com_GenerateHashValue(const char *fname, const unsigned int size)
{
    const byte *s;
    unsigned long hash;
    int c;

    s = (byte *)fname;
    hash = 0;

    while ((c = hash_locase[(byte)*s++]) != '\0')
    {
        hash = hash * 101 + c;
    }

    hash = (hash ^ (hash >> 10) ^ (hash >> 20));
    hash &= (size - 1);

    return hash;
}

int Q_stricmpn(const char *s1, const char *s2, int n)
{
    int c1, c2;

    // bk001129 - moved in 1.17 fix not in id codebase
    if (s1 == NULL)
    {
        if (s2 == NULL)
            return 0;
        else
            return -1;
    }
    else if (s2 == NULL)
        return 1;

    do
    {
        c1 = *s1++;
        c2 = *s2++;

        if (!n--)
        {
            return 0; // strings are equal until end point
        }

        if (c1 != c2)
        {
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
        }
    } while (c1);

    return 0; // strings are equal
}

void SkipRestOfLine(const char **data)
{
    const char *p;
    int c;

    p = *data;

    if (!*p)
        return;

    while ((c = *p) != '\0')
    {
        p++;
        if (c == '\n')
        {
            com_lines++;
            break;
        }
    }

    *data = p;
}

int COM_Compress(char *data_p)
{
    const char *in;
    char *out;
    int c;
    bool newline = false, whitespace = false;

    in = out = data_p;
    while ((c = *in) != '\0')
    {
        // skip double slash comments
        if (c == '/' && in[1] == '/')
        {
            while (*in && *in != '\n')
            {
                in++;
            }
            // skip /* */ comments
        }
        else if (c == '/' && in[1] == '*')
        {
            while (*in && (*in != '*' || in[1] != '/'))
                in++;
            if (*in)
                in += 2;
            // record when we hit a newline
        }
        else if (c == '\n' || c == '\r')
        {
            newline = true;
            in++;
            // record when we hit whitespace
        }
        else if (c == ' ' || c == '\t')
        {
            whitespace = true;
            in++;
            // an actual token
        }
        else
        {
            // if we have a pending newline, emit it (and it counts as whitespace)
            if (newline)
            {
                *out++ = '\n';
                newline = false;
                whitespace = false;
            }
            else if (whitespace)
            {
                *out++ = ' ';
                whitespace = false;
            }
            // copy quoted strings unmolested
            if (c == '"')
            {
                *out++ = c;
                in++;
                while (1)
                {
                    c = *in;
                    if (c && c != '"')
                    {
                        *out++ = c;
                        in++;
                    }
                    else
                    {
                        break;
                    }
                }
                if (c == '"')
                {
                    *out++ = c;
                    in++;
                }
            }
            else
            {
                *out++ = c;
                in++;
            }
        }
    }

    *out = '\0';

    return out - data_p;
}

unsigned int crc32_buffer(const byte *buf, unsigned int len)
{
    static unsigned int crc32_table[256];
    static bool crc32_inited = false;

    unsigned int crc = 0xFFFFFFFFUL;

    if (!crc32_inited)
    {
        unsigned int c;
        int i, j;

        for (i = 0; i < 256; i++)
        {
            c = i;
            for (j = 0; j < 8; j++)
                c = (c & 1) ? (c >> 1) ^ 0xEDB88320UL : c >> 1;
            crc32_table[i] = c;
        }
        crc32_inited = true;
    }

    while (len--)
    {
        crc = crc32_table[(crc ^ *buf++) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFFUL;
}

char *COM_SkipPath(char *pathname)
{
    char *last;

    last = pathname;
    while (*pathname)
    {
        if (*pathname == '/')
            last = pathname + 1;
        pathname++;
    }
    return last;
}

const char *COM_Parse(const char **data_p)
{
    return COM_ParseExt(data_p, true);
}

int Com_Split(char *in, char **out, int outsz, int delim)
{
    int c;
    char **o = out, **end = out + outsz;
    // skip leading spaces
    if (delim >= ' ')
    {
        while ((c = *in) != '\0' && c <= ' ')
            in++;
    }
    *out = in;
    out++;
    while (out < end)
    {
        while ((c = *in) != '\0' && c != delim)
            in++;
        *in = '\0';
        if (!c)
        {
            // don't count last null value
            if (out[-1][0] == '\0')
                out--;
            break;
        }
        in++;
        // skip leading spaces
        if (delim >= ' ')
        {
            while ((c = *in) != '\0' && c <= ' ')
                in++;
        }
        *out = in;
        out++;
    }
    // sanitize last value
    while ((c = *in) != '\0' && c != delim)
        in++;
    *in = '\0';
    c = out - o;
    // set remaining out pointers
    while (out < end)
    {
        *out = in;
        out++;
    }
    return c;
}

int Q_strncmp(const char *s1, const char *s2, int n)
{
    int c1, c2;

    do
    {
        c1 = *s1++;
        c2 = *s2++;

        if (!n--)
        {
            return 0; // strings are equal until end point
        }

        if (c1 != c2)
        {
            return c1 < c2 ? -1 : 1;
        }
    } while (c1);

    return 0; // strings are equal
}

const char *Q_stristr(const char *s, const char *find)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != 0)
    {
        if (c >= 'a' && c <= 'z')
        {
            c -= ('a' - 'A');
        }
        len = strlen(find);
        do
        {
            do
            {
                if ((sc = *s++) == 0)
                    return NULL;
                if (sc >= 'a' && sc <= 'z')
                {
                    sc -= ('a' - 'A');
                }
            } while (sc != c);
        } while (Q_stricmpn(s, find, len) != 0);
        s--;
    }
    return s;
}

int LongSwap(int l)
{
    byte b1, b2, b3, b4;

    b1 = l & 255;
    b2 = (l >> 8) & 255;
    b3 = (l >> 16) & 255;
    b4 = (l >> 24) & 255;

    return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}