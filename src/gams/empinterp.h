#ifndef EMPINTERP_H
#define EMPINTERP_H

/**
 * @file empinterp.h
 *
 * @brief EMP interpreter common data structures
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "compat.h"
#include "empdag_common.h"
#include "empdag_data.h"
#include "empinterp_data.h"
#include "empinterp_fwd.h"
#include "empparser_data.h"
#include "gdx_reader.h"
#include "mdl_data.h"
#include "ovfinfo.h"
#include "reshop_data.h"

struct ovf_param_def;
struct ovf_param_def_list;

enum parser_health {
   PARSER_OK      = 0,
   PARSER_ERROR   = 1,
   PARSER_PANIC   = 2,
};

// TODO: DELETE?
enum _status {
   _STAT_NONE = 0,
   _STAT_HAS_OBJVAR   = 1,
   _STAT_PREV_VAR_AMBIGUOUS = 2,
   _STAT_VAR_ENDS_OBJEXPR = 4,
};


// TODO: DELETE?
enum _dag_idents {
   IDENT_VAR   = 1,
   IDENT_EQU   = 2,
   IDENT_NODE  = 4,
   IDENT_VALFN = 8,
   IDENT_CST   = 16,
   IDENT_SUM   = 32,
   IDENT_TIMES   = 64,
};

typedef struct gms_indices_data {
   uint8_t nargs;
   uint8_t num_loopiterators;                /**< Number of loop iterator */
   uint8_t num_sets;
   uint8_t num_localsets;
   IdentData idents[GMS_MAX_INDEX_DIM];
} GmsIndicesData;

typedef struct {
   IdentType type;
   unsigned len;
   Lexeme lexeme;
   const Lequ *data;
} VmVector;


/** State of the interpreter */
typedef struct {
   bool has_bilevel;        /**< Bilevel keyword has been parsed              */
   bool has_dag_node;            /**< there is a DAG */
   bool has_dualequ;        /**< Dualequ keyword has been parsed              */
   bool has_equilibrium;    /**< Equilibrium keyword has been parsed */
   bool has_implicit_Nash;  /**< Has an implicit Nash struct (no EMPDAG or equilibrium) */
   bool has_single_mp;      /**< Single MP */
   bool bilevel_in_progress; /**< True if we are parsing a bilevel statement */
} InterpParsedKwds;

typedef struct NamedIntsArray {
   unsigned len;
   unsigned max;
   IntArray *list;
   const char **names;
} NamedIntsArray;

static inline void namedints_free_elt(IntArray arr)
{
   IntArray arr_ = arr;
   rhp_int_empty(&arr_);
}

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX namedints
#define RHP_LIST_TYPE NamedIntsArray
#define RHP_ELT_TYPE IntArray
#define RHP_ELT_FREE namedints_free_elt
#define RHP_ELT_INVALID ((IntArray) {.len = 0, .max = 0, .arr = NULL})
#include "namedlist_generic.inc"
#define valid_set(obj)  ((obj).arr != NULL)

typedef struct NamedDblArray {
   unsigned len;
   unsigned max;
   double *list;
   const char **names;
} NamedScalarArray;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX namedscalar
#define RHP_LIST_TYPE NamedDblArray
#define RHP_ELT_TYPE double
#define RHP_ELT_INVALID (NAN)
#include "namedlist_generic.inc"

typedef struct NamedVecArray {
   unsigned len;
   unsigned max;
   Lequ *list;
   const char **names;
} NamedVecArray;

#include "lequ.h" /* for lequ_dealloc */
#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX namedvec
#define RHP_LIST_TYPE NamedVecArray
#define RHP_ELT_TYPE Lequ
#define RHP_ELT_FREE lequ_empty
#define RHP_ELT_FREE_TAKE_ADDRESS
#define RHP_ELT_INVALID ((Lequ) {.len = 0, .max = 0, .coeffs = NULL, .vis = NULL})
#include "namedlist_generic.inc"
#define valid_vector(obj)  (((obj).coeffs != NULL) && ((obj).vis != NULL))

typedef struct NamedMultiSets {
   unsigned len;
   unsigned max;
   struct gdx_multiset *list;
   const char **names;
} NamedMultiSets;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX multisets
#define RHP_LIST_TYPE NamedMultiSets
#define RHP_ELT_TYPE struct gdx_multiset
#define RHP_ELT_INVALID ((struct gdx_multiset) {.dim = 0, .idx = 0, .gdxreader = NULL})
#include "namedlist_generic.inc"

typedef struct gdxalias {
   unsigned index;
   unsigned dim;
   GdxSymType gdxtype;
   IdentType  type;
   const char *name;
} GdxAlias;

