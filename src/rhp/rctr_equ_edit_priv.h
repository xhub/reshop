#ifndef RCTR_EQU_EDIT_PRIV_H
#define RCTR_EQU_EDIT_PRIV_H

/** @file rctr_equ_edit_priv.h
*
*   @brief Private container functions. Do no use these directly
*/

#include "ctr_rhp.h"
#include "equvar_helpers.h"
#include "rhp_fwd.h"

int rctr_equ_scal(Container *ctr, Equ *e, double coeff) NONNULL;

static inline NONNULL int rctr_equ_is_readonly(const Container *ctr, rhp_idx ei)
{
   assert(ctr_is_rhp(ctr)); assert(valid_ei_(ei, rctr_totalm(ctr), __func__));

   RhpContainerData *cdat = (RhpContainerData *)ctr->data;
   return cdat->current_stage != cdat->equ_stage[ei];
}

#endif
