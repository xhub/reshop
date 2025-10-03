#ifndef EMBCODE_EMPINFO_H
#define EMBCODE_EMPINFO_H

#include "compat.h"

#define EMBCODE_GMDOUT_FNAME_NOEXT "embrhp_gdx"
#define EMBCODE_GMDOUT_FNAME       EMBCODE_GMDOUT_FNAME_NOEXT ".dat"

int embcode_process_empinfo(void *gmdobj, const char *scrdir, const char *fname) NONNULL;

#endif
