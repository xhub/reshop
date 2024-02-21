#ifndef OVF_FN_HELPERS_H
#define OVF_FN_HELPERS_H

#include "compat.h"
#include "macros.h"

struct ovf_param_list;
struct ovf_param_def_list;

CHECK_RESULT const struct ovf_param*
ovf_find_param(const char *name, const struct ovf_param_list *plist);
CHECK_RESULT const struct ovf_param_def*
ovfparamdef_find(const struct ovf_param_def_list *plist, const char *kwnamestart,
                 unsigned kwnamelen, unsigned *pidx);

#define OVF_CHECK_PARAM(POINTER, VAL) \
  if (!POINTER) { error("%s :: parameter not found!", __func__); \
    return VAL; }

#endif /* OVF_FN_HELPERS_H  */
