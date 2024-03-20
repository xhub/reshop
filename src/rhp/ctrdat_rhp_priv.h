#ifndef RHP_MODEL_REPR_PRIV_H
#define RHP_MODEL_REPR_PRIV_H

#include <stdbool.h>

#include "compat.h"
#include "ctrdat_rhp_data.h"
#include "rhp_fwd.h"

extern const char * const rmdl_solvernames[];

struct tofree_list;
struct ctrdata_rhp;

typedef enum {
   VNL_Lagrangian = 0,
//   VN_Unset = 0,
   VNL_VIfunc,
   VNL_VIzerofunc,
} VecNamesLookupTypes;

/* @brief names for vector-type variables */
struct vnames_list {
  bool active;
  unsigned len;
  unsigned max;
  rhp_idx *start;
  rhp_idx *end;
  const char **names;  /**< names */
};

struct vnames;
typedef struct vnames_fooclookup VecNamesFoocLookup;

void vnames_free(struct vnames *vnames);
struct vnames* vnames_new(enum vecnames_type type) MALLOC_ATTR(vnames_free,1);
void vnames_freefrommdl(struct vnames *vnames);
struct vnames* vnames_getregular(struct vnames *vnames) NONNULL;

void vnames_list_free(struct vnames_list *l);
struct vnames_list* vnames_list_new(void) MALLOC_ATTR(vnames_list_free,1);
int vnames_list_start(struct vnames_list *l, rhp_idx idx, char* name) NONNULL;
void vnames_list_stop(struct vnames_list *l, rhp_idx idx) NONNULL;

void vnames_lookup_free(VecNamesFoocLookup* dat);
NONNULL_AT(2) MALLOC_ATTR(vnames_lookup_free,1)
VecNamesFoocLookup* vnames_lookup_new(unsigned len, const Model *mdl_fooc,
                                      const rhp_idx *ei_fooc2vi_fooc);
VecNamesLookupTypes *vnames_lookup_gettypes(VecNamesFoocLookup *dat) NONNULL;
int vnames_lookup_copyname(VecNamesFoocLookup *dat, unsigned idx, char *name, unsigned len);

int chk_ctrdat_space(struct ctrdata_rhp *ctrdat, unsigned nb, const char *fn) NONNULL;
void rctrdat_mem2free(struct tofree_list *mem2free) NONNULL;

bool rctr_eq_not_deleted(struct ctrdata_rhp *cdat, rhp_idx ei) NONNULL;

#endif /*RHP_MODEL_REPR_PRIV_H*/