typedef struct AliasArray {
   unsigned len;
   unsigned max;
   GdxAlias *list;
   const char **names;
} AliasArray;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX aliases
#define RHP_LIST_TYPE AliasArray
#define RHP_ELT_TYPE struct gdxalias
#define RHP_ELT_INVALID ((struct gdxalias) {.name = NULL, .index = 0})
#include "namedlist_generic.inc"

typedef struct gdxparam {
   int idx;
   int dim;
} GdxParam;

typedef struct ParamArray {
   unsigned len;
   unsigned max;
   GdxParam *list;
   const char **names;
} NamedParamArray;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX params
#define RHP_LIST_TYPE ParamArray
#define RHP_ELT_TYPE struct gdxparam
#define RHP_ELT_INVALID ((struct gdxparam) {.idx = -1, .dim = -1})
#include "namedlist_generic.inc"

typedef struct {
   AliasArray aliases;
   NamedIntsArray sets;
   NamedMultiSets multisets;
   NamedVecArray vectors;
   NamedScalarArray scalars;
   NamedParamArray params;
   NamedIntsArray localsets;
   NamedVecArray localvectors;
} CompilerGlobals;

typedef struct gdx_readers {
   unsigned len;
   unsigned max;
   GdxReader *list;
} GdxReaders;


typedef struct gms_sym_iterator {
   bool active;
   bool compact;
   bool loop_needed;
   IdentData ident;
   GmsIndicesData indices;
   int uels[GMS_MAX_INDEX_DIM];
} GmsSymIterator;

typedef struct edge_weight {
   double cst;
   rhp_idx ei;         /**< mapping where the term appears */
   unsigned nvars;
   rhp_idx vi[] __counted_by(nvars);
} EdgeWeight;

typedef struct linklabels_list {
   unsigned len;
   unsigned max;
   struct linklabels **list;
} LinkLabelsList;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX linklabels_list
#define RHP_LIST_TYPE linklabels_list
#define RHP_ELT_TYPE struct linklabels*
#define RHP_ELT_INVALID NULL
#include "list_generic.inc"
#define valid_problem_edge(obj)  ((obj) && (obj).dagid < UINT_MAX)

/* ---------------------------------------------------------------------
 * The DagLabel(s) are used to either store the  
 * or store the children to a CCFlib or Nash node
 *
 * Note: since we store the pointer to the struct in the VmData, we
 * can't store all runtime allocated 
 * --------------------------------------------------------------------- */

typedef struct linklabels {
   uint8_t dim;
   uint8_t num_var;
   LinkType linktype;
   uint16_t label_len;     /**< label length */
   unsigned num_children;
   unsigned max_children;  /**< Max number of children  */
   daguid_t daguid_parent; /**< daguid ot the parent */
   const char *label;      /**< Label                 */
   int *uels_var;          /**< uel_var[num_var] per child */
   rhp_idx *vi;            /**< Optional variable index    */
   double *coeff;          /**< Optional coefficient       */
   void **extras;           /**< extra objects              */
   int data[];              /**< Layout: uels[dim] + pos[num_vars]*/
} LinkLabels;

typedef struct linklabel {
   uint8_t dim;
   LinkType linktype;
   uint16_t label_len;
   daguid_t daguid_parent;
   const char *label;
   double coeff;
   rhp_idx vi;
   int uels[] __counted_by(dim);
} LinkLabel;

/** The DualLabels data structure links EmpDag record with mpids.
 * Given thelabel(...), we link it to already created mpids */
typedef struct dual_labels {
   uint8_t dim;
   uint8_t num_var;
   uint16_t label_len;       /**< label length */
   const char *label;        /**< Basename of the parent */
   int *uels_var;            /**< uel_var[num_var] per child */
   DualOperatorData *opdat;  /**< Data for the operation */
   MpIdArray mpid_uals;      /**< Arrays of mp duals. Contains the length data */
   int data[];               /**< Layout: uels[dim] + pos[num_vars]*/
} DualsLabel;

typedef struct dual_label {
   uint8_t dim;
   uint16_t label_len;
   mpid_t mpid_dual;
   const char *label;
   int uels[] __counted_by(dim);
} DualLabel;

typedef struct LinkLabels2Arcs {
   unsigned len;
   unsigned max;
   LinkLabels **arr;
} LinkLabels2Arcs;

typedef struct linklabel2arc {
   unsigned len;
   unsigned max;
   LinkLabel **arr;
} LinkLabel2Arc;

