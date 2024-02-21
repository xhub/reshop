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

/*!\file reshop_error_handling.h
 * \brief error handling functions and data structures*/

#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

#include <setjmp.h>
#include <stdbool.h>

#define RESHOP_SETJMP_INTERNAL_START setjmp(*reshop_get_internal_jmp_buf())
#define RESHOP_SETJMP_INTERNAL_STOP reshop_release_internal_jmp_buf();

#define RESHOP_SETJMP_EXTERNAL_START setjmp(*reshop_get_jmp_buf())
#define RESHOP_SETJMP_EXTERNAL_STOP reshop_release_jmp_buf();

/* Get the external jmp buffer and mark it as used
 * \return the external jmp buffer
 */
jmp_buf* reshop_get_jmp_buf(void);

/** Release the internal jmp buffer: this indicates that it is no longer in
 * use and that there should be no longjmp() call to this part of the stack.
 * The user should call this function whenever the call to a numerics
 * function has been successful*/
void reshop_release_jmp_buf(void);

/* Get the internal jmp buffer and mark it as used
 * \warning this function is meant to be called inside the numerics library.
 * To use the exception handler from an external library/executable, use
 * reshop_get_jmp_buf()
 * \return the internal jmp buffer
 */
jmp_buf* reshop_get_internal_jmp_buf(void);

/** Release the internal jmp buffer: this indicates that it is no longer in
 * use and that there should be no longjmp() call to this part of the stack.
 * The user should call this function whenever the call to a numerics
 * function has been successful.
 * \warning This should not be called outside the numerics library. Use
 * reshop_release_jmp_buf() instead when calling from another library/executable
 */
void reshop_release_internal_jmp_buf(void);

/* Function to call whenever a fatal error occurred. This function either call
 * longjmp if setjmp has been called previously and is still active. If not,
 * it calls abort().
 * \param code error code
 * \param msn error message
 */
void reshop_fatal_error(int code, const char* msg);

/* Get the last error message
 * \return the error message
 */
const char* reshop_fatal_error_msg(void);

#endif /* ERROR_HANDLING_H  */
