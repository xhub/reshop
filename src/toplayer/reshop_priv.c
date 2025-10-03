#include "reshop_config.h"

#include "macros.h"
#include "reshop_priv.h"
#include "tlsdef.h"

static tlsvar char backend_info[64] = "unknown";

void set_userinfo(const char *userinfo)
{
   COPYSTR(backend_info, userinfo, 64);
}

const char *get_userinfo(void)
{
   return backend_info;
}
