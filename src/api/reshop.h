#ifndef RESHOP_H
#define RESHOP_H

#include <stdbool.h>
#include <stddef.h>

struct rhp_avar;
struct rhp_aequ;
struct rhp_mdl;
struct rhp_ctr_filter_ops;
struct rhp_nlnode;
struct rhp_nltree;
struct rhp_mathprgm;
struct rhp_equilibrium;
struct rhp_ovf_def;
struct rhp_spmat;
struct rhp_empdag_arcVF;

/** @brief backend type */
enum rhp_backendtype {
   RHP_BACKEND_GAMS_GMO   = 0,                 /**< GAMS (GMO) container     */
   RHP_BACKEND_RHP        = 1,                 /**< internal container       */
   RHP_BACKEND_JULIA      = 2,                 /**< Julia container          */
   RHP_BACKEND_AMPL       = 3,                 /**< AMPL container           */
};

/** Type of mathematical programm */
enum rhp_mp_type {
   RHP_MP_UNDEF = 0,   /**< not defined/set */
   RHP_MP_OPT,         /**< optimisation problem */
   RHP_MP_VI,          /**< Variational Inequality */
   RHP_MP_CCFLIB,      /**< Mathematical Programm in compact form            */
   RHP_MP_TYPE_LEN,
};

/** @brief Type of optimization problem */
enum rhp_sense {
  RHP_MIN       = 0,                               /**< Minimization problem */
  RHP_MAX       = 1,                               /**< Maximization problem */
  RHP_FEAS      = 2,                               /**< Feasibility problem  */
  RHP_NOSENSE   = 3,                               /**< No sense set         */
  RHP_SENSE_LEN = 4,
};

/** @brief Type of constraints  */
enum rhp_cons_types {
   RHP_CON_GT = 1,          /**< Greater-than constraint */
   RHP_CON_LT = 2,          /**< Less-than constraint */
   RHP_CON_EQ = 4,          /**< Equality constraint */
   RHP_CON_SOC = 6,         /**< Inclusion in the Second Order cone */
   RHP_CON_RSOC = 7,        /**< Inclusion in the Rotated Second Order cone */
   RHP_CON_EXP = 8,         /**< Inclusion in the Exponential cone */
   RHP_CON_DEXP = 9,        /**< Inclusion in the Dual Exponential cone */
   RHP_CON_POWER = 10,      /**< Inclusion in the Power cone */
   RHP_CON_DPOWER = 11,     /**< Inclusion in the Dual Power cone */
};

/**  basis information */
enum rhp_basis_status {
   RHP_BASIS_UNSET = 0,         /**< unset value        */
   RHP_BASIS_LOWER = 1,         /**< at lower bound     */
   RHP_BASIS_UPPER = 2,         /**< at upper bound     */
   RHP_BASIS_BASIC = 3,         /**< is basic           */
   RHP_BASIS_SUPERBASIC = 4,    /**< is superbasic      */
   RHP_BASIS_FIXED = 5,         /**< is fixed           */
   RHP_BASIS_LEN   = 6,
};

/** @brief Custom print function */
typedef void (*rhp_print_fn)(void *data, unsigned mode, const char *buf);
typedef void (*rhp_flush_fn)(void* env);

enum { RHP_OK = 0 };
#define RHP_INVALID_IDX  (SIZE_MAX-1)

/** @brief option values */
union rhp_optval {
    bool    bval;
    double  dval;
    int     ival;
    char   *sval;
};

#ifndef rhp_idx
#define rhp_idx int
#endif


#ifndef RHP_PUBDLL_ATTR
#if defined _WIN32 || defined __CYGWIN__
  #ifdef reshop_EXPORTS
    #ifdef __GNUC__
      #define RHP_PUBDLL_ATTR __attribute__ ((dllexport))
    #else
      #define RHP_PUBDLL_ATTR __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #else
    #ifdef __GNUC__
      #define RHP_PUBDLL_ATTR __attribute__ ((dllimport))
    #else
      #define RHP_PUBDLL_ATTR __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #endif
#else /* defined _WIN32 || defined __CYGWIN__ */
  #if (__GNUC__ >= 4) || defined(__clang__)
    #define RHP_PUBDLL_ATTR __attribute__ ((visibility ("default")))
  #else
    #define RHP_PUBDLL_ATTR
  #endif
#endif
#endif /*  RHP_PUBDLL_ATTR  */

#ifndef RHP_NODISCARD
#if __STDC_VERSION__ > 202300L
#  define RHP_NODISCARD [[nodiscard]] 
#else
#  define RHP_NODISCARD
#endif
#endif

#ifndef RHP_PUBLIB
#define RHP_PUBLIB RHP_PUBDLL_ATTR RHP_NODISCARD
#endif


