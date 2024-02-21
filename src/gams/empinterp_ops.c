#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_priv.h"
#include "empinterp_utils.h"
#include "empparser_priv.h"
#include "empparser_utils.h"
#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
#include "ovf_parameter.h"
#include "ovfinfo.h"
#include "printout.h"
#include "toplayer_utils.h"


//NOLINTBEGIN
static int imm_gms_resolve_param(Interpreter* restrict interp, unsigned * restrict p)
{
   TO_IMPLEMENT("GAMS Parameter resolution not imnplemented in immediate mode");
}

static int imm_gms_resolve_set(Interpreter* restrict interp, unsigned * restrict p)
{
   TO_IMPLEMENT("GAMS Set resolution not imnplemented in immediate mode");
}
//NOLINTEND

int parser_filter_start(Interpreter* restrict interp)
{
   assert(!interp->gms_sym_iterator.active);

   interp->gms_sym_iterator.active = true;
   interp->gms_sym_iterator.compact = true;
   interp->gms_sym_iterator.loop_needed = false;
   interp->gms_sym_iterator.indices.nargs = interp->cur.gms_dct.dim;

   assert(interp->cur.gms_dct.dim < GMS_MAX_INDEX_DIM);
   
   memset(&interp->gms_sym_iterator.uels, 0, sizeof(int)*interp->cur.gms_dct.dim);

   return OK;
}

int parser_filter_set(Interpreter* restrict interp, unsigned i, int val)
{
   assert(interp->gms_sym_iterator.active);
   if (val < 0) {
      error("[empinterp] ERROR: slicing by set is not supported yet. Please expand "
            "identifier '%.*s' for symbol '%.*s'.\n",
            emptok_getstrlen(&interp->peek), emptok_getstrstart(&interp->peek),
            emptok_getstrlen(&interp->cur), emptok_getstrstart(&interp->cur));
      return Error_NotImplemented;
   }

   assert(interp->gms_sym_iterator.active && i < GMS_MAX_INDEX_DIM);
   interp->gms_sym_iterator.uels[i] = val;
   
   if (val > 0) {
      interp->gms_sym_iterator.compact = false;
   }

   return OK;
}

NONNULL_ONEIDX(1) static
int imm_common_nodeinit(Interpreter *interp, daguid_t uid, DagRegisterEntry *regentry)
{
   if (regentry) {
      regentry->daguid = uid;
      S_CHECK(dagregister_add(&interp->dagregister, regentry));
   }

   if (valid_uid(interp->uid_grandparent)) {
      errormsg("[empinterp] ERROR: grandparent uid is valid, please file a bug\n");
      return Error_EMPRuntimeError;
   }

   interp->uid_grandparent = interp->uid_parent;
   interp->uid_parent = uid;

   return OK;
}

NONNULL static int imm_common_nodefini(Interpreter *interp)
{
   interp->uid_parent = interp->uid_grandparent;
   interp->uid_grandparent = EMPDAG_UID_NONE;

   return OK;
}

static int imm_gms_resolve(Interpreter* restrict interp)
{
   assert(interp->gms_sym_iterator.active && !interp->gms_sym_iterator.loop_needed);
   TokenType toktype = parser_getcurtoktype(interp);
   DctResolveData data;

   data.type = GmsSymIteratorTypeImm;
   data.symiter.imm.toktype = toktype;
   data.symiter.imm.symidx = interp->cur.gms_dct.idx;
   data.symiter.imm.symiter = &interp->gms_sym_iterator;
   data.scratch = &interp->cur.iscratch;

   dctHandle_t dct = interp->dct;

   switch (toktype) {
   case TOK_GMS_VAR:
      data.payload.v = &interp->cur.payload.v;
      break;
   case TOK_GMS_EQU:
      data.payload.e = &interp->cur.payload.e;
      break;
   default:
      error("[empinterp] ERROR: cannot resolve token '%s'", toktype2str(toktype));
      return Error_EMPRuntimeError;
   }

   interp->gms_sym_iterator.active = false;

   return dct_resolve(dct, &data);
}

