#include "empinfo.h"
#include "empinterp.h"
#include "empinterp_ops_utils.h"
#include "empinterp_priv.h"
#include "empinterp_symbol_resolver.h"
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

#include "dctmcc.h"

//NOLINTBEGIN
UNUSED static int imm_gms_resolve_param(Interpreter* restrict interp, unsigned * restrict p)
{
   TO_IMPLEMENT("GAMS Parameter resolution not implemented in immediate mode");
}

UNUSED static int imm_gms_resolve_set(Interpreter* restrict interp, unsigned * restrict p)
{
   TO_IMPLEMENT("GAMS Set resolution not implemented in immediate mode");
}
//NOLINTEND

int gmssym_iterator_init(Interpreter* restrict interp)
{
   GmsSymIterator * restrict iterator = &interp->gms_sym_iterator;
   assert(!iterator->active);
   assert(interp->cur.symdat.dim < GMS_MAX_INDEX_DIM);

   iterator->active = true;
   iterator->compact = true;
   iterator->loop_needed = false;


   memset(&iterator->uels, 0, sizeof(int)*interp->cur.symdat.dim);

   gmsindices_init(&iterator->indices);

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

NONNULL static const char *get_daguid_name(EmpDag *empdag, daguid_t uid)
{
   if (!valid_uid(uid)) { return "None"; }

   return  empdag_getname(empdag, uid);
}

NONNULL_AT(1) static
int imm_common_nodeinit(Interpreter *interp, daguid_t uid, DagRegisterEntry *regentry)
{
   if (regentry) {
      regentry->daguid_parent = uid;
      S_CHECK(dagregister_add(&interp->dagregister, regentry));
   }

   if (valid_uid(interp->daguid_grandparent)) {
      errormsg("[empinterp] ERROR: grandparent uid is valid, please file a bug\n");
      return Error_EMPRuntimeError;
   }

   trace_empinterp("[empinterp] interpreter daguid (GP,P) push: (%s,%s) -> (%s,%s)\n",
                   get_daguid_name(&interp->mdl->empinfo.empdag, interp->daguid_grandparent),
                   get_daguid_name(&interp->mdl->empinfo.empdag, interp->daguid_parent),
                   get_daguid_name(&interp->mdl->empinfo.empdag, interp->daguid_parent),
                   get_daguid_name(&interp->mdl->empinfo.empdag, uid));

   interp->daguid_grandparent = interp->daguid_parent;
   interp->daguid_parent = uid;

   return OK;
}

NONNULL static int imm_common_nodefini(Interpreter *interp)
{
   trace_empinterp("[empinterp] interpreter daguid (GP,P) pop: (%s,%s) -> (%s,%s)\n",
                   get_daguid_name(&interp->mdl->empinfo.empdag, interp->daguid_grandparent),
                   get_daguid_name(&interp->mdl->empinfo.empdag, interp->daguid_parent),
                   get_daguid_name(&interp->mdl->empinfo.empdag, EMPDAG_UID_NONE),
                   get_daguid_name(&interp->mdl->empinfo.empdag, interp->daguid_grandparent));

   interp->daguid_parent = interp->daguid_grandparent;
   interp->daguid_grandparent = EMPDAG_UID_NONE;

   return OK;
}

static int imm_gms_resolve(Interpreter* restrict interp, UNUSED unsigned *p)
{
   assert(interp->gms_sym_iterator.active && !interp->gms_sym_iterator.loop_needed);
   TokenType toktype = parser_getcurtoktype(interp);
   GmsResolveData data;

   data.type = GmsSymIteratorTypeImm;
   data.symiter.imm.toktype = toktype;
   data.symiter.imm.symidx = interp->cur.symdat.idx;
   data.symiter.imm.symiter = &interp->gms_sym_iterator;
   data.iscratch = &interp->cur.iscratch;

   GmsSymIterator * restrict symiter = &interp->gms_sym_iterator;
   int * restrict uels = symiter->uels;
   int * restrict domindices = interp->cur.symdat.domindices;
   IdentData * restrict idents = symiter->indices.idents;
   assert(symiter->indices.nargs == 0 || symiter->indices.nargs == symiter->ident.dim);

   dctHandle_t dct = interp->dct;

   for (unsigned i = 0, len = gmsindices_nargs(&symiter->indices); i < len; ++i) {
      IdentData *ident = &idents[i];
      switch (ident->type) {
      case IdentUEL: {
         int uelidx = (int)ident->idx;
         uels[i] = uelidx;
         if (uelidx > 0) {
            symiter->compact = false;
         }
         break;
      case IdentUniversalSet:
         uels[i] = 0;
         break;
      case IdentSet: {
         char domstr[GMS_SSSIZE];
         dctDomName(dct, domindices[i], domstr, sizeof(domstr));
         if (!strncasecmp(domstr, ident->lexeme.start, ident->lexeme.len)) {
            uels[i] = 0;
         } else {
            error("[empinterp] ERROR line %u: subset selection is not implemented yet!\n",
                  ident->lexeme.linenr);
            return Error_NotImplemented;
         }
         }
         break;
      default:
         error("[empinterp] ERROR line %u: while resolving symbol '%.*s', "
               "unsupported ident '%.*s' of type %s at the %u location",
               interp->linenr, emptok_getstrlen(&interp->cur),
               emptok_getstrstart(&interp->cur), ident->lexeme.len,
               ident->lexeme.start, identtype_str(ident->type), i);
         return Error_EMPRuntimeError;
      }

      }
   }

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

   // TODO gmd_resolve
   return dct_resolve(dct, &data);
}

NONNULL static
int imm_identaslabels(Interpreter * restrict interp, unsigned * restrict p, LinkType edge_type)
{
   UNUSED const char* basename = emptok_getstrstart(&interp->cur);
   UNUSED unsigned basename_len = emptok_getstrlen(&interp->cur);

   TokenType toktype;
   S_CHECK(advance(interp, p, &toktype))

   if (toktype != TOK_LPAREN) {
      TO_IMPLEMENT("Label without indices");
   }

   GmsIndicesData indices;
   gmsindices_init(&indices);

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
      S_CHECK(genlabelname(regentry, interp, &labelname));
      interp->regentry = NULL;
   }

   A_CHECK(*mp, empdag_newmpnamed(empdag, sense, labelname));

   trace_empinterp("[empinterp] line %u: new MP(%s) #%u with sense %s\n",
                   interp->linenr, empdag_getmpname(empdag, (*mp)->id), (*mp)->id,
                   sense2str(sense));

   S_CHECK(imm_common_nodeinit(interp, mpid2uid((*mp)->id), regentry));

   free(labelname);

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

   return OK;
}

