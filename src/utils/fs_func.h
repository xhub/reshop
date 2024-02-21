#ifndef RESHOP_FS_FUNC_H
#define RESHOP_FS_FUNC_H

#include "compat.h"
#include <stdbool.h>

bool dir_exists(const char *dirname) NONNULL;
int new_unique_dirname(char *newdir, unsigned newdir_len) NONNULL;

#endif /* RESHOP_FS_FUNC_H */