/* -------------------------------------------------------------------------
 * Variable functions
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
struct rhp_avar* rhp_avar_new(void);
RHP_PUBLIB
struct rhp_avar* rhp_avar_newcompact(unsigned size, unsigned start);
RHP_PUBLIB
struct rhp_avar* rhp_avar_newlist(unsigned size, rhp_idx *vis);
RHP_PUBLIB
struct rhp_avar* rhp_avar_newlistcopy(unsigned size, rhp_idx *vis);
RHP_PUBLIB
void rhp_avar_free(struct rhp_avar* v);
RHP_PUBLIB
int rhp_avar_get(const struct rhp_avar *v, unsigned i, rhp_idx *vi);
RHP_PUBLIB
int rhp_avar_set_list(struct rhp_avar *v, unsigned size, rhp_idx *list);
RHP_PUBLIB
unsigned rhp_avar_size(const struct rhp_avar *v);
RHP_PUBLIB
const char* rhp_avar_gettypename(const struct rhp_avar *v);

/* -------------------------------------------------------------------------
 * Equation functions
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
struct rhp_aequ* rhp_aequ_new(void);
RHP_PUBLIB
struct rhp_aequ* rhp_aequ_newcompact(unsigned size, rhp_idx start);
RHP_PUBLIB
struct rhp_aequ* rhp_aequ_newlist(unsigned size, rhp_idx *eis);
RHP_PUBLIB
struct rhp_aequ* rhp_aequ_newlistcopy(unsigned size, rhp_idx *eis);
RHP_PUBLIB
int rhp_aequ_get(const struct rhp_aequ *e, unsigned i, rhp_idx *ei);
RHP_PUBLIB
void rhp_aequ_free(struct rhp_aequ* e);
RHP_PUBLIB
unsigned rhp_aequ_size(const struct rhp_aequ *e);
RHP_PUBLIB
const char* rhp_aequ_gettypename(const struct rhp_aequ *e);

RHP_PUBLIB
char rhp_avar_contains(const struct rhp_avar *v, rhp_idx vi);
RHP_PUBLIB
char rhp_aequ_contains(const struct rhp_aequ *e, rhp_idx ei);

/* -------------------------------------------------------------------------
 * Model creation/destruction functions
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
struct rhp_mdl *rhp_mdl_new(unsigned backend);
RHP_PUBLIB
void rhp_mdl_free(struct rhp_mdl *mdl);

/* -------------------------------------------------------------------------
 * Model public API
 * ------------------------------------------------------------------------- */

