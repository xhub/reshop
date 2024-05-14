#ifndef GAMS_EMPINFOFILE_READER_H
#define GAMS_EMPINFOFILE_READER_H

#include "rhp_fwd.h"


NONNULL_AT(1,2)
int empinterp_process(Model *mdl, const char *empinfo_fname, const char *gmd_fname);

#endif /* GAMS_EMPINFOFILE_READER_H */
