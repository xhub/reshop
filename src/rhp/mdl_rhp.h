#ifndef MODEL_RMDL_H
#define MODEL_RMDL_H

#include "mdl_data.h"
#include "rhp_fwd.h"

/** @file mdl_rhp.h
 *
 * @brief ReSHOP model functions
 */

int rmdl_checkobjequvar(const Model *mdl, rhp_idx objvar, rhp_idx objequ) NONNULL;

int rmdl_dup_equ(Model *mdl, rhp_idx *eidx, unsigned lin_space, rhp_idx vi_no) NONNULL;
int rmdl_equ_flip(Model *mdl, rhp_idx ei, rhp_idx *ei_new) NONNULL;
int rmdl_equ_rm(Model* mdl, rhp_idx eidx) NONNULL;
int rmdl_appendequs(Model *mdl, const Aequ *e) NONNULL;

int rmdl_getobjequ(const Model *mdl, rhp_idx *objequ) NONNULL;
int rmdl_getobjvar(const Model *mdl, rhp_idx *objvar) NONNULL;
int rmdl_getsense(const Model *mdl, RhpSense *objsense) NONNULL;
int rmdl_setobjfun(Model *mdl, rhp_idx ei) NONNULL;
int rmdl_setobjvar(Model *mdl, rhp_idx objvar) NONNULL;
int rmdl_setobjsense(Model *mdl, RhpSense objsense) NONNULL;

int rmdl_set_simpleprob(Model *mdl, const MpDescr *descr) NONNULL;

int rmdl_initfromfullmdl(Model *mdl, Model *mdl_up) NONNULL;
int rmdl_fix_objequ_value(Model *mdl);
int rmdl_remove_fixedvars(Model *mdl) NONNULL;

int rmdl_incstage(Model *mdl) NONNULL;
Fops* rmdl_getfops(const Model *mdl) NONNULL;

int rmdl_export_latex(Model *mdl, const char *phase_name) NONNULL;
int rmdl_ensurefops_activedefault(Model *mdl) NONNULL;
int rmdl_prepare_export(Model * restrict mdl_src, Model * restrict mdl_dst) NONNULL;

#endif /* MODEL_RMDL_H */
