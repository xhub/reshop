#include "reshop.h"
#include "rhpidx.h"
#include "status.h"
#include "toplayer_utils.h"

/**
 * @brief Return the largest valid index or size
 *
 * @ingroup publicAPI
 *
 * @return maximum index or size value. Anything larger indicates an error.
 */
size_t rhp_getidxmax(void)
{
   return IdxMaxValid;
}

/**
 * @brief Get the textual representation of a 'sense' value
 *
 * @ingroup publicAPI
 *
 * @param sense the sense value
 *
 * @return      the string representation of the sense value
 */
const char* rhp_sensestr(unsigned sense)
{
   return sense2str(sense);
}

/**
 * @brief The textual representation of a status value
 *
 * @ingroup publicAPI
 *
 * @param status the status value
 *
 * @return       the string representation of the status value
 */
const char *rhp_status_descr(int status)
{
   return status_descr(status);
}