static int imm_mp_setaschild(Interpreter *interp, MathPrgm *mp)
{
   if (valid_uid(interp->daguid_child)) {
      error("[empinterp] ERROR at line %u: child daguid has valid value, "
            "should be invalid\n", interp->linenr);
      return Error_EMPRuntimeError;
   }

   interp->daguid_child = mpid2uid(mp->id);

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

static int imm_nash_new(Interpreter *interp, Nash **nash)
{
   EmpDag *empdag = &interp->mdl->empinfo.empdag;

   /* ---------------------------------------------------------------------
    * If we had a label, then use it!
    * --------------------------------------------------------------------- */
   char *labelname = NULL;

   DagRegisterEntry *regentry = interp->regentry;
   if (regentry) {
      S_CHECK(genlabelname(regentry, interp, &labelname));
      interp->regentry = NULL;
   }

   A_CHECK(*nash, empdag_newnashnamed(empdag, labelname));

   S_CHECK(imm_common_nodeinit(interp, nashid2uid((*nash)->id), regentry));

   trace_empinterp("[empinterp] line %u: new Nash(%s) #%u\n", interp->linenr,
                   empdag_getnashname(empdag, (*nash)->id), (*nash)->id);

   return OK;
}

static int imm_nash_addmp(Interpreter *interp, nashid_t nashid, MathPrgm *mp)
{
   return empdag_nashaddmpbyid(&interp->mdl->empinfo.empdag, nashid, mp->id);

}

static int imm_nash_finalize(Interpreter *interp, UNUSED Nash *nash)
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

   ovfdef->vi_ovf = avar_fget(v, 0);

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

   if (O_Output & PO_TRACE_EMPINTERP) {
      ovf_param_print(param, PO_TRACE_EMPINTERP);
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
      S_CHECK(genlabelname(regentry, interp, &label));
      interp->regentry = NULL;
   }

   MathPrgm *mp_;
   A_CHECK(mp_, empdag_newmpnamed(empdag, RhpNoSense, label));
   S_CHECK(mp_from_ccflib(mp_, ccflib_idx));

   trace_empinterp("[empinterp] line %u: new CCFLIB MP(%s) #%u\n",
                   interp->linenr, empdag_getmpname(empdag, mp_->id), mp_->id);

   S_CHECK(imm_common_nodeinit(interp, mpid2uid(mp_->id), regentry));
   *mp = mp_;

   free(label);

   return OK;
}

