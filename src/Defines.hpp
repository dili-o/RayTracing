#pragma once

#include <cstdlib>
#include <limits>
#include <random>
#include <stdint.h>

#if !defined(_MSC_VER)
#include <signal.h>
#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#define HLX_PLATFORM_WINDOWS 1

#ifndef _WIN64
#error "64-bit is required on Windows"
#endif // _WIN64

#else
#error "Unsupported Platform!"
#endif // WIN32 || _WIN32 || __WIN32__

// Macros ////////////////////////////////////////////////////////////////

#define ArraySize(array) (sizeof(array) / sizeof((array)[0]))

#if defined(_MSC_VER)
#define HLX_INLINE inline
#define HLX_FINLINE __forceinline
#define HLX_DEBUG_BREAK __debugbreak();
#define HLX_DISABLE_WARNING(warning_number)                                    \
  __pragma(warning(disable : warning_number))
#define HLX_CONCAT_OPERATOR(x, y) x##y
#else
#define HLX_INLINE inline
#define HLX_FINLINE always_inline
#define HLX_DEBUG_BREAK raise(SIGTRAP);
#define HLX_CONCAT_OPERATOR(x, y) x y
#endif // MSVC

#define HLX_STRINGIZE(L) #L
#define HLX_MAKESTRING(L) HLX_STRINGIZE(L)
#define HLX_CONCAT(x, y) HLX_CONCAT_OPERATOR(x, y)
#define HLX_LINE_STRING HLX_MAKESTRING(__LINE__)
#define HLX_FILELINE(MESSAGE) __FILE__ "(" HLX_LINE_STRING ") : " MESSAGE

// Unique names
#define HLX_UNIQUE_SUFFIX(PARAM) HLX_CONCAT(PARAM, __LINE__)

// Native types typedefs /////////////////////////////////////////////////
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

typedef const char *cstring;

static const u64 u64_max = UINT64_MAX;
static const i64 i64_max = INT64_MAX;
static const u32 u32_max = UINT32_MAX;
static const i32 i32_max = INT32_MAX;
static const u16 u16_max = UINT16_MAX;
static const i16 i16_max = INT16_MAX;
static const u8 u8_max = UINT8_MAX;
static const i8 i8_max = INT8_MAX;

#ifdef USE_DOUBLE_PRECISION
typedef double real;
#else
typedef float real;
#endif

// Constants
const real infinity = std::numeric_limits<real>::infinity();
const real pi = 3.1415926535897932385f;

// Utility Functions
inline real degrees_to_radians(real degrees) { return degrees * pi / 180.f; }

inline real random_real() {
  static std::uniform_real_distribution<real> distribution(0.f, 1.f);
  static std::mt19937 generator(std::random_device{}());
  return distribution(generator);
}

inline real random_real(real min, real max) {
  return min + (max - min) * random_real();
}

inline real linear_to_gamma(real linear_component) {
  if (linear_component > 0.f)
    return std::sqrt(linear_component);
  return 0.f;
}

#if defined(_WIN32)
#ifdef HELIX_EXPORT
#ifdef HELIX_SHARED
#define HLX_API __declspec(dllexport)
#else
#define HLX_API __declspec(dllimport)
#endif
#else
#define HLX_API
#endif
#else
#define HLX_API
#endif
