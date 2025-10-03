#ifndef DEFINES_H
#define DEFINES_H

#include <stdio.h>
#include <inttypes.h>

// Unsigned int types.
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef uint64_t u64;

// Signed int types.
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

// Regular int types.
typedef char i8;
typedef short i16;
typedef int i32;

// Floating point types
typedef float f32;
typedef double f64;

// Boolean types
typedef u8  b8;
typedef u32 b32;

#define Gigabytes(count) (u64) (((u64)(count)) * 1024 * 1024 * 1024)
#define Megabytes(count) (u64) (((u64)(count)) * 1024 * 1024)
#define Kilobytes(count) (u64) (((u64)(count)) * 1024)

#ifdef _WIN32
#include <basetsd.h>

typedef UINT_PTR  rhpfd_t;
#define FDP       "%llu"

#else

typedef int rhpfd_t;
#define FDP "%d"

#endif

#endif //DEFINES_H
