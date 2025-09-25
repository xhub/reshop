#ifndef ERROR_HANDLING_H
#define ERROR_HANDLING_H

/** @file reshop_error_handling.h
 *  @brief error handling functions and data structures
 */

#include <setjmp.h>
#include <stdbool.h>

#define RESHOP_SETJMP_INTERNAL_START setjmp(*reshop_get_internal_jmp_buf())
#define RESHOP_SETJMP_INTERNAL_STOP reshop_release_internal_jmp_buf();

#define RESHOP_SETJMP_EXTERNAL_START setjmp(*reshop_get_jmp_buf())
#define RESHOP_SETJMP_EXTERNAL_STOP reshop_release_jmp_buf();

/** Get the external jmp buffer and mark it as used
 * @return the external jmp buffer
 */
jmp_buf* reshop_get_jmp_buf(void);

/** Release the internal jmp buffer: this indicates that it is no longer in
 * use and that there should be no longjmp() call to this part of the stack.
 * The user should call this function whenever the call to a numerics
 * function has been successful*/
void reshop_release_jmp_buf(void);

/** Get the internal jmp buffer and mark it as used
 * @warning this function is meant to be called inside the reshop library.
 * To use the exception handler from an external library/executable, use
 * reshop_get_jmp_buf()
 *
 * @return the internal jmp buffer
 */
jmp_buf* reshop_get_internal_jmp_buf(void);

/** Release the internal jmp buffer: this indicates that it is no longer in
 * use and that there should be no longjmp() call to this part of the stack.
 * The user should call this function whenever the call to a numerics
 * function has been successful.
 * \warning This should not be called outside of reshop. Use
 * reshop_release_jmp_buf() instead when calling from another library/executable
 */
void reshop_release_internal_jmp_buf(void);

/** Function to call whenever a fatal error occurred. This function either call
 * longjmp if setjmp has been called previously and is still active. If not,
 * it calls abort().
 *
 * @param code error code
 * @param msg  error message
 */
void handle_fatal_error(int code, const char* msg);

/* Get the last error message
 * \return the error message
 */
const char* get_last_fatal_error_msg(void);

#endif /* ERROR_HANDLING_H  */
