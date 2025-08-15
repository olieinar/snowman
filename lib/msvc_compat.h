#pragma once
#include <cmath>
#include <cstdint>

#ifdef _MSC_VER
// M_PI, etc.
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// GCC's __builtin_clz replacement
#include <intrin.h>
inline int msvc_clz32(uint32_t x) {
	unsigned long idx;
	if (_BitScanReverse(&idx, x)) return 31 - (int)idx;
	return 32; // undefined for 0, but safe fallback
}
#ifndef __builtin_clz
#define __builtin_clz(x) msvc_clz32((uint32_t)(x))
#endif

// sincosf replacement
inline void sincosf(float x, float* s, float* c) {
	*s = std::sinf(x);
	*c = std::cosf(x);
}

// ssize_t on MSVC
#include <BaseTsd.h>
using ssize_t = SSIZE_T;
#endif
