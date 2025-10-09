#ifndef RHP_OPTIONS_H
#define RHP_OPTIONS_H

#include "compat.h"
#include "option.h"
#include "rhp_fwd.h"
#include "tlsdef.h"

enum rhp_options_enum {
   Options_Display_EmpDag,
   Options_Display_Equations,
   Options_Display_OvfDag,
   Options_Display_Timings,
   Options_Dump_Scalar_Models,
   Options_EMPInfoFile,
   Options_Expensive_Checks,
   Options_GUI,
   Options_Output,
   Options_Output_Subsolver_Log,
   Options_Pathlib_Name,
   Options_Png_Viewer,
   Options_Save_EmpDag,
   Options_Save_OvfDag,
   Options_SolveSingleOptAs,
   Options_Subsolveropt,
   Options_Time_Limit,
   Options_Last = Options_Time_Limit,
};

extern tlsvar struct option rhp_options[];

/* TODO: cleanup by eliminating these defines */
#define O_Output \
   rhp_options[Options_Output].value.i
#define O_Output_Subsolver_Log \
   rhp_options[Options_Output_Subsolver_Log].value.b
#define O_Subsolveropt \
   rhp_options[Options_Subsolveropt].value.i

int option_addcommon(struct option_list *list) NONNULL;

bool optvalb(const Model *mdl, enum rhp_options_enum opt) NONNULL;
int optvali(const Model *mdl, enum rhp_options_enum opt) NONNULL;
char* optvals(const Model *mdl, enum rhp_options_enum opt) MALLOC_ATTR(free, 1);

#endif /* RHP_OPTIONS_H */
