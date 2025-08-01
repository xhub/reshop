
#include "toplayer_utils.h"
#include "mdl_data.h"

const char* sense2str(unsigned sense)
{
   switch(sense) {
   case RhpMin:
      return "min";
   case RhpMax:
      return "max";
   case RhpFeasibility:
      return "feasibility";
   case RhpDualSense:
      return "dual sense";
   case RhpNoSense:
      return "no sense";
   default:
      return "invalid sense";
   }
}

