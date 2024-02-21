#ifndef EMPINTERP_H
#define EMPINTERP_H

/**
 * @file empinterp.h
 *
 * @brief EMP interpreter common data structures
 */

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "compat.h"
#include "empdag_data.h"
#include "empinterp_data.h"
#include "empinterp_fwd.h"
#include "empparser.h"
#include "equ.h"
#include "gdx_reader.h"
#include "mdl_data.h"
#include "ovfinfo.h"
#include "reshop_data.h"
#include "var.h"

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

typedef struct lexeme {
   unsigned linenr;
   unsigned len;
   const char* start;
} Lexeme;


typedef struct ident_data {
   IdentType type;
   uint8_t dim;
   Lexeme lexeme;
   unsigned idx;
   void *ptr;
} IdentData;

typedef struct gms_indices_data {
   uint8_t nargs;
   uint8_t num_iterators;
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

struct emptok {
   enum emptok_type type;
   unsigned linenr;
   unsigned len;
   const char* start;
   struct gms_dct gms_dct;
   IntScratch iscratch;
   DblScratch dscratch;
   union {
      double real;
      rhp_idx idx;
      Avar v;
      Aequ e;
      char *label;
   } payload;
};

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

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX namedints
#define RHP_LIST_TYPE NamedIntsArray
#define RHP_ELT_TYPE IntArray
#define RHP_ELT_INVALID ((IntArray) {.len = 0, .max = 0, .list = NULL})
#include "namedlist_generic.inc"
#define valid_set(obj)  ((obj).list != NULL)

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

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX namedvec
#define RHP_LIST_TYPE NamedVecArray
#define RHP_ELT_TYPE Lequ
#define RHP_ELT_INVALID ((Lequ) {.len = 0, .max = 0, .coeffs = NULL, .vis = NULL})
#include "namedlist_generic.inc"
#define valid_vector(obj)  (((obj).coeffs != NULL) && ((obj).vis != NULL))

typedef struct NamedMultiSets {
   unsigned len;
   unsigned max;
   struct multiset *list;
   const char **names;
} NamedMultiSets;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX multisets
#define RHP_LIST_TYPE NamedMultiSets
#define RHP_ELT_TYPE struct multiset
#define RHP_ELT_INVALID ((struct multiset) {.dim = 0, .idx = 0, .gdxreader = NULL})
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

typedef struct {
   AliasArray aliases;
   NamedIntsArray sets;
   NamedMultiSets multisets;
   NamedVecArray vectors;
   NamedScalarArray  scalars;
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
   rhp_idx vi[];
} EdgeWeight;

typedef struct dag_labels_list {
   unsigned len;
   unsigned max;
   struct dag_labels **list;
} DagLabelsList;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX dag_labels_list
#define RHP_LIST_TYPE dag_labels_list
#define RHP_ELT_TYPE struct dag_labels*
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

typedef struct dag_labels {
   uint8_t dim;
   uint8_t num_var;
   EdgeType edge_type;
   uint16_t basename_len;
   unsigned num_children;
   unsigned max_children;  /**< Max number of children  */
   daguid_t daguid;        /**< daguid ot the parent */
   const char *basename;
   int *uels_var; /* uel_var[num_var] */
   int data[]; /* Layout: uels[dim] + pos[num_vars]*/
} DagLabels;


typedef struct dag_label {
   uint8_t dim;
   EdgeType edge_type;
   uint16_t basename_len;
   daguid_t daguid; 
   const char *basename;
   int uels[];  /* Layout: uels[dim] */
} DagLabel;


typedef struct DagLabels2Edges {
   unsigned len;
   unsigned max;
   DagLabels **list;
} DagLabels2Edges;

typedef struct DagLabel2Edge {
   unsigned len;
   unsigned max;
   DagLabel **list;
} DagLabel2Edge;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX daglabels2edges
#define RHP_LIST_TYPE DagLabels2Edges
#define RHP_ELT_TYPE DagLabels*
#define RHP_ELT_INVALID NULL
#include "list_generic.inc"

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX daglabel2edge
#define RHP_LIST_TYPE DagLabel2Edge
#define RHP_ELT_TYPE DagLabel*
#define RHP_ELT_INVALID NULL
#include "list_generic.inc"

/* ---------------------------------------------------------------------
 *  This is a registry of all labels that have been defined.
 *  If required, in the 2nd phase, the labels used in the EMP model are
 *  resolved to yield the edges of the DAG.
 * --------------------------------------------------------------------- */

#define DagRegisterEntry DagLabel

typedef struct DagRegister {
   unsigned len;
   unsigned max;
   DagRegisterEntry **list;
} DagRegister;

#define RHP_LOCAL_SCOPE
#define RHP_LIST_PREFIX dagregister
#define RHP_LIST_TYPE DagRegister
#define RHP_ELT_TYPE DagRegisterEntry*
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
   unsigned linenr;
   size_t read;
   const char *linestart;
   const char *linestart_old;
   char *buf;
   char *tmpstr;
   const char *empinfo_fname;
   unsigned tmpstrlen;

   Model *mdl;
   void *dct;

   /* Tokens */
   struct emptok cur;
   struct emptok peek;
   struct emptok pre;

   /* Parser state info */
   InterpParsedKwds state;
   KeywordLexemeInfo last_kw_info;
   InterpFinalization finalize;

   /* Parser state info */
   const struct parser_ops *ops;

   struct empvm_compiler *compiler;

   /* EMPDAG register */
   DagRegisterEntry *regentry;
   DagLabel *dag_root_label;
   DagRegister dagregister;

   DagLabels2Edges labels2edges;
   DagLabel2Edge label2edge;

   /* Other dynamic data */
   GdxReaders gdx_readers;
   CompilerGlobals globals;

   GmsSymIterator gms_sym_iterator;        /**< Iterator for GAMS symbol     */
   /* Set filtering data structures */
   GmsIndicesData indices_membership_test;

   /* Immediate mode data*/
   daguid_t uid_parent;                    /**< UID of the EMPDAG parent node */
   daguid_t uid_grandparent;               /**< UID of the EMPDAG parent node */
   UIntArray edgevfovjs;                 /**< UID of CCFLIB to be added */
} Interpreter;


