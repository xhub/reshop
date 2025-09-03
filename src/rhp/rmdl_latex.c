#include "asprintf.h"

#include <stdio.h>

#include "container.h"
#include "ctrdat_rhp.h"
#include "empinfo.h"
#include "equ.h"
#include "equvar_metadata.h"
#include "instr.h"
#include "mdl.h"
#include "mdl_gams.h"
#include "mdl_rhp.h"
#include "nltree.h"
#include "nltree_priv.h"
#include "pool.h"
#include "pprint.h"
#include "printout.h"
#include "rhp_fwd.h"
#include "status.h"
#include "var.h"

#define DEFINE_STR() \
DEFSTR(VAR_X," ") \
DEFSTR(VAR_B,"\\in \\mathbb{B}") \
DEFSTR(VAR_I,"\\in \\mathbb{Z}") \
DEFSTR(VAR_SOS1,"SOS1") \
DEFSTR(VAR_SOS2,"SOS2") \
DEFSTR(VAR_SC,"semi-continuous") \
DEFSTR(VAR_SI,"semi-integer") \
DEFSTR(VAR_IND,"indicator") \
DEFSTR(VAR_POLYHEDRAL,"polyhedral cone") \
DEFSTR(VAR_SOC,"\\in K_{\\mathrm{SOC}}") \
DEFSTR(VAR_RSOC,"\\in K_{\\mathrm{RSOC}}") \
DEFSTR(VAR_EXP,"\\in K_{\\mathrm{EXP}}") \
DEFSTR(VAR_DEXP,"\\in K^*_{\\mathrm{EXP}}") \
DEFSTR(VAR_POWER,"\\in K_{\\mathrm{POWER}}") \
DEFSTR(VAR_DPOWER,"\\in K^*_{\\mathrm{POWER}}")

#define DEFSTR(id, str) char id[sizeof(str)];

typedef struct {
   union {
      struct {
      DEFINE_STR()
   };
   char dummystr[1];
   };
} VarNames;
#undef DEFSTR

#define DEFSTR(id, str) .id = str,

static const VarNames varnames = {
DEFINE_STR()
};
#undef DEFSTR

#define DEFSTR(id, str) offsetof(VarNames, id),
static const unsigned varnames_offsets[] = {
DEFINE_STR()
};

RESHOP_STATIC_ASSERT(sizeof(varnames_offsets)/sizeof(varnames_offsets[0]) == VAR_TYPE_LEN,
                     "var_type and var_type_latex must be synchronized")