static int imm_mp_ccflib_finalize(Interpreter* restrict interp, MathPrgm *mp)
{
   S_CHECK(mp_finalize(mp));

   /* TODO: what are we doing thing here? */
   S_CHECK(imm_common_nodefini(interp));

//   return rhp_uint_add(&interp->edgevfovjs, mp->id);
   return OK;
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

UNUSED static int read_param(UNUSED Interpreter *interp, UNUSED unsigned *p,
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

UNUSED static int imm_ctr_dualvar(Interpreter *interp, bool is_flipped)
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

static int imm_read_param(Interpreter *interp, unsigned *p, IdentData *data,
                          const char *ident_str, unsigned *param_gidx)
{
   TO_IMPLEMENT("read_param in immediate mode");
}

static int imm_read_elt_vector(Interpreter *interp, const char *identstr,
                               IdentData *ident, GmsIndicesData *gmsindices, double *val)
{
   unsigned idx;
   const NamedVecArray *container = ident->type == IdentVector ? 
      &interp->globals.vectors : &interp->globals.localvectors;

   idx = namedvec_findbyname_nocase(container, identstr);
   if (idx == UINT_MAX) {
      error("[empinterp] unexpected runtime error: couldn't find vector '%s'\n", identstr);
      return Error_EMPRuntimeError;
   }

   const Lequ * vec = &container->list[idx];
   
   if (gmsindices->nargs != 1) {
      error("[empinterp] ERROR line %u: GAMS indices for symbol '%s' has dimension %u, expected 1\n", 
            interp->linenr, identstr, gmsindices->nargs);
      return Error_EMPRuntimeError;
   }

   IdentData *id = &gmsindices->idents[0];

   if (id->type != IdentUEL) {
      error("[empinterp] ERROR line %u: GAMS indices for symbol '%s' has dimension %u, expected 1\n", 
            interp->linenr, identstr, gmsindices->nargs);
      return Error_EMPRuntimeError;
   }

   unsigned uel = id->idx;

   int *uels = vec->vis;
   for (unsigned i = 0, len = vec->len; i < len; ++i) {
      if (uels[i] == uel) {
         *val = vec->coeffs[i];
         return OK;
      }
   }

   error("[empinterp] ERROR line %u: could not find UEL '%*s' #%u in vector '%s'\n",
         interp->linenr, id->lexeme.len, id->lexeme.start, uel, identstr);

   return Error_EMPIncorrectInput;
}

const ParserOps parser_ops_imm = {
   .type = ParserOpsImm,
   .ccflib_new            = imm_mp_ccflib_new,
   .ccflib_finalize       = imm_mp_ccflib_finalize,
   .ctr_markequasflipped  = imm_ctr_markequasflipped,
   .gms_get_uelidx        = get_uelidx_via_dct,
   .gms_get_uelstr        = get_uelstr_via_dct,
   .gms_resolve_sym       = imm_gms_resolve,
   .identaslabels         = imm_identaslabels,
   .mp_addcons            = imm_mp_addcons,
   .mp_addvars            = imm_mp_addvars,
   .mp_addvipairs         = imm_mp_addvipairs,
   .mp_addzerofunc        = imm_mp_addzerofunc,
   .mp_finalize           = imm_mp_finalize,
   .mp_new                = imm_mp_new,
   .mp_setaschild         = imm_mp_setaschild,
   .mp_setobjvar          = imm_mp_setobjvar,
   .mp_setprobtype        = imm_mp_setprobtype,
   .nash_finalize         = imm_nash_finalize,
   .nash_new              = imm_nash_new,
   .nash_addmp            = imm_nash_addmp,
   .ovf_addbyname         = imm_ovf_addbyname,
   .ovf_addarg            = imm_ovf_addarg,
   .ovf_paramsdefstart    = imm_ovf_getparamsdef,
   .ovf_setparam          = imm_ovf_setparam,
   .ovf_setrhovar         = imm_ovf_setrhovar,
   .ovf_check             = imm_ovf_check,
   .ovf_param_getvecsize  = imm_ovf_param_getvecsize,
   .ovf_getname           = imm_ovf_getname,
   .read_param            = imm_read_param,
   .read_elt_vector       = imm_read_elt_vector,
   .resolve_lexeme_as_gmssymb = dct_findlexeme,
};
