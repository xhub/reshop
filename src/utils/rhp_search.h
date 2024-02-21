#ifndef RHP_SEARCH
#define RHP_SEARCH

#include "compat.h"

unsigned bin_search_int(const int *arr, unsigned n, rhp_idx elt);
unsigned bin_search_uint(const unsigned *arr, unsigned n, unsigned elt);

#define bin_search_idx bin_search_int

#endif /* RHP_SEARCH  */
