#include "asprintf.h"
#include "rhp_dot_exports.h"

#include <stdio.h>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <fileapi.h>

#define fileno(X) (X)
#define fsync(X) !FlushFileBuffers((HANDLE)(X))

#else

#include <unistd.h>

#endif

#include "container.h"
#include "empdag.h"
#include "empinfo.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovfinfo.h"
#include "printout.h"
#include "toplayer_utils.h"


const char * const nodestyle_min   =  "color=lightblue1";
const char * const nodestyle_max   =  "color=lightsalmon1";
const char * const nodestyle_vi    =  "color=lightseagreen";
const char * const nodestyle_dual  =  "color=darkorange";
const char * const nodestyle_nash  =  "color=lightgreen";

const char * const arcstyle_CTRL = "color=\"magenta:invis:magenta\"";
const char * const arcstyle_NASH = "color=lightgreen, arrowhead=box";
const char * const arcstyle_VF   = "color=red";

typedef struct {
   const char *labelcolor;
   const char *equname;
   char *weight;
   bool do_free_weight;
} ArcVFStrings;

static int arcVF_basic_arcdat(const ArcVFData *arcVF, const MathPrgm *mp,
                              const Model *mdl, ArcVFStrings *strs)
{
   if (arcVF->type != ArcVFBasic) {
      strs->equname = arcVFType2str(arcVF->type);
      return OK;
   }

   const ArcVFBasicData *dat = &arcVF->basic_dat;
   rhp_idx ei = dat->ei;
   if (valid_ei(ei)) {
      rhp_idx objequ = mp_getobjequ(mp);
      strs->labelcolor = ei == objequ ? "blue" : "magenta";
      strs->equname = ctr_printequname(&mdl->ctr, ei);
   } else if (ei == IdxCcflib) {
      strs->labelcolor = "brown";
      strs->equname = "CCF obj";
   } else if (ei == IdxObjFunc) {
      strs->labelcolor = "green";
      strs->equname = "objequ";
   } else {
      const EmpDag *empdag = &mdl->empinfo.empdag;
      error("[empdag:dot] ERROR: invalid equation for VF arc between MP(%s) and "
            "MP(%s)\n", empdag_getmpname(empdag, mp->id),
            empdag_getmpname(empdag, arcVF->mpid_child));
      strs->equname = "ERROR invalid equation index";
      strs->labelcolor = "red";
      strs->weight = NULL;

      return OK;
   }

   double cst = arcVF->basic_dat.cst;
   rhp_idx vi = arcVF->basic_dat.vi;
   strs->weight = NULL;
   strs->do_free_weight = true;
   if (cst != 1.) {
      if (valid_vi(vi)) {
         IO_CALL(asprintf(&strs->weight, "%e %s", cst, ctr_printvarname(&mdl->ctr, vi)));
      } else {
         IO_CALL(asprintf(&strs->weight, "%e", cst));
      }
   } else {
      if (valid_vi(vi)) {
         IO_CALL(asprintf(&strs->weight, "%s", ctr_printvarname(&mdl->ctr, vi)));
      } else {
         strs->weight = "";
         strs->do_free_weight = false;
      }
   }

   return OK;
}

