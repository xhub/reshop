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
   IdentOriginEmpDag,  /**< Ident is an EMPDAG label */
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
   IdentSymbolSlice,
   IdentVar,
   IdentEqu,
   IdentEmpDagLabel,    /**< Ident is a EMPDAG label */
   IdentTypeMaxValue,
} IdentType;

/** Lexeme data */
typedef struct lexeme {
   unsigned linenr;    /**< line number of the lexeme   */
   unsigned len;       /**< length of the lexeme */
   const char* start;  /**< Start of the lexeme string  */
} Lexeme;

/** Data associated with an identifier */
typedef struct ident_data {
   IdentType type;      /**< Symbol type                   */
   IdentOrigin origin;  /**< Symbol origin                 */
   uint8_t dim;         /**< Dimension of the symbol       */
   Lexeme lexeme;       /**< Lexeme                        */
   unsigned idx;        /**< 1-based index                 */
   void *ptr;           /**< Pointer associated with symbol */ 
} IdentData;

/** Data associated with a GAMS symbol */
typedef struct gams_symb_data {
   IdentData ident;             /**< Identifier data (symbol name)           */
   bool read;                  /**< true if the object was successfully read */
//   int domindices[GLOBAL_MAX_INDEX_DIM]; /**< Indices of the domains */
} GamsSymData;


typedef enum {
   LogSumExp = 0,
   SmoothingOperatorSchemeLast = LogSumExp,
} SmoothingOperatorScheme;

typedef struct smoothing_operator_data {
   SmoothingOperatorScheme scheme;
   double parameter;
   unsigned parameter_position;
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

static inline int gdxsymtype2ident(enum gdxSyType gdxtype, IdentData *ident)
{
   int dim = ident->dim;
   assert(dim <= GMS_MAX_INDEX_DIM);
   assert(ident->origin == IdentOriginGdx || ident->origin == IdentOriginGmd);

   switch (gdxtype) {
   case dt_set:
      if (dim == 1) { ident->type = IdentSet; }
      else { ident->type = IdentMultiSet; };
      return OK;
   case dt_par:
      if (dim == 0) { ident->type = IdentScalar; }
      else if (dim == 1) { ident->type = IdentVector; }
      else { ident->type = IdentParam; }
      return OK;
   case dt_alias:
      ident->type = IdentAlias;
      return OK;
   case dt_var:
      ident->type = IdentVar;
      return OK;
      case dt_equ:
      ident->type = IdentEqu;
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
   case IdentUEL:
      return TOK_GMS_UEL;
   case IdentNotFound:
      return TOK_IDENT;
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

static inline void tok2lexeme(Token * restrict tok, Lexeme * restrict lexeme)
{
   lexeme->linenr = tok->linenr;
   lexeme->len = tok->len;
   lexeme->start = tok->start;
}



#endif // !EMPINTERP_DATA_H
