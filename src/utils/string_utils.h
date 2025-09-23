#include <limits.h>
#include <string.h>

// for strcasecmp on windows
#include "compat.h"

static inline unsigned _findnamecase(const char ** list, unsigned len, const char* name)
{
  for (unsigned i = 0; i < len; ++i) {
    if (!strcasecmp(name, list[i])) {
      return i;
    }
  }
  return UINT_MAX;
}

static inline unsigned _findname(const char ** list, unsigned len, const char* name)
{
  for (unsigned i = 0; i < len; ++i) {
    if (!strcmp(name, list[i])) {
      return i;
    }
  }
  return UINT_MAX;
}

/* Fast toupper */
static inline char RhpToUpper(char c) { return (c >= 'a' && c <= 'z') ? c & ~0x20 : c; }
static inline char RhpToLower(char c) { return (c >= 'A' && c <= 'Z') ? c | 0x20 : c; }

