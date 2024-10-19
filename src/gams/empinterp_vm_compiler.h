#ifndef EMPPARSER_VM_COMPILER_H
#define EMPPARSER_VM_COMPILER_H

#include <stdbool.h>

#include "compat.h"
#include "empinterp.h"
#include "empinterp_vm.h"
#include "empinterp_fwd.h"

typedef struct {
   LIDX_TYPE iter_lidx;    /**< LocalVar index for the iterator            */
   LIDX_TYPE idx_lidx;     /**< LocalVar index for the iterator index      */
   LIDX_TYPE idxmax_lidx;  /**< localVar index for the max of the iterator index */
   GIDX_TYPE idxmax_gidx;  /**< Global index for the max of the iterator index */
   unsigned tapepos_at_loopstart;  /**< Position of the loop start in the code */
} IteratorData;

typedef struct {
   unsigned size;          /**< Number of true iterators */
   unsigned loopobj_gidx;  /**< global index for the object inducing the loop */
   EmpVmOpCode loopobj_opcode; /**< Update opcode for the loop object         */
   uint8_t iteridx2dim[GMS_MAX_INDEX_DIM]; /**< Iterators index to gams indices */
   IdentData idents[GMS_MAX_INDEX_DIM];
   IteratorData iters[GMS_MAX_INDEX_DIM];
} LoopIterators;

void empvm_compiler_free(struct empvm_compiler* c);
bool resolve_local(struct empvm_compiler* c,  struct ident_data *ident);
NONNULL MALLOC_ATTR(empvm_compiler_free,1) OWNERSHIP_RETURNS
struct empvm_compiler* empvm_compiler_init(Interpreter * restrict interp);
NONNULL void empvm_compiler_setgmd(Interpreter * restrict interp);

NONNULL
int parse_loop(struct interpreter * restrict interp, unsigned * restrict p);
int parse_sum(Interpreter * restrict interp, unsigned * restrict p) NONNULL;
int parse_defvar(Interpreter * restrict interp, unsigned * restrict p) NONNULL;

int loop_initandstart(Interpreter * restrict interp, Tape * restrict tape,
                      LoopIterators * restrict iterators) NONNULL;
int vm_labeldef_condition(Interpreter * interp, unsigned * restrict p,
                          const char *basename, unsigned basename_len,
                          GmsIndicesData* gmsindices) NONNULL;
int vm_labeldef_loop(Interpreter * interp, unsigned * restrict p,
                     const char *basename, unsigned basename_len,
                     GmsIndicesData* gmsindices) NONNULL;
int vm_nash(Interpreter * interp, unsigned * restrict p, const char* argname,
            unsigned argname_len, GmsIndicesData* gmsindices) NONNULL;

int vm_add_VFobjSimple_arc(Interpreter * interp, unsigned * restrict p, const char* argname,
            unsigned argname_len, GmsIndicesData* gmsindices) NONNULL;

int vm_add_Ctrl_edge(Interpreter * interp, unsigned * restrict p, const char* argname,
                     unsigned argname_len, GmsIndicesData* gmsindices) NONNULL;

int empvm_finalize(Interpreter *interp) NONNULL;

int c_switch_to_compmode(Interpreter *interp, bool *switched) NONNULL;
int c_switch_to_immmode(Interpreter *interp) NONNULL;

int hack_scalar2vmdata(Interpreter *interp, unsigned idx) NONNULL;

#endif // !EMPPARSER_VM_COMPILER_H


