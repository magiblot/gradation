#ifndef GRADATION_UTIL_H
#define GRADATION_UTIL_H

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))

#include <strings.h>

static int stricmp(const char *a, const char *b) noexcept
{
    return strcasecmp(a, b);
}

#endif // _MSC_VER

template <class T>
constexpr inline T clamp(T v, T lo, T hi)
{
    return v < lo ? lo : (hi < v ? hi : v);
}

#endif // GRADATION_UTIL_H
