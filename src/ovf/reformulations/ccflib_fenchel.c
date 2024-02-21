#include "macros.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "ovf_common.h"
#include "rhp_fwd.h"


typedef struct {
   unsigned z_start;
   unsigned z_bnd_start;
   unsigned c_bnd_start;
   unsigned c_end;
   const char *ccfname;
   unsigned ccfnamelen;
} CcfLibFenchelDat;

#if 0

int ccflib_primal_addmult_nonbox_cons(Model *mdl, CcfLibFenchelDat *dat)
{
   struct container *ctr = mdl->ctr;
   struct model_repr *model = (struct model_repr *)mdl->ctr.data;
   dat->z_start = model->total_n;
   unsigned idx_z = 0;
   char *z_name;

      /* -------------------------------------------------------------------
       * Add the multipliers for the nonbox constraints. This is always well
       * defined
       *
       * If the OVF is a sup, the multipliers belong to the polar cone
       * Otherwise, we use a trick and the multiplier belongs to the dual cone
       * ------------------------------------------------------------------- */

      NEWNAME(z_name, dat->ccfname, dat->ccfnamelen, "_z");
      model_varname_start(model, z_name);
      for (unsigned j = 0; j < n_z; ++j) {
         enum CONES cone;
         void *cone_data;
         S_CHECK_EXIT(op->get_cone_nonbox(ovfd, j, &cone, &cone_data));
         rhp_idx vidx = IdxNA;

         if (ovf_ppty.sup) {
            S_CHECK_EXIT(model_add_multiplier_polar(ctr, cone, cone_data, &vidx));
         } else {
            S_CHECK_EXIT(model_add_multiplier_dual(ctr, cone, cone_data, &vidx));
         }

         if (valid_vi(vidx)) {
            c[idx_z] = c[j];
            idx_z++;
            if (mp) {
               S_CHECK_EXIT(mp_addvar(mp, vidx));
            }
         }
      }

      model_varname_end(model);

      /* -------------------------------------------------------------------
       * Add the multipliers for the upper bounds
       *
       * If the OVF is a sup, the multipliers belong to the polar cone
       * Otherwise, we use a trick and the multiplier belongs to the dual cone
       *
       * OUTPUT:
       *   c filled from c_bnd_start until ...
       *   z_bnd_revidx of size c_end-c_bnd_start maps the index of c to the index of u
       * ------------------------------------------------------------------- */

      NEWNAME(z_name, ccfname, ccfnamelen, "_zbnd");
      model_varname_start(model, z_name);
      z_bnd_start = model->total_n;
      c_bnd_start = idx_z;

      for (unsigned i = 0, j = 0; i < n_u; ++i) {
         if (isfinite(var_ubnd[i])) {
            rhp_idx vidx = IdxNA;

            if (ovf_ppty.sup) {
               S_CHECK_EXIT(model_add_multiplier_polar(ctr, CONE_R_MINUS, NULL, &vidx));
            } else {
               S_CHECK_EXIT(model_add_multiplier_dual(ctr, CONE_R_MINUS, NULL, &vidx));
            }
            assert(valid_vi(vidx));
            if (mp) {
               S_CHECK_EXIT(mp_addvar(mp, vidx));
            }
            c[idx_z] = var_ubnd[i];
            z_bnd_revidx[j] = i;
            j++;
            idx_z++;
         }
      }

      model_varname_end(model);

      c_end = idx_z;
      n_z = model->total_n - z_start;

}

#endif
