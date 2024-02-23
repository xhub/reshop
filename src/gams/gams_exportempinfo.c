#include "reshop_config.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "container.h"
#include "ctrdat_gams.h"
#include "empinfo.h"
#include "equvar_metadata.h"
#include "fs_func.h"
#include "gams_exportempinfo.h"
#include "gams_utils.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "printout.h"

#include "gevmcc.h"
#include "gmomcc.h"
#include "status.h"

#define GET_PROPER_COLNAME(ctr, vidx, buf, bufout)                             \
  S_CHECK(ctr_copyvarname(ctr, vidx, buf, sizeof(buf)));                       \
  gams_fix_quote_names(buf, bufout);

#define GET_PROPER_ROWNAME(ctr, eidx, buf, bufout)                             \
  S_CHECK(ctr_copyequname(ctr, eidx, buf, sizeof(buf)));                       \
  gams_fix_quote_names(buf, bufout);

struct empinfo_dat {
  FILE *empinfo_file;
  size_t line_len;
};

/* -------------------------------------------------------------------------
 * GAMS has issue reading line bigger than a certain size
 * ------------------------------------------------------------------------- */

#define EMPINFO_MAX 1000
static int _add_empinfo(struct empinfo_dat *file, const char *buf) {
  int res;

  size_t len_elt = strlen(buf) + 1;
  file->line_len += len_elt;

  if (file->line_len > EMPINFO_MAX) {
    res = fputs("\n", file->empinfo_file);
    if (res == EOF) {
      return Error_SystemError;
      }

    file->line_len = len_elt;
  }

  res = fprintf(file->empinfo_file, " %s", buf);

  if (res < 0) {
    error("%s :: Could not write data\n", __func__);
    return Error_SystemError;
  }
  if ((size_t)res != len_elt) {
    error("%s :: Wrote %d characters instead of %zu\n",
             __func__, res, len_elt);
    return Error_SystemError;
  }

  return OK;
}

static int _check_no_child_mp(const EmpDag *empdag,
                              const MathPrgm *mp) {
  if (empdag_mphaschild(empdag, mp->id)) {
    error(
             "%s :: MP problem %d has some child.\n"
             "This is not yet supported in GAMS\n",
             __func__, mp->id);
    return Error_NotImplemented;
  }

  return OK;
}

static int _print_all_vars(struct empinfo_dat *file, IntArray *vars,
                           Container *ctr, int objvaridx) {
  char buf[GMS_SSSIZE];
  char bufout[2 * GMS_SSSIZE];

  for (size_t vi = 0; vi < vars->len; ++vi) {
    rhp_idx vidx = vars->arr[vi];
    if (vidx != objvaridx) {
      assert(vidx < ctr->n);
      GET_PROPER_COLNAME(ctr, vidx, buf, bufout);
      S_CHECK(_add_empinfo(file, bufout));
    }
  }

  return OK;
}

static int _print_all_equs(struct empinfo_dat *file, IntArray *equs,
                           Container *ctr) {
  char buf[GMS_SSSIZE];
  char bufout[2 * GMS_SSSIZE];

  for (size_t ei = 0; ei < equs->len; ++ei) {
    rhp_idx eidx = equs->arr[ei];
    assert(eidx < ctr->m);
    GET_PROPER_ROWNAME(ctr, eidx, buf, bufout);
    S_CHECK(_add_empinfo(file, bufout));
  }

  return OK;
}

static int _print_vars(struct empinfo_dat *file, IntArray *vars,
                       Model *mdl, rhp_idx objvaridx) {
  char buf[GMS_SSSIZE];
  char bufout[2 * GMS_SSSIZE];

  for (size_t vi = 0; vi < vars->len; ++vi) {
    rhp_idx vidx = mdl_getcurrentvi(mdl, vars->arr[vi]);
    if (valid_vi(vidx) && vidx != objvaridx) {
      GET_PROPER_COLNAME(&mdl->ctr, vidx, buf, bufout);
      S_CHECK(_add_empinfo(file, bufout));
    }
  }

  return OK;
}

