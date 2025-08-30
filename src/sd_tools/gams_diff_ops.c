#include "container.h"
#include "ctr_gams.h"
#include "equ.h"
#include "nltree.h"
#include "gams_diff_ops.h"
#include "macros.h"
#include "mdl.h"
#include "open_lib.h"
#include "printout.h"
#include "sd_tool.h"
#include "tlsdef.h"

struct gams_diff_data {
   int *instrs;
   int *args;
   int len;
   int *d_instrs;
   int *d_args;
   int d_maxlen;
};

static tlsvar int (*compute_opcode_diff)(int **, int **, int *, const int *, const int *, int, char *) = NULL;
static tlsvar void* libopcodediff_handle = NULL;

#if defined(_WIN32) && !defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "win-compat.h"

  void cleanup_opcode_diff(void)
  {
      if (libopcodediff_handle)
      {
        FreeLibrary(libopcodediff_handle);
        libopcodediff_handle = NULL;
      }
  }

#elif defined(__GNUC__) & !defined(__APPLE__)
#include <dlfcn.h>

   __attribute__ ((destructor)) static void cleanup_opcode_diff(void)
{
    if (libopcodediff_handle)
    {
       dlclose(libopcodediff_handle);
       libopcodediff_handle = NULL;
    }
  }
#endif

static int _load_opcode_diff(void)
{
   if (!compute_opcode_diff) {
      if (!libopcodediff_handle) {
         libopcodediff_handle = open_library(DLL_FROM_NAME("opcode_diff"), 0);
         if (!libopcodediff_handle) {
            error("%s :: Could not find "DLL_FROM_NAME("opcode_diff")". "
                     "Some functionalities may not be available\n", __func__);
            return Error_SystemError;
         }
      }

      *(void **) (&compute_opcode_diff) = get_function_address(libopcodediff_handle, "opcode_diff");
      if (!compute_opcode_diff) {
         error("%s :: Could not find function named opcode_diff"
                  " in " DLL_FROM_NAME("opcode_diff")". Some functionalities may not "
                  "be available\n", __func__);
         return Error_SystemError;
      }
   }

   return OK;
}

static int gams_diff_deriv(struct sd_tool *sd_tool, int vidx, Equ *e)
{
   struct gams_diff_data *diff_data = (struct gams_diff_data *)sd_tool->data;
   char errmsg[256];
   int rc;

   if (!diff_data) {
     return OK;
   }

   if (diff_data->len == 0) {
      error("%s :: no nlopcode!\n", __func__);
      return OK;
   }

   if ((rc = compute_opcode_diff(&diff_data->d_instrs,
                                 &diff_data->d_args,
                                 &diff_data->d_maxlen,
                                 diff_data->instrs,
                                 diff_data->args,
                                 vidx+1,
                                 errmsg)) != EXIT_SUCCESS) {
      error("%s :: call to the opcode diff failed with error %d "
                         "and the message is ``%s''\n", __func__, rc, errmsg);
      return Error_SymbolicDifferentiation;
   }

   if (diff_data->d_args[0] > 0) {
      unsigned codelen = (unsigned)diff_data->d_args[0];
      e->idx = diff_data->d_args[codelen-1] - 1;
      S_CHECK(equ_nltree_fromgams(e, diff_data->d_args[0], diff_data->d_instrs,
                                    diff_data->d_args));
   }

   return OK;
}

static int gams_diff_alloc(struct sd_tool *sd_tool, Container *ctr, rhp_idx ei)
{
   CALLOC_(sd_tool->data, struct gams_diff_data, 1);

   struct gams_diff_data *diff_data = (struct gams_diff_data *)sd_tool->data;

   /* ----------------------------------------------------------------------
    * Get the GAMS opcode and update the variable index
    *
    * gams_genopcode uses some temporary memory from the container
    * we need to allocate our own
    * ---------------------------------------------------------------------- */

   int *instrs_tmp;
   int *args_tmp;

   // HACK ARENA
   S_CHECK(gctr_genopcode(ctr, ei, &diff_data->len, &instrs_tmp, &args_tmp));

   /* ----------------------------------------------------------------------
    * instrs_tmp and args_tmp are temporary memory
    * Now that we have the right size, we can allocate the proper amount of
    * memory
    * ---------------------------------------------------------------------- */

   int codelen = diff_data->len;
   if (codelen > 0) {
      int *tmparray;
      MALLOC_(tmparray, int, 2*codelen);
      diff_data->instrs = tmparray;
      diff_data->args = &tmparray[codelen];
      memcpy(diff_data->instrs, instrs_tmp, sizeof(int) * codelen);
      memcpy(diff_data->args, args_tmp, sizeof(int) * codelen);

      /*  Set the right equation number */
      diff_data->args[diff_data->len-1] = 1 + ei;
   } else {
     FREE(sd_tool->data);
   }

   // HACK ARENA
   ctr_relmem_old(ctr);

   /* ----------------------------------------------------------------------
    * Ensure that the external library is loaded
    * ---------------------------------------------------------------------- */

   S_CHECK(_load_opcode_diff());

   return OK;
}

static void gams_diff_dealloc(struct sd_tool *sd_tool)
{
   if (!sd_tool->data) return;

   struct gams_diff_data *diff_data = (struct gams_diff_data *)sd_tool->data;

   FREE(diff_data->instrs);
   diff_data->args = NULL; /* diff_data->instrs and diff_data->args are contiguous  */
   FREE(diff_data->d_instrs);
   FREE(diff_data->d_args);
   FREE(diff_data);
}

const struct sd_ops gams_diff_ops = {
   .allocdata    = gams_diff_alloc,
   .deallocdata  = gams_diff_dealloc,
   .assemble     = NULL,
   .deriv        = gams_diff_deriv,
};
