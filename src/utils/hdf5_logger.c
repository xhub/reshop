/* Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 *
 * Copyright 2018 INRIA.
 * Copyright 2020 Olivier Huber <oli.huber@gmail.com>
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

#include "hdf5_logger.h"
#include "printout.h"
#include "status.h"

#ifdef WITH_HDF5

#define _ISOC99_SOURCE

#include <H5Apublic.h>
#include <H5Dpublic.h>
#include <H5Fpublic.h>
#include <H5Gpublic.h>
#include <H5Ipublic.h>
#include <H5Ppublic.h>
#include <H5Spublic.h>
#include <H5Zpublic.h>
#include <math.h>

#include "macros.h"
#include "rhp_LA.h"

bool logh5_check_gzip(void)
{

  unsigned filter_info;

  htri_t avail = H5Zfilter_avail(H5Z_FILTER_DEFLATE);
  if (!avail) {
    printf ("gzip filter not available.\n");
    return false;
  }

  herr_t status = H5Zget_filter_info(H5Z_FILTER_DEFLATE, &filter_info);
  if ( status < 0 || !(filter_info & H5Z_FILTER_CONFIG_ENCODE_ENABLED) ||
      !(filter_info & H5Z_FILTER_CONFIG_DECODE_ENABLED) ) {
    printf ("gzip filter not available for encoding and decoding.\n");
    return false;
  }

  return true;

}

/** Initialize the hdf5 logging
 * \param filename name of the hdf5 file
 * \param iter_max maximum number of iteration (or an upper bound)
 * \return a struct containing all the info related to the hdf5 logging
 */
struct logh5* logh5_init(const char* filename, const unsigned iter_max)
{
  herr_t status;

  assert(filename);
  assert(iter_max > 0);

  struct logh5* logger;
  CALLOC_NULL(logger, struct logh5, 1);

  logger->file = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  /* formula : 2 ("i-") + 1 (end '\0') + number of char to represent the number
   * of iteration*/
  logger->itername_len = 4 + (unsigned)lrint(log10(iter_max));

  CALLOC_EXIT_NULL(logger->itername, char, logger->itername_len);

  logger->group = 0;

  return logger;

_exit:
  logh5_end(logger);
  return NULL;
}

int logh5_end(struct logh5* logger)
{
  if (!logger) { return OK; }

  herr_t status = 0;

  if (logger->group) {
    error("%s :: group pointer was not 0! Most likely the last opened group was not properly closed\n", __func__);
    logh5_end_iter(logger);
  }

  status = H5Fclose(logger->file);
  logger->file = 0;

  FREE(logger->itername);

  FREE(logger);

  return status < 0 ? Error_HDF5_IO : OK;
}