typedef struct DualsLabelArray {
   unsigned len;
   unsigned max;
   DualsLabel **arr;
} DualsLabelArray;

typedef struct DualLabelArray {
   unsigned len;
   unsigned max;
   DualLabel **arr;
} DualLabelArray;

#include "empinterp_linkbuilder.h" /* For linklabels_free */
#define RHP_LOCAL_SCOPE
#define RHP_ARRAY_PREFIX linklabels2arcs
#define RHP_ARRAY_TYPE LinkLabels2Arcs
#define RHP_ELT_FREE linklabels_free
#define RHP_ELT_TYPE LinkLabels*
#define RHP_ELT_INVALID NULL
#include "array_generic.inc"

#define RHP_LOCAL_SCOPE
#define RHP_ARRAY_PREFIX linklabel2arc
#define RHP_ARRAY_TYPE linklabel2arc
#define RHP_ELT_FREE FREE
#define RHP_ELT_TYPE LinkLabel*
#define RHP_ELT_INVALID NULL
#include "array_generic.inc"

#define RHP_LOCAL_SCOPE
#define RHP_ARRAY_PREFIX dual_label
#define RHP_ARRAY_TYPE DualLabelArray
#define RHP_ELT_FREE FREE
#define RHP_ELT_TYPE DualLabel*
#define RHP_ELT_INVALID NULL
#include "array_generic.inc"

#define RHP_LOCAL_SCOPE
#define RHP_ARRAY_PREFIX dualslabel_arr
#define RHP_ARRAY_TYPE DualsLabelArray
#define RHP_ELT_FREE dualslabel_free
#define RHP_ELT_TYPE DualsLabel*
#define RHP_ELT_INVALID NULL
#include "array_generic.inc"

/* ---------------------------------------------------------------------
 *  This is a registry of all labels that have been defined.
 *  If required, in the 2nd phase, the labels used in the EMP model are
 *  resolved to yield the edges of the DAG.
 * --------------------------------------------------------------------- */

#define DagRegisterEntry LinkLabel

typedef struct DagRegister {
   unsigned len;
   unsigned max;
   DagRegisterEntry **list;
} DagRegister;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX dagregister
#define RHP_LIST_TYPE DagRegister
#define RHP_ELT_TYPE DagRegisterEntry*
#define RHP_ELT_FREE FREE
#define RHP_ELT_INVALID NULL
#include "list_generic.inc"
#define valid_registry_entry(obj)  ((obj) && (obj).dagid < UINT_MAX)


typedef struct {
   TokenType type;
   unsigned linenr;
   unsigned len;
   const char *start;
   const char *linestart;
} KeywordLexemeInfo;

typedef struct {
   mpid_t mp_owns_remaining_vars;
   mpid_t mp_owns_remaining_equs;
} InterpFinalization;

typedef struct interpreter {
   enum parser_health health;
   bool peekisactive;
   bool err_shown;               /**< Error message already shown */
   unsigned linenr;
   size_t read;
   const char *linestart;
   const char *linestart_old;
   char *buf;
   char *tmpstr;
   const char *empinfo_fname;
   unsigned tmpstrlen;

   Model *mdl;
   void *gmdcpy;

   void *dct;
   void *gmd;
   bool gmd_fromgdx;
   bool gmd_own;

   /* Tokens */
   Token cur;
   Token peek;
   Token pre;

   /* Parser state info */
   InterpParsedKwds state;
   KeywordLexemeInfo last_kw_info;
   InterpFinalization finalize;

   /* Parser state info */
   const struct interp_ops *ops;

   struct empvm_compiler *compiler;

   /* EMPDAG register */
   DagRegisterEntry *regentry;
   LinkLabel *dag_root_label;
   DagRegister dagregister;

   LinkLabels2Arcs linklabels2arcs;
   LinkLabel2Arc linklabel2arc;
   DualsLabelArray dualslabel;
   DualLabelArray dual_label;

   /* Other dynamic data */
   GdxReaders gdx_readers;
   CompilerGlobals globals;

   GmsSymIterator gms_sym_iterator;        /**< Iterator for GAMS symbol     */
   GmsIndicesData gmsindices;              /**< Indices                      */
   /* Set filtering data structures */
   GmsIndicesData indices_membership_test;

   /* Immediate mode data*/
   daguid_t daguid_parent;                    /**< UID of the EMPDAG parent node */
   daguid_t daguid_grandparent;               /**< UID of the EMPDAG parent node */
   daguid_t daguid_child;                     /**< */
} Interpreter;


typedef enum {
   InterpreterOpsImm,
   InterpreterOpsCompiler,
   InterpreterOpsEmb,
} InterpreterOpsType;