static int imm_gms_parse(Interpreter * restrict interp, unsigned * restrict p)
{
   TokenType toktype;

   /* We always start, as it resets the filter, which is always required */
   S_CHECK(parser_filter_start(interp));

   /* ---------------------------------------------------------------------
    * We distinguish between peeked token being '(' or not
    * --------------------------------------------------------------------- */

   unsigned p2 = *p;
   S_CHECK(peek(interp, &p2, &toktype));

   if (toktype == TOK_LPAREN) {
      unsigned i = 0;
      interp_peekseqstart(interp);

      do {
         bool has_single_quote = false, has_double_quote = false;

         if (i >= GMS_MAX_INDEX_DIM) {
            error("[empinterp] ERROR: EMP identifier '%.*s' has more than %u identifiers!\n",
                  interp->cur.len, interp->cur.start, GMS_MAX_INDEX_DIM);
            return Error_EMPIncorrectInput;
         }

         /* get the next token */
         S_CHECK(peek(interp, &p2, &toktype));

         if (toktype == TOK_SINGLE_QUOTE) {
            has_single_quote = true;
            S_CHECK(peek(interp, &p2, &toktype));
         } else if (toktype == TOK_DOUBLE_QUOTE) {
            has_double_quote = true;
            S_CHECK(peek(interp, &p2, &toktype));
         }

         PARSER_EXPECTS_PEEK(interp,
                             "A string (UEL (set element), subset, variable) is required",
                             TOK_GMS_SET, TOK_GMS_UEL, TOK_STAR, TOK_REAL);

         /* -----------------------------------------------------------------
          * If we have a real, it could still be a UEL
          * ----------------------------------------------------------------- */

         if (toktype == TOK_REAL) {
            S_CHECK(gms_find_ident_in_dct(interp, &interp->peek));
            toktype = parser_getpeektoktype(interp);
            if (toktype != TOK_GMS_UEL) {
               error("[empinterp] ERROR: the number '%.*s' is not a UEL\n",
                     interp->peek.len, interp->peek.start);
               return Error_EMPIncorrectInput;
            }
         }

         switch (toktype) {
         case TOK_GMS_SET:
            S_CHECK(parser_filter_set(interp, i, -interp->peek.gms_dct.idx));
            break;
         case TOK_GMS_UEL:
            S_CHECK(parser_filter_set(interp, i, interp->peek.gms_dct.idx));
            break;
         case TOK_STAR:
            S_CHECK(parser_filter_set(interp, i, 0));
            break;
         default:
            errormsg("[empinterp] ERROR: unexpected failure.\n");
            return Error_RuntimeError;
         }

         i++;

         S_CHECK(peek(interp, &p2, &toktype));

         if (has_single_quote) {
            S_CHECK(parser_expect_peek(interp, "Closing \"'\" expected", TOK_SINGLE_QUOTE));
            S_CHECK(peek(interp, &p2, &toktype));
         } else if (has_double_quote) {
            S_CHECK(parser_expect_peek(interp, "Closing '\"' expected", TOK_DOUBLE_QUOTE));
            S_CHECK(peek(interp, &p2, &toktype));
         }

      } while (toktype == TOK_COMMA);

      S_CHECK(parser_expect_peek(interp, "Closing ')' is required", TOK_RPAREN));

      ptrdiff_t sym_total_len = interp->peek.start - interp->cur.start + interp->peek.len;

      assert(sym_total_len >= 0 && sym_total_len < INT_MAX);

      if (i != interp->cur.gms_dct.dim) {
         error("[empinterp] ERROR: GAMS symbol '%.*s' has dimension %d but %u "
               "indices were given!\n",
               interp->cur.len, interp->cur.start, interp->cur.gms_dct.dim, i);
         return Error_EMPIncorrectInput;
      }

      /* update the counter */
      *p = p2;
      interp_peekseqend(interp);

   }

   S_CHECK(imm_gms_resolve(interp));

   interp->cur.gms_dct.read = true;

   return OK;
}



NONNULL static
int imm_identaslabels(Interpreter * restrict interp, unsigned * restrict p, EdgeType edge_type)
{
   const char* basename = emptok_getstrstart(&interp->cur);
   unsigned basename_len = emptok_getstrlen(&interp->cur);

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   if (toktype != TOK_LPAREN) {
      TO_IMPLEMENT("Label without indices");
   }

   GmsIndicesData indices;
   gms_indicesdata_init(&indices);

   S_CHECK(parse_gmsindices(interp, p, &indices));

   TO_IMPLEMENT("imm_identaslabels needs to be finished");
}


