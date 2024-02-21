#include <assert.h>
#include <limits.h>

#include "mdl_data.h"
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

const size_t backendnamelen = sizeof(mdl_backendnames)/sizeof(char*);
RESHOP_STATIC_ASSERT(sizeof(mdl_backendnames)/sizeof(char*) == (RHP_BACKEND_AMPL+1),
      "Inconsistency in model backends")

#define DEFSTR(id, str) char id[sizeof(str)];

typedef struct {
   union {
      struct {
      DEFINE_STR()
   };
   char dummystr[1];
   };
} ProbTypesNames;
#undef DEFSTR

#define DEFSTR(id, str) .id = str,

static const ProbTypesNames probtypesnames = {
DEFINE_STR()
};
#undef DEFSTR

#define DEFSTR(id, str) offsetof(ProbTypesNames, id),
static const unsigned probtypesnames_offsets[] = {
DEFINE_STR()
};

const unsigned probtypeslen = sizeof(probtypesnames_offsets)/sizeof(unsigned);

RESHOP_STATIC_ASSERT(sizeof(probtypesnames_offsets)/sizeof(unsigned) == (MdlProbType_last+1),
      "Inconsistency in problem types")
const char* probtype_name(ProbType type)
{
   if (type >= probtypeslen) { return "unknown problem type"; }   

   return probtypesnames.dummystr + probtypesnames_offsets[type];
}

unsigned backend_idx(const char *backendname)
{
   for (unsigned i = 0; i <= backendnamelen; ++i) {
      if (!strcasecmp(backendname, backend_name(i))) {
         return i;
      }
   }

   return UINT_MAX;
}

bool probtype_hasmetadata(const ProbType type)
{
   switch(type) {
   case MdlProbType_emp:
   case MdlProbType_mcp:
   case MdlProbType_mpec:
   case MdlProbType_vi:
     return true;
   default:
     return false;
   }
}

const char* backend_name(unsigned backendtype)
{
   if (backendtype < backendnamelen) {
      return mdl_backendnames[backendtype];
   } 

   return "INVALID";
  
}

bool probtype_isopt(ProbType type)
{
  assert(type != MdlProbType_none);

  switch (type) {
  case MdlProbType_lp:
  case MdlProbType_nlp:
  case MdlProbType_dnlp:
  case MdlProbType_mip:
  case MdlProbType_minlp:
  case MdlProbType_miqcp:
  case MdlProbType_mpec:
  case MdlProbType_qcp:
    return true;
  default:
    return false;
  }
}

bool probtype_isvi(ProbType type)
{
  assert(type != MdlProbType_none);

  switch (type) {
  case MdlProbType_mcp:
  case MdlProbType_vi:
    return true;
  default:
    return false;
  }
}
