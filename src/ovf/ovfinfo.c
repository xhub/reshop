#include <stdio.h>

#include "empinfo.h"
#include "macros.h"
#include "mdl.h"
#include "ovf_generator.h"
#include "ovf_options.h"
#include "printout.h"
#include "ovfinfo.h"
#include "ovf_functions.h"
#include "ovf_parameter.h"
#include "status.h"
#include "var.h"


const char * const ovf_synonyms[][2] = {
   /* Synonym, OVF name */
   { "expectedvalue", "expectation" },
   { "sumpospart", "sum_pos_part"  },
   { NULL, NULL },
};

const char * const ovf_always_compat[] = {
   "expectation",
   NULL,
};

#undef DECLARE_OVF_FUNCTION
#define DECLARE_OVF_FUNCTION(X) #X,
const char* const ovf_names[] = {
   DECLARE_OVF
};

const unsigned ovf_numbers = sizeof(ovf_names)/sizeof(char*);

#undef DECLARE_OVF_FUNCTION
#define DECLARE_OVF_FUNCTION(X) &OVF_##X##_datagen,

const struct ovf_genops* const ovf_datagen[] = {
   DECLARE_OVF
};


#undef DECLARE_OVF_FUNCTION
#define DECLARE_OVF_FUNCTION(X) {.p = OVF_##X##_params, .s = &OVF_##X##_params_len},

const struct ovf_param_def_list ovf_params[] = {
   DECLARE_OVF
};

#undef DECLARE_OVF_FUNCTION
#define DECLARE_OVF_FUNCTION(X) X##_sense,

const RhpSense ovf_sense[] = {
   DECLARE_OVF
};


int ovfinfo_alloc(EmpInfo *empinfo)
{
   struct ovfinfo *ovfinfo;
   CALLOC_(ovfinfo, struct ovfinfo, 1);
   empinfo->ovf = ovfinfo;
   ovfinfo->refcnt = 1;
   return OK;
}

void ovfinfo_dealloc(EmpInfo *empinfo)
{
   if (empinfo->ovf) {
      empinfo->ovf->refcnt--;
      if (empinfo->ovf->refcnt > 0) return;

      OvfDef *ovfdef = empinfo->ovf->ovf_def;
      while (ovfdef) {
          OvfDef *tmp = ovfdef->next;
          ovf_def_free(ovfdef);
          ovfdef = tmp;
      }
      empinfo->ovf->ovf_def = NULL;

      FREE(empinfo->ovf);
   }
}

struct ovfinfo *ovfinfo_borrow(struct ovfinfo *ovfinfo)
{
   if (ovfinfo->refcnt == UINT_MAX) {
      errormsg("[OVF] ERROR: OVFinfo has reach max value of refcnt\n");
      return NULL;
   }

   ovfinfo->refcnt++;
   return ovfinfo;
}

static int ovf_def_fill(OvfDef *ovfdef, unsigned ovf_idx)
{
   ovfdef->ovf_vidx = IdxNA;

   S_CHECK(ovf_fill_params(&ovfdef->params, ovf_idx));

   ovfdef->idx = ovf_idx;

   ovfdef->generator = ovf_datagen[ovf_idx];
   ovfdef->sense = ovf_sense[ovf_idx];
   ovfdef->reformulation = OVF_Scheme_Unset;
   A_CHECK(ovfdef->args, avar_newblock(2));
   ovfdef->eis = NULL;
   ovfdef->coeffs = NULL;
   ovfdef->num_empdag_children = 0;
   ovfdef->name = NULL;
   ovfdef->status = OvfNoStatus;

   return OK;
}

OvfDef* ovf_def_new_ovfinfo(struct ovfinfo *ovfinfo, unsigned ovf_idx)
{
   OvfDef* ovfdef;

   if (!ovfinfo->ovf_def) {
      CALLOC_NULL(ovfinfo->ovf_def, OvfDef, 1);
      ovfdef = ovfinfo->ovf_def;
   } else {
      OvfDef * qdef = ovfinfo->ovf_def;
      while(qdef->next) { qdef = qdef->next; }

      CALLOC_NULL(qdef->next, OvfDef, 1);
      ovfdef = qdef->next;
   }

   ovfinfo->num_ovf++;

   SN_CHECK(ovf_def_fill(ovfdef, ovf_idx));

   return ovfdef;

}

OvfDef* ovf_def_new(unsigned ovf_idx)
{
   OvfDef* ovfdef;
   CALLOC_NULL(ovfdef, OvfDef, 1);

   SN_CHECK_EXIT(ovf_def_fill(ovfdef, ovf_idx));

   return ovfdef;

_exit:
   FREE(ovfdef);
   return NULL;
}

