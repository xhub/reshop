#ifndef SD_TOOL_H
#define SD_TOOL_H

#include <stdbool.h>
#include "rhp_fwd.h"

/** @file sd_tool.h
 *
 *  @brief Symbolic Differentiation tool
 */

enum sd_tool_type {
   SDT_ANY,             /**< Pick any available SD tool  */
   SDT_CASADI,          /**< Use CaSaDi for SD */
   SDT_CPPAD,           /**< Use CppAD for SD  */
   SDT_OPCODE,          /**< Use an opcode-based implementation */
   __ADT_TYPE_LEN
};

struct sd_tool {
   void *data;
   Lequ *lequ;
   const struct sd_ops *ops;
};

struct sd_ops {
   int (*allocdata)(struct sd_tool *ad_tool, Container *ctr, rhp_idx ei);
   int (*allocdata_fromequ)(struct sd_tool *ad_tool, Container *ctr, const Equ *e);
   void (*deallocdata)(struct sd_tool *ad_tool);
   int (*assemble)(struct sd_tool *ad_tool, Container *ctr, EmpInfo *empinfo);
   int (*deriv)(struct sd_tool *adt, int vidx, Equ *e);
};

void sd_tool_free(struct sd_tool *adt);
struct sd_tool* sd_tool_alloc(enum sd_tool_type adt_type, Container *ctr, rhp_idx ei)
MALLOC_ATTR(sd_tool_free,1);
struct sd_tool* sd_tool_alloc_fromequ(enum sd_tool_type adt_type, Container *ctr, const Equ *e)
MALLOC_ATTR(sd_tool_free,1);

int sd_tool_assemble(struct sd_tool *adt, Container *ctr, EmpInfo *empinfo);
//int ad_tool_fooc(struct ad_tool *adt, Container *ctr_fooc);
int sd_tool_deriv(struct sd_tool *adt, int vidx, Equ *e);

static inline 
bool sd_tool_has_assemble(struct sd_tool *adt) { return adt->ops->assemble; }

#endif /*  SD_TOOL_H */
