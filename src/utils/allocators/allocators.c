/* SPDX-License-Identifier: MIT */
/* This code started from https://github.com/PixelRifts/c-codebase */

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>

// Dependency on the OS. Although it is generic
#include "allocators.h"
#include "allocators-os.h"
#include "macros.h"
#include "printout.h"
#include "tlsdef.h"

#define M_ARENA_MAX Gigabytes(1)
#define M_ARENA_COMMIT_SIZE Kilobytes(8)

tlsvar u64 pagesize = M_ARENA_COMMIT_SIZE;

#ifdef HAS_UNISTD
#include <unistd.h>
#endif

#ifdef _SC_PAGESIZE
static CONSTRUCTOR_ATTR void get_pagesize(void)
{
   pagesize = (u64)sysconf(_SC_PAGESIZE);
}
#endif

// 16 is needed for SSE instructions
// For performance, AVX2 requires 32, AVX 512 requires 64
#define DEFAULT_ALIGNMENT MAX(sizeof(void*), 16)



DBGUSED static bool is_power_of_two(uintptr_t x) {
   return (x & (x-1)) == 0;
}

static u64 align_forward_u64(u64 ptr, u64 align)
{
   u64 p, a, modulo;

   assert(is_power_of_two(align));

   p = ptr;
   a = (size_t)align;
   // Same as (p % a) but faster as 'a' is a power of two
   modulo = p & (a-1);

   if (modulo != 0) {
      // If 'p' address is not aligned, push the address to the
      // next value which is aligned
      p += a - modulo;
   }
   return p;
}


//~ Arena

void* arena_alloc(M_Arena* arena, u64 size) {
   void* memory = 0;

   // align!
   u8 alignement = arena->alignement;
   size = align_forward_u64(size, alignement);

   if (arena->allocated_size + size > arena->committed_size) {
      if (arena->static_size) {
         assert(0 && "Static-Size Arena is out of memory");
         return NULL;
      }
       u64 commit_size = size;

      assert(pagesize > 0);
      commit_size += pagesize - 1;
      commit_size -= commit_size % pagesize;

       if (arena->committed_size >= arena->max) {
          return NULL;
       }

       OS_MemoryCommit(arena->memory + arena->committed_size, commit_size);
       MEMORY_POISON(arena->memory + arena->committed_size, commit_size);
       arena->committed_size += commit_size;
   }

   memory = arena->memory + arena->allocated_size;
   arena->allocated_size += size;

   MEMORY_UNPOISON(memory, size);
   MEMORY_UNDEF(memory, size);

   return memory;
}

int arena_alloc_blocks(M_Arena *arena, unsigned num_blocks,
                       void *blocks[VMT(static num_blocks)],
                       u64 sizes[VMT(static num_blocks)])
{
   assert(num_blocks > 1);
   u64 required_size = 0;
   u8 alignement = arena->alignement;

   for (unsigned i = 0; i < num_blocks; ++i) {
      required_size += align_forward_u64(sizes[i], alignement);
   }

   if (required_size == 0) {
      assert(0 && "alloc of size 0");
      errormsg("[arena] FATAL ERROR: allocation of size 0\n");
      return Error_RuntimeError;
   }

   u8 *mem;
   A_CHECK(mem, arena_alloc(arena, required_size));

   for (unsigned i = 0; i < num_blocks; ++i) {
      blocks[i] = mem;
      mem += align_forward_u64(sizes[i], alignement);
   }

   return OK;
}

void* arena_alloc_zero(M_Arena* arena, u64 size) {
   void* result = arena_alloc(arena, size);
   memset(result, 0, size);
   return result;
}

void arena_dealloc(M_Arena* arena, u64 size) {
   if (size > arena->allocated_size) {
      size = arena->allocated_size;
   }

   arena->allocated_size -= size;
   MEMORY_POISON(arena->memory + arena->allocated_size, size);
}

void arena_dealloc_to(M_Arena* arena, u64 pos)
{
   assert(pos > arena->allocated_size);
   if (pos > arena->max) { assert(0 && "wrong position"); pos = arena->max; }
   MEMORY_POISON(arena->memory + pos, arena->allocated_size - pos);
   arena->allocated_size = pos;
}

void* arena_raise(M_Arena* arena, void* ptr, u64 size)
{
   void* raised = arena_alloc(arena, size);
   memcpy(raised, ptr, size);
   return raised;
}

void* arena_alloc_array_sized(M_Arena* arena, u64 elem_size, u64 count)
{
   return arena_alloc(arena, elem_size * count);
}

int arena_init(M_Arena* arena)
{
   memset(arena, 0, sizeof(M_Arena));
   arena->max = arena->reserved_size = M_ARENA_MAX;
   arena->memory = OS_MemoryReserve(arena->max);
   arena->static_size = false;
   arena->alignement = DEFAULT_ALIGNMENT;

   return arena->memory ? OK : Error_SystemError;
}

int arena_init_sized(M_Arena* arena, u64 max)
{
   memset(arena, 0, sizeof(M_Arena));
   arena->max = arena->reserved_size = max;
   arena->memory = OS_MemoryReserve(arena->max);
   arena->static_size = false;
   arena->alignement = DEFAULT_ALIGNMENT;

   return arena->memory ? OK : Error_SystemError;
}

