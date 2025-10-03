#ifndef RESHOP_FS_FUNC_H
#define RESHOP_FS_FUNC_H

#include "compat.h"
#include <stdbool.h>

bool dir_exists(const char *dirname) NONNULL;
bool file_readable_silent(const char *fname) NONNULL;
int file_readable(const char *fname) NONNULL;
int new_unique_dirname(char *newdir, unsigned newdir_len) NONNULL;

#endif /* RESHOP_FS_FUNC_H */
