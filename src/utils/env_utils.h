#ifndef ENV_UTILS_H
#define ENV_UTILS_H

#include "compat.h"

const char *find_rhpenvvar(const char* optname, char **env_varname, size_t *optname_maxlen) NONNULL;

#endif