static int print_mp_arcs(const EmpDag* empdag, FILE* f)
{
   const struct mp_namedarray* mps = &empdag->mps;

   for (unsigned i = 0, len = mps->len; i < len; ++i) {
      const MathPrgm *mp = mps->arr[i];

      if (!mp || (mps->Carcs[i].len == 0 && mps->Varcs[i].len == 0)) continue;

      /* Poor man's detection of truly hidden MPs */
      if (mp_ishidden(mp) && !mp_ishidable(mp)) {
         continue;
      }

      const UIntArray *Carcs = &mps->Carcs[i];
      unsigned mp_id = mp->id;

      for (unsigned j = 0, alen = Carcs->len; j < alen; ++j) {

         unsigned uid = Carcs->arr[j];
         bool isMP = uidisMP(uid);
         unsigned id = uid2id(uid);
 

         IO_CALL(fprintf(f, " MP%u -> %s%u [%s];\n", mp_id,
                         isMP ? "MP" : "Nash", id, arcstyle_CTRL));
      }

      const VarcArray *Varcs = &mps->Varcs[i];

      for (unsigned j = 0, alen = Varcs->len; j < alen; ++j) {

         const struct rhp_empdag_arcVF* arcVF = &Varcs->arr[j];
         unsigned mpchild_id = arcVF->mpid_child;

         ArcVFStrings VFstrings = {.do_free_weight = false};

         switch (arcVF->type) {
         case ArcVFBasic:
            arcVF_basic_arcdat(arcVF, mp, empdag->mdl, &VFstrings);
            break;
         case ArcVFUnset:
            VFstrings.labelcolor = "red";
            VFstrings.equname = "INVALID arcVF";
            break;
         default:
            VFstrings.labelcolor = "red";
            VFstrings.equname = "unsupported arcVF type";
         }

         IO_CALL(fprintf(f, " MP%u -> MP%u [label=<%s",
                         mp_id, mpchild_id, VFstrings.equname)); //VFstrings.weight,
 
         if (VFstrings.weight) {
            IO_CALL(fprintf(f, "<BR/>%s", VFstrings.weight));
         }

         IO_CALL(fprintf(f, ">, fontcolor=%s, %s];\n",
                         VFstrings.labelcolor, arcstyle_VF));

         if (VFstrings.do_free_weight) { FREE(VFstrings.weight); }

      }

   }

   return OK;
}

static int print_nash_arcs(const struct nash_namedarray* mpes, FILE* f)
{
   for (unsigned i = 0, len = mpes->len; i < len; ++i) {
      if (mpes->arcs[i].len == 0) continue;

      const UIntArray *arcs = &mpes->arcs[i];
      unsigned mpe_id = mpes->arr[i]->id;


      for (unsigned j = 0, alen = arcs->len; j < alen; ++j) {

         unsigned uid = arcs->arr[j];
         bool isMP = uidisMP(uid);
         assert(isMP);
         unsigned id = uid2id(uid);
       
         const char *arcstyle = arcstyle_NASH;

         IO_CALL(fprintf(f, " Nash%u -> %s%u [%s];\n", mpe_id, isMP ? "MP" : "Nash", id, arcstyle));
      }

   }

   return OK;
}

static int print_mp_nodes(const struct mp_namedarray* mps, FILE* f, const Container *ctr)
{
    MathPrgm ** const mps_arr = mps->arr;

   char *hidden_mps = NULL;

   for (unsigned i = 0, len = mps->len; i < len; ++i) {
      const MathPrgm *mp = mps_arr[i];

      if (!mp) continue;

      const char *nodestyle_mp, *prefix;
      RhpSense sense = mp_getsense(mp);
      switch(sense) {
      case RhpMin:
         nodestyle_mp = nodestyle_min;
         prefix = sense2str(sense);
         break;
      case RhpMax:
         nodestyle_mp = nodestyle_max;
         prefix = sense2str(sense);
         break;
      case RhpFeasibility:
         nodestyle_mp = nodestyle_vi;
         prefix = mptype2str(mp->type);
         break;
      case RhpDualSense:
         nodestyle_mp = nodestyle_dual;
         prefix = mptype2str(mp->type);
         break;
      default:
         error("%s :: unsupported sense %s #%d", __func__, sense2str(sense), sense);
         return Error_InvalidValue;
      }

      /* -------------------------------------------------------------------
       * If there is no MP name available, make one up
       * ------------------------------------------------------------------- */

      const char *name = mps->names[i];
      char *lname = NULL;
      if (!name) {
         const char *mp_ident = NULL;

         switch(mp->type) {
         case MpTypeCcflib: {
            const OvfDef *ccf = mp->ccflib.ccf;
            IO_CALL(asprintf(&lname, "CCF '%s'", ovf_getname(ccf)));
            break;
         }
         case MpTypeOpt:
            if (valid_vi(mp->opt.objvar)) {
               mp_ident = ctr_printvarname(ctr, mp->opt.objvar);
               IO_CALL(asprintf(&lname, "%s %s", prefix, mp_ident));
               break;
            } else if (valid_ei(mp->opt.objequ)) {
               mp_ident = ctr_printequname(ctr, mp->opt.objequ);
               IO_CALL(asprintf(&lname, "%s %s", prefix, mp_ident));
               break;
            }

         FALLTHRU
         default:
            IO_CALL(asprintf(&lname, "%s (%u)", prefix, mp->id));
         }

         name = lname;
      }


      if (mp_ishidden(mp)) { 
         char *tmp = hidden_mps;
         IO_CALL(asprintf(&hidden_mps, "%s MP%u [label=\"%s\", %s];\n", tmp ? tmp : "",
                 mp->id, name, nodestyle_mp));
         free(tmp);
      } else {
         IO_CALL(fprintf(f, " MP%u [label=\"%s\", %s];\n", mp->id, name, nodestyle_mp));
      }

      free(lname);
   }

   if (hidden_mps) {
      IO_CALL(fprintf(f, " subgraph cluster_mps_hidden { \n "
                      "label = \"Defining MPs\"; bgcolor=\"#ececec\";\n%s}\n",
                      hidden_mps));
      free(hidden_mps);
   }

   return OK;
}