/* TODO evaluate the next 2 functions */
//RHP_PUBLIB
//int rhp_mdl_copyequname(const struct rhp_mdl *mdl, rhp_idx ei, char *name, unsigned len);
//RHP_PUBLIB
//int rhp_mdl_copyvarname(const struct rhp_mdl *mdl, rhp_idx vi, char *name, unsigned len);
RHP_PUBLIB
int rhp_mdl_fixvar(struct rhp_mdl *mdl, rhp_idx vi, double val);
RHP_PUBLIB
int rhp_mdl_getvarsmult(const struct rhp_mdl *mdl, struct rhp_avar *v, double *mult);
RHP_PUBLIB
int rhp_mdl_getvarsbasis(const struct rhp_mdl *mdl, struct rhp_avar *v, int *basis_status);
RHP_PUBLIB
int rhp_mdl_getvarsval(const struct rhp_mdl *mdl, struct rhp_avar *v, double *vals);
RHP_PUBLIB
int rhp_mdl_getequsmult(const struct rhp_mdl *mdl, struct rhp_aequ *e, double *mult);
RHP_PUBLIB
int rhp_mdl_getequsbasis(const struct rhp_mdl *mdl, struct rhp_aequ *e, int *basis_status);
RHP_PUBLIB
int rhp_mdl_getequsval(const struct rhp_mdl *mdl, struct rhp_aequ *e, double *vals);
RHP_PUBLIB
int rhp_mdl_getequbyname(const struct rhp_mdl *mdl, const char* name, rhp_idx *ei);
RHP_PUBLIB
int rhp_mdl_getallequsmult(const struct rhp_mdl *mdl, double *mult);
RHP_PUBLIB
int rhp_mdl_getallequsval(const struct rhp_mdl *mdl, double *vals);
RHP_PUBLIB
int rhp_mdl_getallvarsmult(const struct rhp_mdl *mdl, double *mult);
RHP_PUBLIB
int rhp_mdl_getallvarsval(const struct rhp_mdl *mdl, double *vals);
RHP_PUBLIB
int rhp_mdl_getequbasis(const struct rhp_mdl *mdl, rhp_idx ei, int *basis_status);
RHP_PUBLIB
int rhp_mdl_getequcst(const struct rhp_mdl *mdl, rhp_idx ei, double *val);
RHP_PUBLIB
int rhp_mdl_getequval(const struct rhp_mdl *mdl, rhp_idx ei, double *val);
RHP_PUBLIB
int rhp_mdl_getequperp(const struct rhp_mdl *mdl, rhp_idx ei, rhp_idx *vi);
RHP_PUBLIB
int rhp_mdl_getequmult(const struct rhp_mdl *mdl, rhp_idx ei, double *mult);
RHP_PUBLIB
int rhp_mdl_getequtype(struct rhp_mdl *mdl, rhp_idx ei, unsigned *type, unsigned *cone);
RHP_PUBLIB
int rhp_mdl_getmodelstat(const struct rhp_mdl *mdl, int *modelstat);
RHP_PUBLIB
struct rhp_mathprgm* rhp_mdl_getmpforvar(const struct rhp_mdl *mdl, rhp_idx vi);
RHP_PUBLIB
struct rhp_mathprgm* rhp_mdl_getmpforequ(const struct rhp_mdl *mdl, rhp_idx ei);
RHP_PUBLIB
int rhp_mdl_getobjequ(const struct rhp_mdl *mdl, rhp_idx *objequ);
RHP_PUBLIB
int rhp_mdl_getobjequs(const struct rhp_mdl *mdl, struct rhp_aequ *objs);
RHP_PUBLIB
int rhp_mdl_getsense(const struct rhp_mdl *mdl, unsigned *objsense);
RHP_PUBLIB
int rhp_mdl_getobjvar(const struct rhp_mdl *mdl, rhp_idx *objvar);
RHP_PUBLIB
int rhp_mdl_getprobtype(const struct rhp_mdl *mdl, unsigned *probtype);
RHP_PUBLIB
int rhp_mdl_getsolvername(const struct rhp_mdl *mdl, char const ** solvername);
RHP_PUBLIB
int rhp_mdl_getsolvestat(const struct rhp_mdl *mdl, int *solvestat);
RHP_PUBLIB
int rhp_mdl_getspecialfloats(const struct rhp_mdl *mdl, double *minf, double *pinf, double* nan);
RHP_PUBLIB
int rhp_mdl_getvarperp(const struct rhp_mdl *mdl, rhp_idx vi, rhp_idx *ei);
RHP_PUBLIB
int rhp_mdl_getvarbasis(const struct rhp_mdl *mdl, rhp_idx vi, int *basis_status);
RHP_PUBLIB
int rhp_mdl_getvarbounds(const struct rhp_mdl *mdl, rhp_idx vi, double* lb, double *ub);
RHP_PUBLIB
int rhp_mdl_getvarbyname(const struct rhp_mdl *mdl, const char* name, rhp_idx *vi);
RHP_PUBLIB
int rhp_mdl_getvarlb(const struct rhp_mdl *mdl, rhp_idx vi, double *lower_bound);
RHP_PUBLIB
int rhp_mdl_getvarval(const struct rhp_mdl *mdl, rhp_idx vi, double *val);
RHP_PUBLIB
int rhp_mdl_getvarmult(const struct rhp_mdl *mdl, rhp_idx vi, double *mult);
RHP_PUBLIB
int rhp_mdl_getvartype(const struct rhp_mdl *mdl, rhp_idx vi, unsigned *type);
RHP_PUBLIB
int rhp_mdl_getvarub(const struct rhp_mdl *mdl, rhp_idx vi, double *upper_bound);
RHP_PUBLIB
unsigned rhp_mdl_nequs(const struct rhp_mdl *mdl);
RHP_PUBLIB
unsigned rhp_mdl_nvars(const struct rhp_mdl *mdl);
RHP_PUBLIB
const char* rhp_mdl_printequname(const struct rhp_mdl *mdl, rhp_idx ei);
RHP_PUBLIB
const char* rhp_mdl_printvarname(const struct rhp_mdl *mdl, rhp_idx vi);
RHP_PUBLIB
int rhp_mdl_setname(struct rhp_mdl *mdl, const char *name);
RHP_PUBLIB
int rhp_mdl_resize(struct rhp_mdl *mdl, unsigned n, unsigned m);
RHP_PUBLIB
int rhp_mdl_setequbasis(struct rhp_mdl *mdl, rhp_idx ei, int basis_status);
RHP_PUBLIB
int rhp_mdl_setequcst(struct rhp_mdl *mdl, rhp_idx ei, double val);
RHP_PUBLIB
int rhp_mdl_setequname(struct rhp_mdl *mdl, rhp_idx ei, const char *name);
RHP_PUBLIB
int rhp_mdl_setequmult(struct rhp_mdl *mdl, rhp_idx ei, double marginal);
RHP_PUBLIB
int rhp_mdl_setequtype(struct rhp_mdl *mdl, rhp_idx ei, unsigned type, unsigned cone);
RHP_PUBLIB
int rhp_mdl_setequvarperp(struct rhp_mdl *mdl, rhp_idx ei, rhp_idx vi);
RHP_PUBLIB
int rhp_mdl_setequval(struct rhp_mdl *mdl, rhp_idx ei, double level);
RHP_PUBLIB
int rhp_mdl_setprobtype(struct rhp_mdl *mdl, unsigned probtype);
RHP_PUBLIB
int rhp_mdl_setobjsense(struct rhp_mdl *mdl, unsigned objsense);
RHP_PUBLIB
int rhp_mdl_setobjvar(struct rhp_mdl *mdl, rhp_idx vi);
RHP_PUBLIB
int rhp_mdl_setequrhs(struct rhp_mdl *mdl, rhp_idx ei, double val);
RHP_PUBLIB
int rhp_mdl_setsolvername(struct rhp_mdl *mdl, const char *solvername);
RHP_PUBLIB
int rhp_mdl_setvarbasis(struct rhp_mdl *mdl, rhp_idx vi, int basis_status);
RHP_PUBLIB
int rhp_mdl_setvarbounds(struct rhp_mdl *mdl, rhp_idx vi, double lb, double ub);
RHP_PUBLIB
int rhp_mdl_setvarlb(struct rhp_mdl *mdl, rhp_idx vi, double lb);
RHP_PUBLIB
int rhp_mdl_setvarmult(struct rhp_mdl *mdl, rhp_idx vi, double varm);
RHP_PUBLIB
int rhp_mdl_setvarname(struct rhp_mdl *mdl, rhp_idx vi, const char *name);
RHP_PUBLIB
int rhp_mdl_setvartype(struct rhp_mdl *mdl, rhp_idx vi, unsigned type);
RHP_PUBLIB
int rhp_mdl_setvarub(struct rhp_mdl *mdl, rhp_idx vi, double ub);
RHP_PUBLIB
int rhp_mdl_setvarval(struct rhp_mdl *mdl, rhp_idx vi, double val);
RHP_PUBLIB
int rhp_mdl_exportmodel(struct rhp_mdl *mdl, struct rhp_mdl *mdl_dst);
RHP_PUBLIB
const char *rhp_mdl_modelstattxt(const struct rhp_mdl *mdl, int modelstat);
RHP_PUBLIB
const char *rhp_mdl_solvestattxt(const struct rhp_mdl *mdl, int solvestat);

