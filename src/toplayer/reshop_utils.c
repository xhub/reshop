#include "reshop.h"
#include "rhpidx.h"
#include "toplayer_utils.h"

/**
 * @brief Return the largest valid index or size
 *
 * @return maximum index or size value. Anything larger indicates an error.
 */
size_t rhp_getidxmax(void)
{
   return IdxMaxValid;
}

const char* rhp_sensestr(unsigned sense)
{
   return sense2str(sense);
}