int logh5_new_iter(struct logh5* logger, unsigned iter)
{
  herr_t status = 0;

  if (logger->group) {
    error("logh5_new_iter :: group pointer was not 0! Most likely the last opened group was not properly closed\n");
    status = H5Gclose (logger->group);
  }

  snprintf(logger->itername, logger->itername_len, "i-%u", iter);

  logger->group = H5Gcreate (logger->file, logger->itername, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

  return status == 0 ? OK : Error_HDF5_IO;
}

int logh5_end_iter(struct logh5* logger)
{
  assert(logger);
  assert(logger->group);

  if (!logger->group) {
    error("logh5_end_iter :: group pointer is NULL !!\n");
    return Error_HDF5_NullPointer;
  }

  herr_t status = H5Gclose (logger->group);
  logger->group = 0;

  return status < 0 ? Error_HDF5_IO : OK;
}

int logh5_inc_iter(struct logh5* logger)
{
  if (logger->iter > 0) {
    S_CHECK(logh5_end_iter(logger));
  }

  logger->iter++;
  S_CHECK(logh5_new_iter(logger, logger->iter));

  return OK;
}

static int logh5_write_dset(hid_t loc_id, const char* name, hid_t type, hid_t space, void* val)
{
  herr_t status;
  hid_t dset = H5Dcreate (loc_id, name, type, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Dwrite (dset, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, val);
  status = H5Dclose (dset);

  return status < 0 ? Error_HDF5_IO : OK;
}

static int logh5_write_attr(hid_t loc_id, const char* name, hid_t type, hid_t space, void* val)
{
  herr_t status;
  hid_t dset = H5Acreate (loc_id, name, type, space, H5P_DEFAULT, H5P_DEFAULT);
  status = H5Awrite (dset, type, val);
  status = H5Aclose (dset);

  return status < 0 ? Error_HDF5_IO : OK;
}

int logh5_scalar_double(struct logh5* logger, double val, const char* name)
{
  hid_t loc_id = logger->group > 0 ? logger->group : logger->file;
  assert(name);

  herr_t status;
  hid_t space = H5Screate(H5S_SCALAR);

  status = logh5_write_dset(loc_id, name, H5T_NATIVE_DOUBLE, space, &val);
  status = H5Sclose(space);

  return status < 0 ? Error_HDF5_IO : OK;
}


static int _logh5_scalar_integer(ptrdiff_t val, const char* name, hid_t loc_id)
{
  assert(name);

  hid_t type;
  herr_t status;

  switch (sizeof(ptrdiff_t))
  {
    case sizeof(int32_t):
    {
      type = H5T_NATIVE_INT_LEAST32;
      break;
    }
    case sizeof(int64_t):
    {
      type = H5T_NATIVE_INT_LEAST64;
      break;
    }
    default:
      error("logh5 :: unsupported integer length %lu\n", sizeof(ptrdiff_t));
      return Error_HDF5_Unsupported;
  }

  hid_t space = H5Screate(H5S_SCALAR);
  status = logh5_write_dset(loc_id, name, type, space, &val);
  status = H5Sclose(space);

  return status < 0 ? Error_HDF5_IO : OK;
}

static int _logh5_scalar_uinteger(size_t val, const char* name, hid_t loc_id)
{
  assert(name);

  hid_t type;
  herr_t status;

  switch (sizeof(size_t))
  {
  case sizeof(int32_t):
    type = H5T_NATIVE_UINT_LEAST32;
    break;
  case sizeof(int64_t):
    type = H5T_NATIVE_UINT_LEAST64;
    break;
  default:
    error("logh5 :: unsupported unsigned integer length %lu\n", sizeof(size_t));
    return Error_HDF5_Unsupported;
  }

  hid_t space = H5Screate(H5S_SCALAR);
  status = logh5_write_dset(loc_id, name, type, space, &val);
  status = H5Sclose(space);

  return status < 0 ? Error_HDF5_IO : OK;
}

static int _logh5_vec_double(size_t size, double* vec, const char* name, hid_t loc_id)
{
  assert(vec);
  assert(name);

  hsize_t dims[1] = {size};
  hid_t type = H5T_NATIVE_DOUBLE;
  herr_t status;

  hid_t space =  H5Screate_simple(1, dims, NULL);
  status = logh5_write_dset(loc_id, name, type, space, vec);
  status = H5Sclose(space);

  return status < 0 ? Error_HDF5_IO : OK;
}

int logh5_attr_uinteger(struct logh5* logger, size_t val, const char* name)
{
  hid_t loc_id = logger->group > 0 ? logger->group : logger->file;
  assert(name);

  hid_t type;
  herr_t status;

  switch (sizeof(size_t))
  {
    case sizeof(int32_t):
    {
      type = H5T_NATIVE_UINT_LEAST32;
    break;
    }
    case sizeof(int64_t):
    {
      type = H5T_NATIVE_UINT_LEAST64;
      break;
    }
    default:
      error("logh5 :: unsupported unsigned integer length %lu\n", sizeof(size_t));
      return Error_HDF5_Unsupported;
  }

  hid_t space = H5Screate(H5S_SCALAR);
  status = logh5_write_attr(loc_id, name, type, space, &val);
  status = H5Sclose(space);

  return status < 0 ? Error_HDF5_IO : OK;
}

static int _logh5_vec_int64(size_t size, int64_t* vec, const char* name, hid_t loc_id)
{
  assert(vec);
  assert(name);

  hsize_t dims[1] = {size};
  hid_t type = H5T_NATIVE_INT_LEAST64;
  herr_t status;

  hid_t space =  H5Screate_simple(1, dims, NULL);
  status = logh5_write_dset(loc_id, name, type, space, vec);
  status = H5Sclose(space);

  return status < 0 ? Error_HDF5_IO : OK;
}

static int _logh5_vec_int32(size_t size, int32_t* vec, const char* name, hid_t loc_id)
{
  assert(vec);
  assert(name);

  hsize_t dims[1] = {size};
  hid_t type = H5T_NATIVE_INT_LEAST32;
  herr_t status;

  hid_t space =  H5Screate_simple(1, dims, NULL);
  status = logh5_write_dset(loc_id, name, type, space, vec);
  status = H5Sclose(space);

  return status < 0 ? Error_HDF5_IO : OK;
}

int logh5_scalar_integer(struct logh5* logger, ptrdiff_t val, const char* name)
{
   hid_t loc_id = logger->group > 0 ? logger->group : logger->file;
   return _logh5_scalar_integer(val, name, loc_id);
}

int logh5_scalar_uinteger(struct logh5* logger, size_t val, const char* name)
{
   hid_t loc_id = logger->group > 0 ? logger->group : logger->file;
   return _logh5_scalar_uinteger(val, name, loc_id);
}

int logh5_vec_double(struct logh5* logger, size_t size, double* vec, const char* name)
{
   hid_t loc_id = logger->group > 0 ? logger->group : logger->file;
   return _logh5_vec_double(size, vec, name, loc_id);
}

int logh5_vec_int64(struct logh5* logger, size_t size, int64_t* vec, const char* name)
{
   hid_t loc_id = logger->group > 0 ? logger->group : logger->file;
   return _logh5_vec_int64(size, vec, name, loc_id);
}

int logh5_vec_int32(struct logh5* logger, size_t size, int32_t* vec, const char* name)
{
   hid_t loc_id = logger->group > 0 ? logger->group : logger->file;
   return _logh5_vec_int32(size, vec, name, loc_id);
}

int logh5_sparse(struct logh5* logger, struct sp_matrix* mat, const char* name)
{
  int result = OK;
  /* Create subgroup based on the name of the matrix*/
  hid_t mat_group;
  if (logger->group) {
    mat_group = H5Gcreate(logger->group, name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  } else {
    mat_group = H5Gcreate(logger->file, name, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  }

  result = _logh5_scalar_integer(mat->nnzmax, "nzmax", mat_group);
  result = _logh5_scalar_integer(mat->m, "m", mat_group);
  result = _logh5_scalar_integer(mat->n, "n", mat_group);
  result = _logh5_scalar_integer(mat->nnz, "nz", mat_group);

  if (sizeof(RHP_INT) == sizeof(int64_t)) {
    result = _logh5_vec_int64(mat->n+1, (int64_t*)mat->p, "p", mat_group);
    result = _logh5_vec_int64(mat->nnz, (int64_t*)mat->i, "i", mat_group);
  } else if (sizeof(RHP_INT) == sizeof(int32_t)) {
    result = _logh5_vec_int32(mat->n+1, mat->p, "p", mat_group);
    result = _logh5_vec_int32(mat->nnz, mat->i, "i", mat_group);
  } else {
    error("%s :: unknown pointer size %lu\n", __func__, sizeof(RHP_INT));
    result = Error_HDF5_Unsupported;
  }
  result = _logh5_vec_double(mat->nnz, mat->x, "x", mat_group);

  if (name) {
    herr_t status = H5Gclose (mat_group);
    if (status < 0) { result = Error_HDF5_IO; }
  }

  return result;
}


int logh5_mat_dense(struct logh5* logger, size_t size0, size_t size1, double* mat, const char* name)
{
  hid_t loc_id = logger->group > 0 ? logger->group : logger->file;
  assert(mat);
  assert(name);

  hsize_t dims[2] = {size0, size1};
  hid_t type = H5T_NATIVE_DOUBLE;
  herr_t status;

  hid_t space =  H5Screate_simple(2, dims, NULL);
  status = logh5_write_dset(loc_id, name, type, space, mat);
  status = H5Sclose(space);

  return status < 0 ? Error_HDF5_IO : OK;
}

int logh5_vec_uint64(struct logh5* logger, size_t size, uint64_t* vec, const char* name)
{
  hid_t loc_id = logger->group > 0 ? logger->group : logger->file;
  assert(vec);
  assert(name);

  hsize_t dims[1] = {size};
  hid_t type = H5T_NATIVE_UINT_LEAST64;
  herr_t status;

  hid_t space =  H5Screate_simple(1, dims, NULL);
  status = logh5_write_dset(loc_id, name, type, space, vec);
  status = H5Sclose(space);

  return status < 0 ? Error_HDF5_IO : OK;
}

#else /* WITH_HDF5 */

bool logh5_check_gzip(void)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return false;
}

struct logh5* logh5_init(const char* filename, const unsigned iter_max)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return NULL;
}

int logh5_end(struct logh5* logger)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_new_iter(struct logh5* logger, unsigned iter)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_inc_iter(struct logh5* logger)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_end_iter(struct logh5* logger)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_scalar_double(struct logh5* logger, double val, const char* name)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_scalar_integer(struct logh5* logger, ptrdiff_t val, const char* name)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_scalar_uinteger(struct logh5* logger, size_t val, const char* name)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_vec_double(struct logh5* logger, size_t size, double* vec, const char* name)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_sparse(struct logh5* logger, struct sp_matrix* mat, const char* name)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}


int logh5_mat_dense(struct logh5* logger, size_t size0, size_t size1, double* mat, const char* name)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_vec_int32(struct logh5* logger, size_t size, int32_t* vec, const char* name)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_vec_int64(struct logh5* logger, size_t size, int64_t* vec, const char* name)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

int logh5_vec_uint64(struct logh5* logger, size_t size, uint64_t* vec, const char* name)
{
  errormsg("logh5 :: ReSHOP has been compiled with no HDF5 support!\n");
  return Error_RuntimeError;
}

#endif /*  WITH_HDF5 */