static int imm_mp_new(Interpreter *interp, RhpSense sense, MathPrgm **mp)
{
   EmpDag *empdag = &interp->mdl->empinfo.empdag;

   /* ---------------------------------------------------------------------
    * If we had a label, then use it!
    * --------------------------------------------------------------------- */
   char *labelname = NULL;

   DagRegisterEntry *regentry = interp->regentry;
   if (regentry) {
      S_CHECK(genlabelname(regentry, interp->dct, &labelname));
      interp->regentry = NULL;
   }

   A_CHECK(*mp, empdag_newmpnamed(empdag, sense, labelname));

   trace_empinterp("[empinterp] line %u: new MP(%s) #%u with sense %s\n",
                   interp->linenr, empdag_getmpname(empdag, (*mp)->id), (*mp)->id,
                   sense2str(sense));

   S_CHECK(imm_common_nodeinit(interp, mpid2uid((*mp)->id), regentry));
   return OK;
}

static int imm_mp_addcons(Interpreter *interp, MathPrgm *mp)
{
   Aequ *e = &interp->cur.payload.e;
   return mp_addconstraints(mp, e);
}

static int imm_mp_addvars(Interpreter *interp, MathPrgm *mp)
{
   Avar *v = &interp->cur.payload.v;
   return mp_addvars(mp, v);
}

static int imm_mp_addvipairs(Interpreter *interp, MathPrgm *mp)
{
   assert(interp->pre.type == TOK_GMS_EQU);
   Avar *v = &interp->cur.payload.v;
   Aequ *e = &interp->pre.payload.e;
   return mp_addvipairs(mp, e, v);
}

static int imm_mp_addzerofunc(Interpreter *interp, MathPrgm *mp)
{
   Avar *v = &interp->cur.payload.v;
   return mp_addvipairs(mp, NULL, v);
}

static int imm_mp_finalize(UNUSED Interpreter *interp, MathPrgm *mp)
{
   S_CHECK(imm_common_nodefini(interp));

   S_CHECK(mp_finalize(mp));

   /* This needs to be executed after mp_finalize() */
   if (interp->edgevfovjs.len > 0) {

      rhp_idx objequ = mp_getobjequ(mp);
      assert(valid_ei(objequ)); // TODO support the case where there is no objequ
      
      EmpDag *empdag = &interp->mdl->empinfo.empdag;

      EdgeVF edgevf;
      edgeVFb_init(&edgevf, objequ);
      mpid_t mp_id = mp->id;

      for (unsigned i = 0, len = interp->edgevfovjs.len; i < len; ++i) {

         edgevf.child_id = rhp_uint_at(&interp->edgevfovjs, i);

         S_CHECK(empdag_mpVFmpbyid(empdag, mp_id, &edgevf));

      }

      interp->edgevfovjs.len = 0;
   }

   return OK;
}

static int imm_mp_setobjvar(Interpreter *interp, MathPrgm *mp)
{
   const Avar *v = &interp->cur.payload.v;
   unsigned dim = avar_size(v);

   if (dim != 1) {
      int offset;
      error("[empparser] %nERROR while setting the objective variable of MP(%s):\n",
            &offset, mp_getname(mp));
      error("%*sexpecting '%.*s' to be a scalar variable, got dim = %u.\n", offset,
            "",  emptok_getstrlen(&interp->cur), emptok_getstrstart(&interp->cur),
            dim);
      return Error_EMPIncorrectInput;
   }
   return mp_setobjvar(mp, avar_fget(&interp->cur.payload.v, 0));
}

static int imm_mp_setprobtype(UNUSED Interpreter *interp, MathPrgm *mp, unsigned type)
{
   return mp_setprobtype(mp, type);
}

static int imm_mp_settype(UNUSED Interpreter *interp, MathPrgm *mp, unsigned type)
{
   return mp_settype(mp, type);
}

static int imm_mpe_new(Interpreter *interp, Mpe **mpe)
{
   EmpDag *empdag = &interp->mdl->empinfo.empdag;

   /* ---------------------------------------------------------------------
    * If we had a label, then use it!
    * --------------------------------------------------------------------- */
   char *labelname = NULL;

   DagRegisterEntry *regentry = interp->regentry;
   if (regentry) {
      S_CHECK(genlabelname(regentry, interp->dct, &labelname));
      interp->regentry = NULL;
   }

   A_CHECK(*mpe, empdag_newmpenamed(empdag, labelname));

   S_CHECK(imm_common_nodeinit(interp, mpeid2uid((*mpe)->id), regentry));

   trace_empinterp("[empinterp] line %u: new MPE(%s) #%u\n", interp->linenr,
                   empdag_getmpename(empdag, (*mpe)->id), (*mpe)->id);

   return OK;
}

