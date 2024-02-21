#include "compat.h"
#include "container.h"
#include "equvar_helpers.h"
#include "equvar_metadata.h"
#include "macros.h"
#include "ctrdat_rhp.h"
#include "ctrdat_rhp_priv.h"
#include "mdl.h"
#include "printout.h"

const char * const rmdl_solvernames[] = {"unset",
                                 "pathvi",
                                 "pathvi:mcp",
                                 "path",
                                 NULL
};

#define RHP_DEFINE_STRUCT
#define RHP_LOCAL_SCOPE 1
#define RHP_LIST_PREFIX _tofree_list
#define RHP_LIST_TYPE tofree_list
#define RHP_ELT_TYPE void *
#define RHP_ELT_INVALID NULL
#include "list_generic.inc"


int cdat_add2free(struct ctrdata_rhp *model, void *mem)
{
  if (!model->mem2free) {
    MALLOC_(model->mem2free, struct tofree_list, 1);
    _tofree_list_init(model->mem2free);
  }

  return _tofree_list_add(model->mem2free, mem);
}

int chk_ctrdat_space(struct ctrdata_rhp *ctrdat, unsigned nb, const char *fn)
{
   if (ctrdat->max_n < ctrdat->total_n + nb) {
      error("%s :: cannot add %d more variables to the current %zu"
               " ones: the size would exceed the maximum size %zu\n", fn, nb,
               ctrdat->total_n, ctrdat->max_n);
      return Error_SizeTooSmall;
   }

   return OK;
}

void rctrdat_mem2free(struct tofree_list *mem2free)
{
  for (unsigned i = 0; i < mem2free->len; ++i) {
    FREE(mem2free->list[i]);
  }
  _tofree_list_free(mem2free);
}

struct vnames_list* vnames_list_new(void)
{
  struct vnames_list *l;
  MALLOC_NULL(l, struct vnames_list, 1);

  l->active = false;
  l->len = l->max = 0;
  l->start = l->end = NULL;
  l->names = NULL;

  return l;
}

void vnames_list_free(struct vnames_list *l)
{
  FREE(l->start);
  FREE(l->end);

  for (size_t i = 0; i < l->len; ++i) {
    FREE(l->names[i]);
  }

  FREE(l->names);

  FREE(l);
}

int vnames_list_start(struct vnames_list *l, rhp_idx idx, const char* name)
{
  assert(!l->active);
  l->active = true;

  if (l->max <= l->len) {
    l->max = MAX(2*l->max, l->len + 10);
    REALLOC_(l->names, const char *, l->max);
    REALLOC_(l->start, rhp_idx, l->max);
    REALLOC_(l->end, rhp_idx, l->max);
  }

  l->names[l->len] = name;
  l->start[l->len] = idx;

  return OK;
}

void vnames_list_stop(struct vnames_list *l, rhp_idx idx)
{
  l->active = false;
  l->end[l->len++] = idx;
}

NONNULL
static void _vnames_astype(struct vnames* vnames, enum vecnames_type type)
{
  vnames->type = type;
  vnames->start = IdxNA;
  vnames->end = IdxNA;
  vnames->next = NULL;
}

struct vnames* vnames_new(enum vecnames_type type)
{
  struct vnames *vnames;
  MALLOC_NULL(vnames, struct vnames, 1);
  _vnames_astype(vnames, type);

  switch (type) {
  case VNAMES_REGULAR:
    AA_CHECK(vnames->list, vnames_list_new());
    break;
  case VNAMES_UNSET:
  case VNAMES_MULTIPLIERS:
  case VNAMES_LAGRANGIAN_GRADIENT:
  default:
    ;
  }

  return vnames;
}

/**
 * @brief free the data structure
 *
 * @param vnames  the vnames to free
 */
void vnames_free(struct vnames *vnames)
{
  while (vnames) {
    switch (vnames->type) {
    case VNAMES_REGULAR:
    {
      vnames_list_free(vnames->list);
      break;
    }
    case VNAMES_MULTIPLIERS:
      break;
    case VNAMES_LAGRANGIAN_GRADIENT:
      break;
    case VNAMES_UNSET:
      break;
    default:
      ;
    }

    struct vnames *old_vnames = vnames;
    vnames = vnames->next;
    FREE(old_vnames);
  }

}

void vnames_freefrommdl(struct vnames *vnames)
{
  if (!vnames) return;

  switch (vnames->type) {
  case VNAMES_REGULAR:
  {
    vnames_list_free(vnames->list);
    break;
  }
  case VNAMES_FOOC_LOOKUP:
      vnames_lookup_free(vnames->fooc_lookup);
      break;
  case VNAMES_MULTIPLIERS:
    break;
  case VNAMES_LAGRANGIAN_GRADIENT:
    break;
  case VNAMES_UNSET:
    break;
  default:
    ;
  }

  vnames_free(vnames->next);
}

