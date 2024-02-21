#include "container.h"
#include "macros.h"
#include "mdl.h"
#include "ctrdat_rhp.h"
#include "rmdl_data.h"
#include "rmdl_options.h"
#include "printout.h"

#define SET_OPT_BOOL(S, STR, DEFAULT_VAL) S.name = STR; S.type = RMDL_OPTION_BOOL; S.p.b = DEFAULT_VAL;
#define SET_OPT_DBL(S, STR, DEFAULT_VAL) S.name = STR; S.type = RMDL_OPTION_DBL; S.p.d = DEFAULT_VAL;
#define SET_OPT_INT(S, STR, DEFAULT_VAL) S.name = STR; S.type = RMDL_OPTION_INT; S.p.i = DEFAULT_VAL;
#define SET_OPT_STR(S, STR, DEFAULT_VAL) S.name = STR; S.type = RMDL_OPTION_STR; S.p.s = DEFAULT_VAL;

static NONNULL void rmdl_options_getvalue(struct rmdl_option *opt, void *val) 
{
   switch (opt->type) {
   case RMDL_OPTION_BOOL:
      (*(bool *)val) = opt->p.b;
      break;
   case RMDL_OPTION_DBL:
      (*(double *)val) = opt->p.d;
      break;
   case RMDL_OPTION_INT:
      (*(int *)val) = opt->p.i;
      break;
   case RMDL_OPTION_STR:
      (*(char **)val) = opt->p.s;
      break;
   case RMDL_OPTION_UNSET:
      error("%s :: option name %s is unset!\n", __func__, opt->name);
   }

}

static NONNULL void rmdl_options_setvalue(struct rmdl_option *opt, union opt_t val)
{
   switch (opt->type) {
   case RMDL_OPTION_BOOL:
      opt->p.b = val.b;
      break;
   case RMDL_OPTION_DBL:
      opt->p.d = val.d;
      break;
   case RMDL_OPTION_INT:
      opt->p.i = val.i;
      break;
   case RMDL_OPTION_STR:
      opt->p.s = val.s;
      break;
   case RMDL_OPTION_UNSET:
      error("%s :: option name %s is unset!\n", __func__, opt->name);
   }

}

struct rmdl_option *rmdl_set_options(void)
{
   struct rmdl_option *opt;
   MALLOC_NULL(opt, struct rmdl_option, 6);

   SET_OPT_DBL(opt[0], "atol", 0.);
   SET_OPT_INT(opt[1], "iterlimit", -1);
   SET_OPT_BOOL(opt[2], "keep_files", false);
   SET_OPT_INT(opt[3], "solver_option_file_number", 1);
   SET_OPT_DBL(opt[4], "rtol", 1e-8);
   opt[5].name = NULL;

   return opt;
}

int rmdl_getoption(const Model *mdl, const char *opt, void *val)
{
   RhpModelData *mdldat = mdl->data;
   assert(val && mdldat->options);

   size_t i = 0;
   while(mdldat->options[i].name) {
      if (!strcmp(opt, mdldat->options[i].name)) {
         rmdl_options_getvalue(&mdldat->options[i], val);
         return OK;
      }
      i++;
    }

   error("%s :: no option named ``%s'' found\nThe available ones"
                      "are:", __func__, opt);
   i = 0;
   while(mdldat->options[i].name) {
     error(" %s", mdldat->options[i].name);
     i++;
   }

   errormsg("\n");

   return Error_OptionNotFound;
}

int rmdl_setoption(const Model *mdl, const char *opt, union opt_t val)
{
   RhpModelData *mdldat = mdl->data;
   size_t i = 0;
   while(mdldat->options[i].name) {
      if (!strcmp(opt, mdldat->options[i].name)) {
         rmdl_options_setvalue(&mdldat->options[i], val);
         return OK;
      }
      i++;
    }

   error("%s :: no option named ``%s'' found\nThe available ones "
                      "are:", __func__, opt);
   i = 0;
   while(mdldat->options[i].name) {
     error(" %s", mdldat->options[i].name);
     i++;
   }

   errormsg("\n");

   return Error_OptionNotFound;
}