static int imm_mpe_addmp(UNUSED Interpreter *interp, unsigned mpe_id, MathPrgm *mp)
{
   return empdag_mpeaddmpbyid(&interp->mdl->empinfo.empdag, mpe_id, mp->id);

}

static int imm_mpe_finalize(UNUSED Interpreter *interp, Mpe *mpe)
{
   return imm_common_nodefini(interp);
}

static int imm_ovf_addbyname(UNUSED Interpreter* restrict interp, EmpInfo *empinfo,
                          const char *name, void **ovfdef_data)
{
   OvfDef *ovfdef;
   S_CHECK(ovf_addbyname(empinfo, name, &ovfdef));
   *ovfdef_data = ovfdef;

   return OK;
}

static int imm_ovf_setrhovar(Interpreter* restrict interp, void *ovfdef_data)
{
   OvfDef *ovfdef = ovfdef_data;
   Avar *v = &interp->cur.payload.v;
   unsigned rho_size = avar_size(v);

   if (rho_size != 1) {
      error("[empinterp] ERROR: CCF variable '%.*s' should have size 1, got %u\n",
            interp->cur.len, interp->cur.start, rho_size);
      return Error_EMPIncorrectInput;
   }

   ovfdef->ovf_vidx = avar_fget(v, 0);

   return OK;
}

static int imm_ovf_addarg(Interpreter* restrict interp, void *ovfdef_data)
{
   OvfDef *ovfdef = ovfdef_data;

   return avar_extend(ovfdef->args, &interp->cur.payload.v);
}

static int imm_ovf_getparamsdef(UNUSED Interpreter* restrict interp, void *ovfdef_data,
                             const OvfParamDefList **paramsdef)
{
   OvfDef *ovfdef = ovfdef_data;

   *paramsdef = ovf_getparamdefs(ovfdef->idx);

   return OK;
}

static int imm_ovf_setparam(Interpreter* restrict interp, void *ovfdef_data, unsigned i,
                         OvfArgType type, OvfParamPayload payload)
{
   OvfDef *ovfdef = ovfdef_data;
   assert(i < ovfdef->params.size);
   OvfParam * restrict param = &ovfdef->params.p[i];

   param->type = type;
   if (param->def->get_vector_size) {
      size_t vecsize = param->def->get_vector_size(ovf_argsize(ovfdef));
      param->size_vector = vecsize;

   }

   switch (type) {
   case ARG_TYPE_SCALAR:
      param->val = payload.val;
      break;
   case ARG_TYPE_VEC:
   {
      unsigned vecsize = param->size_vector;
      assert(vecsize > 0);
      MALLOC_(param->vec, double, vecsize);
      memcpy(param->vec, payload.vec, vecsize*sizeof(double));
      break;
   }
   case ARG_TYPE_UNSET:
      if (!param->def->mandatory) {
         /*  Here we are fine, this was not a mandatory argument */
         if (param->def->default_val) {
            unsigned nargs = ovf_argsize(ovfdef);
            S_CHECK(param->def->default_val(param, nargs));
         }
      } else {
         error("[empinterp] ERROR line %u: mandatory parameter '%s' not found (OVF '%s')\n",
         interp->linenr, param->def->name, ovfdef->name);
         return Error_NotFound;
      }
      break;
   default:
   {
 
      error("[empinfo] ERROR: unsupported CCF param type '%s'\n", ovf_argtype_str(type));
      return Error_EMPRuntimeError;
   }
   }

   return OK;
}

static int imm_ovf_check(Interpreter* restrict interp, void *ovfdef_data)
{
   if (O_Output & PO_TRACE_EMPINTERP) {
      OvfDef *ovfdef = ovfdef_data;
      ovf_def_print(ovfdef, PO_TRACE_EMPINTERP, interp->mdl);
   }

   return OK;
}

static int imm_mp_ccflib_new(Interpreter* restrict interp, unsigned ccflib_idx,
                             MathPrgm **mp)
{

   EmpDag *empdag = &interp->mdl->empinfo.empdag;

   /* ---------------------------------------------------------------------
    * If we had a label, then use it!
    * --------------------------------------------------------------------- */
   char *label = NULL;

   DagRegisterEntry *regentry = interp->regentry;
   if (regentry) {
      S_CHECK(genlabelname(regentry, interp->dct, &label));
      interp->regentry = NULL;
   }

   MathPrgm *mp_;
   A_CHECK(mp_, empdag_newmpnamed(empdag, RhpNoSense, label));
   S_CHECK(mp_from_ccflib(mp_, ccflib_idx));

   trace_empinterp("[empinterp] line %u: new CCFLIB MP(%s) #%u\n",
                   interp->linenr, empdag_getmpname(empdag, mp_->id), mp_->id);

   S_CHECK(imm_common_nodeinit(interp, mpid2uid(mp_->id), regentry));
   *mp = mp_;

   return OK;
}

