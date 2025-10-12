#include <assert.h>
#include <limits.h>

#include "mdl_data.h"
#include "macros.h"
#include "printout.h"
#include "reshop.h"

const char* const mdl_backendnames[] = {
   "GAMS",
   "RHP",
   "JULIA",
   "AMPL",
};

#define DEFINE_STR() \
DEFSTR(MdlProbType_none,"none") \
DEFSTR(MdlProbType_lp,"lp") \
DEFSTR(MdlProbType_nlp,"nlp") \
DEFSTR(MdlProbType_dnlp,"dnlp") \
DEFSTR(MdlProbType_mip,"mip") \
DEFSTR(MdlProbType_minlp,"minlp") \
DEFSTR(MdlProbType_miqcp,"miqcp") \
DEFSTR(MdlProbType_qcp,"qcp") \
DEFSTR(MdlProbType_mcp,"mcp") \
DEFSTR(MdlProbType_mpec,"mpec") \
DEFSTR(MdlProbType_vi,"vi") \
DEFSTR(MdlProbType_emp,"emp") \
DEFSTR(MdlProbType_cns,"cns") \

static const size_t backendnamelen = ARRAY_SIZE(mdl_backendnames);
RESHOP_STATIC_ASSERT(ARRAY_SIZE(mdl_backendnames) == (RhpBackendAmpl+1),
                     "Inconsistency in model backends")

#define DEFSTR(id, str) char id[sizeof(str)];

typedef struct {
   union {
      struct {
      DEFINE_STR()
   };
   char dummystr[1];
   };
} MdlTypesNames;
#undef DEFSTR

#define DEFSTR(id, str) .id = str,

static const MdlTypesNames mdltypesnames = {
DEFINE_STR()
};
#undef DEFSTR

#define DEFSTR(id, str) offsetof(MdlTypesNames, id),
static const unsigned mdltypesnames_offsets[] = {
DEFINE_STR()
};

const unsigned mdltypeslen = ARRAY_SIZE(mdltypesnames_offsets);

RESHOP_STATIC_ASSERT(ARRAY_SIZE(mdltypesnames_offsets) == (MdlType_last+1),
                     "Inconsistency in problem types")


const char* mdltype_name(ModelType type)
{
   if (type >= mdltypeslen) { return "unknown problem type"; }

   return mdltypesnames.dummystr + mdltypesnames_offsets[type];
}

unsigned backend_idx(const char *backendname)
{
   for (unsigned i = 0; i <= backendnamelen; ++i) {
      if (!strcasecmp(backendname, backend2str(i))) {
         return i;
      }
   }

   return UINT_MAX;
}

bool mdltype_hasmetadata(ModelType type)
{
   switch(type) {
   case MdlType_emp:
   case MdlType_mcp:
   case MdlType_mpec:
   case MdlType_vi:
     return true;
   default:
     return false;
   }
}

const char* backend2str(unsigned backendtype)
{
   if (backendtype < backendnamelen) {
      return mdl_backendnames[backendtype];
   } 

   return "INVALID BACKEND";
  
}

bool mdltype_isopt(ModelType type)
{
  assert(type != MdlType_none);

  switch (type) {
  case MdlType_lp:
  case MdlType_nlp:
  case MdlType_dnlp:
  case MdlType_mip:
  case MdlType_minlp:
  case MdlType_miqcp:
  case MdlType_mpec:
  case MdlType_qcp:
    return true;
  default:
    return false;
  }
}

bool mdltype_isvi(ModelType type)
{
  assert(type != MdlType_none);

  switch (type) {
  case MdlType_mcp:
  case MdlType_vi:
    return true;
  default:
    return false;
  }
}

int backend_throw_notimplemented_error(unsigned backend, const char *fn)
{
   error("%s ERROR: support for backend %s not implemented yet\n", fn,
         backend2str(backend));
   return Error_NotImplemented;
}
