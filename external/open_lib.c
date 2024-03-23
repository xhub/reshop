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



#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#ifdef _WIN32
#include <windows.h>
typedef HMODULE PluginHandle;
#else
typedef void* PluginHandle;
#endif

#include "open_lib.h"
#include "printout.h"

void* open_library(const char* lib_name, int flags)
{
  void* HandleRes;

#ifdef _WIN32
  HandleRes = (void*) LoadLibrary(lib_name);
  if (!HandleRes)
  {
    int err = (int)GetLastError();
    error("[open_library] ERROR: LoadLibrary error number %d while trying to open %s\n", err, lib_name);
  }

#else
  /* -------------------------------------------------------------------------------------- *
   * For RTLD_DEEPBIND, see                                                                 *
   * https://stackoverflow.com/questions/34073051/when-we-are-supposed-to-use-rtld-deepbind *
   * We may want to change this behaviour                                                   *
   * -------------------------------------------------------------------------------------- */

  int mode = RTLD_LAZY      /* MANDATORY flag: load symbol in a lazy way (as needed) */
#ifdef RTLD_DEEPBIND
           | RTLD_DEEPBIND  /* resolves symbols in the DSO locally first */
#endif
           | RTLD_LOCAL     /* keep symbols in the loaded DSO local */
           | flags;

  /* disable RTLD_DEEPBIND on apple and with ASAN */
#if defined(RTLD_DEEPBIND) || defined(__SANITIZE_ADDRESS__) || (defined(__clang__) && __has_feature(address_sanitizer))
  mode &= ~RTLD_DEEPBIND;
#endif

  HandleRes = dlopen(lib_name, mode);

  if (!HandleRes) {
    error("[open_library] ERROR: dlopen error '%s while trying to open library '%s'\n", dlerror(), lib_name);
  }
#endif

  return HandleRes;
}

void* get_function_address(void* handle, const char* func)
{
  void* ptr;
#ifdef _WIN32
  HMODULE handleW = (HMODULE) handle;
  ptr = (void*) GetProcAddress(handleW, func);
  if (!ptr)
  {
    DWORD err = GetLastError();
    error("[open_library] Error %lu while trying to find procedure %s\n", err, func);
  }
#else
  ptr = dlsym(handle, func);
  if (!ptr) {
    error("[open_library] Error %s while trying to find procedure %s\n", dlerror(), func);
  }
#endif
  return ptr;
}

