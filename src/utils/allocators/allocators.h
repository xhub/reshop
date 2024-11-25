/* SPDX-License-Identifier: MIT */
/* This code started from https://github.com/PixelRifts/c-codebase */

#ifndef RESHOP_ALLOCATORS_H
#define RESHOP_ALLOCATORS_H

#include <stdlib.h>
#include <stdbool.h>


#include "rhp_defines.h"
#include "rhp_fwd.h"
#include "rhp_compiler_defines.h"

//~ Arena (Linear Allocator)

/* Arena object */
typedef struct M_Arena {
   u8* memory;
   u64 max;               /**< length of memory pointer */
   u64 reserved_size;     /**< Reserved virtual memory */
   u64 allocated_size;    /**< Allocated size */
   u64 committed_size;    /**< Committed size */
   u8 alignement;         /**< Memory alignement */
   bool static_size;
} M_Arena;

void* arena_alloc(M_Arena* arena, u64 size);
void* arena_alloc_zero(M_Arena* arena, u64 size);
int arena_alloc_blocks(M_Arena *arena, unsigned num_blocks, void *blocks[VMT(static num_blocks)],
                       u64 sizes[VMT(static num_blocks)]) NONNULL;

void  arena_dealloc(M_Arena* arena, u64 size);
void  arena_dealloc_to(M_Arena* arena, u64 pos);
void* arena_raise(M_Arena* arena, void* ptr, u64 size);
void* arena_alloc_array_sized(M_Arena* arena, u64 elem_size, u64 count);

#define arena_alloc_array(arena, elem_type, count) \
arena_alloc_array_sized(arena, sizeof(elem_type), count)

int arena_free(M_Arena* arena);
int arena_init(M_Arena* arena);
int arena_init_sized(M_Arena* arena, u64 max);
M_Arena* arena_create(u64 max) MALLOC_ATTR(arena_free, 1);
void arena_clear(M_Arena* arena);

/** Arena for temporary objects / workspaces
 *
 * For this usage, we don't want to reallocate / change the virtual memory mappings
 * as pointers are used in the code and that would invalidate them. Hence, we
 * allocate a new arena if we run out of space.
 */
typedef struct M_ArenaLink {
   M_Arena arena;   /**< Main arena backing this object                     */
   M_ArenaLink *next;   /**< Next Arena, used in case we run out of memory here */
} M_ArenaLink;

/** Struct with ArenaTemp and rewind position */
typedef struct M_ArenaTempStamp {
   M_ArenaLink* arena;  /**< Arena for temporary objects                    */
   u64 pos_rewind;      /**< position to rewind to                          */
} M_ArenaTempStamp;


int              arenalink_free(M_ArenaLink *arena);
int              arenalink_init_sized(M_ArenaLink *arena, u64 size);
M_ArenaLink*     arenalink_create(u64 max) MALLOC_ATTR(arenalink_free, 1);
int              arenalink_alloc_blocks(M_ArenaLink *arenaL, unsigned num_blocks,
                                        void *blocks[VMT(static num_blocks)],
                                        u64 sizes[VMT(static num_blocks)]) NONNULL;
M_ArenaTempStamp arenalink_begin(M_ArenaLink* arena);
void             arenalink_end(M_ArenaTempStamp stamp);
int              arenalink_empty(M_ArenaLink *arena) NONNULL;


//~ Scratch Helpers
// A scratch block is just a view into an arena
/* TODO port to C11 threading
#include "tctx.h"

M_Scratch scratch_get(void);
void scratch_reset(M_Scratch* scratch);
void scratch_return(M_Scratch* scratch);
//~ Pool (Pool Allocator)

typedef struct M_PoolFreeNode M_PoolFreeNode;
struct M_PoolFreeNode { M_PoolFreeNode* next; };

typedef struct M_Pool {
   u8* memory;
   u64 max;
   u64 commit_position;
   u64 element_size;

   M_PoolFreeNode* head;
} M_Pool;

#define M_POOL_MAX Gigabytes(1)
#define M_POOL_COMMIT_CHUNK 32

void pool_init(M_Pool* pool, u64 element_size);
void pool_clear(M_Pool* pool);
void pool_free(M_Pool* pool);

void* pool_alloc(M_Pool* pool);
void  pool_dealloc(M_Pool* pool, void* ptr);
void  pool_dealloc_range(M_Pool* pool, void* ptr, u64 count);
*/

#endif //RESHOP_ALLOCATORS_H
