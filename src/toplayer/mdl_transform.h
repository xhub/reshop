#ifndef MDL_TRANSFORM_H
#define MDL_TRANSFORM_H

/** @file mdl_transform.h
*
*   @brief model transformation
*/

#include "rhp_fwd.h"


int mdl_transform_tomcp(Model *mdl, Model **mdl_target) NONNULL;
int mdl_transform_emp_togamsmdltype(Model *mdl_src, Model **mdl_target) NONNULL;


#endif