static const char * const func_name[fndummy+1][2] = {
   {"\\operatorname{mapval}",""},
   {"\\operatorname{ceil}",""},
   {"\\operatorname{floor}",""},
   {"\\operatorname{round}",""},
   {"\\operatorname{mod}",""},
   {"\\operatorname{trunc}",""},             /*  5 */
   {"\\operatorname{sgn}",""},
   {"\\min\\,\\{","\\}"},
   {"\\max\\,\\{","\\}"},
   {"{","}^2"},
   {"\\operatorname{exp}",""},               /* 10 */
   {"\\operatorname{log}",""},
   {"\\operatorname{log10}",""},
   {"\\sqrt{","}"},
   {"\\left|", "\\right|"},
   {"\\operatorname{cos}",""},               /* 15 */
   {"\\operatorname{sin}",""},
   {"\\operatorname{arctan}",""},
   {"\\operatorname{errf}",""},
   {"\\operatorname{dunfm}",""},
   {"\\operatorname{dnorm}",""},             /* 20 */
   {"\\operatorname{power}",""},
   {"\\operatorname{jdate}",""},
   {"\\operatorname{jtime}",""},
   {"\\operatorname{jstart}",""},
   {"\\operatorname{jnow}",""},              /* 25 */
   {"\\operatorname{error}",""},
   {"\\operatorname{gyear}",""},
   {"\\operatorname{gmonth}",""},
   {"\\operatorname{gday}",""},
   {"\\operatorname{gdow}",""},              /* 30 */
   {"\\operatorname{gleap}",""},
   {"\\operatorname{ghour}",""},
   {"\\operatorname{gminute}",""},
   {"\\operatorname{gsecond}",""},
   {"\\operatorname{curseed}",""},           /* 35 */
   {"\\operatorname{timest}",""},
   {"\\operatorname{timeco}",""},
   {"\\operatorname{timeex}",""},
   {"\\operatorname{timecl}",""},
   {"\\operatorname{frac}",""},              /* 40 */
   {"\\operatorname{errorl}",""},
   {"\\operatorname{heaps}",""},
   {"\\operatorname{fact}",""},
   {"\\operatorname{unfmi}",""},
   {"\\operatorname{pi}",""},                /* 45 */
   {"\\operatorname{ncpf}",""},
   {"\\operatorname{ncpcm}",""},
   {"\\operatorname{entropy}",""},
   {"\\operatorname{sigmoid}",""},
   {"\\operatorname{log2}",""},              /* 50 */
   {"\\operatorname{boolnot}",""},
   {"\\operatorname{booland}",""},
   {"\\operatorname{boolor}",""},
   {"\\operatorname{boolxor}",""},
   {"\\operatorname{boolimp}",""},           /* 55 */
   {"\\operatorname{booleqv}",""},
   {"\\operatorname{relopeq}",""},
   {"\\operatorname{relopgt}",""},
   {"\\operatorname{relopge}",""},
   {"\\operatorname{reloplt}",""},           /* 60 */
   {"\\operatorname{relople}",""},
   {"\\operatorname{relopne}",""},
   {"\\operatorname{ifthen}",""},
   {"\\power{",""},
   {"\\operatorname{edist}",""},             /* 65 */
   {"\\operatorname{div}",""},
   {"\\operatorname{div0}",""},
   {"\\operatorname{sllog10}",""},
   {"\\operatorname{sqlog10}",""},
   {"\\operatorname{slexp}",""},             /* 70 */
   {"\\operatorname{sqexp}",""},
   {"\\operatorname{slrec}",""},
   {"\\operatorname{sqrec}",""},
   {"\\power{",""},
   {"\\power{",""},           /* 75 */
   {"\\operatorname{centropy}",""},
   {"\\operatorname{gmillisec}",""},
   {"\\operatorname{maxerror}",""},
   {"\\operatorname{timeel}",""},
   {"\\operatorname{gamma}",""},             /* 80 */
   {"\\operatorname{loggamma}",""},
   {"\\operatorname{beta}",""},
   {"\\operatorname{logbeta}",""},
   {"\\operatorname{gammareg}",""},
   {"\\operatorname{betareg}",""},           /* 85 */
   {"\\operatorname{sinh}",""},
   {"\\operatorname{cosh}",""},
   {"\\operatorname{tanh}",""},
   {"\\operatorname{mathlastrc}",""},
   {"\\operatorname{mathlastec}",""},        /* 90 */
   {"\\operatorname{mathoval}",""},
   {"\\operatorname{signpower}",""},
   {"\\operatorname{handle}",""},
   {"\\operatorname{ncpvusin}",""},
   {"\\operatorname{ncpvupow}",""},          /* 95 */
   {"\\operatorname{binomial}",""},
   {"\\operatorname{rehandle}",""},
   {"\\operatorname{gamsver}",""},
   {"\\operatorname{delhandle}",""},
   {"\\operatorname{tan}",""},               /* 100 */
   {"\\operatorname{arccos}",""},
   {"\\operatorname{arcsin}",""},
   {"\\operatorname{arctan2}",""},
   {"\\operatorname{sleep}",""},
   {"\\operatorname{heapf}",""},             /* 105 */
   {"\\operatorname{cohandle}",""},
   {"\\operatorname{gamsrel}",""},
   {"\\operatorname{poly}",""},
   {"\\operatorname{licensestatus}",""},
   {"\\operatorname{licenselevel}",""},      /* 110 */
   {"\\operatorname{heaplimit}",""},
   {"\\operatorname{dummy}",""},
};


static inline NONNULL int _printvarname(const Container *ctr, rhp_idx i, FILE *f)
{
   IO_PRINT(fputs("\\text{\\textbf{\\detokenize{", f));
   IO_PRINT(fputs(ctr_printvarname(ctr, i), f));
   IO_PRINT(fputs("}}}", f));

   return OK;
}

static inline NONNULL int _printequname(const Container *ctr, rhp_idx i, FILE *f)
{
   IO_PRINT(fputs("\n \\textbf{\\detokenize{", f));
   IO_PRINT(fputs(ctr_printequname(ctr, i), f));
   IO_PRINT(fputs("}}: \\begin{dmath}", f));

   return OK;
}


#define VAR_PRE(node, ctr, f) \
   S_CHECK(_printvarname(ctr, VIDX_R(node->value), f));