/* -------------------------------------------------------------------------
 * EMP info API
 * ------------------------------------------------------------------------- */

//RHP_PUBLIB
//int rhp_setroot_equil(struct rhp_mdl *mdl, struct mpe **mpe);
RHP_PUBLIB
int rhp_ensure_mp(struct rhp_mdl *mdl, unsigned reserve);
RHP_PUBLIB
void rhp_print_emp(const struct rhp_mdl *mdl);
RHP_PUBLIB
int rhp_empdag_rootsetmp(struct rhp_mdl *mdl, struct rhp_mathprgm *mp);
RHP_PUBLIB
int rhp_empdag_rootsetmpe(struct rhp_mdl *mdl, struct rhp_equilibrium *mpe);

/* -------------------------------------------------------------------------
 * GAMS specific functions
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
struct rhp_mdl *rhp_gms_newfromcntr(const char *cntrfile);
RHP_PUBLIB
int rhp_gms_setgamscntr(struct rhp_mdl *mdl, const char *cntrfile);
RHP_PUBLIB
int rhp_gms_setgamsdir(struct rhp_mdl *mdl, const char *gamsdir);
RHP_PUBLIB
int rhp_gams_setglobalgamscntr(const char *cntrfile);
RHP_PUBLIB
int rhp_gams_setglobalgamsdir(const char *gamsdir);
RHP_PUBLIB
int rhp_gms_writesol2gdx(struct rhp_mdl *mdl, const char *gdxname);


/* -------------------------------------------------------------------------
 * Mathematical Programm API
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
int rhp_mp_addconstraint(struct rhp_mathprgm *mp, rhp_idx ei);
RHP_PUBLIB
int rhp_mp_addequ(struct rhp_mathprgm *mp, rhp_idx ei);
RHP_PUBLIB
int rhp_mp_addvar(struct rhp_mathprgm *mp, rhp_idx vi);
RHP_PUBLIB
int rhp_mp_addvars(struct rhp_mathprgm *mp, struct rhp_avar *v);
RHP_PUBLIB
int rhp_mp_addvipair(struct rhp_mathprgm *mp, rhp_idx ei, rhp_idx vi);
RHP_PUBLIB
int rhp_mp_addvipairs(struct rhp_mathprgm *mp, struct rhp_aequ *e, struct rhp_avar *v);
RHP_PUBLIB
int rhp_mp_finalize(struct rhp_mathprgm *mp);
RHP_PUBLIB
unsigned rhp_mp_getid(const struct rhp_mathprgm *mp);
RHP_PUBLIB
const struct rhp_mdl* rhp_mp_getmdl(const struct rhp_mathprgm *mp);
RHP_PUBLIB
rhp_idx rhp_mp_getobjequ(const struct rhp_mathprgm *mp);
RHP_PUBLIB
rhp_idx rhp_mp_getobjvar(const struct rhp_mathprgm *mp);
RHP_PUBLIB
unsigned rhp_mp_getsense(const struct rhp_mathprgm *mp);
RHP_PUBLIB
const char* rhp_mp_getname(const struct rhp_mathprgm *mp);
RHP_PUBLIB
unsigned rhp_mp_ncons(const struct rhp_mathprgm *mp);
RHP_PUBLIB
unsigned rhp_mp_nmatched(const struct rhp_mathprgm *mp);
RHP_PUBLIB
unsigned rhp_mp_nvars(const struct rhp_mathprgm *mp);
RHP_PUBLIB
int rhp_mp_print(struct rhp_mathprgm *mp);
RHP_PUBLIB
int rhp_mp_setname(struct rhp_mathprgm *mp, const char* name);
RHP_PUBLIB
int rhp_mp_setobjequ(struct rhp_mathprgm *mp, rhp_idx ei);
RHP_PUBLIB
int rhp_mp_setobjvar(struct rhp_mathprgm *mp, rhp_idx vi);
RHP_PUBLIB
int rhp_mp_settype(struct rhp_mathprgm *mp, unsigned type);

/* -------------------------------------------------------------------------
 * EMPDAG MPE (MP Equilibrium) API
 * ------------------------------------------------------------------------- */
