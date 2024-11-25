#ifndef RESHOP_MEM_DEBUG_H
#define RESHOP_MEM_DEBUG_H

/** @file mem-debug.h 
*
*   @brief memory debug defines
*/

#ifdef WITH_VALGRIND

#include <valgrind/memcheck.h>

#define MEMORY_UNDEF      VALGRIND_MAKE_MEM_UNDEFINED
#define MEMORY_POISON     VALGRIND_MAKE_MEM_POISON  
#define MEMORY_UNPOISON   VALGRIND_MAKE_MEM_DEFINED

#elif __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)

#include <sanitizer/asan_interface.h>

#define MEMORY_POISON            ASAN_POISON_MEMORY_REGION
#define MEMORY_UNPOISON          ASAN_UNPOISON_MEMORY_REGION

#elif __has_feature(memory_sanitizer) || defined(__SANITIZE_MEMORY__)

#include <sanitizer/msan_interface.h>

#define MEMORY_UNDEF             __msan_poison

#endif

#ifndef MEMORY_UNDEF
#define MEMORY_UNDEF(addr, size)    ((void)(addr), (void)(size))
#endif

#ifndef MEMORY_POISON
#define MEMORY_POISON(addr, size)   ((void)(addr), (void)(size))
#endif

#ifndef MEMORY_UNPOISON
#define MEMORY_UNPOISON(addr, size) ((void)(addr), (void)(size))
#endif

#endif
