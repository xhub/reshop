/* SPDX-License-Identifier: MIT */
/* This code started from https://github.com/PixelRifts/c-codebase */

#include "reshop_config.h"

#include <assert.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <errhandlingapi.h>
#include <memoryapi.h>

#else

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#endif

#include "allocators-os.h"
#include "macros.h"
#include "printout.h"

void* OS_MemoryReserve(u64 size) {
    void *memory;
#ifdef _WIN32
    memory = VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
   if (!memory) {
      dw = GetLastError();
      error("FATAL ERROR: Could not reserve memory. Error code is %lu\n", dw);
   }

#else

   memory = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);

   if (MAP_FAILED == memory) {
      assert(0 && "mmap failed");
      int errno_ = errno;
      char *msg, buf[256];
      STRERROR(errno_, buf, sizeof(buf)-1, msg);
      error("FATAL ERROR: mmap() failed with msg: %s\n", msg);
      memory = NULL;
   }
#endif

   return memory;
}

int OS_MemoryCommit(void* memory, u64 size) {

#ifdef _WIN32
   void *ret = VirtualAlloc(memory, size, MEM_COMMIT, PAGE_READWRITE);
   if (!ret) {
      dw = GetLastError();
      error("FATAL ERROR: Could not commit memory. Error code is %lu\n", dw);
      return Error_SystemError;
   }
#else
   SYS_CALL_RET(mprotect(memory, size, PROT_READ | PROT_WRITE));
#endif

   return OK;
}

int OS_MemoryDecommit(void* memory, u64 size)
{
#ifdef _WIN32
   int ret = VirtualFree(memory, size, MEM_DECOMMIT);
   if (!ret) {
      dw = GetLastError();
      error("FATAL ERROR: Could not decommit memory. Error code is %lu\n", dw);
      return Error_SystemError;
   }
#else
   SYS_CALL_RET(mprotect(memory, size, PROT_NONE));
#endif

   return OK;
}

int OS_MemoryRelease(void* memory, u64 size)
{
#ifdef _WIN32
   int ret = VirtualFree(memory, size, MEM_RELEASE);
   if (!ret) {
      dw = GetLastError();
      error("FATAL ERROR: Could not release memory. Error code is %lu\n", dw);
      return Error_SystemError;
   }
#else
    SYS_CALL_RET(munmap(memory, size));

#endif

return OK;
}


