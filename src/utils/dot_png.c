#include "asprintf.h"
#include <stdbool.h>

#include "dot_png.h"
#include "macros.h"
#include "printout.h"

int dot2png(const char *fname)
{
   char *cmd;
   IO_PRINT(asprintf(&cmd, "dot -Tpng -O \"%s\"", fname));
   int rc = system(cmd); /* YOLO */
   if (rc) {
      error("[dot2png] executing '%s' yielded return code %d\n", cmd, rc);
      FREE(cmd);
   }
   FREE(cmd);

   return OK;
}

int view_png(const char *fname, const char *png_viewer)
{
   bool free_png_viewer = false;

  if (!png_viewer || png_viewer[0] == '\0') {
#ifdef __APPLE__
      png_viewer = "open -f ";
#elif defined(__linux__)
      png_viewer = "feh - &";
#else
      png_viewer = NULL;
#endif
   } else {
      free_png_viewer = true;
   }

   if (png_viewer) {
      char *cmd;
      IO_PRINT(asprintf(&cmd, "cat \"%s.png\" | %s", fname, png_viewer));
      int rc = system(cmd); /* YOLO */
      if (rc) {
         error("[empdag] ERROR: executing '%s' yielded return code %d\n", cmd, rc);
      }
      FREE(cmd);
   }

   if (free_png_viewer) { FREE(png_viewer); }

   return OK;
}
