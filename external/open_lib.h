/* Siconos is a program dedicated to modeling, simulation and control
 * of non smooth dynamical systems.
 *
 * Copyright 2016 INRIA.
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

/** \file open_lib.h
 * \brief function to open library and find functions in then (useful for
 * plugin */

#ifndef OPEN_LIB_H
#define OPEN_LIB_H

/**
 *  @brief open a library and return an handle (casted as void*)
 *
 *  @param lib_name  name of the library
 *  @param flags     additional flags (for dlopen)
 *
 *  @return          the handle to the library
 */
void* open_library(const char* lib_name, int flags);

/**
 *  @brief open a library and return an handle (casted as void*). Does not error
 *
 *  @param lib_name  name of the library
 *  @param flags     additional flags (for dlopen)
 *
 *  @return          the handle to the library
 */
void* open_library_nofail(const char* lib_name, int flags);

/**
 *  @brief close a previously opened dynamic library
 *
 *  @param handle  handle
 */
void close_library(void *handle);

/**
 *  @brief get the address of a function in an already opened lib
 *
 *  @param handle  plugin handle to the library
 *  @param func    name of function
 *
 *  @return       the pointer to the address of the function
 */
void* get_function_address(void* handle, const char* func);

#endif
