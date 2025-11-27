#ifndef RESHOP_H
#define RESHOP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>


typedef struct rhp_avar rhp_vars_t;
typedef struct rhp_aequ rhp_equs_t;
typedef struct rhp_mdl rhp_mdl_t;
struct rhp_ctr_filter_ops;
typedef struct rhp_nlnode rhp_nlnode_t;
typedef struct rhp_nltree rhp_nltree_t;
typedef struct rhp_mathprgm rhp_mathprgm_t;
typedef struct rhp_nash_equilibrium rhp_nash_equilibrium_t;
typedef struct rhp_ovfdef rhp_ovf_t;
struct rhp_spmat;
typedef struct rhp_empdag_arcVF rhp_empdag_arcVF_t;

/** @brief backend type */
enum rhp_backendtype {
   RhpBackendGamsGmo    = 0,                 /**< GAMS (GMO) container     */
   RhpBackendReSHOP     = 1,                 /**< internal container       */
   RhpBackendJulia      = 2,                 /**< Julia container          */
   RhpBackendAmpl       = 3,                 /**< AMPL container           */
};

/** @brief Type of mathematical programm */
enum rhp_mp_type {
   RhpMpUndefType = 0, /**< not defined/set */
   RhpMpOpt,           /**< optimisation problem */
   RhpMpVi,            /**< Variational Inequality */
   RhpMpCcflib,        /**< Mathematical Programm in compact form            */
};

