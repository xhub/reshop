#include <assert.h>

#include "ovfinfo.h"
#include "ovf_fn_helpers.h"
#include "ovf_parameter.h"
#include "printout.h"

const struct ovf_param*
ovf_find_param(const char *name, const struct ovf_param_list *plist)
{
   assert(plist);
   assert(name);

   for (size_t i = 0; i < plist->size; ++i) {
      if (!strcmp(plist->p[i].def->name, name)) {
         return &plist->p[i];
      }
   }

   error("%s :: could not find a parameter named %s.\n", __func__,
            name);
   return NULL;
}

const OvfParamDef*
ovfparamdef_find(const OvfParamDefList *plist, const char *kwnamestart,
                 unsigned kwnamelen, unsigned *pidx)
{
   assert(plist);
   assert(kwnamestart);

   for (unsigned i = 0, len = *plist->s; i < len; ++i) {
      if (!strncasecmp(plist->p[i]->name, kwnamestart, kwnamelen)) {
         *pidx = i;
         return plist->p[i];
      }
   }

   error("[ovfparam] Could not find a parameter named '%.*s'. Valid parameter "
         "names are: \n",
         kwnamelen, kwnamestart);

   for (unsigned i = 0, len = *plist->s; i < len; ++i) {
      error("%s\n", plist->p[i]->name);
   }

   *pidx = UINT_MAX;

   return NULL;
}