static int _print_equs(struct empinfo_dat *file, IntArray *equs,
                       Model *mdl) {
  char buf[GMS_SSSIZE];
  char bufout[2 * GMS_SSSIZE];

  for (size_t ei = 0; ei < equs->len; ++ei) {
    rhp_idx eidx = mdl_getcurrentei(mdl, equs->arr[ei]);
    if (valid_ei(eidx)) {
      GET_PROPER_ROWNAME(&mdl->ctr, eidx, buf, bufout);
      S_CHECK(_add_empinfo(file, bufout));
    }
  }

  return OK;
}

static int print_mp_opt_(struct empinfo_dat *file, MathPrgm *mp,
                         Model *mdl) {
  /* ----------------------------------------------------------------------
   * For a optimization program, the syntax is
   *
   * (min|max) objvar all_vars all_equs
   *
   * ---------------------------------------------------------------------- */
  char buf[GMS_SSSIZE];
  char bufout[2 * GMS_SSSIZE];

  RhpSense sense = mp_getsense(mp);
  assert(sense == RhpMin || sense == RhpMax);

  IO_CALL(fputs(sense == RhpMin ? "\nmin" : "\nmax", file->empinfo_file));

  rhp_idx mp_objvar = mp_getobjvar(mp);

  if (!valid_vi(mp_objvar)) {
    error(
             "%s :: MP %d has an invalid objective variable "
             "%d\n",
             __func__, mp->id, mp_objvar);
    return Error_IndexOutOfRange;
  }

  rhp_idx objvaridx = mdl_getcurrentvi(mdl, mp_objvar);

  if (!valid_vi(objvaridx)) {
    error(
             "%s :: optimization programm %d has no valid "
             "objective variable\n",
             __func__, mp->id);
    return Error_Unconsistency;
  }

  GET_PROPER_COLNAME(&mdl->ctr, objvaridx, buf, bufout);

  IO_CALL(fprintf(file->empinfo_file, " %s", bufout));
  file->line_len = strlen(buf) + 5;

  if (mdl->mdl_up) {
    S_CHECK(_print_vars(file, &mp->vars, mdl, objvaridx));

    S_CHECK(_print_equs(file, &mp->equs, mdl));
  } else {
    S_CHECK(_print_all_vars(file, &mp->vars, &mdl->ctr, objvaridx));

    S_CHECK(_print_all_equs(file, &mp->equs, &mdl->ctr));
  }

  return OK;
}

/* TODO(Xhub) create a version without the ctr_mtr  */
static int _print_mp_vi(struct empinfo_dat *file, MathPrgm *mp,
                        Model *mdl) {
  /* -------------------------------------------------------
   * For a VI programm, the syntax is
   *
   * vi zero_func_vars {var eqn} constraints_equs
   *
   * where {var eqn} can be repeated multiples times
   *
   * ------------------------------------------------------- */

  char buf[GMS_SSSIZE];
  char bufout[2 * GMS_SSSIZE];

  IO_CALL(fputs("\nvi", file->empinfo_file));
  file->line_len = 2;

  IntArray *equs = &mp->equs;
  struct equ_meta *equ_md = mp->mdl->ctr.equmeta;
  IntArray *vars = &mp->vars;
  struct var_meta *var_md = mp->mdl->ctr.varmeta;

  /* ----------------------------------------------------------------------
   * If there are zero functions, loop over all the variables to define those
   * ---------------------------------------------------------------------- */

  unsigned nb_zerofunc = mp_getnumzerofunc(mp);

  if (nb_zerofunc > 0) {
    for (size_t vi = 0; vi < vars->len; ++vi) {
      rhp_idx vidx = vars->arr[vi];
      if (var_md[vidx].ppty == VarPerpToZeroFunctionVi) {
        rhp_idx new_vidx = mdl_getcurrentvi(mdl, vidx);
        GET_PROPER_COLNAME(&mdl->ctr, new_vidx, buf, bufout);
        S_CHECK(_add_empinfo(file, bufout));
      }
    }
  }

  /* ----------------------------------------------------------------------
   * Define the pairs ``equation variable''
   * ---------------------------------------------------------------------- */

  for (size_t vi = 0; vi < vars->len; ++vi) {
    rhp_idx vidx = vars->arr[vi];
    rhp_idx eidx = var_md[vidx].dual;
    if (!valid_ei(eidx)) {
      if (var_md[vidx].ppty == VarPerpToZeroFunctionVi) {
        continue;
      }

      error("%s :: Error in MP %d of type %s: the equation associated with the "
            "variable '%s' (#%d) is not well defined\n",
               __func__, mp_getid(mp), mp_gettypestr(mp),
               ctr_printvarname(&mdl->ctr, vidx), vidx);
      return Error_Unconsistency;
    }

    rhp_idx new_eidx = mdl_getcurrentei(mdl, eidx);
    assert(valid_ei(new_eidx) && "new equation index is invalid");
    GET_PROPER_ROWNAME(&mdl->ctr, new_eidx, buf, bufout);
    S_CHECK(_add_empinfo(file, bufout));
    rhp_idx new_vidx = mdl_getcurrentvi(mdl, vidx);
    assert(valid_vi(new_vidx));
    GET_PROPER_COLNAME(&mdl->ctr, new_vidx, buf, bufout);
    S_CHECK(_add_empinfo(file, bufout));
  }

  /* ----------------------------------------------------------------------
   * Define all the equations
   * ---------------------------------------------------------------------- */

  if (mp_getnumcons(mp) > 0) {
    for (size_t ei = 0; ei < equs->len; ++ei) {
      rhp_idx eidx = equs->arr[ei];
      assert(valid_ei(eidx));
      if (equ_md[eidx].role == EquConstraint) {
        rhp_idx new_eidx = mdl_getcurrentei(mdl, eidx);
        GET_PROPER_ROWNAME(&mdl->ctr, new_eidx, buf, bufout);
        S_CHECK(_add_empinfo(file, bufout));
      }
    }
  }

  return OK;
}