RHP_PUBLIB
unsigned rhp_mpe_getid(const struct rhp_equilibrium *mpe);
RHP_PUBLIB
const char* rhp_mpe_getname(const struct rhp_equilibrium *mpe);
RHP_PUBLIB
unsigned rhp_mpe_getnumchildren(const struct rhp_equilibrium *mpe);
RHP_PUBLIB
int rhp_mpe_print(struct rhp_equilibrium *mpe);

/* -------------------------------------------------------------------------
 * EMPDAG MP (Mathematical Programming) API
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
struct rhp_mathprgm *rhp_empdag_newmp(struct rhp_mdl *mdl, unsigned sense);
RHP_PUBLIB
int rhp_empdag_mpaddmpVF(struct rhp_mdl *mdl, struct rhp_mathprgm *mp,
                         struct rhp_mathprgm *mp_child, struct rhp_empdag_arcVF *edgeVF);
RHP_PUBLIB
int rhp_empdag_mpaddmpCTRL(struct rhp_mdl *mdl,  struct rhp_mathprgm *mp,
                           struct rhp_mathprgm *mp_child);

/* -------------------------------------------------------------------------
 * EMPDAG Equilibrium API
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
struct rhp_equilibrium* rhp_empdag_newmpe(struct rhp_mdl *mdl);
RHP_PUBLIB
int rhp_empdag_mpeaddmp(struct rhp_mdl *mdl, struct rhp_equilibrium* mpe, struct rhp_mathprgm *mp);

/* -------------------------------------------------------------------------
 * EMPDAG edgeVF (value function)
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
struct rhp_empdag_arcVF * rhp_edgeVF_new(void);
RHP_PUBLIB
int rhp_edgeVF_init(struct rhp_empdag_arcVF *edgeVF, rhp_idx ei);
RHP_PUBLIB
int rhp_edgeVF_free(struct rhp_empdag_arcVF *edgeVF);
RHP_PUBLIB
int rhp_edgeVF_setvar(struct rhp_empdag_arcVF *edgeVF, rhp_idx vi);
RHP_PUBLIB
int rhp_edgeVF_setcst(struct rhp_empdag_arcVF *edgeVF, double cst);


/* -------------------------------------------------------------------------
 * ReSHOP internal model modification
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * RIM: adding variables
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
int rhp_add_var(struct rhp_mdl *mdl, rhp_idx *vi);
RHP_PUBLIB
int rhp_add_varnamed(struct rhp_mdl *mdl, rhp_idx *vi, const char *name);
RHP_PUBLIB
int rhp_add_vars(struct rhp_mdl *mdl, unsigned size, struct rhp_avar *vout);
RHP_PUBLIB
int rhp_add_varsnamed(struct rhp_mdl *mdl, unsigned size, struct rhp_avar *vout, const char* name);
RHP_PUBLIB
int rhp_add_posvars(struct rhp_mdl *mdl, unsigned size, struct rhp_avar *vout);
RHP_PUBLIB
int rhp_add_posvarsnamed(struct rhp_mdl *mdl, unsigned size, struct rhp_avar *vout, const char* name);
RHP_PUBLIB
int rhp_add_negvars(struct rhp_mdl *mdl, unsigned size, struct rhp_avar *vout);
RHP_PUBLIB
int rhp_add_negvarsnamed(struct rhp_mdl *mdl, unsigned size, struct rhp_avar *vout, const char* name);
RHP_PUBLIB
int rhp_add_varsinbox(struct rhp_mdl *mdl, unsigned size, struct rhp_avar *vout, double lb, double ub);
RHP_PUBLIB
int rhp_add_varsinboxnamed(struct rhp_mdl *mdl, unsigned size, struct rhp_avar *vout, const char* name,
                           double lb, double ub);
RHP_PUBLIB
int rhp_add_varsinboxes(struct rhp_mdl *mdl, unsigned size, struct rhp_avar *vout, double *lb, double *ub);
RHP_PUBLIB
int rhp_add_varsinboxesnamed(struct rhp_mdl *mdl, unsigned size, struct rhp_avar *vout, const char* name,
                             double *lb, double *ub);


RHP_PUBLIB
int rhp_set_var_sos1(struct rhp_mdl *mdl, struct rhp_avar *v, double *weights);
RHP_PUBLIB
int rhp_set_var_sos2(struct rhp_mdl *mdl, struct rhp_avar *v, double *weights);

/* -------------------------------------------------------------------------
 * RIM: adding equation
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
int rhp_add_con(struct rhp_mdl *mdl, unsigned type, rhp_idx *ei);
RHP_PUBLIB
int rhp_add_connamed(struct rhp_mdl *mdl, unsigned size, rhp_idx *ei, const char *name);
RHP_PUBLIB
int rhp_add_cons(struct rhp_mdl *mdl, unsigned size, unsigned type, struct rhp_aequ *eout);
RHP_PUBLIB
int rhp_add_consnamed(struct rhp_mdl *mdl, unsigned size, unsigned type, struct rhp_aequ *eout, const char *name);
RHP_PUBLIB
int rhp_add_func(struct rhp_mdl *mdl, rhp_idx *ei);
RHP_PUBLIB
int rhp_add_funcnamed(struct rhp_mdl *mdl, rhp_idx *ei, const char *name);
RHP_PUBLIB
int rhp_add_funcs(struct rhp_mdl *mdl, unsigned size, struct rhp_aequ *eout);
RHP_PUBLIB
int rhp_add_funcsnamed(struct rhp_mdl *mdl, unsigned size, struct rhp_aequ *eout, const char *name);

RHP_PUBLIB
int rhp_mdl_setobjequ(struct rhp_mdl *mdl, rhp_idx objequ);
/*  TODO: see if we can delete the rest */
//RHP_PUBLIB
//int rmdl_addinit_equs(struct rhp_mdl *mdl, unsigned nb);
RHP_PUBLIB
int rhp_add_equation(struct rhp_mdl *mdl, rhp_idx *ei);
RHP_PUBLIB
int rhp_add_equations(struct rhp_mdl *mdl, unsigned nb, struct rhp_aequ *eout);
RHP_PUBLIB
int rhp_add_equality_constraint(struct rhp_mdl *mdl, rhp_idx *ei);
RHP_PUBLIB
int rhp_add_exp_constraint(struct rhp_mdl *mdl, rhp_idx *ei);
RHP_PUBLIB
int rhp_add_greaterthan_constraint(struct rhp_mdl *mdl, rhp_idx *ei);
RHP_PUBLIB
int rhp_add_lessthan_constraint(struct rhp_mdl *mdl, rhp_idx *ei);
RHP_PUBLIB
int rhp_add_power_constraint(struct rhp_mdl *mdl, rhp_idx *ei);
RHP_PUBLIB
int rhp_add_soc_constraint(struct rhp_mdl *mdl, rhp_idx *ei);

