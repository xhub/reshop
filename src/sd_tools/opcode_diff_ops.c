#include "container.h"
#include "ctr_gams.h"
#include "equ.h"
#include "nltree.h"
#include "opcode_diff_ops.h"
#include "macros.h"
#include "mdl.h"
#include "open_lib.h"
#include "printout.h"
#include "sd_tool.h"
#include "tlsdef.h"

struct opcode_diff_data {
   int *instrs;
   int *args;
   int len;
   int *d_instrs;
   int *d_args;
   int d_maxlen;
};

static inline void addcode(int ** restrict deriv_instrs, int ** restrict deriv_args,
                           int * restrict len, unsigned * restrict max_len,
                           enum nlopcode c, int f)
{
   assert(*max_len > 0);

   if ((unsigned)*len >= *max_len) {
      (*max_len) *= 2;
      REALLOC(*deriv_instrs, int, *max_len);
      REALLOC(*deriv_args, int, *max_len);
   }

   (*deriv_instrs)[*len] = c;
   (*deriv_args)[*len] = f;
   (*len)++;
}

static inline void copyblock(int ** restrict deriv_instrs,
                             int ** restrict deriv_args,
                             int *  restrict deriv_len,
                             unsigned * restrict deriv_maxlen,
                             int s, const int * restrict expend,
                             const int * restrict instrs,
                             const int * restrict args)
{
   int i, istart = 0, iend = -1;

   if (expend[s] > -1) {
      /* ohuber: added assert(s > 0) to fix bug reading before start */
      assert(s > 0);
      if (expend[s-1] >= -1) {          /* no swap */
         istart = expend[s-1] + 1;
         iend = expend[s];
      }
   }

#if 0
   if (expend[s] > 0) {
      if (expend[s-1] >= 0) {           /* no swap */
         istart = expend[s-1] + 1;
         iend = expend[s];
      } else {                          /* top return top-1 */
         istart = expend[s-2] + 1;
         iend = -expend[s-1];
      }
   } else {                             /* top-1 return top */
      istart = 1 - expend[s];
      iend = expend[s+1];
   }
#endif

   /*  TODO(xhub) optimize that with memcpy for long copy ... */
   for (i = istart; i <= iend; i++) {
      addcode(deriv_instrs, deriv_args, deriv_len, deriv_maxlen, instrs[i], args[i]);
   }

   int len = *deriv_len;
   while (len > 0 &&
          (*deriv_instrs)[len-1] == nlFuncArgN) {
      len--;
   }

   *deriv_len = len;
}

static void reverse(int * restrict instrs, int * restrict args, int m, int n)
{
   while (m < n) {
      int instr = instrs[m];
      int arg = args[m];
      instrs[m] = instrs[n];
      args[m] = args[n];
      instrs[n] = instr;
      args[n] = arg;
      m++;
      n--;
   }
}

static void swap(int * restrict instrs, int * restrict args, int a, int b, int c)
{
   /* ---------------------------------------------------------------------
    *
    *        a
    *   ----        ----
    *   u'   b      v'
    *   ----        u
    *   v'    ->    mul
    *   u           ----
    *   mul  c      u'
    *   ----        ----
    *
    * --------------------------------------------------------------------- */

   reverse(instrs, args, a+1, b);
   reverse(instrs, args, b+1, c);
   reverse(instrs, args, a+1, c);
}