typedef struct interp_ops {
   InterpreterOpsType type;
   int (*ccflib_new)(Interpreter* restrict interp, unsigned ccf_idx, MathPrgm **mp);
   int (*ccflib_finalize)(Interpreter* restrict interp, MathPrgm *mp);
   int (*ctr_markequasflipped)(Interpreter* restrict interp);
   int (*identaslabels)(Interpreter * interp, unsigned * p, LinkType edge_type);
   int (*gms_add_uel)(Interpreter* restrict interp, const Token *tok, unsigned i); 
   int (*gms_get_uelidx)(Interpreter *interp, const char *uelstr, int *uelidx);
   int (*gms_get_uelstr)(Interpreter *interp, int uelidx, unsigned uelstrlen, char *uelstr);
   int (*label_getidentstr)(Interpreter* restrict interp, const Token *tok, char **ident, unsigned *ident_len);
   int (*mp_addcons)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_addvars)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_addvipairs)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_addzerofunc)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_finalize)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_new)(Interpreter* restrict interp, RhpSense sense, MathPrgm **mp);
   int (*mp_setaschild)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_setobjvar)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_setprobtype)(Interpreter* restrict interp, MathPrgm *mp, unsigned type);
   int (*nash_addmp)(Interpreter* restrict interp, nashid_t nashid, MathPrgm *mp);
   int (*nash_finalize)(Interpreter* restrict interp, Nash *nash);
   int (*nash_new)(Interpreter* restrict interp, Nash **nash);
   int (*ovf_addbyname)(Interpreter* restrict interp, EmpInfo *empinfo, const char *name, void ** ovfdef_data);
//   int (*ovf_args_init)(Parser* restrict parser, void *ovfdef_data);
   int (*ovf_addarg)(Interpreter* restrict interp, void *ovfdef_data);
   int (*ovf_check)(Interpreter* restrict interp, void *ovfdef_data);
   int (*ovf_paramsdefstart)(Interpreter* restrict interp, void *ovfdef_data,
                             const struct ovf_param_def_list **paramsdef);
   int (*ovf_setparam)(Interpreter* restrict interp, void *ovfdef_data, unsigned i,
                       OvfArgType type, OvfParamPayload payload);
   int (*ovf_setrhovar)(Interpreter* restrict interp, void *ovfdef_data);
   unsigned (*ovf_param_getvecsize)(Interpreter* restrict interp, void *ovfdef_data,
                                    const struct ovf_param_def *pdef);
   const char *(*ovf_getname)(Interpreter* restrict interp, void *ovfdef_data);
   int (*read_elt_vector)(Interpreter *interp, const char *vectorname,
                          IdentData *ident, GmsIndicesData *gmsindices,
                          double *val);
   int (*read_gms_symbol)(Interpreter* restrict interp, unsigned *p); 
   int (*read_param)(Interpreter *interp, unsigned *p, IdentData *data,
                      unsigned *param_gidx);
   int (*resolve_tokasident)(Interpreter *interp, IdentData *ident);
} InterpreterOps;

extern const struct interp_ops interp_ops_imm;
extern const struct interp_ops interp_ops_compiler;

int gmssym_iterator_init(Interpreter* restrict interp) NONNULL;
int parser_filter_set(Interpreter* restrict interp, unsigned i, int val) NONNULL;

int empinterp_finalize(Interpreter *interp) NONNULL;

static inline unsigned emptok_getlinenr(const Token *tok)
{
   return tok->linenr;
}

static inline unsigned emptok_getstrlen(const Token *tok)
{
   return tok->len;
}

static inline const char * emptok_getstrstart(const Token *tok)
{
   return tok->start;
}

static inline enum emptok_type emptok_gettype(Token *tok)
{
   return tok->type;
}

static inline void emptok_settype(Token *tok, enum emptok_type type)
{
   tok->type = type;
}


UNUSED NONNULL static
enum emptok_type parser_getcurtoktype(const struct interpreter *interp) {
   return interp->cur.type;
}

UNUSED NONNULL static
enum emptok_type parser_getpeektoktype(const struct interpreter *interp) {
   return interp->peek.type;
}

UNUSED NONNULL static
enum emptok_type parser_getpretoktype(const struct interpreter *interp) {
   return interp->pre.type;
}

UNUSED NONNULL static
rhp_idx parser_getequvaridx(const struct interpreter *interp) {
   assert(interp->cur.type == TOK_GMS_VAR || interp->cur.type == TOK_GMS_EQU);
   return interp->cur.payload.idx;
}


#endif /* EMPPARSER_H */
