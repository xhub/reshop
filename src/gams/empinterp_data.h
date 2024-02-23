#ifndef EMPPARSER_DATA_H
#define EMPPARSER_DATA_H

#include <stdbool.h>
#include "gclgms.h"

struct gdx_reader;

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

/** Data associated with a GAMS symbol */
typedef struct gms_dct {
   int idx;                       /**< 1-based index     */
   int dim;                       /**< Symbol dimension  */
   int type;                      /**< Symbol type       */
   int alias_idx;                 /**< Alias index     */
   int alias_type;                /**< Alias type       */
   char alias_name[GMS_SSSIZE];   /**< name of the alias */
   bool read;                     /**< true if the object was successfully read */
} GamsSymData;

typedef struct multiset {
   int idx;           /**< 1-based index in GDX file */
   int dim;           /**< Symbol dimension  */
   int uels[GMS_MAX_INDEX_DIM];
   struct gdx_reader * gdxreader; /**< struct where this array has been found */
} MultiSet;

#endif // !EMPPARSER_DATA_H