int opcode_diff(int ** restrict deriv_instrs,
               int ** restrict deriv_args,
               int * restrict pderiv_maxlen,
               const int * restrict instrs,
               const int * restrict args,
               int vidx, char *errmsg)
{
   int *expend, *expderiv;
   bool *derivexists;
   int s, nargs;

   int deriv_len = 0;
   unsigned deriv_maxlen = *pderiv_maxlen;

   int codelen = args[0];

   if (deriv_maxlen < (unsigned)codelen) {
      deriv_maxlen = MAX(codelen, 1);
      REALLOC_(*deriv_instrs, int, codelen);
      REALLOC_(*deriv_args, int, codelen);
   }

   if (codelen <= 0) {
      deriv_args[0] = 0;
      (*deriv_instrs)[0] = nlHeader;
      return EXIT_SUCCESS;
   }

   CALLOC_(derivexists, bool, codelen + 1);
   MALLOC_(expend, int, (2*codelen + 2));
   expderiv = &expend[codelen + 1];

   s = 0;
   expend[s] = -1;
   expderiv[s] = -1;

   if (instrs[0] != nlHeader) {
      snprintf(errmsg, 256, "malformed opcode, the first instruction is not nlHeader, but %d!\n", instrs[0]);
      return EXIT_FAILURE;
   }

   for (int k = 0; k < codelen; k++) {
      enum nlopcode key = instrs[k];

      switch (key) {
      case nlNoOp:
         break;

      case nlHeader:
         addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, key, args[k]);
         break;

      case nlStore:
         if (deriv_len == 1) {
            /* Make sure that the value is zero as we only stored the Header. */
            addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_zero);
         }
         addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, key, args[k]);
         break;

      case nlPushV:
         s++;
         if (args[k] == vidx) {
            derivexists[s] = true;
            addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_one);
         } else {
            derivexists[s] = false;
         }
         break;

      case nlUMinV:
         s++;
         if (args[k] == vidx) {
            derivexists[s] = true;
            addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_one);
            addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlUMin, 0);
         } else {
            derivexists[s] = false;
         }
         break;

      case nlPushZero:
      case nlPushI:
         s++;
         derivexists[s] = false;
         break;

      case nlAdd:
         s--;
         if (derivexists[s]) {
            if (derivexists[s+1]) {             /* u v */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlAdd, 0);
            }                                   /* u c */
         } else {
            if (derivexists[s+1]) {             /* c v */
               derivexists[s] = true;
            }                                   /* c c */
         }
         break;

      case nlAddV:
         if (args[k] == vidx) {
            if (derivexists[s]) {               /* u v */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlAddI, nlconst_one);
            } else {                            /* c v */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_one);
               derivexists[s] = true;
            }
         }
         break;

      case nlAddI:
         break;

      case nlSub:
         s--;
         if (derivexists[s]) {
            if (derivexists[s+1]) {             /* u v */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlSub, 0);
            }                                   /* u c */
         } else {
            if (derivexists[s+1]) {             /* c v */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlUMin, 0);
               derivexists[s] = true;
            }                                   /* c c */
         }
         break;

      case nlSubV:
         if (args[k] == vidx) {
            if (derivexists[s]) {               /* u v */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlSubI, nlconst_one);
            } else {                            /* c v */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_one);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlUMin, 0);
               derivexists[s] = true;
            }
         }
         break;

      case nlSubI:
         break;

      case nlMul:
         s--;
         if (derivexists[s]) {
            if (derivexists[s+1]) {             /* u v       */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* v'u       */
               swap(*deriv_instrs, *deriv_args, expderiv[s-1], expderiv[s], deriv_len-1);
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* u'v       */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlAdd, 0);        /* v'u + u'v */
            } else {                            /* u c       */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);
            }
         } else {
            if (derivexists[s+1]) {             /* c v       */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);
               derivexists[s] = true;
            }
         }
         break;

      case nlMulV:
         if (args[k] == vidx) {                /* v' = 1     */
            if (derivexists[s]) {                  /* u v        */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMulV, vidx);         /* u'v        */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);  /* u          */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlAdd, 0);           /* u'v + u    */
            } else {                               /* c v        */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               derivexists[s] = true;
            }
         } else {
            if (derivexists[s]) {                  /* u c        */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMulV, args[k]);
            }
         }
         break;

      case nlMulI:
         if (derivexists[s]) {                     /* u c        */
            addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMulI, args[k]);
         }
         break;

      case nlMulIAdd:                /* pushi mul add   u + v*c  */
         s--;
         if (derivexists[s]) {
            if (derivexists[s+1]) {                /* u + v*c    */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMulI, args[k]);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlAdd, 0);
            }
         } else {
            if (derivexists[s+1]) {                /* a   v*c    */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMulI, args[k]);
               derivexists[s] = true;
            }
         }
         break;

      case nlDiv:
         s--;
         if (derivexists[s]) {
            if (derivexists[s+1]) {                 /* u v        */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);            /* v'u        */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlUMin, 0);           /* -v'u       */
               swap(*deriv_instrs, *deriv_args, expderiv[s-1], expderiv[s], deriv_len-1);
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);            /* u'v        */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlAdd, 0);            /* u'v - v'u  */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnsqr);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
            } else {                                /* u c        */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
            }
         } else {
            if (derivexists[s+1]) {                 /* c v        */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);            /* v'u        */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlUMin, 0);           /* -v'u       */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args); /* v          */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnsqr);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
               derivexists[s] = true;
            }
         }
         break;

      case nlDivV:
         if (args[k] == vidx) {                /* v' = 1      */
            if (derivexists[s]) {                  /* u v         */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMulV, vidx);         /* u'v         */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlSub, 0);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushV, vidx);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnsqr);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
            } else {                               /* c v         */
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);  /* v'u   v'=1  */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlUMin, 0);          /* -v'u        */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushV, vidx);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnsqr);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
               derivexists[s] = true;
            }
         } else {
            if (derivexists[s]) {                  /* u c         */
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDivV, args[k]);
            }
         }
         break;

      case nlDivI:
         if (derivexists[s]) {
            addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDivI, args[k]);
         }
         break;

      case nlUMin:
         if (derivexists[s]) {
            addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlUMin, 0);
         }
         break;

      case nlCallArg1:
         if (derivexists[s]) {
            switch (args[k]) {
            case fnsqr:
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMulI, nlconst_two);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* apply chain rule */
               break;

            case fnexp:
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnexp);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* apply chain rule */
               break;

            case fnlog:
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_one);
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* apply chain rule */
               break;

            case fnlog10:
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_ooln10);
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* apply chain rule */
               break;

            case fnlog2:
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_ooln2);
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* apply chain rule */
               break;

            case fnsin:
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fncos);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* apply chain rule */
               break;

            case fncos:
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnsin);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlUMin, 0);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* apply chain rule */
               break;

            case fnarctan:
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_one);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_one);
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnsqr);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlAdd, 0);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* apply chain rule */
               break;

            case fnerrf:
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnsqr);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMulI, nlconst_half);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlUMin, 0);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnexp);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMulI, nlconst_oosqrt2pi);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* apply chain rule */
               break;

            case fnsqrt:
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_half);
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, args[k]);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* apply chain rule */
               break;

            case fnabs:
               copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_zero);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg2, fnrelopge);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_one);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlPushI, nlconst_one);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlUMin, 0);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlFuncArgN, 3);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArgN, fnifthen);
               addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);        /* deriv*ifthen(func>=0,1,-1) */
               break;

            case fntrunc:
            case fnfloor:
            case fnceil:
            case fnround:
            case fnsign:
               derivexists[s] = false;
               deriv_len = expderiv[s-1] + 1;  /* get rid of deriv code */
               break;

            default:
               snprintf(errmsg, 256, "*** cannot diff %s %s\n",
                        nlinstr2str(key),
                        func_code_name[args[k]]);
               return EXIT_FAILURE;
            }
         }
         break;

      case nlCallArg2:
         s--;
         if (derivexists[s]) {
            if (derivexists[s+1]) {     /* u v */
               switch (args[k]) {
               case fnrpower:           /* u**v*(v'ln(u) + u'*v/u) */
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnlog);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);
                  swap(*deriv_instrs, *deriv_args, expderiv[s-1], expderiv[s], deriv_len-1);
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlDiv, 0);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlAdd, 0);
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg2, args[k]);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);
                  break;

               default:
                  snprintf(errmsg, 256, "*** cannot diff %s %s\n",
                           nlinstr2str(key),
                           func_code_name[args[k]]);
                  return EXIT_FAILURE;
               }
            } else {                    /* u c */
               switch (args[k]) {
               case fnrpower:
               case fnpower:
               case fnvcpower:
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args);       /* c   */
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);                  /* u'c */
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);         /* u   */
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args);       /* c   */
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlSubI, nlconst_one);       /* c-1 */
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg2, args[k]); /* power(u,c-1) */
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);  /* u'c*power(u,c-1) */
                  break;

               default:
                  snprintf(errmsg, 256, "*** cannot diff %s %s\n",
                           nlinstr2str(key),
                           func_code_name[args[k]]);
                  return EXIT_FAILURE;
               }
            }
         } else {
            if (derivexists[s+1]) {     /* c v */
               derivexists[s] = true;
               switch (args[k]) {
               case fnrpower:
               case fncvpower:          /* c**v*ln(c)*v' */
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s+1, expend, instrs, args);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg2, args[k]);
                  copyblock(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, s, expend, instrs, args);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlCallArg1, fnlog);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);
                  addcode(deriv_instrs, deriv_args, &deriv_len, &deriv_maxlen, nlMul, 0);
                  break;

               default:
                  snprintf(errmsg, 256, "*** cannot diff %s %s\n",
                           nlinstr2str(key),
                           func_code_name[args[k]]);
                  return EXIT_FAILURE;
               }
            }
         }
         break;

      case nlFuncArgN:
         snprintf(errmsg, 256, "*** cannot diff %s\n", nlinstr2str(key));
         nargs = args[k];
         k++;
         s = s - nargs + 1;
         break;

      default:
         snprintf(errmsg, 256, "differ: unknown instruction %s at location %5d\n",
                  nlinstr2str(key), k);
         return EXIT_FAILURE;
      }

      expderiv[s] = deriv_len - 1;
      expend[s] = k;
   }

   (*deriv_args)[0] = deriv_len;
   *pderiv_maxlen = deriv_maxlen;

   FREE(derivexists);
   FREE(expend);

   return EXIT_SUCCESS;
}

