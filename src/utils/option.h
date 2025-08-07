#ifndef OPTION_H
#define OPTION_H

#include <stdbool.h>

#include "compat.h"
#include "rhp_fwd.h"

enum option_setid {
   OptSet_Main = 0,
   OptSet_OVF,
   OptSet_Last = OptSet_OVF,
};

/** Option type */
typedef enum option_type {
   OptBoolean = 0,            /**< Boolean option  */
   OptChoice,                 /**< Choice option   */
   OptDouble,                 /**< Double option   */
   OptInteger,                /**< Integer option  */
   OptString,                 /**< String option   */
   OptType_Last = OptString,
} OptType;

struct option {
   const char *name;
   const char *description;
   OptType type;

   union {
      double d;
      int i;
      bool b;
      const char *s;
   } value;
};

typedef struct option_set {
   unsigned char id;
   bool alloced;
   unsigned numopts;

   struct option *opts;
} OptSet;

typedef struct option_list {
   unsigned len;
   OptSet **optsets;
} OptList;

typedef struct {
   unsigned setid;
   unsigned optnum;
} OptIterator;

int optset_add(OptList *optlist, OptSet *optset) NONNULL;
int optset_syncenv(OptSet *optset) NONNULL;

int optint_getrange(struct option *opt, int *min, int *max) NONNULL;

int optchoice_getopts(struct option *opt, const char *const (**strs)[2], unsigned *len) NONNULL;
const char* optchoice_getdefaultstr(struct option *opt) NONNULL;
int optchoice_set(struct option *opt, const char *optval) NONNULL;

bool opt_find(const char *name, struct option_set **ret_optset, unsigned *index) NONNULL;
int opt_setfromstr(struct option *opt, const char *str) NONNULL;

struct option *opt_iter(OptIterator *iter) NONNULL;

const char *opttype_name(OptType type);

#endif /* OPTION_H */
