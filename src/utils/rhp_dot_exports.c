#include "asprintf.h"

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
      error("%s :: Could not create file named '%s'\n", __func__, fname);
      return Error_SystemError;
   }

   IO_CALL_EXIT(fputs("digraph structs {\n node [shape=\"plaintext\", style=\"filled, rounded\", margin=0.2];\n", f));
   IO_CALL_EXIT(fprintf(f, "label=\"%s model '%.*s' #%u: equation '%s'",
                        mdl_fmtargs(mdl), mdl_printequname(mdl, ei)));

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

int dot2png(const char *fname)
{
   char *cmd;
   IO_CALL(asprintf(&cmd, "dot -Tpng -O \"%s\"", fname));
   int rc = system(cmd); /* YOLO */
   if (rc) {
      error("[empdag] executing '%s' yielded return code %d\n", cmd, rc);
      FREE(cmd);
   }
   FREE(cmd);

   return OK;
}

int view_png(const char *fname, Model *mdl)
{
   bool free_png_viewer = false;
   char *png_viewer = optvals(mdl, Options_Png_Viewer);

  if (!png_viewer || png_viewer[0] == '\0') {
      free(png_viewer); // optvals requires the callee to free
#ifdef __apple__
      png_viewer = "open -f "
#elif defined(__linux__)
      png_viewer = "feh - &";
#endif
   } else {
      free_png_viewer = true;
   }

   if (png_viewer) {
      char *cmd;
      IO_CALL(asprintf(&cmd, "cat \"%s.png\" | %s", fname, png_viewer));
      int rc = system(cmd); /* YOLO */
      if (rc) {
         error("[empdag] ERROR: executing '%s' yielded return code %d\n", cmd, rc);
      }
      FREE(cmd);
   }

   if (free_png_viewer) { FREE(png_viewer); }

   return OK;
}

int view_equ_as_png(Model *mdl, rhp_idx ei)
{
   int status = OK;
   char *fname = NULL;

   S_CHECK(chk_ei(mdl, ei, __func__));

   S_CHECK_EXIT(equ2dot(mdl, ei, &fname));

   if (fname) {
      S_CHECK_EXIT(dot2png(fname));
      S_CHECK_EXIT(view_png(fname, mdl));
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

int dot_export_equs(Model *mdl)
{
   int status = OK;
   char *optval = optvals(mdl, Options_Display_Equations);

   size_t len = optval ? strlen(optval) : 0;

   if (len == 0) { free(optval); return OK; }

   if (!strncmp(optval, "+all", len)) {
      S_CHECK_EXIT(export_all_equs(mdl));
   }

   /* TODO: support filtering once symbol names have been sorted */
_exit:
   free(optval);
   return status;
}
