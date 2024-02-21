#ifndef RMDL_OPTIONS_H
#define RMDL_OPTIONS_H

#include <stddef.h>
#include <stdbool.h>

#include "rhp_fwd.h"
#include "option.h"

/**
 * @brief Type of option
 */
enum rmdl_option_type {
   RMDL_OPTION_UNSET,
   RMDL_OPTION_DBL,
   RMDL_OPTION_INT,
   RMDL_OPTION_STR,
   RMDL_OPTION_BOOL,
};

/**
 * @brief Payload for options
 */
union opt_t {
      double d;     /**< double */
      ptrdiff_t i;  /**< integer*/
      char *s;      /**< string */
      bool b;       /**< boolean */
};

/**
 * @brief Option for RMDL
 */
struct rmdl_option {
   const char *name;           /**< name of the option */
   enum rmdl_option_type type;  /**< type of option     */
   union opt_t p;              /**< payload            */
};

extern struct rmdl_option rmdl_options[];

int rmdl_getoption(const Model *mdl, const char *opt, void *val);
int rmdl_setoption(const Model *mdl, const char *opt, union opt_t val);
struct rmdl_option *rmdl_set_options(void);
#endif