#define VAR_IN(node, ctr, f) 
#define VAR_POST(node, ctr, f) 

#define CST_PRE(node, ctr, f) \
   S_CHECK(pprint_f(ctr->pool->data[CIDX_R(node->value)], f, false));
#define CST_IN(node, ctr, f) 
#define CST_POST(node, ctr, f) 

#define ADD_PRE(node, ctr, f) IO_PRINT(fputs(" \\left( ", f));
#define ADD_IN(node, ctr, f) IO_PRINT(fputs(" + ", f));
#define ADD_POST(node, ctr, f) IO_PRINT(fputs(" \\right) ", f));

#define SUB_PRE(node, ctr, f) IO_PRINT(fputs(" \\left( ", f));
#define SUB_IN(node, ctr, f) IO_PRINT(fputs(" - ", f));
#define SUB_POST(node, ctr, f) IO_PRINT(fputs(" \\right) ", f));

#define MUL_PRE(node, ctr, f) IO_PRINT(fputs(" ", f));
#define MUL_IN(node, ctr, f) IO_PRINT(fputs(" \\cdot", f));
#define MUL_POST(node, ctr, f) IO_PRINT(fputs(" ", f));

#define DIV_PRE(node, ctr, f) IO_PRINT(fputs(" \\revfrac{ ", f));
#define DIV_IN(node, ctr, f) IO_PRINT(fputs(" }{ ", f));
#define DIV_POST(node, ctr, f) IO_PRINT(fputs(" } ", f));

#define UMIN_PRE(node, ctr, f) IO_PRINT(fputs(" - (", f));
#define UMIN_IN(node, ctr, f) 
#define UMIN_POST(node, ctr, f) IO_PRINT(fputs(" )", f));

#define CALL1_PRE(node, ctr, f) IO_PRINT(fputs(func_name[node->value][0], f));
#define CALL1_IN(node, ctr, f)
#define CALL1_POST(node, ctr, f) IO_PRINT(fputs(func_name[node->value][1], f));

#define CALL2_PRE(node, ctr, f) IO_PRINT(fputs(func_name[node->value][0], f));
#define CALL2_IN(node, ctr, f) IO_PRINT(fputs(" , ", f));
#define CALL2_POST(node, ctr, f) IO_PRINT(fputs(func_name[node->value][1], f));

#define VISIT_NODE _node_latex
#define ENV_TYPE FILE*

#include "nltree_transversal.inc"

static int nltree_latex(Container *ctr, NlTree *tree, FILE *f)
{
   if (!tree->root) {
      return OK;
   }

   S_CHECK(VISIT_NODE(tree->root, ctr, f));
   return OK;
}
static const char * var_type_latex(enum var_type typ)
{
   if (typ >= VAR_TYPE_LEN) { return "unknown var type"; }   

   return varnames.dummystr + varnames_offsets[typ];
}

static NONNULL int _print_equconesymbol(Equ *e, FILE *f)
{
   int status = OK;

   switch (e->cone) {
   case CONE_R_PLUS:
   case CONE_R_MINUS:
   case CONE_0:
      IO_PRINT_EXIT(pprint_f(-equ_get_cst(e), f, false));
      break;
   case CONE_R:
      break;
   case CONE_POLYHEDRAL:
      IO_PRINT_EXIT(fputs("K_{\\mathrm{poly}} ", f));
      break;
   case CONE_SOC:
      IO_PRINT_EXIT(fputs("K_{\\mathrm{SOC}} ", f));
      break;
   case CONE_RSOC:
      IO_PRINT_EXIT(fputs("K_{\\mathrm{RSOC}} ", f));
      break;
   case CONE_EXP:
      IO_PRINT_EXIT(fputs("K_{\\mathrm{EXP}} ", f));
      break;
   case CONE_DEXP:
      IO_PRINT_EXIT(fputs("K^*_{\\mathrm{EXP}} ", f));
      break;
   case CONE_POWER:
      IO_PRINT_EXIT(fputs("K_{\\mathrm{POWER}} ", f));
      break;
   case CONE_DPOWER:
      IO_PRINT_EXIT(fputs("K^*_{\\mathrm{POWER}} ", f));
      break;
   default:
      IO_PRINT_EXIT(fprintf(f, "Unsupported cone %d ", e->cone));
   }

_exit:
   return status;
}

