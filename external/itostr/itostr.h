/*
Copyright (c) 2019 Steffen Illhardt

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef ITOSTR_H_INCLUDED__
#define ITOSTR_H_INCLUDED__
#if (-1) != (~0) // Binary operations used rely on two's complement (rather than one's complement or sign-magnitude).
#error "The implementation doesn't use two's complement for signed number representation."
#endif

#include <stddef.h>
#include <stdint.h>

/** \brief Type-cast to the required type of the first argument passed to one of the functions.
 * Use SIGNEDC for the signedto... functions and UNSIGNEDC for the unsignedto... functions, respectively. */
#ifndef __cplusplus
#define SIGNEDC(x) ((intmax_t)(x))
#define UNSIGNEDC(x) ((uintmax_t)(x))
#else
#define SIGNEDC(x) (static_cast<intmax_t>(x))
#define UNSIGNEDC(x) (static_cast<uintmax_t>(x))

extern "C" {
#endif

// SIGNED
/** \brief Convert a signed integer to a null-terminated string of char using the specified base.
 *         If base is 10 and value is negative, the resulting string is preceded with a minus sign.
 *         With any other base, value is always considered unsigned.
 * \param value   Value to be converted to a string. It is treated as a signed type of the size given by the valsize parameter.
 * \param valsize Size of the type passed to the value parameter in bytes.
 * \param buffer  Buffer array where to store the resulting null-terminated string.
 * \param bufsize Size of the array given by the buffer parameter in characters.
 * \param base    Numerical base (between 2 and 36) used to represent the value as a string.
 * \return size_t Length of the resulting string without the terminating null character, or 0 if the function fails. */
size_t signedtostr(intmax_t value, size_t valsize, char *buffer, size_t bufsize, int base);

/** \brief Convert a signed integer to a null-terminated string of wchar_t using the specified base.
 *         If base is 10 and value is negative, the resulting string is preceded with a minus sign.
 *         With any other base, value is always considered unsigned.
 * \param value   Value to be converted to a string. It is treated as a signed type of the size given by the valsize parameter.
 * \param valsize Size of the type passed to the value parameter in bytes.
 * \param buffer  Buffer array where to store the resulting null-terminated string.
 * \param bufsize Size of the array given by the buffer parameter in wide characters.
 * \param base    Numerical base (between 2 and 36) used to represent the value as a string.
 * \return size_t Length of the resulting string without the terminating null character, or 0 if the function fails. */
size_t signedtowcs(intmax_t value, size_t valsize, wchar_t *buffer, size_t bufsize, int base);

// UNSIGNED
/** \brief Convert an unsigned integer to a null-terminated string of char using the specified base.
 * \param value   Value to be converted to a string. It is treated as an unsigned type of the size given by the valsize parameter.
 * \param valsize Size of the type passed to the value parameter in bytes.
 * \param buffer  Buffer array where to store the resulting null-terminated string.
 * \param bufsize Size of the array given by the buffer parameter in characters.
 * \param base    Numerical base (between 2 and 36) used to represent the value as a string.
 * \return size_t Length of the resulting string without the terminating null character, or 0 if the function fails. */
size_t unsignedtostr(uintmax_t value, size_t valsize, char *buffer, size_t bufsize, int base);

/** \brief Convert an unsigned integer to a null-terminated string of wchar_t using the specified base.
 * \param value   Value to be converted to a string. It is treated as an unsigned type of the size given by the valsize parameter.
 * \param valsize Size of the type passed to the value parameter in bytes.
 * \param buffer  Buffer array where to store the resulting null-terminated string.
 * \param bufsize Size of the array given by the buffer parameter in wide characters.
 * \param base    Numerical base (between 2 and 36) used to represent the value as a string.
 * \return size_t Length of the resulting string without the terminating null character, or 0 if the function fails. */
size_t unsignedtowcs(uintmax_t value, size_t valsize, wchar_t *buffer, size_t bufsize, int base);

#ifdef __cplusplus
}
#endif

#endif