/** @brief Type of optimization problem */
enum rhp_sense {
  RHP_MIN        = 0,                               /**< Minimization problem */
  RHP_MAX        = 1,                               /**< Maximization problem */
  RHP_FEAS       = 2,                               /**< Feasibility problem  */
  RHP_DUAL_SENSE = 3,                               /**< Dual Problem         */
  RHP_NOSENSE    = 4,                               /**< No sense set         */
  RHP_SENSE_LEN  = 5,
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

/** @brief basis information */
enum rhp_basis_status {
   RhpBasisUnset = 0,         /**< unset value        */
   RhpBasisLower = 1,         /**< at lower bound     */
   RhpBasisUpper = 2,         /**< at upper bound     */
   RhpBasisBasic = 3,         /**< is basic           */
   RhpBasisSuperBasic = 4,    /**< is superbasic      */
   RhpBasisFixed = 5,         /**< is fixed           */
};

enum { RHP_OK = 0 };
#define RHP_INVALID_IDX  (SIZE_MAX-1)

/** @brief Abstract equation or variable type */
enum rhp_equvar_type {
   RHP_EQUVAR_COMPACT     = 0,  /**< Compact: continuous indices */
   RHP_EQUVAR_LIST        = 1,  /**< List: given as a list of indices */
   RHP_EQUVAR_SORTEDLIST  = 2,  /**< List: given as a sorted list of indices */
   RHP_EQUVAR_BLOCK       = 3,  /**< Block: collection of abstract variable/equation */
   RHP_EQUVAR_UNSET       = 4,   /**< Unset */
};

/** @brief Option type */
enum rhp_option_type {
   RhpOptBoolean = 0,            /**< Boolean option  */
   RhpOptChoice,                 /**< Choice option   */
   RhpOptDouble,                 /**< Double option   */
   RhpOptInteger,                /**< Integer option  */
   RhpOptString,                 /**< String option   */
};

/** @brief Printing flags */
enum rhp_print_flags {
   RhpPrintUseColors    = 1,     /**< use ANSI colors  */
   RhpPrintNoStdOutErr  = 2,     /**< print does not go to default stdout/stderr */
};

/** @brief Custom print function */
typedef void (*rhp_print_fn)(void *data, unsigned mode, const char *buf);
/** @brief Custom flush function */
typedef void (*rhp_flush_fn)(void* env);

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


/* -------------------------------------------------------------------------
 * Compiler macro definitions
 * ------------------------------------------------------------------------- */

#if !defined(__clang__) && defined(__GNUC__) && (__GNUC__ >= 11)
   #define RHP_MALLOC(...) __attribute__ ((malloc, malloc(__VA_ARGS__)))
#elif defined (__GNUC__)
   #define RHP_MALLOC(...) __attribute__ ((malloc))
#else 
   #define RHP_MALLOC(...) /* __VA_ARGS__ */
#endif

#ifndef RHP_PUBDLL_ATTR
#if defined _WIN32 || defined __CYGWIN__
  #ifdef reshop_EXPORTS
    #ifdef __GNUC__
      #define RHP_PUBDLL_ATTR __attribute__ ((dllexport))
    #else
      #define RHP_PUBDLL_ATTR __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #elif defined(reshop_static_build)  /* static build: no need for declspec */
      #define RHP_PUBDLL_ATTR 
  #else
    #ifdef __GNUC__
      #define RHP_PUBDLL_ATTR __attribute__ ((dllimport))
    #else
      #define RHP_PUBDLL_ATTR __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #endif
#else /* defined _WIN32 || defined __CYGWIN__ */
  #if ( defined(__GNUC__) && (__GNUC__ >= 4) ) || defined(__clang__)
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
 * End of compiler macro definitions
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * Variable functions
 * ------------------------------------------------------------------------- */

#define RHP_VARS_ALLOC RHP_PUBLIB RHP_MALLOC(rhp_avar_free)
RHP_PUBLIB void rhp_avar_free(rhp_vars_t* v);
RHP_VARS_ALLOC rhp_vars_t* rhp_avar_new(void);
RHP_VARS_ALLOC rhp_vars_t* rhp_avar_newcompact(unsigned size, unsigned start);
RHP_VARS_ALLOC rhp_vars_t* rhp_avar_newlist(unsigned size, rhp_idx *vis);
RHP_VARS_ALLOC rhp_vars_t* rhp_avar_newlistcopy(unsigned size, rhp_idx *vis);
#undef RHP_VARS_ALLOC

RHP_PUBLIB int rhp_avar_get(const rhp_vars_t *v, unsigned i, rhp_idx *vidx);
RHP_PUBLIB int rhp_avar_set_list(rhp_vars_t *v, unsigned size, rhp_idx *vis);
RHP_PUBLIB int rhp_avar_get_list(rhp_vars_t *v, rhp_idx **vis);
RHP_PUBLIB unsigned rhp_avar_size(const rhp_vars_t *v);
RHP_PUBLIB unsigned rhp_avar_gettype(const rhp_vars_t *v);
RHP_PUBLIB const char* rhp_avar_gettypename(const rhp_vars_t *v);
RHP_PUBLIB bool rhp_avar_ownmem(const rhp_vars_t *v);
RHP_PUBLIB short rhp_avar_contains(const rhp_vars_t *v, rhp_idx vi);

/* -------------------------------------------------------------------------
 * Equation functions
 * ------------------------------------------------------------------------- */

#define RHP_EQUS_ALLOC RHP_PUBLIB RHP_MALLOC(rhp_aequ_free)
RHP_PUBLIB void rhp_aequ_free(rhp_equs_t *e);
RHP_EQUS_ALLOC rhp_equs_t* rhp_aequ_new(void);
RHP_EQUS_ALLOC rhp_equs_t* rhp_aequ_newcompact(unsigned size, rhp_idx start);
RHP_EQUS_ALLOC rhp_equs_t* rhp_aequ_newlist(unsigned size, rhp_idx *eis);
RHP_EQUS_ALLOC rhp_equs_t* rhp_aequ_newlistcopy(unsigned size, rhp_idx *eis);
#undef RHP_EQUS_ALLOC

RHP_PUBLIB int rhp_aequ_get(const rhp_equs_t *e, unsigned i, rhp_idx *eidx);
RHP_PUBLIB unsigned rhp_aequ_size(const rhp_equs_t *e);
RHP_PUBLIB int rhp_aequ_get_list(rhp_equs_t *e, rhp_idx **eis);
RHP_PUBLIB unsigned rhp_aequ_gettype(const rhp_equs_t *e);
RHP_PUBLIB const char* rhp_aequ_gettypename(const rhp_equs_t *e);
RHP_PUBLIB bool rhp_aequ_ownmem(const rhp_equs_t *e);

RHP_PUBLIB short rhp_aequ_contains(const rhp_equs_t *e, rhp_idx ei);

/* -------------------------------------------------------------------------
 * Model creation/destruction functions
 * ------------------------------------------------------------------------- */

RHP_PUBLIB void rhp_mdl_free(rhp_mdl_t *mdl);
RHP_PUBLIB RHP_MALLOC(rhp_mdl_free) rhp_mdl_t *rhp_mdl_new(unsigned backend);

/* -------------------------------------------------------------------------
 * Model public API
 * ------------------------------------------------------------------------- */

/* TODO evaluate the next 2 functions */
//RHP_PUBLIB
//int rhp_mdl_copyequname(const rhp_mdl_t *mdl, rhp_idx ei, char *name, unsigned len);
//RHP_PUBLIB
//int rhp_mdl_copyvarname(const rhp_mdl_t *mdl, rhp_idx vi, char *name, unsigned len);
RHP_PUBLIB int rhp_mdl_fixvar(rhp_mdl_t *mdl, rhp_idx vi, double val);

RHP_PUBLIB int rhp_mdl_getvarsbasis(const rhp_mdl_t *mdl, rhp_vars_t *v, int *vbasis_info);
RHP_PUBLIB int rhp_mdl_getvarsdual(const rhp_mdl_t *mdl, rhp_vars_t *v, double *vdual);
RHP_PUBLIB int rhp_mdl_getvarslevel(const rhp_mdl_t *mdl, rhp_vars_t *v, double *vlevel);

RHP_PUBLIB int rhp_mdl_getequsbasis(const rhp_mdl_t *mdl, rhp_equs_t *e, int *ebasis_info);
RHP_PUBLIB int rhp_mdl_getequsdual(const rhp_mdl_t *mdl, rhp_equs_t *e, double *edual);
RHP_PUBLIB int rhp_mdl_getequslevel(const rhp_mdl_t *mdl, rhp_equs_t *e, double *elevel);

RHP_PUBLIB int rhp_mdl_getequbyname(const rhp_mdl_t *mdl, const char* name, rhp_idx *ei);

RHP_PUBLIB int rhp_mdl_getallequsbasis(const rhp_mdl_t *mdl, int *basis_infos);
RHP_PUBLIB int rhp_mdl_getallequsdual(const rhp_mdl_t *mdl, double *duals);
RHP_PUBLIB int rhp_mdl_getallequslevel(const rhp_mdl_t *mdl, double *levels);
RHP_PUBLIB int rhp_mdl_getallvarsbasis(const rhp_mdl_t *mdl, int *basis_infos);
RHP_PUBLIB int rhp_mdl_getallvarsdual(const rhp_mdl_t *mdl, double *duals);
RHP_PUBLIB int rhp_mdl_getallvarslevel(const rhp_mdl_t *mdl, double *levels);

RHP_PUBLIB int rhp_mdl_getequbasis(const rhp_mdl_t *mdl, rhp_idx ei, int *basis_info);
RHP_PUBLIB int rhp_mdl_getequcst(const rhp_mdl_t *mdl, rhp_idx ei, double *cst);
RHP_PUBLIB int rhp_mdl_getequlevel(const rhp_mdl_t *mdl, rhp_idx ei, double *level);
RHP_PUBLIB int rhp_mdl_getequperp(const rhp_mdl_t *mdl, rhp_idx ei, rhp_idx *vi);
RHP_PUBLIB int rhp_mdl_getequdual(const rhp_mdl_t *mdl, rhp_idx ei, double *dual);
RHP_PUBLIB int rhp_mdl_getequtype(rhp_mdl_t *mdl, rhp_idx ei, unsigned *type,
                                  unsigned *cone);
RHP_PUBLIB int rhp_mdl_getmodelstat(const rhp_mdl_t *mdl, int *modelstat);
RHP_PUBLIB rhp_mathprgm_t* rhp_mdl_getmpforvar(const rhp_mdl_t *mdl, rhp_idx vi);
RHP_PUBLIB rhp_mathprgm_t* rhp_mdl_getmpforequ(const rhp_mdl_t *mdl, rhp_idx ei);
RHP_PUBLIB int rhp_mdl_getobjequ(const rhp_mdl_t *mdl, rhp_idx *objequ);
RHP_PUBLIB int rhp_mdl_getobjequs(const rhp_mdl_t *mdl, rhp_equs_t *objs);
RHP_PUBLIB int rhp_mdl_getsense(const rhp_mdl_t *mdl, unsigned *sense);
RHP_PUBLIB int rhp_mdl_getobjvar(const rhp_mdl_t *mdl, rhp_idx *objvar);
RHP_PUBLIB int rhp_mdl_gettype(const rhp_mdl_t *mdl, unsigned *type);
RHP_PUBLIB int rhp_mdl_getsolvername(const rhp_mdl_t *mdl, char const ** solvername);
RHP_PUBLIB int rhp_mdl_getsolvestat(const rhp_mdl_t *mdl, int *solvestat);
RHP_PUBLIB int rhp_mdl_getspecialfloats(const rhp_mdl_t *mdl, double *minf, double *pinf,
                                        double *nan);
RHP_PUBLIB int rhp_mdl_getvarperp(const rhp_mdl_t *mdl, rhp_idx vi, rhp_idx *ei);
RHP_PUBLIB int rhp_mdl_getvarbasis(const rhp_mdl_t *mdl, rhp_idx vi, int *basis_status);
RHP_PUBLIB int rhp_mdl_getvarbounds(const rhp_mdl_t *mdl, rhp_idx vi, double* lb,
                                    double *ub);
RHP_PUBLIB int rhp_mdl_getvarbyname(const rhp_mdl_t *mdl, const char* name, rhp_idx *vi);
RHP_PUBLIB int rhp_mdl_getvarlb(const rhp_mdl_t *mdl, rhp_idx vi, double *lb);
RHP_PUBLIB int rhp_mdl_getvarlevel(const rhp_mdl_t *mdl, rhp_idx vi, double *level);
RHP_PUBLIB int rhp_mdl_getvardual(const rhp_mdl_t *mdl, rhp_idx vi, double *dual);
RHP_PUBLIB int rhp_mdl_getvartype(const rhp_mdl_t *mdl, rhp_idx vi, unsigned *type);
RHP_PUBLIB int rhp_mdl_getvarub(const rhp_mdl_t *mdl, rhp_idx vi, double *ub);
RHP_PUBLIB unsigned rhp_mdl_nequs(const rhp_mdl_t *mdl);
RHP_PUBLIB unsigned rhp_mdl_nvars(const rhp_mdl_t *mdl);
RHP_PUBLIB const char* rhp_mdl_printequname(const rhp_mdl_t *mdl, rhp_idx ei);
RHP_PUBLIB const char* rhp_mdl_printvarname(const rhp_mdl_t *mdl, rhp_idx vi);
RHP_PUBLIB int rhp_mdl_setname(rhp_mdl_t *mdl, const char *name);
RHP_PUBLIB int rhp_mdl_resize(rhp_mdl_t *mdl, unsigned n, unsigned m);
RHP_PUBLIB int rhp_mdl_setequbasis(rhp_mdl_t *mdl, rhp_idx ei, int basis_status);
RHP_PUBLIB int rhp_mdl_setequcst(rhp_mdl_t *mdl, rhp_idx ei, double cst);
RHP_PUBLIB int rhp_mdl_setequname(rhp_mdl_t *mdl, rhp_idx ei, const char *name);
RHP_PUBLIB int rhp_mdl_setequdual(rhp_mdl_t *mdl, rhp_idx ei, double dual);
RHP_PUBLIB int rhp_mdl_setequtype(rhp_mdl_t *mdl, rhp_idx ei, unsigned type,
                                  unsigned cone);
RHP_PUBLIB int rhp_mdl_setequvarperp(rhp_mdl_t *mdl, rhp_idx ei, rhp_idx vi);
RHP_PUBLIB int rhp_mdl_setequlevel(rhp_mdl_t *mdl, rhp_idx ei, double level);
RHP_PUBLIB int rhp_mdl_settype(rhp_mdl_t *mdl, unsigned type);
RHP_PUBLIB int rhp_mdl_setobjsense(rhp_mdl_t *mdl, unsigned objsense);
RHP_PUBLIB int rhp_mdl_setobjvar(rhp_mdl_t *mdl, rhp_idx objvar);
RHP_PUBLIB int rhp_mdl_setequrhs(rhp_mdl_t *mdl, rhp_idx ei, double rhs);
RHP_PUBLIB int rhp_mdl_setsolvername(rhp_mdl_t *mdl, const char *solvername);
RHP_PUBLIB int rhp_mdl_setvarbasis(rhp_mdl_t *mdl, rhp_idx vi, int basis_status);
RHP_PUBLIB int rhp_mdl_setvarbounds(rhp_mdl_t *mdl, rhp_idx vi, double lb, double ub);
RHP_PUBLIB int rhp_mdl_setvarlb(rhp_mdl_t *mdl, rhp_idx vi, double lb);
RHP_PUBLIB int rhp_mdl_setvardual(rhp_mdl_t *mdl, rhp_idx vi, double dual);
RHP_PUBLIB int rhp_mdl_setvarname(rhp_mdl_t *mdl, rhp_idx vi, const char *name);
RHP_PUBLIB int rhp_mdl_setvartype(rhp_mdl_t *mdl, rhp_idx vi, unsigned type);
RHP_PUBLIB int rhp_mdl_setvarub(rhp_mdl_t *mdl, rhp_idx vi, double ub);
RHP_PUBLIB int rhp_mdl_setvarlevel(rhp_mdl_t *mdl, rhp_idx vi, double level);
RHP_PUBLIB int rhp_mdl_exportmodel(rhp_mdl_t *mdl, rhp_mdl_t *mdl_dst);
RHP_PUBLIB const char *rhp_mdl_modelstattxt(const rhp_mdl_t *mdl, int modelstat);
RHP_PUBLIB const char *rhp_mdl_solvestattxt(const rhp_mdl_t *mdl, int solvestat);

/* -------------------------------------------------------------------------
 * EMP info API
 * ------------------------------------------------------------------------- */

//RHP_PUBLIB
RHP_PUBLIB int rhp_ensure_mp(rhp_mdl_t *mdl, unsigned reserve);
RHP_PUBLIB void rhp_print_emp(const rhp_mdl_t *mdl);
RHP_PUBLIB int rhp_empdag_rootsetmp(rhp_mdl_t *mdl, rhp_mathprgm_t *mp);
RHP_PUBLIB int rhp_empdag_rootsetnash(rhp_mdl_t *mdl, rhp_nash_equilibrium_t *nash);

/* -------------------------------------------------------------------------
 * GAMS specific functions
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_gms_newfromcntr(const char *cntrfile, rhp_mdl_t **mdlout);
RHP_PUBLIB int rhp_gms_setgamscntr(rhp_mdl_t *mdl, const char *cntrfile);
RHP_PUBLIB int rhp_gms_setgamsdir(rhp_mdl_t *mdl, const char *gamsdir);
RHP_PUBLIB int rhp_gams_setglobalgamscntr(const char *cntrfile);
RHP_PUBLIB int rhp_gams_setglobalgamsdir(const char *gamsdir);
RHP_PUBLIB int rhp_gms_writesol2gdx(rhp_mdl_t *mdl, const char *gdxname);


/* -------------------------------------------------------------------------
 * Mathematical Programm API
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_mp_addconstraint(rhp_mathprgm_t *mp, rhp_idx ei);
RHP_PUBLIB int rhp_mp_addequ(rhp_mathprgm_t *mp, rhp_idx ei);
RHP_PUBLIB int rhp_mp_addvar(rhp_mathprgm_t *mp, rhp_idx vi);
RHP_PUBLIB int rhp_mp_addvars(rhp_mathprgm_t *mp, rhp_vars_t *v);
RHP_PUBLIB int rhp_mp_addvipair(rhp_mathprgm_t *mp, rhp_idx ei, rhp_idx vi);
RHP_PUBLIB int rhp_mp_addvipairs(rhp_mathprgm_t *mp, rhp_equs_t *e, rhp_vars_t *v);
RHP_PUBLIB int rhp_mp_finalize(rhp_mathprgm_t *mp);
RHP_PUBLIB unsigned rhp_mp_getid(const rhp_mathprgm_t *mp);
RHP_PUBLIB const rhp_mdl_t* rhp_mp_getmdl(const rhp_mathprgm_t *mp);
RHP_PUBLIB rhp_idx rhp_mp_getobjequ(const rhp_mathprgm_t *mp);
RHP_PUBLIB rhp_idx rhp_mp_getobjvar(const rhp_mathprgm_t *mp);
RHP_PUBLIB unsigned rhp_mp_getsense(const rhp_mathprgm_t *mp);
RHP_PUBLIB const char* rhp_mp_getname(const rhp_mathprgm_t *mp);
RHP_PUBLIB unsigned rhp_mp_ncons(const rhp_mathprgm_t *mp);
RHP_PUBLIB unsigned rhp_mp_nmatched(const rhp_mathprgm_t *mp);
RHP_PUBLIB unsigned rhp_mp_nvars(const rhp_mathprgm_t *mp);
RHP_PUBLIB int rhp_mp_print(rhp_mathprgm_t *mp);
RHP_PUBLIB int rhp_mp_setname(rhp_mathprgm_t *mp, const char* name);
RHP_PUBLIB int rhp_mp_setobjequ(rhp_mathprgm_t *mp, rhp_idx objequ);
RHP_PUBLIB int rhp_mp_setobjvar(rhp_mathprgm_t *mp, rhp_idx objvar);

/* -------------------------------------------------------------------------
 * EMPDAG Nash (Equilibrium among MPs) API
 * ------------------------------------------------------------------------- */
RHP_PUBLIB unsigned rhp_nash_getid(const rhp_nash_equilibrium_t *nash);
RHP_PUBLIB const char* rhp_nash_getname(const rhp_nash_equilibrium_t *nash);
RHP_PUBLIB unsigned rhp_nash_getnumchildren(const rhp_nash_equilibrium_t *nash);
RHP_PUBLIB void rhp_nash_print(rhp_nash_equilibrium_t *nash);

/* -------------------------------------------------------------------------
 * EMPDAG MP (Mathematical Programming) API
 * ------------------------------------------------------------------------- */

RHP_PUBLIB rhp_mathprgm_t *rhp_empdag_newmp(rhp_mdl_t *mdl, unsigned sense);
RHP_PUBLIB int rhp_empdag_mpaddmpVF(rhp_mdl_t *mdl, rhp_mathprgm_t *mp,
                                    rhp_mathprgm_t *mp_child, rhp_empdag_arcVF_t *arcVF);
RHP_PUBLIB int rhp_empdag_mpaddmpCTRL(rhp_mdl_t *mdl,  rhp_mathprgm_t *mp,
                                      rhp_mathprgm_t *mp_child);

/* -------------------------------------------------------------------------
 * EMPDAG Equilibrium API
 * ------------------------------------------------------------------------- */

RHP_PUBLIB rhp_nash_equilibrium_t* rhp_empdag_newnash(rhp_mdl_t *mdl);
RHP_PUBLIB int rhp_empdag_nashaddmp(rhp_mdl_t *mdl, rhp_nash_equilibrium_t* nash,
                                    rhp_mathprgm_t *mp);

/* -------------------------------------------------------------------------
 * EMPDAG arcVF (value function)
 * ------------------------------------------------------------------------- */

RHP_PUBLIB void rhp_arcVF_free(rhp_empdag_arcVF_t *arcVF);
RHP_PUBLIB RHP_MALLOC(rhp_arcVF_free) rhp_empdag_arcVF_t * rhp_arcVF_new(void);
RHP_PUBLIB int rhp_arcVF_init(rhp_empdag_arcVF_t *arcVF, rhp_idx ei);
RHP_PUBLIB int rhp_arcVF_setvar(rhp_empdag_arcVF_t *arcVF, rhp_idx vi);
RHP_PUBLIB int rhp_arcVF_setcst(rhp_empdag_arcVF_t *arcVF, double cst);

/* -------------------------------------------------------------------------
 * EMPDAG misc
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_empdag_writeDOT(rhp_mdl_t *mdl, FILE *f);

/* -------------------------------------------------------------------------
 * ReSHOP internal model modification
 * ------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------
 * RIM: adding variables
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_add_var(rhp_mdl_t *mdl, rhp_idx *vi);
RHP_PUBLIB int rhp_add_varnamed(rhp_mdl_t *mdl, rhp_idx *vi, const char *name);
RHP_PUBLIB int rhp_add_vars(rhp_mdl_t *mdl, unsigned size, rhp_vars_t *vout);
RHP_PUBLIB int rhp_add_varsnamed(rhp_mdl_t *mdl, unsigned size, rhp_vars_t *vout,
                                 const char* name);
RHP_PUBLIB int rhp_add_posvars(rhp_mdl_t *mdl, unsigned size, rhp_vars_t *vout);
RHP_PUBLIB int rhp_add_posvarsnamed(rhp_mdl_t *mdl, unsigned size, rhp_vars_t *vout,
                                    const char* name);
RHP_PUBLIB int rhp_add_negvars(rhp_mdl_t *mdl, unsigned size, rhp_vars_t *vout);
RHP_PUBLIB int rhp_add_negvarsnamed(rhp_mdl_t *mdl, unsigned size, rhp_vars_t *vout,
                                    const char* name);
RHP_PUBLIB int rhp_add_varsinbox(rhp_mdl_t *mdl, unsigned size, rhp_vars_t *vout,
                                 double lb, double ub);
RHP_PUBLIB int rhp_add_varsinboxnamed(rhp_mdl_t *mdl, unsigned size, rhp_vars_t *vout,
                                      const char* name, double lb, double ub);
RHP_PUBLIB int rhp_add_varsinboxes(rhp_mdl_t *mdl, unsigned size, rhp_vars_t *vout,
                                   double *lbs, double *ubs);
RHP_PUBLIB int rhp_add_varsinboxesnamed(rhp_mdl_t *mdl, unsigned size, rhp_vars_t *vout,
                                        const char* name, double *lbs, double *ubs);

RHP_PUBLIB int rhp_set_var_sos1(rhp_mdl_t *mdl, rhp_vars_t *v, double *weights);
RHP_PUBLIB int rhp_set_var_sos2(rhp_mdl_t *mdl, rhp_vars_t *v, double *weights);

/* -------------------------------------------------------------------------
 * RIM: adding equation
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_add_con(rhp_mdl_t *mdl, unsigned type, rhp_idx *ei);
RHP_PUBLIB int rhp_add_connamed(rhp_mdl_t *mdl, unsigned size, rhp_idx *ei,
                                const char *name);
RHP_PUBLIB int rhp_add_cons(rhp_mdl_t *mdl, unsigned size, unsigned type,
                            rhp_equs_t *eout);
RHP_PUBLIB int rhp_add_consnamed(rhp_mdl_t *mdl, unsigned size, unsigned type,
                                 rhp_equs_t *eout, const char *name);
RHP_PUBLIB int rhp_add_func(rhp_mdl_t *mdl, rhp_idx *ei);
RHP_PUBLIB int rhp_add_funcnamed(rhp_mdl_t *mdl, rhp_idx *ei, const char *name);
RHP_PUBLIB int rhp_add_funcs(rhp_mdl_t *mdl, unsigned size, rhp_equs_t *eout);
RHP_PUBLIB int rhp_add_funcsnamed(rhp_mdl_t *mdl, unsigned size, rhp_equs_t *eout,
                                  const char *name);

RHP_PUBLIB int rhp_mdl_setobjequ(rhp_mdl_t *mdl, rhp_idx ei);
/*  TODO: see if we can delete the rest */
//RHP_PUBLIB
//int rmdl_addinit_equs(rhp_mdl_t *mdl, unsigned nb);
RHP_PUBLIB int rhp_add_equation(rhp_mdl_t *mdl, rhp_idx *ei);
RHP_PUBLIB int rhp_add_equations(rhp_mdl_t *mdl, unsigned nb, rhp_equs_t *eout);
RHP_PUBLIB int rhp_add_equality_constraint(rhp_mdl_t *mdl, rhp_idx *ei);
RHP_PUBLIB int rhp_add_exp_constraint(rhp_mdl_t *mdl, rhp_idx *ei);
RHP_PUBLIB int rhp_add_greaterthan_constraint(rhp_mdl_t *mdl, rhp_idx *ei);
RHP_PUBLIB int rhp_add_lessthan_constraint(rhp_mdl_t *mdl, rhp_idx *ei);
RHP_PUBLIB int rhp_add_power_constraint(rhp_mdl_t *mdl, rhp_idx *ei);
RHP_PUBLIB int rhp_add_soc_constraint(rhp_mdl_t *mdl, rhp_idx *ei);

RHP_PUBLIB int rhp_add_equality_constraint_named(rhp_mdl_t *mdl, rhp_idx *ei,
                                                 const char *name);
RHP_PUBLIB int rhp_add_exp_constraint_named(rhp_mdl_t *mdl, rhp_idx *ei,
                                            const char *name);
RHP_PUBLIB int rhp_add_greaterthan_constraint_named(rhp_mdl_t *mdl, rhp_idx *ei,
                                                    const char *name);
RHP_PUBLIB int rhp_add_lessthan_constraint_named(rhp_mdl_t *mdl, rhp_idx *ei,
                                                 const char *name);
RHP_PUBLIB int rhp_add_power_constraint_named(rhp_mdl_t *mdl, rhp_idx *ei,
                                              const char *name);
RHP_PUBLIB int rhp_add_soc_constraint_named(rhp_mdl_t *mdl, rhp_idx *ei,
                                            const char *name);

/* -------------------------------------------------------------------------
 * RIM: equation modification
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_equ_addbilin(rhp_mdl_t *mdl, rhp_idx ei, rhp_vars_t *v1,
                     rhp_vars_t *v2, double coeff);
RHP_PUBLIB int rhp_equ_addlin(rhp_mdl_t *mdl, rhp_idx ei, rhp_vars_t *v, const double *coeffs);
RHP_PUBLIB int rhp_equ_addlinchk(rhp_mdl_t *mdl, rhp_idx ei, rhp_vars_t *v, const double *coeffs);
RHP_PUBLIB int rhp_equ_addlincoeff(rhp_mdl_t *mdl, rhp_idx ei, rhp_vars_t *v, const double *coeffs, double coeff);
RHP_PUBLIB int rhp_equ_addquadrelative(rhp_mdl_t *mdl, rhp_idx ei, rhp_vars_t *v_row,
                                       rhp_vars_t *v_col, size_t nnz, unsigned *i, unsigned *j,
                                       double *x, double coeff);
RHP_PUBLIB int rhp_equ_addquadabsolute(rhp_mdl_t *mdl, rhp_idx ei, size_t nnz, unsigned *i,
                                       unsigned *j, double *x, double coeff);
RHP_PUBLIB int rhp_equ_addlvar(rhp_mdl_t *mdl, rhp_idx ei, rhp_idx vi, double val);
RHP_PUBLIB int rhp_equ_addnewlvar(rhp_mdl_t *mdl, rhp_idx ei, rhp_idx vi, double val);
RHP_PUBLIB int rhp_equ_setcst(rhp_mdl_t *mdl, rhp_idx ei, double cst);

RHP_PUBLIB int rhp_equ_getcst(rhp_mdl_t *mdl, rhp_idx ei, double *cst);
RHP_PUBLIB int rhp_equ_getlin(rhp_mdl_t *mdl, rhp_idx ei, unsigned *len, rhp_idx **idxs, double **vals);

/* -------------------------------------------------------------------------
 * RIM: equation/variable removal (be careful)
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_delete_equ(rhp_mdl_t *mdl, rhp_idx ei);
RHP_PUBLIB int rhp_delete_var(rhp_mdl_t *mdl, rhp_idx vi);

RHP_PUBLIB int rhp_is_var_valid(rhp_mdl_t *mdl, rhp_idx vi);
RHP_PUBLIB int rhp_is_equ_valid(rhp_mdl_t *mdl, rhp_idx ei);

/* -------------------------------------------------------------------------
 * nltree, for nonlinear equation, functions
 * ------------------------------------------------------------------------- */
RHP_PUBLIB int rhp_nltree_getroot(rhp_nltree_t *tree, rhp_nlnode_t ***node);
RHP_PUBLIB int rhp_nltree_getchild(rhp_nlnode_t **node, rhp_nlnode_t ***child, unsigned i);
RHP_PUBLIB int rhp_nltree_getchild2(rhp_nlnode_t ***node, rhp_nlnode_t ***child, unsigned i);
RHP_PUBLIB int rhp_nltree_arithm(rhp_nltree_t *tree, rhp_nlnode_t ***node,
                                 unsigned opcode, unsigned nb);
RHP_PUBLIB int rhp_nltree_call(rhp_mdl_t *mdl, rhp_nltree_t *tree, rhp_nlnode_t ***node,
                               unsigned opcode, unsigned n_args);
RHP_PUBLIB int rhp_nltree_cst(rhp_mdl_t *mdl, rhp_nltree_t* tree, rhp_nlnode_t ***node,
                              double cst);
RHP_PUBLIB int rhp_nltree_umin(rhp_nltree_t *tree, rhp_nlnode_t ** restrict *node);
RHP_PUBLIB int rhp_nltree_var(rhp_mdl_t *mdl, rhp_nltree_t* tree, rhp_nlnode_t ***node,
                              rhp_idx vi, double coeff);
RHP_PUBLIB int rhp_nltree_addquad(rhp_mdl_t *mdl, rhp_idx ei, struct rhp_spmat* mat,
                                  rhp_vars_t *v, double coeff);
RHP_PUBLIB int rhp_nltree_addlin(rhp_mdl_t *mdl, rhp_idx ei, double* c, rhp_vars_t *v,
                                 double coeff);

/* -------------------------------------------------------------------------
 * ReSHOP internal model change
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_mdl_reserve_equs(rhp_mdl_t *mdl, unsigned size);
RHP_PUBLIB int rhp_mdl_reserve_vars(rhp_mdl_t *mdl, unsigned size);
RHP_PUBLIB unsigned rhp_mdl_nvars_total(const rhp_mdl_t *mdl);
RHP_PUBLIB unsigned rhp_mdl_nequs_total(const rhp_mdl_t *mdl);

RHP_PUBLIB int rhp_mdl_latex(rhp_mdl_t *mdl, const char *filename);

RHP_PUBLIB rhp_nltree_t* rhp_mdl_getnltree(const rhp_mdl_t *mdl, rhp_idx ei);

/* -------------------------------------------------------------------------
 * OVF (Optimal Value Function) API
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_ovf_add(rhp_mdl_t *mdl, const char* name, rhp_idx ovf_vi,
                           rhp_vars_t *v_args, rhp_ovf_t **ovf_def);
RHP_PUBLIB int rhp_ovf_check(rhp_mdl_t *mdl, rhp_ovf_t *ovf_def);
RHP_PUBLIB int rhp_ovf_param_add_scalar(rhp_ovf_t* ovf_def, const char *param_name,
                                        double val);
RHP_PUBLIB int rhp_ovf_param_add_vector(rhp_ovf_t *ovf_def, const char *param_name,
                                        unsigned size, double *vec);
RHP_PUBLIB int rhp_ovf_setreformulation(rhp_ovf_t *ovf_def, const char *reformulation);

/* -------------------------------------------------------------------------
 * ReSHOP model management
 * ------------------------------------------------------------------------- */
RHP_PUBLIB RHP_MALLOC(rhp_mdl_free) rhp_mdl_t *rhp_newsolvermdl(rhp_mdl_t *mdl);

/* -------------------------------------------------------------------------
 * ReSHOP main API
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_process(rhp_mdl_t *mdl, rhp_mdl_t *mdl_solver);
RHP_PUBLIB int rhp_solve(rhp_mdl_t *mdl);
RHP_PUBLIB int rhp_postprocess(rhp_mdl_t *mdl_solver);

/* -------------------------------------------------------------------------
 * ReSHOP printing related 
 * ------------------------------------------------------------------------- */

RHP_PUBLIB void rhp_set_printops(void* data, rhp_print_fn print, rhp_flush_fn flush,
                      unsigned flags);
RHP_PUBLIB void rhp_set_printopsdefault(void);

/* -------------------------------------------------------------------------
 * ReSHOP misc functions
 * ------------------------------------------------------------------------- */

RHP_PUBLIB const char *rhp_status_descr(int status);
RHP_PUBLIB size_t rhp_getidxmax(void);
RHP_PUBLIB const char * rhp_sensestr(unsigned sense);

RHP_PUBLIB enum rhp_backendtype rhp_mdl_getbackend(const rhp_mdl_t *mdl);
RHP_PUBLIB const char* rhp_mdl_getbackendname(const rhp_mdl_t *mdl);
RHP_PUBLIB const char* rhp_mdl_getname(const rhp_mdl_t *mdl);
RHP_PUBLIB unsigned rhp_mdl_getid(const rhp_mdl_t *mdl);

/* -------------------------------------------------------------------------
 * Get some stats about the problem
 * ------------------------------------------------------------------------- */

RHP_PUBLIB size_t rhp_get_nb_lequ_le(rhp_mdl_t *mdl);
RHP_PUBLIB size_t rhp_get_nb_lequ_ge(rhp_mdl_t *mdl);
RHP_PUBLIB size_t rhp_get_nb_lequ_eq(rhp_mdl_t *mdl);
RHP_PUBLIB size_t rhp_get_nb_var_bin(rhp_mdl_t *mdl);
RHP_PUBLIB size_t rhp_get_nb_var_int(rhp_mdl_t *mdl);
RHP_PUBLIB size_t rhp_get_nb_var_lb(rhp_mdl_t *mdl);
RHP_PUBLIB size_t rhp_get_nb_var_ub(rhp_mdl_t *mdl);
RHP_PUBLIB size_t rhp_get_nb_var_interval(rhp_mdl_t *mdl);
RHP_PUBLIB size_t rhp_get_nb_var_fx(rhp_mdl_t *mdl);
RHP_PUBLIB size_t rhp_get_nb_var_sos1(rhp_mdl_t *mdl);
RHP_PUBLIB size_t rhp_get_nb_var_sos2(rhp_mdl_t *mdl);

RHP_PUBLIB int rhp_get_var_sos1(rhp_mdl_t *mdl, rhp_idx vi, unsigned **grps);
RHP_PUBLIB int rhp_get_var_sos2(rhp_mdl_t *mdl, rhp_idx vi, unsigned **grps);

/* -------------------------------------------------------------------------
 * Option management for the ReSHOP container
 * ------------------------------------------------------------------------- */

RHP_PUBLIB int rhp_mdl_setopt_b(const rhp_mdl_t *mdl, const char *optname, unsigned char opt);
RHP_PUBLIB int rhp_mdl_setopt_c(const rhp_mdl_t *mdl, const char *optname, char *choice);
RHP_PUBLIB int rhp_mdl_setopt_d(const rhp_mdl_t *mdl, const char *optname, double optval);
RHP_PUBLIB int rhp_mdl_setopt_i(const rhp_mdl_t *mdl, const char *optname, int optval);
RHP_PUBLIB int rhp_mdl_setopt_s(const rhp_mdl_t *mdl, const char *optname, char *optstr);
RHP_PUBLIB int rhp_mdl_getopttype(const rhp_mdl_t *mdl, const char *optname, unsigned *type);

RHP_PUBLIB int rhp_opt_setb(const char *name, unsigned char boolval);
RHP_PUBLIB int rhp_opt_setc(const char *name, const char *str);
RHP_PUBLIB int rhp_opt_setd(const char *name, double dval);
RHP_PUBLIB int rhp_opt_seti(const char *name, int ival);
RHP_PUBLIB int rhp_opt_sets(const char *name, const char *value);
RHP_PUBLIB int rhp_opt_setfromstr(const char *optstring);

RHP_PUBLIB int rhp_opt_getb(const char *name, int *boolval);
RHP_PUBLIB int rhp_opt_getd(const char *name, double *dval);
RHP_PUBLIB int rhp_opt_geti(const char *name, int *ival);
RHP_PUBLIB int rhp_opt_gets(const char *name, const char **str);
RHP_PUBLIB int rhp_opt_gettype(const char *name, unsigned *type);

/* -------------------------------------------------------------------------
 * Matrix utilities
 * ------------------------------------------------------------------------- */
RHP_PUBLIB void rhp_mat_free(struct rhp_spmat* m);
RHP_PUBLIB struct rhp_spmat* rhp_mat_triplet(unsigned n, unsigned m, unsigned nnz,
                                             int *rowidx, int *colidx, double *data);


/* -------------------------------------------------------------------------
 * Misc information
 * ------------------------------------------------------------------------- */
RHP_PUBLIB const char* rhp_basis_str(enum rhp_basis_status basis);
RHP_PUBLIB const char* rhp_backend_str(enum rhp_backendtype backend);

/* -------------------------------------------------------------------------
 * Solver specific info
 * ------------------------------------------------------------------------- */
RHP_PUBLIB int rhp_PATH_setfname(const char* fname);

/* ------------------------------------------------------------------------
 * Additional display info
 * ------------------------------------------------------------------------ */
RHP_PUBLIB void rhp_print_banner(void);
RHP_PUBLIB const char* rhp_version(void);
RHP_PUBLIB void rhp_set_userinfo(const char *userinfo);
RHP_PUBLIB void rhp_show_ccftrace(unsigned char boolval);
RHP_PUBLIB void rhp_show_containertrace(unsigned char boolval);
RHP_PUBLIB void rhp_show_empdagtrace(unsigned char boolval);
RHP_PUBLIB void rhp_show_empinterptrace(unsigned char boolval);
RHP_PUBLIB void rhp_show_empparsertrace(unsigned char boolval);
RHP_PUBLIB void rhp_show_fooctrace(unsigned char boolval);
RHP_PUBLIB void rhp_show_processtrace(unsigned char boolval);
RHP_PUBLIB void rhp_show_refcnttrace(unsigned char boolval);
RHP_PUBLIB void rhp_show_solreporttrace(unsigned char boolval);
RHP_PUBLIB void rhp_show_backendinfo(unsigned char boolval);
RHP_PUBLIB void rhp_show_timings(unsigned char boolval);
RHP_PUBLIB void rhp_show_solver_log(unsigned char boolval);
RHP_PUBLIB int rhp_syncenv(void);

#undef RHP_PUBDLL_ATTR
#undef RHP_NODISCARD
#undef RHP_PUBLIB

#endif /* RESHOP_H  */