static inline int print_mp_(struct empinfo_dat *file, MathPrgm *mp,
                            Model *mdl) {
  switch (mp->type) {
  case RHP_MP_OPT:
    S_CHECK(print_mp_opt_(file, mp, mdl));
    break;
  case RHP_MP_VI:
    S_CHECK(_print_mp_vi(file, mp, mdl));
    break;
  default:
    error(
             "%s :: declaration of a MP of type %s (#%d) in a "
             "GAMS empinfo file is not supported\n",
             __func__, mp_gettypestr(mp), mp_gettype(mp));
    return Error_NotImplemented;
  }

  return OK;
}

static int _prep_jams_solver(GmsModelData *mdldat, struct ctrdata_gams *gms,
                             struct empinfo_dat *file, char *empinfo_dat)
{
  char buf[GMS_SSSIZE];

  if (!gms->gev) {
    error("%s :: gev object is NULL\n", __func__);
    return Error_NullPointer;
  }

  /* ----------------------------------------------------------------
   * This is a bit ugly, but we need a new empty dir
   * 1. We get the name of the scrdir
   * 2. we try to create a test dir
   * ---------------------------------------------------------------- */

  gevGetStrOpt(gms->gev, gevNameScrDir, buf);

  strncat(buf, "test", sizeof(buf) - strlen(buf) - 1);

  S_CHECK(new_unique_dirname(buf, sizeof buf - 3));
  strncat(buf, DIRSEP, sizeof(buf) - strlen(buf) - 1);
  buf[strlen(buf)] = 0;

  if (mkdir(buf, S_IRWXU)) {
    perror("mkdir");
    error("%s :: failed to create directory %s\n", __func__, buf);
    return Error_SystemError;
  }

  gevSetStrOpt(gms->gev, gevNameScrDir, buf);

  strncat(buf, DIRSEP "empinfo.dat", sizeof(buf) - strlen(buf) - 1);

  file->empinfo_file = fopen(buf, "w");
  file->line_len = 0;

  if (!file->empinfo_file) {
    int lerrno = errno;
    perror("fopen");
    error("%s :: failed to create GAMS empinfo file %s: error %d\n",
             __func__, buf, lerrno);
    return Error_SystemError;
  }

  /*  Save the path to the new empinfo.dat file */
  strncpy(empinfo_dat, buf, GMS_SSSIZE);

  /*  Set the solver to JAMS */
  strncpy(mdldat->solvername, "jams", sizeof("jams"));

  /*  TODO(xhub) set an option for that */
  gevSetIntOpt(gms->gev, gevKeep, 1);

  return OK;
}

