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


#include <stdlib.h>

#include "reshop_error_handling.h"
#include "tlsdef.h"

tlsvar jmp_buf internal_jmp_buf;
tlsvar jmp_buf external_jmp_buf;

tlsvar bool external_jmp_buf_used = false;
tlsvar bool internal_jmp_buf_used = false;

tlsvar const char* internal_jmp_buf_err = NULL;
tlsvar const char* external_jmp_buf_err = NULL;

typedef void (*external_fault_handler_t)(size_t, const char*);
tlsvar external_fault_handler_t external_fault_handler = NULL;

jmp_buf* reshop_get_jmp_buf(void)
{
  external_jmp_buf_used = true;
  return &external_jmp_buf;
}

void reshop_release_jmp_buf(void)
{
  external_jmp_buf_used = false;
}

jmp_buf* reshop_get_internal_jmp_buf(void)
{
  internal_jmp_buf_used = true;
  return &internal_jmp_buf;
}

void reshop_release_internal_jmp_buf(void)
{
  internal_jmp_buf_used = false;
}

void reshop_fatal_error(int code, const char* msg)
{
  if (external_fault_handler) { (*external_fault_handler)(code, msg); }

  if (internal_jmp_buf_used) {

    internal_jmp_buf_used = false;
    internal_jmp_buf_err = msg;
    longjmp(internal_jmp_buf, code);

  } else if (external_jmp_buf_used) {

    external_jmp_buf_used = false;
    external_jmp_buf_err = msg;
    longjmp(external_jmp_buf, code);

  } else { /* This should create a core dump */
    abort();
  }

}

const char* reshop_fatal_error_msg(void)
{
  const char * err_msg = NULL;
  if (internal_jmp_buf_err) {

    err_msg = internal_jmp_buf_err;
    internal_jmp_buf_err = NULL;

  } else if (external_jmp_buf_err) {

    err_msg = external_jmp_buf_err;
    external_jmp_buf_err = NULL;

  }

  return err_msg;
}