typedef enum {
   ParserOpsImm,
   ParserOpsCompiler,
} ParserOptType;

typedef struct parser_ops {
   ParserOptType type;
   int (*ccflib_new)(Interpreter* restrict interp, unsigned ccf_idx, MathPrgm **mp);
   int (*ccflib_finalize)(Interpreter* restrict interp, MathPrgm *mp);
   int (*ctr_markequasflipped)(Interpreter* restrict interp);
   int (*gms_parse)(Interpreter* restrict interp, unsigned *p); 
   int (*gms_add_uel)(Interpreter* restrict interp, const struct emptok *tok, unsigned i); 
   int (*label_getidentstr)(Interpreter* restrict interp, const struct emptok *tok, char **ident, unsigned *ident_len);
   int (*mp_addcons)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_addvars)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_addvipairs)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_addzerofunc)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_finalize)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_new)(Interpreter* restrict interp, RhpSense sense, MathPrgm **mp);
   int (*mp_setobjvar)(Interpreter* restrict interp, MathPrgm *mp);
   int (*mp_setprobtype)(Interpreter* restrict interp, MathPrgm *mp, unsigned type);
   int (*mp_settype)(Interpreter* restrict interp, MathPrgm *mp, unsigned type);
   int (*mpe_addmp)(Interpreter* restrict interp, unsigned equil_id, MathPrgm *mp);
   int (*mpe_finalize)(Interpreter* restrict interp, Mpe *mpe);
   int (*mpe_new)(Interpreter* restrict interp, Mpe **mpe);
   int (*identaslabels)(Interpreter * interp, unsigned * p, EdgeType edge_type);
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
   int (*read_param)(Interpreter *interp, unsigned *p, IdentData *data,
                      const char *ident_str, unsigned *param_gidx);
} ParserOps;

extern const struct parser_ops parser_ops_imm;
extern const struct parser_ops parser_ops_compiler;

int parser_filter_start(Interpreter* restrict interp) NONNULL;
int parser_filter_set(Interpreter* restrict interp, unsigned i, int val) NONNULL;

int empinterp_finalize(Interpreter *interp) NONNULL;

static inline unsigned emptok_getlinenr(const struct emptok *tok)
{
   return tok->linenr;
}

static inline unsigned emptok_getstrlen(const struct emptok *tok)
{
   return tok->len;
}

static inline const char * emptok_getstrstart(const struct emptok *tok)
{
   return tok->start;
}

static inline enum emptok_type emptok_gettype(struct emptok *tok)
{
   return tok->type;
}

static inline void emptok_settype(struct emptok *tok, enum emptok_type type)
{
   tok->type = type;
}


NONNULL static
enum emptok_type parser_getcurtoktype(const struct interpreter *interp) {
   return interp->cur.type;
}

NONNULL static
enum emptok_type parser_getpeektoktype(const struct interpreter *interp) {
   return interp->peek.type;
}

NONNULL static
enum emptok_type parser_getpretoktype(const struct interpreter *interp) {
   return interp->pre.type;
}

NONNULL static
rhp_idx parser_getequvaridx(const struct interpreter *interp) {
   assert(interp->cur.type == TOK_GMS_VAR || interp->cur.type == TOK_GMS_EQU);
   return interp->cur.payload.idx;
}



#endif /* EMPPARSER_H */