void ovf_def_free(OvfDef* restrict ovfdef)
{
   for (unsigned i = 0, len = ovfdef->params.size; i < len; ++i) {
      ovf_param_dealloc(&ovfdef->params.p[i]);
   }

   FREE(ovfdef->params.p);
   avar_free(ovfdef->args);
   FREE(ovfdef->eis);
   FREE(ovfdef->coeffs);
   FREE(ovfdef->name);
   FREE(ovfdef);
}

const char* ovf_getname(const OvfDef *ovf)
{
   return ovf->name ? ovf->name : ovf->generator->name;
}

const OvfParamDefList* ovf_getparamdefs(unsigned ovf_idx)
{
   if (ovf_idx >= ovf_numbers) {
      error("%s :: OVF index %u >= %u, the number of registered OVFs\n",
            __func__, ovf_idx, ovf_numbers);
      return NULL;
   }

   return &ovf_params[ovf_idx];
}

void ovf_print_usage(void)
{
   puts("OVF annotation usage\n");
   puts("OVF f rho OVFname [params]\n");
   puts("f: (MANDATORY) function with image y\n");
   puts("rho: (MANDATORY) value of the objective function of the OVF function, used in the principal optimization problem\n");
   puts("OVFname: (MANDATORY) name of the OVF function\n");
   puts("params: list of parameters (number and meaning different for each one)\n");
   puts("\n");
   puts("list of supported OVF function:");
   for (size_t i = 0; i < sizeof(ovf_names)/sizeof(char*); ++i) {
      printout(PO_INFO, "%s ", ovf_names[i]);
   }
   puts("\n");
}

void ovf_def_print(const OvfDef *ovf, unsigned mode,
                   const Model *mdl)
{
   printout(mode, "[OVF] %5d: function '%s'\n", ovf->idx, ovf_getname(ovf));
   if (valid_vi(ovf->ovf_vidx)) {
      printout(mode, " ** OVF var: #[%5u]  %s\n", ovf->ovf_vidx,
               ctr_printvarname(&mdl->ctr, ovf->ovf_vidx));
   }

   unsigned nargs_vars = avar_size(ovf->args);
   printout(mode, " ** Number of variable arguments: %5u\n", nargs_vars);
   for (unsigned i = 0; i < nargs_vars; ++i) {
      rhp_idx vi = avar_fget(ovf->args, i);
      printout(mode, "\t#[%5u]  %s\n", vi, ctr_printvarname(&mdl->ctr, vi));
   }

   unsigned nargs_empdag = ovf->num_empdag_children;
   printout(mode, " ** Number of VF arguments: %5u\n", nargs_empdag);

   printout(mode, " ** Number of parameters: %5u\n", ovf->params.size);

   for (unsigned i = 0; i < ovf->params.size; ++i) {
      const OvfParam *p = &ovf->params.p[i];
      printout(mode, "\tParameter '%s' of type %s", p->def->name, ovf_argtype_str(p->type));

      switch (p->type) {
         case ARG_TYPE_SCALAR:
            printout(mode, "\n\tValue: %e\n", p->val);
            break;
         case ARG_TYPE_VEC:
            {
               printstr(mode, "\n\tValue:");
               for (unsigned j = 0; j < p->size_vector; ++j) {
                  printout(mode, " %e", p->vec[j]);
               }
               printstr(mode, "\n");
               break;
            }
         case ARG_TYPE_MAT:
            printstr(mode, "\tMatrix printing is not enabled\n");
            break;
         case ARG_TYPE_UNSET:
            printstr(mode, "\n");
            break;
         case ARG_TYPE_VAR:
         case ARG_TYPE_EQU:
            printstr(mode, "\tUnsupported\n");
            break;
         default:
            printout(mode, "Error, unsupported type %d\n", p->type);
      }
   }
}

void ovf_print(const struct ovfinfo *ovf_info, const Model *mdl)
{
   printout(PO_INFO, " ** Information about OVF annotation\n");
   bool has_ovf = false;
   if (ovf_info->ovf_def) {
      has_ovf = true;
      OvfDef *ovf = ovf_info->ovf_def;
      while (ovf) {

         ovf_def_print(ovf, PO_INFO, mdl);
         ovf = ovf->next;
      }

   }

   if (!has_ovf) {
      printout(PO_INFO, " ** No OVF information available\n");
   }
}

const char *ovf_argtype_str(OvfArgType type)
{

   const char* const arg_type_name[] = {
      "unset",
      "scalar",
      "vector",
      "matrix",
      "variable",
      "equation",
      [ARG_TYPE_NESTED] = "nested structure",
   };

   if (type < __ARG_TYPE_SIZE) {
      return arg_type_name[type];
   }

   return "arg type out of bounds!";
}