static int print_nash_nodes(const struct nash_namedarray* nashs, FILE* f, const Container *ctr)
{
   for (unsigned i = 0, len = nashs->len; i < len; ++i) {
      if (!nashs->arr[i]) continue;

      const Nash *nash = nashs->arr[i];
      const char *name = nashs->names[i];
      char *lname = NULL;
      if (!name) {
         IO_CALL(asprintf(&lname, "Nash(%u)", nash->id));
         name = lname;
      }

      IO_CALL(fprintf(f, " Nash%u [label=\"%s\", %s];\n", nash->id, name, nodestyle_nash));

      free(lname);
   }

   return OK;
}

static int empdag2dot(const EmpDag *empdag, FILE *f)
{
   IO_CALL(fputs("digraph structs {\n node [shape=\"box\", style=\"filled, rounded\", margin=0.2];\n", f));
//   IO_CALL(fputs(" edge [headclip=false, tailclip=false];\n", f));
   IO_CALL(fprintf(f, " label=\"EMPDAG for %s model '%.*s' #%u\"; rankdir=LR;\n", mdl_fmtargs(empdag->mdl)));

   S_CHECK(print_mp_nodes(&empdag->mps, f, &empdag->mdl->ctr));
   S_CHECK(print_nash_nodes(&empdag->nashs, f, &empdag->mdl->ctr));

   S_CHECK(print_mp_arcs(empdag, f));
   S_CHECK(print_nash_arcs(&empdag->nashs, f));

   IO_CALL(fputs("\n}\n", f));
   SYS_CALL(fflush(f));
   SYS_CALL(fsync(fileno(f)));

   return OK;
}

int empdag2dotfile(const EmpDag * empdag, const char* fname)
{
   if (empdag->mps.len == 0) return OK;

   int status = OK;
   FILE* f = fopen(fname, "w");

   if (!f) {
      error("%s ERROR: Could not create file named '%s'\n", __func__, fname);
      return OK;
   }

   S_CHECK_EXIT(empdag2dot(empdag, f));

   SYS_CALL(fclose(f));

   S_CHECK(dot2png(fname));

   if (!optvalb(empdag->mdl, Options_Display_EmpDag)) { return OK; }

   return view_png(fname, empdag->mdl);

_exit: /* this is reached only when we error */
   SYS_CALL(fclose(f));
   return status;
}

#ifdef WITH_FMEM
#include "fmem.h"
int empdag2dotenvname(const  EmpDag * empdag, const char* envname)
{
   static unsigned cnt = 0;
   int status = OK; 

   fmem fm;
   fmem_init(&fm);
   FILE* f = fmem_open(&fm, "w");

   if (!f) {
      errormsg("Could not create in-memory file IO\n");
      status = Error_SystemError;
      goto _exit;
   }

   S_CHECK_EXIT(empdag2dot(empdag, f));

   void *mem;
   size_t size;

   fmem_mem(&fm, &mem, &size);

   char *envvar;
   size_t envvar_len = strlen(envname) + 10;
   MALLOC_(envvar, char, envvar_len);

   snprintf(envvar, envvar_len-1,  "%s_%u", envname, cnt);

   setenv(envvar, (const char *)mem, 1);

_exit:
   fmem_term(&fm);
   return status;
}

#else /* WITH_FMEM */

int empdag2dotenvname(const  EmpDag * empdag, const char* envname)
{
   errormsg("ReSHOP was built without fmem support!\n");
   return Error_SystemError;
}

#endif

