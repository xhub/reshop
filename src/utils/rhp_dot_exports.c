#include "asprintf.h"
#include "ctr_gams.h"
#include "ctr_rhp.h"

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

#include "checks.h"
#include "compat.h"
#include "dot_png.h"
#include "mdl.h"
#include "nltree.h"
#include "rhp_dot_exports.h"

NONNULL static int lequ_print_dot(Lequ *lequ, FILE *f, Model *mdl)
{
   int status = OK;

   double *darr  = lequ->coeffs;
   rhp_idx *varr = lequ->vis;
   unsigned len  = lequ->len;

   /* cluster is critical */
   IO_CALL_EXIT(fputs("\n subgraph cluster_lequ {\n rankdir=LR;\n label = \"Linear part\"; "
                      "style=filled; bgcolor=lightyellow; node [shape=\"record\"];\n", f));

   IO_CALL_EXIT(fprintf(f, "\"coefficients\" [ label = \"{ %.4g", darr[0]));

   for (unsigned i = 1; i < len; ++i) {
      IO_CALL_EXIT(fprintf(f, "| %.4g", darr[i]));
   }

   IO_CALL_EXIT(fputs("}\"];\n", f));

   IO_CALL_EXIT(fprintf(f, "\"variables\" [ label = \"{ %s", mdl_printvarname(mdl, varr[0])));

   for (unsigned i = 1; i < len; ++i) {
      IO_CALL_EXIT(fprintf(f, "| %s ", mdl_printvarname(mdl, varr[i])));
   }

   IO_CALL_EXIT(fputs("}\"];\n", f));

   IO_CALL_EXIT(fputs("\n}\n", f));

_exit:
   return status;
}

static int equ2dot(Model *mdl, rhp_idx ei, char **fname_dot)
{
   int status = OK;
   Container *ctr = &mdl->ctr;
   char *fname = NULL;

   if (mdl_ensure_exportdir(mdl) != OK) {
      error("%s ERROR: could not create an export dir", __func__);
      return Error_SystemError;
   }

   const char *export_dir = mdl->commondata.exports_dir;

   IO_CALL(asprintf(&fname, "%s%sequ%d.dot", export_dir, DIRSEP, ei));
   *fname_dot = fname;

   FILE* f = fopen(fname, "w");

   if (!f) {
      error("[system] ERROR: Could not create file named '%s'\n", fname);
      return Error_SystemError;
   }

   IO_CALL_EXIT(fputs("digraph structs {ordering=out;\n node [shape=\"plaintext\", style=\"filled, rounded\", margin=0.2];\n", f));
   IO_CALL_EXIT(fprintf(f, "label=\"%s model '%.*s' #%u: equation '%s'", mdl_fmtargs(mdl), mdl_printequname(mdl, ei)));

   mpid_t mpid = ctr->equmeta ? ctr->equmeta[ei].mp_id : MpId_NA;

   if (mpid_regularmp(mpid)) {
      IO_CALL_EXIT(fprintf(f, " in MP(%s)", empdag_getmpname(&mdl->empinfo.empdag, mpid)));
   }
   IO_CALL_EXIT(fputs("\";\n", f));

   Lequ *lequ = ctr->equs[ei].lequ;
   NlTree *tree = ctr->equs[ei].tree;

   if (lequ && lequ->len > 0) {
      lequ_print_dot(lequ, f, mdl);
   }

   if (tree) {
      nltree_print_dot(ctr->equs[ei].tree, f, mdl);
   }

   IO_CALL_EXIT(fputs("\n}\n", f));

   SYS_CALL(fflush(f));
   SYS_CALL(fsync(fileno(f)));

_exit:
   SYS_CALL(fclose(f));
   return status;
}

int view_equ_as_png(Model *mdl, rhp_idx ei)
{
   int status = OK;
   char *fname = NULL;

   S_CHECK(chk_ei(mdl, ei, __func__));

   if (mdl_is_rhp(mdl)) {
      S_CHECK(rctr_getnl(&mdl->ctr, &mdl->ctr.equs[ei]));
   } if (mdl->backend == RhpBackendGamsGmo && !mdl->ctr.equs[ei].tree) {
      int len, *instrs, *args;

      Container *ctr = &mdl->ctr;
      S_CHECK(gctr_getopcode(ctr, ei, &len, &instrs, &args));

      nlopcode_print(PO_ERROR, instrs, args, len);
      S_CHECK(equ_nltree_fromgams(&ctr->equs[ei], len, instrs, args));

      // HACK ARENA
      ctr_relmem_recursive_old(ctr);
   }


   S_CHECK_EXIT(equ2dot(mdl, ei, &fname));

   if (fname) {
      S_CHECK_EXIT(dot2png(fname));
      S_CHECK_EXIT(view_png_mdl(fname, mdl));
      /* FIXME free fname? */
   }

_exit:
   free(fname);
   return status;
}

NONNULL static int export_all_equs(Model *mdl)
{
   for (unsigned i =0, len = mdl_nequs_total(mdl); i < len; ++i) {
      S_CHECK(view_equ_as_png(mdl, i));
   }

   return OK;
}

/**
 * @brief Export (some) equations as DOT files
 *
 * The option display_equations controls the behavior of this function
 *
 * @param mdl   the model
 *
 * @return      the error code 
 */
int dot_export_equs(Model *mdl)
{
   int status = OK;
   char *optval = optvals(mdl, Options_Display_Equations);

   size_t len = optval ? strlen(optval) : 0;

   if (len == 0) { free(optval); return OK; }

   if (!strncmp(optval, "+all", len)) {
      S_CHECK_EXIT(export_all_equs(mdl));
      goto _exit;
   }

   /* TODO: support filtering once symbol names have been sorted */

   char *start = optval;
   do {
      char *end = strchr(start, ',');
      len = end ? end - start : strlen(start);

      for (size_t i = 0, nequs = mdl_nequs_total(mdl); i < nequs; ++i) {
         if (!strncasecmp(start, mdl_printequname(mdl, i), len)) {
            S_CHECK_EXIT(view_equ_as_png(mdl, i));
         }
      } 

      start = end ? end + 1 : NULL; /* consumer ',' */
   } while (start);

_exit:
   free(optval);
   return status;
}

int view_png_mdl(const char *fname, Model *mdl)
{
   char * png_viewer = optvals(mdl, Options_Png_Viewer);
   int rc = view_png(fname, png_viewer);
   free(png_viewer); // optvals requires the callee to free

   return rc;
}
