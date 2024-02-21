#include <limits.h>
#include <stdbool.h>

#include "macros.h"
#include "rhp_search.h"

#define LOG2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X)) - 1))
 
static unsigned * make_delta(unsigned n)
{
    unsigned n_delta = LOG2(n);
    unsigned *delta;
    MALLOC_NULL(delta, unsigned, n_delta);

    unsigned power = 1;
    unsigned i = 0;
    do {
        assert(i < n_delta);
        unsigned half = power;
        power <<= 1;
        assert(power > 0);
        delta[i] = (n + half) / power;
    } while (delta[i++] != 0);

    return delta;
}

/* TODO(xhub) LOW: this needs proper testing and study  */
/* See also http://jsfiddle.net/VCtqD/1/  */
/*  https://thecodeaddict.wordpress.com/tag/uniform-binary-search/   */
static unsigned unibin_search(rhp_idx *arr, unsigned *delta, rhp_idx idx)
{
    unsigned i = delta[0]-1;  /* midpoint of array */
    unsigned d = 0;

    while (true) {
        if (idx == arr[i]) {
            return i;
        } else if (delta[d] == 0) {
            return -1;
        } else {
            if (idx < arr[i]) {
                i -= delta[++d];
            } else {
                i += delta[++d];
            }
        }
    }
}



/**
 * @brief Simple binary search
 *
 * @param arr  sorted array to the searched
 * @param n    number of elements in the array
 * @param elt  the element to be searched
 *
 * @return     if the element is found in the array: its index in the array
 *             otherwise: UINT_MAX
 */
unsigned bin_search_int(const int * restrict arr, unsigned n, int elt)
{
  unsigned min = 0;
  unsigned max = n - 1;

  while (max >= min) {
      unsigned i = (min + max) / 2;

      if (elt < arr[i]) {
        if (i == 0) {
            goto _exit;
        }
        max = i - 1;
      } else if (elt > arr[i]) {
        min = i + 1;
      } else {
        return i;
      }
    }

_exit:
  return UINT_MAX;
}

/**
 * @brief Simple binary search
 *
 * @param arr  sorted array to the searched
 * @param n    number of elements in the array
 * @param elt  the element to be searched
 *
 * @return     if the element is found in the array: its index in the array
 *             otherwise: UINT_MAX
 */
unsigned bin_search_uint(const unsigned * restrict arr, unsigned n, unsigned elt)
{
  unsigned min = 0;
  unsigned max = n - 1;

  while (max >= min) {
      unsigned i = (min + max) / 2;

      if (elt < arr[i]) {
        if (i == 0) {
            goto _exit;
        }
        max = i - 1;
      } else if (elt > arr[i]) {
        min = i + 1;
      } else {
        return i;
      }
    }

_exit:
  return UINT_MAX;
}
