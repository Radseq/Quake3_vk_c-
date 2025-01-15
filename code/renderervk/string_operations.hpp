#ifndef STRING_OPERATIONS_HPP
#define STRING_OPERATIONS_HPP

#include <string_view>
#include <array>
#include <string>
#include <format>
#include <vector>
#include <cstdarg>
#include <algorithm> // For std::min

template <std::size_t Size>
void Q_strncpyz_cpp(std::array<char, Size> &dest, std::string_view src)
{
    if (Size < 1)
    {
        throw std::invalid_argument("Q_strncpyz_cpp: dest size < 1");
    }

    // Determine the number of characters to copy
    std::size_t length = std::min<std::size_t>(Size - 1, src.size()); // Leave room for null-terminator

    // Copy the characters
    std::copy_n(src.begin(), length, dest.begin());

    // Null-terminate the destination array
    dest[length] = '\0';
}

template <std::size_t Size>
void Q_strncpyz_cpp(std::array<char, Size> &dest, std::string_view src, std::size_t max_cpy_size)
{
    if (Size < 1)
    {
        throw std::invalid_argument("Q_strncpyz_cpp: dest size < 1");
    }

    // Determine the number of characters to copy
    std::size_t length = std::min<std::size_t>({Size - 1, max_cpy_size, src.size()}); // Leave room for null-terminator

    // Copy the characters
    std::copy_n(src.begin(), length, dest.begin());

    // Null-terminate the destination array
    dest[length] = '\0';
}

inline constexpr bool strrchr_sv(std::string_view sv, char character)
{
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

// portable case insensitive compare
inline constexpr int Q_stricmp_cpp(std::string_view s1, std::string_view s2)
{
    // // Compare lengths first for a quick check
    // if (s1.size() != s2.size())
    //     return s1.size() < s2.size() ? -1 : 1;

    // // Compare characters one by one
    // for (size_t i = 0; i < s1.size(); ++i)
    // {
    //     char c1 = s1[i];
    //     char c2 = s2[i];

    //     // Convert to lowercase if uppercase
    //     if (c1 >= 'A' && c1 <= 'Z')
    //         c1 += 'a' - 'A';
    //     if (c2 >= 'A' && c2 <= 'Z')
    //         c2 += 'a' - 'A';

    //     // Compare characters
    //     if (c1 != c2)
    //         return c1 < c2 ? -1 : 1;
    // }
    // return 0; // Strings are equal

    auto to_lower = [](char c)
    { return std::tolower(static_cast<unsigned char>(c)); };

    auto it1 = s1.begin(), it2 = s2.begin();
    while (it1 != s1.end() && it2 != s2.end())
    {
        char c1 = to_lower(*it1++);
        char c2 = to_lower(*it2++);
        if (c1 != c2)
        {
            return c1 < c2 ? -1 : 1;
        }
    }
    if (it1 == s1.end() && it2 == s2.end())
        return 0;
    return it1 == s1.end() ? -1 : 1;
}

std::string_view COM_GetExtension_cpp(std::string_view name);
template <std::size_t Size>
std::string_view COM_GetExtension_cpp(const std::array<char, Size> &name)
{
    std::size_t length = 0;
    for (; length < Size && name[length] != '\0'; ++length)
        ;

    // Convert the array to a string_view for efficient manipulation
    std::string_view nameView(name.data(), length);

    // Find the last '.' and '/'
    auto dot = nameView.find_last_of('.');
    auto slash = nameView.find_last_of('/');

    // Check if a valid extension exists
    if (dot != std::string_view::npos && (slash == std::string_view::npos || slash < dot))
    {
        return nameView.substr(dot + 1); // Return the part after the last '.'
    }

    // No valid extension, return an empty string_view
    return std::string_view{};
}

template <std::size_t Size>
void COM_StripExtension_cpp(std::string_view in, std::array<char, Size> &dest)
{
    if (Size < 1)
    {
        throw std::invalid_argument("COM_StripExtension_cpp: dest size < 1");
    }

    // Find the last '.' and '/'
    auto dot = in.find_last_of('.');
    auto slash = in.find_last_of('/');

    // Determine the effective length to copy
    std::size_t copyLength = in.size();
    if (dot != std::string_view::npos && (slash == std::string_view::npos || slash < dot))
    {
        copyLength = dot;
    }

    // Create a substring view for copying
    std::string_view substring = in.substr(0, copyLength);

    // Use Q_strncpyz_cpp to copy the substring to the destination array
    Q_strncpyz_cpp(dest, substring);
}

std::string_view COM_ParseExt_cpp(const char **text, bool allowLineBreaks);
int Q_stricmpn_cpp(std::string_view s1, std::string_view s2, int n);
float Q_atof_cpp(std::string_view str);
int atoi_from_view(std::string_view sv);

template <int N>
constexpr auto intToStringView()
{
    std::array<char, 12> buffer{}; // Enough to hold -2^31 to 2^31-1 and the null terminator
    char *p = buffer.data() + buffer.size() - 1;
    *p = '\0';
    int value = N;

    do
    {
        *--p = '0' + (value % 10);
        value /= 10;
    } while (value);

    return std::string_view(p);
}

template <std::size_t Size>
std::array<char, Size> va_cpp(const char *format, ...)
{
    static_assert(Size > 0, "Size must be greater than zero.");

    std::array<char, Size> buf{};
    va_list args;
    va_start(args, format);

    // Use std::vsnprintf to safely format into the buffer
    int result = std::vsnprintf(buf.data(), buf.size(), format, args);

    va_end(args);

    if (result < 0)
    {
        // Handle formatting error
        std::snprintf(buf.data(), buf.size(), "Formatting error.");
    }
    else if (static_cast<std::size_t>(result) >= buf.size())
    {
        // Truncated output
        buf[buf.size() - 1] = '\0';
    }

    return buf;
}

template <std::size_t BufferSize = 32000, std::size_t BufferCount = 2, typename... Args>
std::string_view va_cpp(const char *format, Args &&...args)
{
    static_assert(BufferSize > 0, "BufferSize must be greater than zero.");
    static_assert(BufferCount > 0, "BufferCount must be greater than zero.");

    // Thread-local static buffers for thread safety
    thread_local char buffers[BufferCount][BufferSize];
    thread_local int index = 0;

    // Use the current buffer and increment index
    char *buf = buffers[index];
    index = (index + 1) % BufferCount;

    // Format the string into the buffer
    int result = std::snprintf(buf, BufferSize, format, std::forward<Args>(args)...);

    if (result < 0)
    {
        // Handle formatting errors gracefully
        std::snprintf(buf, BufferSize, "Formatting error.");
    }
    else if (static_cast<std::size_t>(result) >= BufferSize)
    {
        // Ensure null termination for truncated strings
        buf[BufferSize - 1] = '\0';
    }

    // Return a string_view pointing to the formatted buffer
    return std::string_view(buf);
}

#endif // STRING_OPERATIONS_HPP