struct vnames* vnames_getregular(struct vnames *vnames)
{

  while (true) {
    switch (vnames->type) {
    case VNAMES_REGULAR:
      if (!vnames->next) {
        return vnames;
      }
      break;
    case VNAMES_UNSET:
      _vnames_astype(vnames, VNAMES_REGULAR);
      AA_CHECK(vnames->list, vnames_list_new());
      return vnames;
    /* fallthrough */
    case VNAMES_MULTIPLIERS:
    case VNAMES_LAGRANGIAN_GRADIENT:
    default:
      ;
    }

    if (vnames->next) {
      vnames = vnames->next;
    } else {
      vnames->next = vnames_new(VNAMES_REGULAR);
      return vnames->next;
    }
  }

  return NULL;
}

/* @brief names for vector-type variables */
typedef struct vnames_fooclookup {
   unsigned len;
   const rhp_idx *ei_fooc2vi_fooc;
   const Model *mdl_fooc;
   VecNamesLookupTypes types[];
} VecNamesFoocLookup;

VecNamesFoocLookup* vnames_lookup_new(unsigned len, const Model *mdl_fooc,
                                      const rhp_idx *ei_fooc2vi_fooc)
{
   VecNamesFoocLookup *dat;
   MALLOCBYTES_NULL(dat, VecNamesFoocLookup, sizeof(VecNames)+sizeof(VecNamesLookupTypes)*len);
   memset(dat->types, 0, sizeof(VecNamesLookupTypes)*len);
   dat->ei_fooc2vi_fooc = ei_fooc2vi_fooc;
   dat->mdl_fooc = mdl_fooc;

   return dat;
}

void vnames_lookup_free(VecNamesFoocLookup* dat)
{
   /* dat->rev_rosetta_vars is owned by another object */
   FREE(dat);
}

VecNamesLookupTypes *vnames_lookup_gettypes(VecNamesFoocLookup *dat)
{
   return dat->types;
}

int vnames_lookup_copyname(VecNamesFoocLookup *dat, unsigned idx, char *name,
                           unsigned len)
{
   assert(dat->mdl_fooc && dat->mdl_fooc->mdl_up);
   const  Model *mdl_fooc = dat->mdl_fooc;
   const Container *ctr_up = &dat->mdl_fooc->mdl_up->ctr;
   rhp_idx vi_fooc = dat->ei_fooc2vi_fooc ? dat->ei_fooc2vi_fooc[idx] : idx;
   assert(vi_fooc < ctr_nvars_total(&mdl_fooc->ctr));
   RhpContainerData *cdat_fooc = (RhpContainerData *)mdl_fooc->ctr.data;
   rhp_idx vi_up = cdat_var_inherited(cdat_fooc, vi_fooc);

   if (!valid_vi_(vi_up, ctr_nvars_total(ctr_up), __func__)) {
      STRNCPY(name, "ERROR: invalid source variable index", len);
      return Error_InvalidValue;
   }

   VecNamesLookupTypes type = dat->types[idx];
   switch (type) {
//   case VN_Unset:
//      error("[container] ERROR: in fooc equation name lookup, no type set for "
//            "index %u\n", idx);
//      return Error_RuntimeError;
   case VNL_VIfunc: {
      rhp_idx ei_up = ctr_up->varmeta[vi_up].dual; assert(valid_ei(ei_up));
      return ctr_copyequname(ctr_up, ei_up, name, len);
   }
   case VNL_VIzerofunc:
      snprintf(name, len, "VI zero function for %s", ctr_printvarname(ctr_up, vi_up));
      return OK;
   case VNL_Lagrangian: {
      mpid_t mp_id = ctr_up->varmeta ? ctr_up->varmeta[vi_up].mp_id : MpId_NA;
      if (mpid_regularmp(mp_id)) {
         const EmpDag *empdag = &mdl_fooc->mdl_up->empinfo.empdag;
         snprintf(name, len, "dLd(MP(%s),%s)", empdag_getmpname(empdag, mp_id),
                  ctr_printvarname(ctr_up, vi_up));
         return OK;
      }
      snprintf(name, len, "dLd(%s)", ctr_printvarname(ctr_up, vi_up));
      return OK;
   }
   default:
      error("[container] ERROR: unknown type %u in fooc equation name lookup", type);
      return Error_RuntimeError;
   }
}

/*  function for internal consistency */
bool rctr_eq_not_deleted(RhpContainerData *cdat, rhp_idx ei)
{
   return !cdat->deleted_equs || !cdat->deleted_equs[ei];
}

// everybody can dream ...
//RESHOP_STATIC_ASSERT(rmdl_solvernames[__RMDL_SOLVER_SIZE] == NULL, "Sync error rmdl_solvernames");
