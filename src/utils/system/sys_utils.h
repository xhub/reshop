#ifndef RHP_SYS_UTILS
#define RHP_SYS_UTILS

#include "macros.h"

void trim_newline(char *str, size_t len) NONNULL;
const char* exe_getfullpath(const char *executable) NONNULL;

#endif
