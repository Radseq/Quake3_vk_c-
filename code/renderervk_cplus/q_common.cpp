// todo this file should not be there i think

#include "q_common.hpp"
#include <cctype> // For std::tolower

int Q_stricmp_plus(const char* s1, const char* s2) {
    if (s1 == nullptr) {
        return (s2 == nullptr) ? 0 : -1;
    } else if (s2 == nullptr) {
        return 1;
    }

    char c1, c2;
    do {
        c1 = *s1++;
        c2 = *s2++;

        // Convert characters to lowercase for comparison
        c1 = static_cast<char>(std::tolower(c1));
        c2 = static_cast<char>(std::tolower(c2));

        if (c1 != c2) {
            return (c1 < c2) ? -1 : 1;
        }
    } while (c1 != '\0');

    return 0;
}