/* -------------------------------------------------------------------------
 * RIM: equation modification
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
int rhp_equ_addbilin(struct rhp_mdl *mdl, rhp_idx ei, struct rhp_avar *v1,
                     struct rhp_avar *v2, double coeff);
RHP_PUBLIB
int rhp_equ_addlin(struct rhp_mdl *mdl, rhp_idx ei, struct rhp_avar *v, const double *coeffs);
RHP_PUBLIB
int rhp_equ_addlinchk(struct rhp_mdl *mdl, rhp_idx ei, struct rhp_avar *v,
                      const double *coeffs);
RHP_PUBLIB
int rhp_equ_addlincoeff(struct rhp_mdl *mdl, rhp_idx ei, struct rhp_avar *v, const double *vals, double coeff);

RHP_PUBLIB
int rhp_equ_addquadrelative(struct rhp_mdl *mdl, rhp_idx ei, struct rhp_avar *v_row,
                            struct rhp_avar *v_col, size_t nnz, unsigned *i, unsigned *j,
                            double *x, double coeff);
RHP_PUBLIB
int rhp_equ_addquadabsolute(struct rhp_mdl *mdl, rhp_idx ei, size_t nnz, unsigned *i,
                            unsigned *j, double *x, double coeff);
RHP_PUBLIB
int rhp_equ_addlvar(struct rhp_mdl *mdl, rhp_idx ei, rhp_idx vi, double val);
RHP_PUBLIB
int rhp_equ_addnewlvar(struct rhp_mdl *mdl, rhp_idx ei, rhp_idx vi, double val);
RHP_PUBLIB
int rhp_equ_setcst(struct rhp_mdl *mdl, rhp_idx ei, double val);

RHP_PUBLIB
int rhp_equ_getcst(struct rhp_mdl *mdl, rhp_idx ei, double *val);
RHP_PUBLIB
int rhp_equ_getlin(struct rhp_mdl *mdl, rhp_idx ei, unsigned *len, rhp_idx **idxs, double **vals);

/* -------------------------------------------------------------------------
 * RIM: equation/variable removal (be careful)
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
int rhp_delete_equ(struct rhp_mdl *mdl, rhp_idx ei);
RHP_PUBLIB
int rhp_delete_var(struct rhp_mdl *mdl, rhp_idx vi);

RHP_PUBLIB
int rhp_is_var_valid(struct rhp_mdl *mdl, rhp_idx vi);
RHP_PUBLIB
int rhp_is_equ_valid(struct rhp_mdl *mdl, rhp_idx ei);

/* -------------------------------------------------------------------------
 * nltree, for nonlinear equation, functions
 * ------------------------------------------------------------------------- */
