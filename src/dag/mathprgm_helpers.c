#include "asnan.h"

#include "cones.h"
#include "container.h"
#include "empdag.h"
#include "empinfo.h"
#include "equ.h"
#include "equ_data.h"
#include "equvar_metadata.h"
#include "mathprgm.h"
#include "mathprgm_helpers.h"
#include "mathprgm_priv.h"
#include "mdl.h"
#include "printout.h"


int mp_identify_objequ(MathPrgm *mp)
{

   mpid_t mpid = mp->id;
   const Container *ctr = &mp->mdl->ctr;
   rhp_idx objvar = mp_getobjvar(mp);
   assert(valid_vi(objvar));

   void *jac = NULL;
   int nlflag;
   double val = SNAN;
   rhp_idx ei, objequ = IdxNA;
   do {
      double val_;
      S_CHECK(ctr_var_iterequs(ctr, objvar, &jac, &val_, &ei, &nlflag));

      /* If this variable appears in another MP, can't substitute*/
      if (ctr->equmeta[ei].mp_id != mpid) { break; };

      /* If the variable appears nonlinearly, can't substitute */
      if (nlflag) { break; }

      /* We've seen this variable in more than one equation of this MP */
      if (objequ != IdxNA) {
         objequ = IdxNA;
         break;
      }

      objequ = ei;
      val = val_;

   } while (jac);

   if (objequ != IdxNA && ctr->equs[objequ].object == ConeInclusion &&
      ctr->equs[objequ].cone == CONE_0) {

      /* The objequ is a mapping */
      ctr->equs[objequ].object = DefinedMapping;
      ctr->equs[objequ].cone   = CONE_NONE;

      trace_process("[MP] MP(%s) now has %s as objective equation\n",
                    mp_getname(mp), ctr_printequname(ctr, objequ));
      S_CHECK(mp_setobjequ_internal(mp, ei));
      S_CHECK(mp_setobjcoef(mp, val));

      return OK;
   }


   trace_process("[MP] MP(%s) has no objective equation\n", mp_getname(mp));

   return OK;
}
