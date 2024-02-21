
#include "toplayer_utils.h"
#include "reshop.h"

const char* sense2str(unsigned sense)
{
   switch(sense) {
   case RHP_MIN:
      return "min";
   case RHP_MAX:
      return "max";
   case RHP_FEAS:
      return "feasibility";
   default:
      return "invalid sense";
   }
}

