#ifndef EMPINTERP_DATA_H
#define EMPINTERP_DATA_H

#include <assert.h>

#include <stdbool.h>
#include "empparser_data.h"
#include "equ.h"
#include "printout.h"

#include "gclgms.h"
#include "reshop_data.h"
#include "var.h"

struct gdx_reader;

typedef enum {
   IdentOriginUnknown,
   IdentOriginGdx,
   IdentOriginGmd,
   IdentOriginDct,
   IdentOriginLocal,
   IdentOriginGlobal,
} IdentOrigin;

typedef enum IdentType {
   IdentNotFound,
   IdentInternalIndex,
   IdentInternalMax,
   IdentLocalSet,
   IdentLocalMultiSet,
   IdentLocalScalar,
   IdentLocalVector,
   IdentLocalParam,
   IdentLoopIterator,
   IdentAlias,
   IdentSet,
   IdentMultiSet,
   IdentScalar,
   IdentVector,
   IdentParam,
   IdentUEL,
   IdentUniversalSet,
   IdentVar,
   IdentEqu,
   IdentTypeMaxValue,
} IdentType;

// TODO: Replace GamsSymData with IdentData?
/** Data associated with a GAMS symbol */
typedef struct gams_symb_data {
   int idx;                       /**< 1-based index     */
   int dim;                       /**< Symbol dimension  */
   void *ptr;                     /**< Pointer associated with symbol */
   bool read;                     /**< true if the object was successfully read */
   IdentType type;                /**< Symbol type       */
   IdentOrigin origin;            /**< Symbol origin */
   int domindices[GLOBAL_MAX_INDEX_DIM]; /**< Indices of the domains */
} GamsSymData;

typedef struct lexeme {
   unsigned linenr;
   unsigned len;
   const char* start;
} Lexeme;


typedef struct ident_data {
   IdentType type;
   IdentOrigin origin;
   uint8_t dim;
   Lexeme lexeme;
   unsigned idx;
   void *ptr;
} IdentData;

typedef enum {
   LogSumExp = 0,
   SmoothingOperatorSchemeLast = LogSumExp,
} SmoothingOperatorScheme;

typedef struct {
   SmoothingOperatorScheme scheme;
   double parameter;
} SmoothingOperatorData;


typedef struct emptok {
   TokenType type;
   unsigned linenr;
   unsigned len;
   const char* start;
   GamsSymData symdat;
   IntScratch iscratch;
   DblScratch dscratch;
   union {
      double real;
      rhp_idx idx;
      Avar v;
      Aequ e;
      char *label;
      SmoothingOperatorData smoothing_operator;
   } payload;
} Token;

typedef struct gdx_multiset {
   int idx;           /**< 1-based index in GDX file */
   int dim;           /**< Symbol dimension  */
   int uels[GMS_MAX_INDEX_DIM];
   struct gdx_reader * gdxreader; /**< struct where this array has been found */
} GdxMultiSet;

static inline int symtype_gdx2rhp(enum gdxSyType gdxtype, GamsSymData *symdat, Token *tok)
{
   int dim = symdat->dim;
   assert(dim <= GMS_MAX_INDEX_DIM);
   assert(symdat->origin == IdentOriginGdx || symdat->origin == IdentOriginGmd);

   switch (gdxtype) {
   case dt_set:
      if (dim == 1) { symdat->type = IdentSet; tok->type = TOK_GMS_SET; }
      else { symdat->type = IdentMultiSet; tok->type = TOK_GMS_MULTISET; };
      return OK;
   case dt_par:
      tok->type = TOK_GMS_PARAM;
      if (dim == 0) { symdat->type = IdentScalar; }
      if (dim == 1) { symdat->type = IdentVector; }
      else { symdat->type = IdentParam; }
      return OK;
   case dt_alias:
      tok->type = TOK_GMS_ALIAS;
      symdat->type = IdentAlias;
      return OK;
   case dt_var:
      tok->type = TOK_GMS_VAR;
      symdat->type = IdentVar;
      return OK;
      case dt_equ:
      tok->type = TOK_GMS_EQU;
      symdat->type = IdentEqu;
      return OK;
   default:
      error("[empinterp] ERROR: gdx type %d not support. Please file a bug report\n", gdxtype);
      return Error_NotImplemented;
   }

}

static inline TokenType ident2toktype(IdentType type)
{
   switch (type) {
   case IdentVar:
      return TOK_GMS_VAR;
   case IdentEqu:
      return TOK_GMS_EQU;
   case IdentAlias:
      return TOK_GMS_ALIAS;
   case IdentSet:
   case IdentMultiSet:
      return TOK_GMS_SET;
   case IdentScalar:
   case IdentVector:
   case IdentParam:
      return TOK_GMS_PARAM;
   default:
      return TOK_ERROR;
   }
}

/* TODO: delete ? 2024.03.22: seems like it could be useful */
UNUSED static inline IdentType toktype2ident(TokenType toktype, unsigned dim)
{
   switch(toktype) {
   case TOK_GMS_ALIAS:
      return IdentAlias;
   case TOK_GMS_SET:
      if (dim == 1) return IdentSet;
      return IdentMultiSet;
   case TOK_GMS_PARAM:
      if (dim == 0) return IdentScalar;
      if (dim == 1) return IdentVector;
      return IdentParam;
   case TOK_GMS_VAR:
      return IdentVar;
   case TOK_GMS_EQU:
      return IdentEqu;
   default:
      return IdentNotFound;
   }
}

#endif // !EMPINTERP_DATA_H
