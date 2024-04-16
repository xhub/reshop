#ifndef OVF_OPTIONS_H
#define OVF_OPTIONS_H

#include <stdbool.h>

#include "compat.h"
#include "option.h"
#include "tlsdef.h"

struct option_list;

enum ovf_options_enum {
   Options_Ovf_Reformulation = 0,
   Options_Ovf_Init_New_Variables,
   Options_Ovf_Last = Options_Ovf_Init_New_Variables,
};

enum ovf_scheme {
   OVF_Scheme_Unset = 0,
   OVF_Equilibrium,
   OVF_Fenchel,
   OVF_Conjugate,
   OVF_Scheme_Last = OVF_Conjugate,
};

extern tlsvar struct option ovf_options[];
extern tlsvar struct option_set ovf_optset;

const char* ovf_getreformulationstr(unsigned i);

#define O_Ovf_Reformulation \
   ovf_options[Options_Ovf_Reformulation].value.i
#define O_Ovf_Init_New_Variables \
   ovf_options[Options_Ovf_Init_New_Variables].value.b

bool optovf_getreformulationmethod(const char *buf, unsigned *value);
int optovf_setreformulation(struct option *optovf_reformulation, const char *optval);
void optovf_getreformulationdata(const char *const (**strs)[2], unsigned *len) NONNULL;

int option_addovf(struct option_list *list) NONNULL;

void option_freeovf(void);

int ovf_syncenv(void);

#endif
