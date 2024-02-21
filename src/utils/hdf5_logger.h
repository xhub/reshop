/* Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 *
 * Copyright 2018 INRIA.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file hdf5_logger.h
 *  @brief Logger using an HDF5 output
 *
 */

#ifndef HDF5_LOGGER_H
#define HDF5_LOGGER_H

#include <stdbool.h>
#include <stddef.h>         // for size_t, ptrdiff_t
#include <stdint.h>         // for int32_t, int64_t, uint64_t

#include "level_logger.h"      // for loglevels, LOGLEVEL_NO

#ifdef WITH_HDF5

#include <H5public.h>
#include <H5Ipublic.h>

#else

#define hid_t void*

#endif /* WITH_HDF5 */

struct sp_matrix;

/** HDF5 logger (for full debug purposes) */
struct logh5 {
  char* itername; /**< name of the group for the current iteration */
  unsigned itername_len; /**< maximum length of itername */
  unsigned iter;       /**< Current iteration */
  hid_t file; /**< handle to the HDF5 file */
  hid_t group; /**< Handle to the current (top-level) group (e.g. "/foo") */
};

bool logh5_check_gzip(void);

struct logh5*  logh5_init(const char* filename, const unsigned iter_max);

/** Close the hdf5 file
 * \param logger the struct related to logging
 * \return true if success, false otherwise
 */
int logh5_end(struct logh5* logger);

/** Start a new iteration in the hdf5 file (create a folder
 * /i-iteration_number
 * \param iter iteration number
 * \param logger the struct related to logging
 * \return true if success, false otherwise
 */
int logh5_new_iter(struct logh5* logger, unsigned iter);

/** End an iteration
 * \param logger the struct related to logging
 * \return true if success, false otherwise
 */
int logh5_end_iter(struct logh5* logger);

int logh5_inc_iter(struct logh5* logger);

int logh5_scalar_double(struct logh5* logger, double val, const char* name);

int logh5_scalar_integer(struct logh5* logger, ptrdiff_t val, const char* name);

int logh5_scalar_uinteger(struct logh5* logger, size_t val, const char* name);

int logh5_attr_uinteger(struct logh5* logger, size_t val, const char* name);

int logh5_vec_double(struct logh5* logger, size_t size, double* vec, const char* name);

int logh5_mat_dense(struct logh5* logger, size_t size0, size_t size1, double* mat, const char* name);

int logh5_sparse(struct logh5* logh5, struct sp_matrix* mat, const char* name);

int logh5_vec_int32(struct logh5* logger, size_t size, int32_t* vec, const char* name);

int logh5_vec_int64(struct logh5* logger, size_t size, int64_t* vec, const char* name);

int logh5_vec_uint64(struct logh5* logger, size_t size, uint64_t* vec, const char* name);

/** filter loglevel for the hdf5 logger. Useful to disable logging if the
 * hdf5 support that been disable.
 * \param l desired loglevel
 * \return l if the hdf5 logger is enabled, LOGLEVEL_NO otherwise*/
static inline RHP_loglevels logh5_loglevel(RHP_loglevels l)
{
#ifdef WITH_HDF5
  return l;
#else
  return RHP_LOGLEVEL_NO;
#endif
}

#endif
