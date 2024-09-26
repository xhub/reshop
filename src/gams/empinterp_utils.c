
#include "compat.h"
#include "empinterp_utils.h"
#include "empinterp_vm_utils.h"
#include "empparser.h"
#include "mdl.h"
#include "printout.h"
#include "win-compat.h"

#include "gamsapi_utils.h"

/**
 * @brief Generate the full label from entry (basename and UELs)
 *
 * Set the label name to basename(uel1_name, uel2_name, uel3_name)
 *
 * @param entry           the entry with basename and uels
 * @param dcth            the handle to the dictionary (dct)
 * @param[out] labelname  the generated labelname
 *
 * @return                the error code
 */
int genlabelname(DagRegisterEntry * restrict entry, Interpreter *interp,
                 char **labelname)
{
   assert(entry->nodename && (entry->nodename_len > 0));

   /* No UEL, just copy basename into labelname */
   if (entry->dim == 0) {
      *labelname = strndup(entry->nodename, entry->nodename_len);
      return OK;
   }

   gdxStrIndex_t uels;
   unsigned uels_len[GMS_MAX_INDEX_DIM];

   size_t strsize = entry->nodename_len;
   size_t size = strsize;

   for (unsigned i = 0, len = entry->dim; i < len; ++i) {
      interp->ops->gms_get_uelstr(interp, entry->uels[i], sizeof(uels[i]), uels[i]);
      uels_len[i] = strlen(uels[i]);
      strsize += uels_len[i];
   }

   strsize += 2 + entry->dim; /* 2 parenthesis and dim-1 commas and NUL*/
   char *lname;
   MALLOC_(lname, char, strsize);

   memcpy(lname, entry->nodename, size);
   lname[size++] = '(';

   unsigned uel_len = uels_len[0];
   memcpy(&lname[size], uels[0], uel_len*sizeof(char));
   size += uel_len;

   for (unsigned i = 1, len = entry->dim; i < len; ++i) {
      lname[size++] = ',';
      uel_len = uels_len[i];
      memcpy(&lname[size], uels[i], uel_len*sizeof(char));
      size += uel_len;
   }

   lname[size++] = ')';
   lname[size] = '\0';
   assert(size+1 == strsize);

   *labelname = lname;

   return OK;
}


DagRegisterEntry* regentry_new(const char *basename, unsigned basename_len,
                               uint8_t dim)
{
   DagRegisterEntry *regentry;
   MALLOCBYTES_NULL(regentry, DagRegisterEntry, sizeof(DagRegisterEntry) + dim*sizeof(int));

   regentry->nodename = basename;
   if (basename_len >= UINT16_MAX) {
      error("[empinterp] EMPDAG label '%s' must be smaller than %u\n", basename, UINT16_MAX);
      FREE(regentry);
      return NULL;
   }

   regentry->nodename_len = basename_len;
   regentry->dim = dim;

   return regentry;
}

DagRegisterEntry* regentry_dup(DagRegisterEntry *regentry)
{
   DagRegisterEntry *regentry_;
   uint8_t dim = regentry->dim;

   size_t obj_size = sizeof(DagRegisterEntry) + dim*sizeof(int);
   MALLOCBYTES_NULL(regentry_, DagRegisterEntry, obj_size);

   memcpy(regentry_, regentry, obj_size);

   return regentry_;
}

const char *arctype_str(ArcType type)
{
   switch (type) {
   case ArcVF:
      return "ArcVF";
   case ArcCtrl:
      return "ArcCtrl";
   case ArcNash:
      return "ArcNash";
   default:
      return "ERROR: invalid arc type";
   }
}

int runtime_error(unsigned linenr)
{
   error("[empinterp] ERROR line %u: unexpected runtime error\n", linenr);
   return Error_RuntimeError;
}

int interp_ops_is_imm(Interpreter *interp)
{

   if (interp->ops->type != ParserOpsImm) {
      error("[empinterp] RUNTIME ERROR at line %u: while parsing the %s "
            "keyword the empinfo interpreter is in the VM mode. In the old-style"
            "empinfo syntax, one cannot use loop or any EMPDAG features.\n",
            interp->linenr, toktype2str(interp->cur.type));
      return Error_EMPRuntimeError;
   }

   return OK;
}

int has_longform_solve(Interpreter *interp)
{
   rhp_idx objvar;
   S_CHECK(mdl_getobjvar(interp->mdl, &objvar));

   if (objvar == IdxNA) {
      error("[empinterp] ERROR: while parsing the %s keyword: the GAMS solve "
            "statement must be in the long form, that is contain a "
            "minimization/maximization statement\n",
            toktype2str(interp->cur.type));

      return Error_EMPIncorrectInput;
   }

   return OK;
}