static NONNULL int _print_varconesymbol(Var *v, FILE *f)
{
   int status = OK;

   if (v->type == VAR_SI) {
      IO_PRINT_EXIT(fputs("semi-integer", f));
   }
   else if (!v->is_conic) {
      if (isfinite(v->bnd.lb) && isfinite(v->bnd.ub)) {
         IO_PRINT_EXIT(fputs("\\in [ ", f));
         IO_PRINT_EXIT(pprint_f(v->bnd.lb, f, false));
         IO_PRINT_EXIT(fputs(", ", f));
         IO_PRINT_EXIT(pprint_f(v->bnd.ub, f, false));
         IO_PRINT_EXIT(fputs("] ", f));
      } else if (isfinite(v->bnd.ub)) {
         IO_PRINT_EXIT(fputs("\\leq ", f));
         IO_PRINT_EXIT(pprint_f(v->bnd.ub, f, false));
      } else if (isfinite(v->bnd.lb)) {
         IO_PRINT_EXIT(fputs("\\geq ", f));
         IO_PRINT_EXIT(pprint_f(v->bnd.lb, f, false));
      } else {
         IO_PRINT_EXIT(fputs("\\text{ free} ", f));
      }
   } else {

      IO_PRINT_EXIT(fputs(" \\in ", f));

      switch (v->cone.type) {
      case CONE_R_PLUS:
         IO_PRINT_EXIT(fputs("\\mathbb{R}_+ ", f));
         break;
      case CONE_R_MINUS:
         IO_PRINT_EXIT(fputs("\\mathbb{R}_- ", f));
         break;
      case CONE_0:
         IO_PRINT_EXIT(fputs(" \\{0\\} ", f));
         break;
      case CONE_R:
         break;
      case CONE_POLYHEDRAL:
         IO_PRINT_EXIT(fputs("K_{\\mathrm{poly}} ", f));
         break;
      case CONE_SOC:
         IO_PRINT_EXIT(fputs("K_{\\mathrm{SOC}} ", f));
         break;
      case CONE_RSOC:
         IO_PRINT_EXIT(fputs("K_{\\mathrm{RSOC}} ", f));
         break;
      case CONE_EXP:
         IO_PRINT_EXIT(fputs("K_{\\mathrm{EXP}} ", f));
         break;
      case CONE_DEXP:
         IO_PRINT_EXIT(fputs("K^*_{\\mathrm{EXP}} ", f));
         break;
      case CONE_POWER:
         IO_PRINT_EXIT(fputs("K_{\\mathrm{POWER}} ", f));
         break;
      case CONE_DPOWER:
         IO_PRINT_EXIT(fputs("K^*_{\\mathrm{POWER}} ", f));
         break;
      default:
         IO_PRINT_EXIT(fprintf(f, "\\text{Unsupported cone %d} ", v->cone.type));
      }
   }

_exit:
   return status;
}

UNUSED static NONNULL int _print_cone_relation(enum cone cone, FILE *f)
{
   int status = OK;

   switch (cone) {
   case CONE_R_PLUS:
      IO_PRINT_EXIT(fputs(" \\geq ", f));
      break;
   case CONE_R_MINUS:
      IO_PRINT_EXIT(fputs(" \\leq ", f));
      break;
   case CONE_R:
      break;
   case CONE_0:
      IO_PRINT_EXIT(fputs(" = ", f));
      break;
   case CONE_POLYHEDRAL:
   case CONE_SOC:
   case CONE_RSOC:
   case CONE_EXP:
   case CONE_DEXP:
   case CONE_POWER:
   case CONE_DPOWER:
      IO_PRINT_EXIT(fputs(" \\in ", f));
      break;
   case CONE_NONE:
      break;
   default:
      IO_PRINT_EXIT(fprintf(f, "\\text{Unsupported cone %d} ", cone));
   }

_exit:
   return status;
}

static NONNULL int _print_cone_relation_rev(enum cone cone, FILE *f)
{
   int status = OK;

   switch (cone) {
   case CONE_R_PLUS:
      IO_PRINT_EXIT(fputs(" \\geq ", f));
      break;
   case CONE_R_MINUS:
      IO_PRINT_EXIT(fputs(" \\leq ", f));
      break;
   case CONE_R:
      break;
   case CONE_0:
      IO_PRINT_EXIT(fputs(" = ", f));
      break;
   case CONE_POLYHEDRAL:
   case CONE_SOC:
   case CONE_RSOC:
   case CONE_EXP:
   case CONE_DEXP:
   case CONE_POWER:
   case CONE_DPOWER:
      IO_PRINT_EXIT(fputs(" \\ni ", f));
      break;
   default:
      IO_PRINT_EXIT(fprintf(f, "\\text{Unsupported cone %d} ", cone));
   }

_exit:
   return status;
}