static int _export_bilevel(struct empinfo_dat *file, MathPrgm *mp,
                           Model *mdl) {
  /* ------------------------------------------------------------
   * Here comes the definition of the bilevel problem in GAMS
   *
   * the syntax for the bilevel keyword is
   *
   * bilevel {varupper}
   * min objequ varlower constraintslower
   * vi {equ var}
   *
   * see https://www.gams.com/latest/docs/UG_EMP_Bilevel.html
   * ------------------------------------------------------------ */

  IO_CALL(fprintf(file->empinfo_file, "\nbilevel "));
  file->line_len = strlen("bilevel ") + 1;

  rhp_idx mp_objvar = mp_getobjvar(mp);

  if (!valid_vi(mp_objvar)) {
    error("%s :: MP %d has no valid objective variable: %d\n",
             __func__, mp->id, mp_objvar);
    return Error_IndexOutOfRange;
  }

  rhp_idx objvaridx = mdl_getcurrentvi(mdl, mp_objvar);

  if (!valid_vi(objvaridx)) {
    error(
             "%s :: optimization programm %d has no valid "
             "objective variable\n",
             __func__, mp->id);
    return Error_Unconsistency;
  }

  /* --------------------------------------------------------------------
   * This is a bit weird, but in the bilevel case, the objective
   * variable of the upper level problem has to be specified via the GMO
   * -------------------------------------------------------------------- */

  struct ctrdata_gams *gms = (struct ctrdata_gams *)mdl->ctr.data;

  gmoObjVarSet(gms->gmo, objvaridx);

  RhpSense sense = mp_getsense(mp);
  if (!(sense == RhpMin || sense == RhpMax || sense == RhpFeasibility)) {
    error("%s :: no valid objective sense given\n", __func__);
    return Error_Unconsistency;
  }

  gmoSenseSet(gms->gmo, sense == RhpMin ? gmoObj_Min : gmoObj_Max);

  if (mdl->mdl_up) {
    S_CHECK(_print_vars(file, &mp->vars, mdl, objvaridx));
  } else {
    S_CHECK(_print_all_vars(file, &mp->vars, &mdl->ctr, objvaridx));
  }

  TO_IMPLEMENT("emptree rework");
}

static int export_dualvars_(struct empinfo_dat *file, Model *mdl,
                            size_t nb_dualvars) {
  char buf[GMS_SSSIZE];
  char bufout[2 * GMS_SSSIZE];

  /* ----------------------------------------------------------------------
   * The format is: dualvar {var eqn}
   * ---------------------------------------------------------------------- */

  IO_CALL(fputs("\ndualvar", file->empinfo_file));
  file->line_len = 7;

  size_t dualvars_seen = 0;
  for (size_t i = 0; i < mdl->ctr.n; ++i) {
    if (mdl->ctr.varmeta[i].type == VarDual) {
      rhp_idx vi_new = mdl_getcurrentvi(mdl, i);
      assert(valid_vi(vi_new));
      GET_PROPER_COLNAME(&mdl->ctr, vi_new, buf, bufout);
      S_CHECK(_add_empinfo(file, bufout));

      rhp_idx eidx = mdl->ctr.varmeta[i].dual;
      rhp_idx ei_new = mdl_getcurrentei(mdl, eidx);
      assert(valid_ei(ei_new) && "new equation index is invalid");
      GET_PROPER_ROWNAME(&mdl->ctr, ei_new, buf, bufout);
      S_CHECK(_add_empinfo(file, bufout));
      dualvars_seen++;
      if (dualvars_seen == nb_dualvars) {
        break;
      }
    }
  }

  return OK;
}

static int export_equilibrium_(struct empinfo_dat *file,
                               const EmpDag *empdag, Mpe *mpe,
                               Model *mdl) {
  IO_CALL(fprintf(file->empinfo_file, "equilibrium"));

  UIntArray mps;
  S_CHECK(empdag_mpe_getchildren(empdag, mpe->id, &mps));
  for (size_t i = 0, len = mps.len; i < len; ++i) {
    MathPrgm *mp;
    S_CHECK(empdag_getmpbyuid(empdag, rhp_uint_at(&mps, i), &mp));

    S_CHECK(_check_no_child_mp(empdag, mp));

    S_CHECK(print_mp_(file, mp, mdl));
  }

  return OK;
}

/**
 *  @brief export the EMP structure in an empinfo file for GAMS
 *
 *  This function exports supported EMP annotation to the empinfo format
 *  supported by GAMS. This allows the use of GAMS to solve some EMP problems.
 *
 *  This function will fail if the EMP structure contains annotations not
 *  supported by GAMS
 *
 *  @param  mdl_gms  the GAMS model (destination)
 *  @param  mdl      the model from which mdl_gms is derived
 *
 *  @return          the error code
 *  */