static int opcode_diff_deriv(struct sd_tool *sd_tool, int vidx, Equ *e)
{
   struct opcode_diff_data *diff_data = (struct opcode_diff_data *)sd_tool->data;
   char errmsg[256];
   int rc;

   if (!diff_data) {
     return OK;
   }

   if (diff_data->len == 0) {
      error("%s :: no nlopcode!\n", __func__);
      return OK;
   }

   if ((rc = opcode_diff(&diff_data->d_instrs,
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

static int opcode_diff_alloc_fromequ(struct sd_tool *sd_tool, Container *ctr, const Equ *e)
{
   CALLOC_(sd_tool->data, struct opcode_diff_data, 1);

   struct opcode_diff_data *diff_data = (struct opcode_diff_data *)sd_tool->data;

   int *instrs_tmp;
   int *args_tmp;
   int codelen;

   if (e->tree && e->tree->root) {
      /* TODO(Xhub) fix this horrible thing: we just need the ctr for the memory */
      // HACK ARENA
      S_CHECK(nltree_buildopcode(ctr, e, &instrs_tmp, &args_tmp, &codelen));
   } else {
      codelen = 0;
   }

   if (codelen > 0) {
      diff_data->len = codelen;
      int *tmparray;
      MALLOC_(tmparray, int, 2*codelen);
      diff_data->instrs = tmparray;
      diff_data->args = &tmparray[codelen];
      memcpy(diff_data->instrs, instrs_tmp, sizeof(int) * codelen);
      memcpy(diff_data->args, args_tmp, sizeof(int) * codelen);

      /*  Set the right equation number */
      diff_data->args[diff_data->len-1] = 1 + e->idx;
   } else {
     FREE(sd_tool->data);
   }

   // HACK ARENA
   ctr_relmem_old(ctr);

   return OK;
}

static int opcode_diff_alloc(struct sd_tool *sd_tool, Container *ctr, rhp_idx ei)
{
   CALLOC_(sd_tool->data, struct opcode_diff_data, 1);

   struct opcode_diff_data *diff_data = (struct opcode_diff_data *)sd_tool->data;

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

   return OK;
}

static void opcode_diff_dealloc(struct sd_tool *sd_tool)
{
   if (!sd_tool->data) return;

   struct opcode_diff_data *diff_data = (struct opcode_diff_data *)sd_tool->data;

   FREE(diff_data->instrs);
   diff_data->args = NULL; /* diff_data->instrs and diff_data->args are contiguous  */
   FREE(diff_data->d_instrs);
   FREE(diff_data->d_args);
   FREE(diff_data);
}

const struct sd_ops opcode_diff_ops = {
   .allocdata    = opcode_diff_alloc,
   .allocdata_fromequ    = opcode_diff_alloc_fromequ,
   .deallocdata  = opcode_diff_dealloc,
   .assemble     = NULL,
   .deriv        = opcode_diff_deriv,
};
