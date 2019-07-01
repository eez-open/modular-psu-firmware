#include <memory.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__EMSCRIPTEN__)

extern "C" char *strndup(const char *s, size_t n) {
    size_t len = strlen(s);
    if (len > n) {
        len = n;
    }
    char *result = (char *)malloc(len + 1);
    if (!result) {
        return NULL;
    }
    memcpy(result, s, len);
    result[len] = '\0';
    return result;
}

#endif