int gms_exportempinfo(Model *mdl_gms)
{
  assert(mdl_gms->backend == RHP_BACKEND_GAMS_GMO);
  const Model *mdl_up = mdl_gms->mdl_up;
  const EmpInfo *empinfo = &mdl_up->empinfo;

  if (empinfo->empdag.mps.len == 0) { return OK; }

  Container *ctr_gms = &mdl_gms->ctr;

  char empinfo_dat[GMS_SSSIZE];

  struct ctrdata_gams *gms = (struct ctrdata_gams *)ctr_gms->data;
  struct empinfo_dat file = {NULL, 0};
  const EmpDag *empdag = &empinfo->empdag;

  /* -------------------------------------------------------------------
   * Right now, only the case where the global MP is an equilibrium is
   * supported
   * ------------------------------------------------------------------- */
    unsigned roots_len = empdag_getrootlen(empdag);

    if (roots_len != 1) {
       error("%s :: EMPDAG %s needs a root of type equilibrium\n",
                __func__, mdl_getname(empdag->mdl));
       return Error_EMPIncorrectInput;
    }

    unsigned root_uid = empdag_getrootuidfast(empdag, 0);
    if (uidisMPE(root_uid)) {

    /* ----------------------------------------------------------------
     * Do all the prepration work: create new directory, new file, ...
     * ---------------------------------------------------------------- */

    S_CHECK(_prep_jams_solver(mdl_gms->data, gms, &file, empinfo_dat));

    /* ----------------------------------------------------------------
     * Perform the export
     * ---------------------------------------------------------------- */

    Mpe *mpe;
    unsigned mpe_id = uid2id(root_uid);

    S_CHECK(empdag_getmpebyid(empdag, mpe_id, &mpe));

    S_CHECK(export_equilibrium_(&file, empdag, mpe, mdl_gms));

    if (empinfo->num_dualvar > 0) {
      S_CHECK(export_dualvars_(&file, mdl_gms, empinfo->num_dualvar));
    }

    /* ----------------------------------------------------------------
     * TODO(xhub) support other equilibrium annotation like
     * - implicit
     * - shared
     * - grouping ...
     * ---------------------------------------------------------------- */

  } else if (uidisMP(root_uid)) {

    /* ----------------------------------------------------------------
     * Do all the prepration work: create new directory, new file, ...
     * ---------------------------------------------------------------- */

    S_CHECK(_prep_jams_solver(mdl_gms->data, gms, &file, empinfo_dat));

    /* ----------------------------------------------------------------
     * In this case, we want to check for a potential bilevel problem
     * definition
     * ---------------------------------------------------------------- */
    MathPrgm *mp;
    unsigned mp_id = uid2id(root_uid);
    S_CHECK(empdag_getmpbyid(empdag, mp_id, &mp));

    if (empdag_mphaschild(empdag, mp->id)) {

      if (mp->type != RHP_MP_OPT) {
        error("%s :: the root MP should have type OPT, got %s (#%d)\n",
                 __func__, mp_gettypestr(mp), mp_gettype(mp));
        return Error_Unconsistency;
      }

      TO_IMPLEMENT("emptree rework");
      // S_CHECK(_export_bilevel(&file, mp, ctr_gms, ctr));

    } else {
      goto _check_emp_probtype;
    }
  } else {
_check_emp_probtype: ; /* TODO URG why do we need a semicolon? */
      ProbType probtype;
      S_CHECK(mdl_getprobtype(mdl_gms, &probtype));
      assert(probtype != MdlProbType_none);
      if (probtype == MdlProbType_emp) {
        error(
                 "%s :: destination container is GAMS and the "
                 "model type is EMP, but no equilibrium or bilevel has "
                 "been found. Check the EMP model definition\n",
                 __func__);
        return Error_Unconsistency;
      }
    }

    /* -------------------------------------------------------------------
     * We use the fact that this pointer is nonzero to indicate that there
     * is a new empinfo file.
     *
     * - close the empinfo file
     * - try to load it (not useful for now ...)
     * ------------------------------------------------------------------- */

    if (file.empinfo_file) {

      SYS_CALL(fclose(file.empinfo_file));
    }

  return OK;
}
