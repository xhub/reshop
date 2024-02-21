#define DECLARE_OVF \
DECLARE_OVF_FUNCTION(huber) \
DECLARE_OVF_FUNCTION(l1) \
DECLARE_OVF_FUNCTION(l2) \
DECLARE_OVF_FUNCTION(sum_pos_part) \
DECLARE_OVF_FUNCTION(hinge) \
DECLARE_OVF_FUNCTION(vapnik) \
DECLARE_OVF_FUNCTION(soft_hinge) \
DECLARE_OVF_FUNCTION(hubnik) \
DECLARE_OVF_FUNCTION(elastic_net) \
DECLARE_OVF_FUNCTION(huber_scaled) \
DECLARE_OVF_FUNCTION(soft_hinge_scaled) \
DECLARE_OVF_FUNCTION(hubnik_scaled) \
DECLARE_OVF_FUNCTION(cvarup) \
DECLARE_OVF_FUNCTION(ecvarup) \
DECLARE_OVF_FUNCTION(ecvarlo) \
DECLARE_OVF_FUNCTION(cvarlo) \
DECLARE_OVF_FUNCTION(expectation) \


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
