#include "asprintf.h"
#include "checks.h"
#include "compat.h"
#include "mdl.h"
#include "nltree.h"
#include "rhp_dot_exports.h"

static int equ2dot(Model *mdl, rhp_idx ei, char **fname_dot)
{
   Container *ctr = &mdl->ctr;
   char *fname = NULL;

   if (mdl_ensure_exportdir(mdl) != OK) {
      error("%s ERROR: could not create an export dir", __func__);
      return Error_SystemError;
   }

   const char *export_dir = mdl->commondata.exports_dir;

   if (!ctr->equs[ei].tree) {
      goto _exit;
   }

   IO_CALL(asprintf(&fname, "%s%sequ%d.dot", export_dir, DIRSEP, ei));
   *fname_dot = fname;
   nltree_print_dot(ctr->equs[ei].tree, fname, ctr);

_exit:
   return OK;
}

int dot2png(const char *fname)
{
   char *cmd;
   IO_CALL(asprintf(&cmd, "dot -Tpng -O %s", fname));
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
   const char *png_viewer = optvals(mdl, Options_Png_Viewer);

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
      char *cmd;
      IO_CALL(asprintf(&cmd, "cat %s.png | %s", fname, png_viewer));
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

   if (fname) { /* equ may have no NL content */
      S_CHECK_EXIT(dot2png(fname));
      S_CHECK_EXIT(view_png(fname, mdl));
   }

_exit:
   FREE(fname);
   return status;
}