UNUSED static NONNULL int _print_equconerev(Equ *e, FILE *f)
{
   S_CHECK(_print_equconesymbol(e, f));
   S_CHECK(_print_cone_relation_rev(e->cone, f));

   return OK;
}

static NONNULL int _print_equcone(Equ *e, FILE *f)
{
   if (e->object == ConeInclusion) {
      S_CHECK(_print_cone_relation_rev(e->cone, f));
      S_CHECK(_print_equconesymbol(e, f));
   }

   return OK;
}

/**
 * @brief output a latex representation of the model
 *
 * @param mdl    the model to output
 * @param fname  the filename for the output
 *
 * @return       the error code
 */
static int rmdl_latex(Model *mdl, const char *fname)
{
   assert(mdl_is_rhp(mdl));
   int status = OK;

   Container *ctr = &mdl->ctr;
   struct ctrdata_rhp *model = (struct ctrdata_rhp *)ctr->data;
   size_t total_n = model->total_n, total_m = model->total_m;

   //struct var_meta * restrict vmd = ctr->varmeta;
   struct equ_meta * restrict emd = ctr->equmeta;

   FILE *f = fopen(fname, RHP_WRITE_TEXT "+");
   if (!f) {
      error("%s :: error opening file named %s\n", __func__, fname);
      return Error_FileOpenFailed;
   }

   if (mdl->empinfo.empdag.type == EmpDag_Empty) {
      IO_PRINT_EXIT(fputs("Classical optimization Problem\n", f));

      RhpSense sense;
      S_CHECK_EXIT(rmdl_getsense(mdl, &sense));

      switch (sense) {
      case RhpMin:
         IO_PRINT_EXIT(fputs("minimize ", f));
         break;
      case RhpMax:
         IO_PRINT_EXIT(fputs("maximize ", f));
         break;
      case RhpFeasibility:
         IO_PRINT_EXIT(fputs("feasibility ", f));
         break;
      case RhpNoSense:
         IO_PRINT_EXIT(fputs("sense not set ", f));
         break;
      default:
         IO_PRINT_EXIT(fputs("unexpected sense ", f));
      }

      rhp_idx objvar, objequ;
      rmdl_getobjvar(mdl, &objvar);
      rmdl_getobjequ(mdl, &objequ);

      if (valid_vi(objvar)) {
         S_CHECK_EXIT(_printvarname(ctr, objvar, f))
      } else if (valid_ei(objequ)) {
         S_CHECK_EXIT(_printequname(ctr, objequ, f))

      }
   } else {

      IO_PRINT_EXIT(fputs("EMP DAG structure\n", f));
      /* TODO: implementent tikz */

//      IO_PRINT_EXIT(fputs("\\begin{tikzpicture}\\node(0,0){", f));
//      if (root->type == MP_TREE_MP) {
//         IO_PRINT_EXIT(fprintf(f, "MP %u ", root->p.mp->id));
//      } else {
//         IO_PRINT_EXIT(fputs("Equil", f));
//      }
//      IO_PRINT_EXIT(fputs("};\n", f));
//      IO_PRINT_EXIT(fputs("\\end{tikzpicture}\n", f));
   }
   /*  We cannot execute this check since some reformulation need to introduce more variable
    *  than effectively needed*/
   IO_PRINT_EXIT(fputs("\\newline\\textbf{Variables}\n\n$", f));
    for (size_t i = 0; i < total_n; ++i) {
      if (model->vars[i]) {
         Var *v = &ctr->vars[i];
         S_CHECK_EXIT(_printvarname(ctr, i, f));
         IO_PRINT_EXIT(fputs(var_type_latex(v->type), f));
         switch (v->type) {
         case VAR_X:
         case VAR_I:
            IO_PRINT_EXIT(fprintf(f, " \\in [%e, %e]", v->bnd.lb, v->bnd.ub));
            break;
         case VAR_SC:
            IO_PRINT_EXIT(fprintf(f, " \\in \\{0\\} \\cup [%e, %e]", v->bnd.lb, v->bnd.ub));
            break;
         case VAR_SI:
            IO_PRINT_EXIT(fprintf(f, " \\in \\{0\\} \\cup [%u, %u]", v->si.lb, v->si.ub));
            break;
         default:
            ;
         }
         IO_PRINT_EXIT(fputs("\\newline\n", f));
      }
   }
    IO_PRINT_EXIT(fputs("$\n \\textbf{Equations}:\n\n", f));

   for (size_t i = 0; i < total_m; ++i) {
      if (model->equs[i]) {
         Equ *e = &ctr->equs[i];
         Lequ *le = e->lequ;
         bool need_plus = false;

         S_CHECK_EXIT(_printequname(ctr, i, f));

         if (le && le->len > 0) {
            IO_PRINT_EXIT(fputs("\\left[ ", f));
            rhp_idx * restrict vidx = le->vis;
            double * restrict vals = le->coeffs;
            S_CHECK_EXIT(pprint_f(vals[0], f, false));
            S_CHECK_EXIT(_printvarname(ctr, vidx[0], f));
            for (size_t j = 1, len = le->len; j < len; ++j) {
               S_CHECK_EXIT(pprint_f(vals[j], f, true));
            S_CHECK_EXIT(_printvarname(ctr, vidx[j], f));
            }
            IO_PRINT_EXIT(fputs(" \\right]_{\\mathrm{lin}} ", f));
            need_plus = true;
         }

         if (e->tree && e->tree->root) {
            if (need_plus) {
               IO_PRINT_EXIT(fputs(" + ", f));
            }

            IO_PRINT_EXIT(fputs("\\left[ ", f));
            S_CHECK(nltree_latex(ctr, e->tree, f));
            IO_PRINT_EXIT(fputs(" \\right]_{\\mathrm{nl}} ", f));
         }

         if (emd && emd[i].role == EquViFunction) {
            double cst = e->p.cst;
            if (cst != 0.) {
              IO_PRINT_EXIT(fprintf(f, "+ %e ", e->p.cst));
            }

            IO_PRINT_EXIT(fputs(" \\perp ", f));
            if (!valid_vi(emd[i].dual)) {
               error("%s :: ill-defined dual variable %d for equation %s\n",
                        __func__, emd[i].dual, ctr_printequname(ctr, i));
            }
            S_CHECK_EXIT(_printvarname(ctr, emd[i].dual, f));
            Var *v = &ctr->vars[emd[i].dual];

            S_CHECK(_print_varconesymbol(v, f));

         } else if (!emd || emd[i].role == EquConstraint) {
            S_CHECK_EXIT(_print_equcone(e, f));
         }

         IO_PRINT_EXIT(fputs("\\end{dmath}\n", f));
      }
   }
 
_exit:
   if (f) { int rc = fclose(f); if (rc) { status = Error_SystemError; }}
   return status;
}

