/* SPDX-License-Identifier: MIT */
/* This code started from https://github.com/PixelRifts/c-codebase */

#include "reshop_config.h"

#include <assert.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <errhandlingapi.h>
#include <memoryapi.h>

#else

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>

#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

#include "allocators-os.h"
#include "macros.h"
#include "printout.h"

/* References
*
* For windows:
* - VirtualAlloc: https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc?redirectedfrom=MSDN
*
* For Linux:
* - https://docs.kernel.org/arch/x86/x86_64/mm.html explains the virtual memory map, 
*   with all the ranges
* */




void* OS_MemoryReserve(u64 size) {
   void *memory;
#ifdef _WIN32
   memory = VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
   if (!memory) {
      DWORD dw = GetLastError();
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
      DWORD dw = GetLastError();
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
      DWORD dw = GetLastError();
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
      DWORD dw = GetLastError();
      error("FATAL ERROR: Could not release memory. Error code is %lu\n", dw);
      return Error_SystemError;
   }
#else
   SYS_CALL_RET(munmap(memory, size));

#endif

   return OK;
}

void *OS_CheapRealloc(void* memory, u64 oldsz, u64 newsz)
{
   void *new_memory;
#if defined(__APPLE__) /* We use mach_vm_remap */

   /* WARNING: we might have to use mach_vm_allocate
    *
    * kern_return_t kr = mach_vm_allocate(task, &addr, size, VM_FLAGS_ANYWHERE);
    *
    * See <mach/vm_statistics.h> for a partial documentation of the VM_FLAG_*
    *
    * https://github.com/joncampbell123/dosbox-x/blob/master/src/cpu/dynamic_alloc_common.h
    * https://github.com/apple-oss-distributions/dyld/blob/main/libdyld/dyld_process_info_notify.cpp
    *
    * TODO: unclear what the "don't copy" boolean means. Looking at darling, it
    * seems to copy the page content, and triggers MAP_PRIVATE. Otherwise,
    * MAP_SHARED is enabled, see
    * https://github.com/CuriousTommy/darling-newlkm/blob/master/osfmk/duct/duct_vm_user.c#L478
    *
    * If needed, we can use vm_prot_t cur_prot, max_prot; rather than NULL
    */

   mach_vm_address_t new_addr, old_addr = (mach_vm_address_t) memory;

   mach_port_t self = mach_task_self();
   kern_return_t kr = mach_vm_remap(self,
                                    &new_addr, oldsz,
                                    0,                         //alignment mask
                                    VM_FLAGS_OVERWRITE,
                                    self, old_addr,
                                    false,                     // Don't copy
                                    NULL, NULL,          // min/max protection
                                    VM_INHERIT_DEFAULT);

   if (kr != KERN_SUCCESS) {
      error("%s returned %d: %s", "mach_vm_remap", kr, mach_error_string(kr));
      mach_vm_deallocate(self, old_addr, oldsz);
      return NULL;
	}

   mach_vm_deallocate(self, old_addr, oldsz);

   new_memory = (void*)new_addr;

#elif defined (__linux__) /* we assume remap is available */

   /* mremap unmaps the old memory mapping, unless MREMAP_DONTUNMAP is passed */
   new_memory = mremap(memory, oldsz, newsz, MREMAP_MAYMOVE);

#else
   /* Windows may not have an equivalent. Look at there chromium code where
    *  MapViewOfFile / UnmapViewOfFile is used  */

   new_memory = realloc(memory, newsz);

#endif

   return new_memory;
}
