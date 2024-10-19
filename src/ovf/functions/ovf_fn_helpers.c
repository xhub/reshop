#include <assert.h>

#include "ovfinfo.h"
#include "ovf_fn_helpers.h"
#include "ovf_parameter.h"
#include "printout.h"

const char * const ovfparam_synonyms[][2] = {
   /* Synonym, OVF name */
   { "risk_weight", "risk_wt" },
   { NULL, NULL },
};

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

static const char * ovfparam_synonyms_str(const char *name, unsigned len)
{
   size_t i = 0;
   while (ovfparam_synonyms[i][0]) {
      if (!strncasecmp(name, ovfparam_synonyms[i][0], len)) {
         return ovfparam_synonyms[i][1];
      }
      ++i;
   }

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

   const char *kwname = ovfparam_synonyms_str(kwnamestart, kwnamelen);

   if (kwname) {
      unsigned kwlen = strlen(kwname);
      for (unsigned i = 0, len = *plist->s; i < len; ++i) {
         if (!strncasecmp(plist->p[i]->name, kwname, kwlen)) {
            *pidx = i;
            return plist->p[i];
         }
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