int rmdl_export_latex(Model *mdl, const char *phase_name)
{
   if (!mdl_is_rhp(mdl)) return OK;

   const char *latex_dir = mygetenv("RHP_EXPORT_LATEX");
   if (latex_dir) {
      char *fname;
      IO_PRINT(asprintf(&fname, "%s" DIRSEP "mdl_%s_%p.tex", latex_dir,
                       phase_name, (void*)mdl));
      S_CHECK(rmdl_latex(mdl, fname));
      FREE(fname);

      if (empdag_exists(&mdl->empinfo.empdag)) {
         IO_PRINT(asprintf(&fname, "%s" DIRSEP "empdag__%p.dot",
                               latex_dir, (void*)mdl));
         S_CHECK(empdag2dotfile(&mdl->empinfo.empdag, fname));
         FREE(fname);
      }
   }
   myfreeenvval(latex_dir);

   return OK;
}

int mdl_export_gms(Model *mdl, const char *phase_name)
{
   int status = OK;
  const char * do_export = mygetenv("RHP_EXPORT_GMS");
  if (!do_export) return OK;
   myfreeenvval(do_export);
  
  Model *mdl_gams;
  A_CHECK(mdl_gams, mdl_new(RhpBackendGamsGmo));

  S_CHECK_EXIT(gmdl_set_gamsdata_from_env(mdl_gams));
  S_CHECK_EXIT(mdl_setsolvername(mdl_gams, "CONVERT"));
  S_CHECK_EXIT(rhp_process(mdl, mdl_gams));
  S_CHECK_EXIT(rhp_solve(mdl_gams));
  S_CHECK_EXIT(rhp_postprocess(mdl_gams));

_exit:
   mdl_release(mdl_gams);

  return status;
}

