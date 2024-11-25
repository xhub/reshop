/* SPDX-License-Identifier: MIT */
/* This code started from https://github.com/PixelRifts/c-codebase */

#ifndef RESHOP_ALLOCATORS_OS_H
#define RESHOP_ALLOCATORS_OS_H

#include "rhp_defines.h"

//~ OS Init

//void OS_Init(void);

//~ Memory

void* OS_MemoryReserve(u64 size);
int   OS_MemoryCommit(void* memory, u64 size);
int   OS_MemoryDecommit(void* memory, u64 size);
int   OS_MemoryRelease(void* memory, u64 size);

#endif //RESHOP_ALLOCATORS_OS_H
