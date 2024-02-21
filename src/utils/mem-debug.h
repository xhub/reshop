#ifndef MEM_DEBUG_H
#define MEM_DEBUG_H

#ifdef WITH_VALGRIND
#include <valgrind/memcheck.h>
#else
#define VALGRIND_MAKE_MEM_UNDEFINED(A, B)
#define VALGRIND_MAKE_MEM_NOACCESS(A, B)

#endif

#endif
