#ifndef EMBCODE_EMPINFO_H
#define EMBCODE_EMPINFO_H

#include "compat.h"

#define EMBCODE_DIR "embrhp"
#define EMBCODE_GMDOUT_FNAME "gmd_gdx.dat"

int embcode_process_empinfo(void *gmd, const char *scrdir, const char *fname) NONNULL;

#endif
