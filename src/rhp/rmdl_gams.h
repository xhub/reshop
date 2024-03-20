#ifndef RMDL_GAMS_H
#define RMDL_GAMS_H

#include "rhp_fwd.h"

int rmdl_compress_gams_post(Container *ctr, Container *ctr_gms) NONNULL;
int rmdl_copysolveoptions_gams(Model *ctr, const Model *mdl_gms) NONNULL;
int rctr_reporvalues_from_gams(Container *ctr, const Container *ctr_gms) NONNULL;
int rmdl_exportasgmo(Model* mdl, Model *mdl_gms);

#endif /* RMDL_GAMS_H  */
