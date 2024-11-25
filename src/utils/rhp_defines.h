/* date = September 27th 2021 11:48 am */

#ifndef DEFINES_H
#define DEFINES_H

#include <stdio.h>

// Unsigned int types.
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

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

#endif //DEFINES_H
