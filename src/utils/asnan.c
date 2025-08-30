#include "reshop_config.h"

#include "asnan.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "asnan_pub.h"
#include "compat.h"
#include "macros.h"
#include "printout.h"
#include "tlsdef.h"

struct bck {
   const char *func;
   unsigned len;
   char **symbols;
};

static tlsvar struct bck *snan_funcs = NULL;
static tlsvar size_t snan_funcs_len = 0;

static tlsvar char snan_str[16];

#if (defined(__linux__) || defined(__APPLE__))
#define _has_backtrace
#include <execinfo.h>
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
#include "win-compat.h"
#endif

#ifndef CLEANUP_FNS_HAVE_DECL
static
#endif
DESTRUCTOR_ATTR void cleanup_snans_funcs(void)
{
   size_t i = 0;
   while (i < snan_funcs_len && snan_funcs[i].func) {
      FREE(snan_funcs[i].symbols);
      i++;
   }

   FREE(snan_funcs);
   snan_funcs_len = 0;
}

const char *nansstr(const char *func, UNUSED unsigned char code)
{
   unsigned i = 0;
   while (i < snan_funcs_len && snan_funcs[i].func) {
      i++;
   }

   if (i == snan_funcs_len) {
      snan_funcs_len += 5;
      REALLOC_NULL(snan_funcs, struct bck, snan_funcs_len);
      snan_funcs[snan_funcs_len-1].symbols = NULL;
      snan_funcs[snan_funcs_len-2].symbols = NULL;
      snan_funcs[snan_funcs_len-3].symbols = NULL;
      snan_funcs[snan_funcs_len-4].symbols = NULL;
      snan_funcs[snan_funcs_len-5].symbols = NULL;

      snan_funcs[snan_funcs_len-1].func = NULL;
      snan_funcs[snan_funcs_len-2].func = NULL;
      snan_funcs[snan_funcs_len-3].func = NULL;
      snan_funcs[snan_funcs_len-4].func = NULL;
      snan_funcs[snan_funcs_len-5].func = NULL;
   }

   snan_funcs_len++;

   struct bck *bck = &snan_funcs[i];

   bck->func = strdup(func);

#ifdef _has_backtrace
   void *array[20];
   int size = sizeof array / sizeof array[0];

   size = backtrace(array, size);
   bck->symbols = backtrace_symbols(array, size);
#else
   bck->symbols = NULL;
#endif

   snprintf(snan_str, sizeof(snan_str), "%u", i);

   return snan_str;
}

#ifdef UNUSED_ON_20240320

static void find_snan(double snan)
{
   union u { double d; unsigned i[2]; };
   union u number;
   number.d = snan;

   /* ----------------------------------------------------------------------
    * Endianness issue here
    * ---------------------------------------------------------------------- */

   assert(isnan(snan));

}
#endif

double rhp_asnan(unsigned code)
{
   return MKSNAN(code);
}