RHP_PUBLIB
int rhp_nltree_getroot(struct rhp_nltree *tree, struct rhp_nlnode ***node);
RHP_PUBLIB
int rhp_nltree_getchild(struct rhp_nlnode **node, struct rhp_nlnode ***child, unsigned i);
RHP_PUBLIB
int rhp_nltree_getchild2(struct rhp_nlnode ***node, struct rhp_nlnode ***child, unsigned i);
RHP_PUBLIB
int rhp_nltree_arithm(struct rhp_nltree *tree, struct rhp_nlnode ***node,
                      unsigned opcode, unsigned nb);
RHP_PUBLIB
int rhp_nltree_call(struct rhp_mdl *mdl, struct rhp_nltree *tree,
                    struct rhp_nlnode ***node, unsigned opcode, unsigned n_args);
RHP_PUBLIB
int rhp_nltree_cst(struct rhp_mdl *mdl, struct rhp_nltree* tree, struct rhp_nlnode ***node,
                   double cst);
RHP_PUBLIB
int rhp_nltree_umin(struct rhp_nltree *tree, struct rhp_nlnode ** restrict *node);
RHP_PUBLIB
int rhp_nltree_var(struct rhp_mdl *mdl, struct rhp_nltree* tree,
                struct rhp_nlnode ***node, rhp_idx vi, double coeff);
RHP_PUBLIB
int rhp_nltree_addquad(struct rhp_mdl *mdl, rhp_idx ei,
      struct rhp_spmat* mat, struct rhp_avar *v, double coeff);
RHP_PUBLIB
int rhp_nltree_addlin(struct rhp_mdl *mdl, rhp_idx ei, double* vals,
      struct rhp_avar *v, double coeff);

/* -------------------------------------------------------------------------
 * ReSHOP internal model change
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
int rhp_mdl_reserve_equs(struct rhp_mdl *mdl, unsigned size);
RHP_PUBLIB
int rhp_mdl_reserve_vars(struct rhp_mdl *mdl, unsigned size);
RHP_PUBLIB
unsigned rhp_mdl_nvars_total(const struct rhp_mdl *mdl);
RHP_PUBLIB
unsigned rhp_mdl_nequs_total(const struct rhp_mdl *mdl);

RHP_PUBLIB
int rmdl_analyze_modeltype(struct rhp_mdl *mdl, struct rhp_ctr_filter_ops *fops);

RHP_PUBLIB
int rhp_mdl_latex(struct rhp_mdl *mdl, const char *filename);

RHP_PUBLIB
struct rhp_nltree* rhp_mdl_getnltree(const struct rhp_mdl *mdl, rhp_idx ei);

/* -------------------------------------------------------------------------
 * OVF (Optimal Value Function) API
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
int rhp_ovf_add(struct rhp_mdl *mdl, const char* name, rhp_idx ovf_vi,
                struct rhp_avar *v_args, struct rhp_ovf_def **ovf_def);
RHP_PUBLIB
int rhp_ovf_check(struct rhp_mdl *mdl, struct rhp_ovf_def *ovf_def);
RHP_PUBLIB
int rhp_ovf_param_add_scalar(struct rhp_ovf_def* ovf_def, const char *param_name,
                             double val);
RHP_PUBLIB
int rhp_ovf_param_add_vector(struct rhp_ovf_def *ovf_def, const char *param_name,
                             unsigned n_vals, double *vals);
RHP_PUBLIB
int rhp_ovf_setreformulation(struct rhp_ovf_def *ovf_def, const char *reformulation);

/* -------------------------------------------------------------------------
 * ReSHOP model management
 * ------------------------------------------------------------------------- */
RHP_PUBLIB
struct rhp_mdl *rhp_getsolvermdl(struct rhp_mdl *mdl);