static int imm_mp_ccflib_finalize(Interpreter* restrict interp, MathPrgm *mp)
{
   /* TODO: while are we doing thing here? */
   S_CHECK(imm_common_nodefini(interp));

   EmpDag *empdag = &interp->mdl->empinfo.empdag;

   return rhp_uint_add(&interp->edgevfovjs, mp->id);
}

static unsigned imm_ovf_param_getvecsize(UNUSED Interpreter* restrict interp,
                                      void *ovfdef_data, const OvfParamDef *pdef)
{
   OvfDef *ovfdef = ovfdef_data;
   unsigned nargs = ovf_argsize(ovfdef);

   if (pdef->get_vector_size) {
      return pdef->get_vector_size(nargs);
   }

   return 0;
}

static const char* imm_ovf_getname(UNUSED Interpreter* restrict interp, void *ovfdef_data)
{
   OvfDef *ovfdef = ovfdef_data;
   return ovfdef->name;
}

static int read_param(UNUSED Interpreter *interp, UNUSED unsigned *p,
                      UNUSED IdentData *data, UNUSED DblScratch *scratch,
                      UNUSED unsigned *param_size)
{
   errormsg("[empinterp] Reading a parameter is not supported in immediate mode");
   return Error_NotImplemented;
}

/* ----------------------------------------------------------------------
 * Misc functions
 * ---------------------------------------------------------------------- */

static int imm_ctr_markequasflipped(Interpreter* restrict interp)
{
   Aequ *e = &interp->cur.payload.e; assert(aequ_nonempty(e));
   return ctr_markequasflipped(&interp->mdl->ctr, e);
}

static int imm_ctr_dualvar(Interpreter *interp, bool is_flipped)
{
  /* ----------------------------------------------------------------------
   * Statement is   dualvar v [-] e
   *
   * is_flipped indicates whether the equation is flipped
   * ---------------------------------------------------------------------- */

   assert(interp->pre.type == TOK_GMS_VAR && interp->cur.type == TOK_GMS_EQU);
   Avar *v = &interp->pre.payload.v;
   Aequ *e = &interp->cur.payload.e;

   if (v->size != e->size) {
      Token *vtok = &interp->pre;
      Token *etok = &interp->cur;
      error("[empinterp] ERROR in dualvar statement: variable '%.*s' has size "
            "%u and equation '%.*s' has size %u. The size must be equal.\n",
            emptok_getstrlen(vtok), emptok_getstrstart(vtok), v->size,
            emptok_getstrlen(etok), emptok_getstrstart(etok), e->size);
      return Error_EMPIncorrectInput;
   }

   return mdl_setdualvars(interp->mdl, v, e);
}

const ParserOps parser_ops_imm = {
   .type = ParserOpsImm,
   .ccflib_new            = imm_mp_ccflib_new,
   .ccflib_finalize       = imm_mp_ccflib_finalize,
   .ctr_markequasflipped  = imm_ctr_markequasflipped,
   .gms_parse             = imm_gms_parse,
   .identaslabels         = imm_identaslabels,
   .mp_addcons            = imm_mp_addcons,
   .mp_addvars            = imm_mp_addvars,
   .mp_addvipairs         = imm_mp_addvipairs,
   .mp_addzerofunc        = imm_mp_addzerofunc,
   .mp_finalize           = imm_mp_finalize,
   .mp_new                = imm_mp_new,
   .mp_setobjvar          = imm_mp_setobjvar,
   .mp_setprobtype        = imm_mp_setprobtype,
   .mp_settype            = imm_mp_settype,
   .mpe_finalize          = imm_mpe_finalize,
   .mpe_new               = imm_mpe_new,
   .mpe_addmp             = imm_mpe_addmp,
   .ovf_addbyname         = imm_ovf_addbyname,
   .ovf_addarg            = imm_ovf_addarg,
   .ovf_paramsdefstart    = imm_ovf_getparamsdef,
   .ovf_setparam          = imm_ovf_setparam,
   .ovf_setrhovar         = imm_ovf_setrhovar,
   .ovf_check             = imm_ovf_check,
   .ovf_param_getvecsize  = imm_ovf_param_getvecsize,
   .ovf_getname           = imm_ovf_getname,
};
