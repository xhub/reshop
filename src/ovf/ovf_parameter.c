#include <stdio.h>

#include "macros.h"
#include "printout.h"
#include "ovf_parameter.h"
#include "status.h"



int ovf_fill_params(OvfParamList * params, size_t ovf_idx)
{
   const OvfParamDefList* paramdefs;
   A_CHECK(paramdefs, ovf_getparamdefs(ovf_idx));

   unsigned param_size = *paramdefs->s;
   const OvfParamDef * const * p = paramdefs->p;

   params->size = param_size;
   if (param_size > 0) {
      /* CALLOC serves as initialization */
      CALLOC_(params->p, OvfParam, param_size);

      for (unsigned i = 0; i < param_size; ++i) {
         params->p[i].def = p[i];
      }
   } else {
      params->p = NULL;
   }

   return OK;
}

void ovf_param_print(const struct ovf_param *p, unsigned mode)
{
   if (!p->def) {
      error("%s :: invalid parameter with no definition\n", __func__);
      return;
   }

   printout(mode, "Parameter named %s of type %d\n", p->def->name, p->type);
   switch (p->type) {
   case ARG_TYPE_UNSET:
      printout(mode, "Parameter is unset\n");
      break;

   case ARG_TYPE_SCALAR:
      printout(mode, "%e\n", p->val);
      break;

   case ARG_TYPE_VEC:
   {
      for (unsigned i = 0; i < p->size_vector; ++i) {
         printout(mode, "%e\n", p->vec[i]);
      }
      break;
   }
   default:
      printout(mode, "Printing not yet supported\n");
   }
}

void ovf_param_dealloc(OvfParam *p)
{
   if (!p) { return; }

   switch (p->type) {
   case ARG_TYPE_VEC: FREE(p->vec); return;
   case ARG_TYPE_UNSET: case ARG_TYPE_SCALAR: return;
   default: assert(0 && "unsupported case");
   }
}

size_t ovf_same_as_argsize(size_t argsize)
{
   return argsize;
}

