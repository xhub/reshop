#ifndef EQUVAR_METADATA_H
#define EQUVAR_METADATA_H

#include <stdbool.h>

#include "compat.h"
#include "empdag_data.h"
#include "rhp_fwd.h"

/** @brief Equation role */
typedef enum {
   EquUndefined                 ,  /**< undefined                             */
   EquObjective                 ,  /**< an objective equation                 */
   EquConstraint                ,  /**< a constraint                          */
   EquViFunction                ,  /**< VI function                           */
   EquIsMap                        /**< The equation defines a mapping        */
} EquRole;

/** @brief Equation properties */
typedef enum equ_ppty {
   EquPptyNone           = 0,  /**< undefined                             */
   EquPptyHasDualVar     = 1,  /**< a special dual variable is assigned   */
   EquPptyIsImplicit     = 2,  /**< defining equ for implicit var         */
   EquPptyIsSharedVi     = 4,  /**< a shared constraint of VI type        */
   EquPptyIsSharedGnep   = 8,  /**< a shared constraint of GNEP type      */
   EquPptyIsDeleted      = 64, /**< equation has been deleted             */
} EquPpty;

/** @brief Variable role in an optimization problem */
__extension__ typedef enum ENUM_U8 {
   VarUndefined          ,  /**< undefined                             */
   VarObjective          ,  /**< an objective variable                 */
   VarPrimal             ,  /**< a primal variable                     */
   VarDual               ,  /**< a dual variable                       */
   VarDefiningMap,          /**< variable defining a mapping         */
} VarRole;

/* Bit representation
 *  []
 *   001  1    objmin
 *   010  2    objmax
 *   011  3    dualvar
 *   100  4    variable explicitly defined
 *   101       objmin + explicitly defined
 *   110       objmax + explicitly defined
 *   111  7    variable implicitely defined
 *  1000  8    matched variable
 *  1001  9    variable matched to 0 function
 *
 *  1111  0xf  Mask for basic type
 *
 * 10000  16   bitmask for control relation
 */

/** @brief Variable properties */
__extension__ typedef enum ENUM_U8 {
   VarPptyNone              = 0,  /**< undefined                             */
   VarIsObjMin              = 1,  /**< minimize objective variable           */
   VarIsObjMax              = 2,  /**< maximize objective variable           */
   VarIsDualVar             = 3,  /**< dual variable w.r.t. a constraint     */
   VarIsExplicitlyDefined   = 4,  /**< explicitly defined variable           */
   VarIsImplicitlyDefined   = 7,  /**< implicitly defined variable           */
   VarPerpToViFunction      = 8,  /**< matched variable                      */
   VarPerpToZeroFunctionVi  = 9,  /**< matched variable (to a zero function) */
   VarIsSolutionVar         = 16, /**< Is part of a control relation         */
   VarIsShared              = 32, /**< variable is assigned to multiple nodes*/
   VarIsDeleted             = 128, /**< Deleted variable                     */
} VarPptyType;

typedef enum {
   VarPptyBasicMask         = 0xf, /**< Mask for basic type                  */ 
   VarPptyDefinedMask       = 0x4,
   VarPptyExtendedMask      = 0xf0
} VarPptyMasks;

/** @brief Metadata for equations */
typedef struct equ_meta {
   EquRole role;                  /**< Role of the equation in the problem   */
   EquPpty ppty;                  /**< properties                            */
   rhp_idx dual;                  /**< dual equ or var index                 */
   union {
      mpid_t mp_id;             /**< mathematical program owning this equ  */
   };
} EquMeta;

/** @brief Metadata for variable */
typedef struct var_meta {
   VarRole type;                  /**< metadata type                         */
   VarPptyType ppty;              /**< metadata subtype                      */
   rhp_idx dual;                  /**< dual equ or var index                 */
   mpid_t mp_id;                  /**< mathematical program owning this var  */
} VarMeta;

const char* equrole_name(EquRole role);
const char* varrole_name(VarRole type);
void equmeta_rolemismatchmsg(const Container *ctr, rhp_idx ei, EquRole actual,
                             EquRole expected, const char *fn) NONNULL;

void equmeta_dealloc(struct equ_meta **emd);
struct equ_meta *equmeta_alloc(void) MALLOC_ATTR(equmeta_dealloc,1);
void equmeta_init(struct equ_meta *emd) NONNULL;

void varmeta_dealloc(struct var_meta **vmd);
struct var_meta *varmeta_alloc(void) MALLOC_ATTR(varmeta_dealloc,1);
void varmeta_init(struct var_meta *vmd) NONNULL;

void varmeta_print(const VarMeta * restrict vmeta, rhp_idx vi, const Model *mdl,
                   unsigned mode, unsigned offset) NONNULL;

static inline unsigned vmd_basictype(VarPptyType ppty) {
   return ppty & VarPptyBasicMask;
}

static inline bool equmeta_hasdual(struct equ_meta *emd)
{
   return emd->role == EquViFunction || emd->ppty == EquPptyHasDualVar;
}

static inline bool var_is_defined_objvar(VarMeta *vmeta)
{
   unsigned basic_type = vmd_basictype(vmeta->ppty);

   return (basic_type == (VarIsExplicitlyDefined | VarIsObjMin) ||
           basic_type == (VarIsExplicitlyDefined | VarIsObjMax));
}


#endif /* EQUVAR_METADATA_H */
