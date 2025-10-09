#ifndef OVF_FUNCTIONS_H
#define OVF_FUNCTIONS_H

/* NOTE: keep this list sorted! */
#define OVF_FUNCTIONS \
OVF_FUNCTION_OP(cvarlo, "cvar for the lower tail") \
OVF_FUNCTION_OP(cvarup, "cvar for the upper tail") \
OVF_FUNCTION_OP(ecvarlo, "convex combination of expectation and cvar for the lower tail") \
OVF_FUNCTION_OP(ecvarup, "convex combination of expectation and cvar for the upper tail") \
OVF_FUNCTION_OP(elastic_net, "elastic net: convex combination of l1 and l2 norms") \
OVF_FUNCTION_OP(expectation, "expectation (or mean)") \
OVF_FUNCTION_OP(hinge, "Hinge loss") \
OVF_FUNCTION_OP(huber, "Huber loss") \
OVF_FUNCTION_OP(huber_scaled, "Scaled Huber loss") \
OVF_FUNCTION_OP(hubnik, "Hubnik: Huber and vapnik loss") \
OVF_FUNCTION_OP(hubnik_scaled, "Scaled Hubnik") \
OVF_FUNCTION_OP(l1, "l1 norm") \
OVF_FUNCTION_OP(l2, "l2 norm") \
OVF_FUNCTION_OP(smax, "Maximum over finite set/collection") \
OVF_FUNCTION_OP(smin, "Minimum over finite set/collection") \
OVF_FUNCTION_OP(soft_hinge, "Soft Hinge") \
OVF_FUNCTION_OP(soft_hinge_scaled, "Scaled soft Hinge") \
OVF_FUNCTION_OP(sum_pos_part, "Sum of positive part: SUM max{ (.), 0 }") \
OVF_FUNCTION_OP(vapnik, "Vapnik loss") \


#include "ovf_huber.h"
#include "ovf_l1.h"
#include "ovf_l2.h"
#include "ovf_sum_pos_part.h"
#include "ovf_hinge.h"
#include "ovf_vapnik.h"
#include "ovf_soft_hinge.h"
#include "ovf_hubnik.h"
#include "ovf_elastic_net.h"

#include "ovf_huber_scaled.h"
#include "ovf_soft_hinge_scaled.h"
#include "ovf_hubnik_scaled.h"

#include "ovf_cvarup.h"
#include "ovf_ecvarup.h"
#include "ovf_ecvarlo.h"
#include "ovf_cvarlo.h"
#include "ovf_expectation.h"

#include "ovf_smax_smin.h"

#endif