M_Arena* arena_create(u64 max)
{
   u16 offset = align_forward_u64(sizeof(M_Arena), DEFAULT_ALIGNMENT);
   u64 reserved_size = max+offset;
   void *mem = OS_MemoryReserve(reserved_size);
   if (!mem) {
      return NULL;
   }

   M_Arena *arena = (M_Arena*)mem;
   memset(arena, 0, sizeof(M_Arena));
   arena->memory = ((u8*)mem) + offset;
   arena->reserved_size = reserved_size;

   return arena;
}

void arena_clear(M_Arena* arena)
{
   arena_dealloc(arena, arena->allocated_size);
}

int arena_free(M_Arena* arena)
{
   if (!arena->memory) { return OK; }

   void *mem = arena->memory;
   u64 size = arena->reserved_size;
   /* The arena itself could be part of the memory ... */
   memset(arena, 0, sizeof(M_Arena));
   int rc = OS_MemoryRelease(mem, size);

   assert(rc == OK);

   return rc;
}

//~ Temp arena

int arenalink_init_sized(M_ArenaLink *arena, u64 size)
{
   arena->next = NULL;
   return arena_init_sized(&arena->arena, size);
}

M_ArenaLink* arenalink_create(u64 max)
{
   u16 offset = align_forward_u64(sizeof(M_ArenaLink), DEFAULT_ALIGNMENT);
   u64 reserved_size = max+offset;
   void *mem = OS_MemoryReserve(reserved_size);
   if (!mem) {
      return NULL;
   }

   M_ArenaLink *arena = (M_ArenaLink*)mem;
   memset(&arena->arena, 0, sizeof(M_ArenaLink));
   arena->arena.memory = ((u8*)mem) + offset;
   arena->arena.reserved_size = reserved_size;

   return arena;
}

int arenalink_alloc_blocks(M_ArenaLink *arenaL, unsigned num_blocks,
                           void *blocks[VMT(static num_blocks)],
                           u64 sizes[VMT(static num_blocks)])
{
   while (arenaL->next) { arenaL = arenaL->next; }

   return arena_alloc_blocks(&arenaL->arena, num_blocks, blocks, sizes);
}

int arenalink_free(M_ArenaLink *arenaL)
{
   if (!arenaL) { return OK; }

   M_ArenaLink *next = arenaL->next;
   if (next) {
      arenalink_free(next);
   }

   return arena_free(&arenaL->arena);
}

int  arenalink_empty(M_ArenaLink *arenaL)
{
   return arenalink_free(arenaL);
}

M_ArenaTempStamp arenalink_begin(M_ArenaLink* arenatemp)
{
   return (M_ArenaTempStamp) { arenatemp, arenatemp->arena.allocated_size };
}

void arenalink_end(M_ArenaTempStamp stamp)
{
   arena_dealloc_to(&stamp.arena->arena, stamp.pos_rewind);

   M_ArenaLink *next = stamp.arena->next;
   if (stamp.arena->next) {
      arenalink_free(next);
   }
}

//~ Scratch Blocks
/* 
M_Scratch scratch_get(void) {
   ThreadContext* ctx = (ThreadContext*) OS_ThreadContextGet();
   return tctx_scratch_get(ctx);
}

void scratch_reset(M_Scratch* scratch) {
   ThreadContext* ctx = (ThreadContext*) OS_ThreadContextGet();
   tctx_scratch_reset(ctx, scratch);
}

void scratch_return(M_Scratch* scratch) {
   ThreadContext* ctx = (ThreadContext*) OS_ThreadContextGet();
   tctx_scratch_return(ctx, scratch);
}
//~ Pool

void pool_init(M_Pool* pool, u64 element_size)
{
   memset(pool, 0, sizeof(M_Pool));
   pool->memory = OS_MemoryReserve(M_POOL_MAX);
   pool->max = M_POOL_MAX;
   pool->element_size = align_forward_u64(element_size, DEFAULT_ALIGNMENT);
}

void pool_clear(M_Pool* pool)
{
   // WARNING: review this case
   for (M_PoolFreeNode* it = (M_PoolFreeNode*)pool->memory, *preit = it;
   it <= (M_PoolFreeNode*)pool->memory + pool->commit_position;
   preit = it, it += pool->element_size) {
      preit->next = it;
   }
   pool->head = (M_PoolFreeNode*)pool->memory;
}

void pool_free(M_Pool* pool)
{
   if (pool->memory) { return; }
   OS_MemoryRelease(pool->memory, pool->max);
   memset(pool, 0, sizeof(M_Pool));
}

void* pool_alloc(M_Pool* pool)
{
   if (pool->head) {
      void* ret = pool->head;
      pool->head = pool->head->next;
      return ret;
   }

   if (pool->commit_position + M_POOL_COMMIT_CHUNK * pool->element_size >= pool->max) {
      assert(0 && "Pool is out of memory");
      return NULL;
   }
   void* commit_ptr = pool->memory + pool->commit_position;
   OS_MemoryCommit(commit_ptr, M_POOL_COMMIT_CHUNK * pool->element_size);
   pool_dealloc_range(pool, commit_ptr, M_POOL_COMMIT_CHUNK);

   return pool_alloc(pool);
}

void pool_dealloc(M_Pool* pool, void* ptr)
{
   ((M_PoolFreeNode*)ptr)->next = pool->head;
   pool->head = ptr;
}

void pool_dealloc_range(M_Pool* pool, void* ptr, u64 count)
{
   u8* it = ptr;
   for (u64 k = 0; k < count; k++) {
      ((M_PoolFreeNode*)it)->next = pool->head;
      pool->head = (M_PoolFreeNode*) it;
      it += pool->element_size;
   }
}

*/