/* -------------------------------------------------------------------------
 * ReSHOP main API
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
int rhp_process(struct rhp_mdl *mdl, struct rhp_mdl *mdl_solver);
RHP_PUBLIB
int rhp_solve(struct rhp_mdl *mdl);
RHP_PUBLIB
int rhp_postprocess(struct rhp_mdl *mdl_solver);

/* -------------------------------------------------------------------------
 * ReSHOP printing related 
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
void rhp_set_printops(void* data, rhp_print_fn print, rhp_flush_fn flush,
                      bool use_asciicolors);
RHP_PUBLIB
void rhp_set_printopsdefault(void);

/* -------------------------------------------------------------------------
 * ReSHOP misc functions
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
const char *rhp_status_descr(int status);
RHP_PUBLIB
size_t rhp_getidxmax(void);
RHP_PUBLIB
const char * rhp_sensestr(unsigned sense);

RHP_PUBLIB
enum rhp_backendtype rhp_mdl_getbackend(const struct rhp_mdl *mdl);
RHP_PUBLIB
const char* rhp_mdl_getbackendname(const struct rhp_mdl *mdl);
RHP_PUBLIB
const char* rhp_mdl_getname(const struct rhp_mdl *mdl);
RHP_PUBLIB
unsigned rhp_mdl_getid(const struct rhp_mdl *mdl);

/* -------------------------------------------------------------------------
 * Get some stats about the problem
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
size_t rhp_get_nb_lequ_le(struct rhp_mdl *mdl);
RHP_PUBLIB
size_t rhp_get_nb_lequ_ge(struct rhp_mdl *mdl);
RHP_PUBLIB
size_t rhp_get_nb_lequ_eq(struct rhp_mdl *mdl);
RHP_PUBLIB
size_t rhp_get_nb_var_bin(struct rhp_mdl *mdl);
RHP_PUBLIB
size_t rhp_get_nb_var_int(struct rhp_mdl *mdl);
RHP_PUBLIB
size_t rhp_get_nb_var_lb(struct rhp_mdl *mdl);
RHP_PUBLIB
size_t rhp_get_nb_var_ub(struct rhp_mdl *mdl);
RHP_PUBLIB
size_t rhp_get_nb_var_interval(struct rhp_mdl *mdl);
RHP_PUBLIB
size_t rhp_get_nb_var_fx(struct rhp_mdl *mdl);
RHP_PUBLIB
size_t rhp_get_nb_var_sos1(struct rhp_mdl *mdl);
RHP_PUBLIB
size_t rhp_get_nb_var_sos2(struct rhp_mdl *mdl);

RHP_PUBLIB
int rhp_get_var_sos1(struct rhp_mdl *mdl, rhp_idx vi, unsigned **grps);
RHP_PUBLIB
int rhp_get_var_sos2(struct rhp_mdl *mdl, rhp_idx vi, unsigned **grps);

/* -------------------------------------------------------------------------
 * Option management for the ReSHOP container
 * ------------------------------------------------------------------------- */

RHP_PUBLIB
int rhp_set_option_b(const struct rhp_mdl *mdl, const char *optname, unsigned char opt);
RHP_PUBLIB
int rhp_set_option_d(const struct rhp_mdl *mdl, const char *optname, double optval);
RHP_PUBLIB
int rhp_set_option_i(const struct rhp_mdl *mdl, const char *optname, int optval);
RHP_PUBLIB
int rhp_set_option_s(const struct rhp_mdl *mdl, const char *optname, char *optstr);

RHP_PUBLIB
int rhp_opt_setb(const char *name, int bval);
RHP_PUBLIB
int rhp_opt_setc(const char *name, const char *str);
RHP_PUBLIB
int rhp_opt_setd(const char *name, double dval);
RHP_PUBLIB
int rhp_opt_seti(const char *name, int ival);
RHP_PUBLIB
int rhp_opt_sets(const char *name, const char *value);
RHP_PUBLIB
int rhp_opt_setfromstr(char *optstring);

RHP_PUBLIB
int rhp_opt_getb(const char *name, int *bval);
RHP_PUBLIB
int rhp_opt_getd(const char *name, double *dval);
RHP_PUBLIB
int rhp_opt_geti(const char *name, int *ival);
RHP_PUBLIB
int rhp_opt_gets(const char *name, char **str);
RHP_PUBLIB
int rhp_opt_gettype(const char *name, unsigned *type);

/* -------------------------------------------------------------------------
 * Matrix utilities
 * ------------------------------------------------------------------------- */
RHP_PUBLIB
void rhp_mat_free(struct rhp_spmat* m);
RHP_PUBLIB
struct rhp_spmat* rhp_mat_triplet(unsigned n, unsigned m, unsigned nnz,
                                  int *rowidx, int *colidx, double *val);


/* -------------------------------------------------------------------------
 * Misc information
 * ------------------------------------------------------------------------- */
RHP_PUBLIB
const char* rhp_basis_str(enum rhp_basis_status basis);

/* -------------------------------------------------------------------------
 * Solver specific info
 * ------------------------------------------------------------------------- */
RHP_PUBLIB
int rhp_PATH_setfname(const char* fname);

/* ------------------------------------------------------------------------
 * Additional display info
 * ------------------------------------------------------------------------ */
RHP_PUBLIB
void rhp_banner(void);
RHP_PUBLIB
const char* rhp_version(void);
RHP_PUBLIB
void rhp_show_containertrace(unsigned char val);
RHP_PUBLIB
void rhp_show_empdagtrace(unsigned char val);
RHP_PUBLIB
void rhp_show_empinterptrace(unsigned char val);
RHP_PUBLIB
void rhp_show_empparsertrace(unsigned char val);
RHP_PUBLIB
void rhp_show_fooctrace(unsigned char val);
RHP_PUBLIB
void rhp_show_processtrace(unsigned char val);
RHP_PUBLIB
void rhp_show_refcnttrace(unsigned char val);
RHP_PUBLIB
void rhp_show_solreporttrace(unsigned char val);
RHP_PUBLIB
void rhp_show_stackinfo(unsigned char val);
RHP_PUBLIB
int rhp_syncenv(void);

#undef RHP_PUBDLL_ATTR
#undef RHP_NODISCARD
#undef RHP_PUBLIB

#endif /* RESHOP_H  */
