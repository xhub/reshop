#include "asprintf.h"

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)

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


const char * const nodestyle_min  = "style=filled,color=lightblue1";
const char * const nodestyle_max  = "style=filled,color=lightsalmon1";
const char * const nodestyle_vi = "style=filled,color=lightseagreen";
const char * const nodestyle_mpe = "style=filled,color=lightgreen";

const char * const arcstyle_CTRL = "color=\"magenta:invis:magenta\"";
const char * const arcstyle_NASH = "color=lightgreen, arrowhead=box";
const char * const arcstyle_VF   = "color=red";

typedef struct {
   const char *labelcolor;
   const char *equname;
   char *weight;
   bool do_free_weight;
} EdgeVFStrings;

static int edgeVF_basic_arcdat(const EdgeVF *edgeVF, MathPrgm *mp,
                               const Model *mdl, EdgeVFStrings *strs)
{
   rhp_idx ei = edgeVF->basic_dat.ei;
   if (valid_ei(ei)) {
      rhp_idx objequ = mp_getobjequ(mp);
      strs->labelcolor = ei == objequ ? "blue" : "magenta";
      strs->equname = ctr_printequname(&mdl->ctr, ei);
   } else if (ei == IdxCcflib) {
      strs->labelcolor = "brown";
      strs->equname = "CCF obj";
   } else {
      const EmpDag *empdag = &mdl->empinfo.empdag;
      error("[empdag:dot] ERROR: invalid equation for VF arc between MP(%s) and "
            "MP(%s)", empdag_getmpname(empdag, mp->id),
            empdag_getmpname(empdag, edgeVF->child_id));
      strs->equname = "ERROR invalid equation index";
      strs->labelcolor = "red";
      return OK;
   }

   double cst = edgeVF->basic_dat.cst;
   rhp_idx vi = edgeVF->basic_dat.vi;
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

static int _print_edges_mps(const EmpDag* empdag, FILE* f)
{
   const struct mp_namedarray* mps = &empdag->mps;

   for (unsigned i = 0, len = mps->len; i < len; ++i) {
      if (mps->Carcs[i].len == 0 && mps->Varcs[i].len == 0) continue;

      const UIntArray *Carcs = &mps->Carcs[i];
      unsigned mp_id = mps->arr[i]->id;

      for (unsigned j = 0, alen = Carcs->len; j < alen; ++j) {

         unsigned uid = Carcs->list[j];
         bool isMP = uidisMP(uid);
         unsigned id = uid2id(uid);
       

         IO_CALL(fprintf(f, " MP%u -> %s%u [%s];\n", mp_id,
                         isMP ? "MP" : "MPE", id, arcstyle_CTRL));
      }

      const struct VFedges *Varcs = &mps->Varcs[i];

      for (unsigned j = 0, alen = Varcs->len; j < alen; ++j) {

         const struct rhp_empdag_Varc* edgeVF = &Varcs->arr[j];
         unsigned mpchild_id = edgeVF->child_id;

         MathPrgm *mp = mps->arr[i];
         EdgeVFStrings VFstrings = {.do_free_weight = false};

         switch (edgeVF->type) {
         case EdgeVFBasic:
            edgeVF_basic_arcdat(edgeVF, mp, empdag->mdl, &VFstrings);
            break;
         case EdgeVFUnset:
            VFstrings.labelcolor = "red";
            VFstrings.equname = "INVALID edgeVF";
            break;
         default:
            VFstrings.labelcolor = "red";
            VFstrings.equname = "unsupported edgeVF type";
         }

         IO_CALL(fprintf(f, " MP%u -> MP%u [label=\"(%s)\", labelfontcolor=%s, %s];\n",
                         mp_id, mpchild_id, VFstrings.equname, //VFstrings.weight,
                         VFstrings.labelcolor, arcstyle_VF));

         if (VFstrings.do_free_weight) { FREE(VFstrings.weight); }

      }

   }

   return OK;
}

static int _print_edges_mpes(const struct mpe_namedarray* mpes, FILE* f)
{
   for (unsigned i = 0, len = mpes->len; i < len; ++i) {
      if (mpes->arcs[i].len == 0) continue;

      const UIntArray *arcs = &mpes->arcs[i];
      unsigned mpe_id = mpes->arr[i]->id;


      for (unsigned j = 0, alen = arcs->len; j < alen; ++j) {

         unsigned uid = arcs->list[j];
         bool isMP = uidisMP(uid);
         assert(isMP);
         unsigned id = uid2id(uid);
       
         const char *arcstyle = arcstyle_NASH;

         IO_CALL(fprintf(f, " MPE%u -> %s%u [%s];\n", mpe_id, isMP ? "MP" : "MPE", id, arcstyle));
      }

   }

   return OK;
}

static int _print_mp_graph(const struct mp_namedarray* mps, FILE* f, const Container *ctr)
{
   for (unsigned i = 0, len = mps->len; i < len; ++i) {
      if (!mps->arr[i]) continue;

      const MathPrgm *mp = mps->arr[i];

      const char *nodestyle_mp, *prefix;
      RhpSense sense = mp_getsense(mp);
      switch(sense) {
      case RHP_MIN:
         nodestyle_mp = nodestyle_min;
         prefix = sense2str(sense);
         break;
      case RHP_MAX:
         nodestyle_mp = nodestyle_max;
         prefix = sense2str(sense);
         break;
      case RHP_FEAS:
         nodestyle_mp = nodestyle_vi;
         prefix = mptype_str(mp->type);
         break;
      default:
         error("%s :: unsupported sense %s #%d", __func__, sense2str(sense), sense);
         return Error_InvalidValue;
      }

      const char *name = mps->names[i];
      if (!name) {
         char *lname;
         const char *mp_ident = NULL;

         switch(mp->type) {
         case MpTypeCcflib: {
            const OvfDef *ccf = mp->ccflib.ccf;
            IO_CALL(asprintf(&lname, "CCF '%s'", ovf_getname(ccf)));
            break;
         }
         case RHP_MP_OPT:
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


      IO_CALL(fprintf(f, " MP%u [label=\"%s\", %s];\n", mp->id, name, nodestyle_mp));
   }

   return OK;
}

static int _print_mpe_graph(const struct mpe_namedarray* mpes, FILE* f, const Container *ctr)
{
   for (unsigned i = 0, len = mpes->len; i < len; ++i) {
      if (!mpes->arr[i]) continue;

      const Mpe *mpe = mpes->arr[i];
      const char *name = mpes->names[i];
      if (!name) {
         char *lname;
         IO_CALL(asprintf(&lname, "MPE(%u)", mpe->id));
         name = lname;
      }

      IO_CALL(fprintf(f, " MPE%u [label=\"%s\", %s];\n", mpe->id, name, nodestyle_mpe));
   }

   return OK;
}

static int empdag2dot(const  EmpDag * empdag, FILE *f)
{
   IO_CALL(fputs("digraph structs {\n node [shape=record];\n", f));
   IO_CALL(fprintf(f, " label=\"EMPDAG for model '%s'\"\n", mdl_getname(empdag->mdl)));

   S_CHECK(_print_mp_graph(&empdag->mps, f, &empdag->mdl->ctr));
   S_CHECK(_print_mpe_graph(&empdag->mpes, f, &empdag->mdl->ctr));

   S_CHECK(_print_edges_mps(empdag, f));
   S_CHECK(_print_edges_mpes(&empdag->mpes, f));

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

   char *cmd;
   IO_CALL(asprintf(&cmd, "dot -Tpng -O %s", fname));
   int rc = system(cmd); /* YOLO */
   if (rc) {
      error("[empdag] executing '%s' yielded return code %d\n", cmd, rc);
      FREE(cmd);
   }
   FREE(cmd);

   if (!optvalb(empdag->mdl, Options_Display_EmpDag)) { return OK; }

   const char *png_viewer = optvals(empdag->mdl, Options_Png_Viewer);
   bool free_png_viewer = false;

  if (!png_viewer || png_viewer[0] == '\0') {
#ifdef __apple__
      png_viewer = "open -f "
#elif defined(__linux__)
      png_viewer = "feh - &";
#endif
   } else {
      free_png_viewer = true;
   }

   if (png_viewer) {
      IO_CALL(asprintf(&cmd, "cat %s.png | %s", fname, png_viewer));
      rc = system(cmd); /* YOLO */
      if (rc) {
         error("[empdag] ERROR: executing '%s' yielded return code %d\n", cmd, rc);
      }
      FREE(cmd);
   }

   if (free_png_viewer) { FREE(png_viewer); }

   return OK;

_exit:
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

