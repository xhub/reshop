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

#include "itostr.h"

size_t signeddecstrconv_impl(intmax_t value, char *buffer, size_t bufsize);
size_t signeddecwcsconv_impl(intmax_t value, wchar_t *buffer, size_t bufsize);
size_t unsignedstrconv_impl(uintmax_t value, char *buffer, size_t bufsize, int base);
size_t unsignedwcsconv_impl(uintmax_t value, wchar_t *buffer, size_t bufsize, int base);

/* ########## public functions ########## */
size_t signedtostr(intmax_t value, size_t valsize, char *buffer, size_t bufsize, int base)
{
  if (buffer == (char*)NULL || bufsize == (size_t)0)
    return (size_t)0;
  if (valsize == (size_t)0 || (valsize & (valsize - 1)) != (size_t)0 || valsize > sizeof(intmax_t)) // restrict valsize to powers of two that don't exceed the maximum size of the value parameter
  {
    *buffer = '0';
    return (size_t)0;
  }
  uintmax_t signmask = UINTMAX_C(128) << ((valsize-1) << 3), typemask = (signmask << 1) - 1; // signmask: one bit set at the MSB position of the passed type, typemask: all bits set in the range of the passed type
  return base == 10 && ((uintmax_t)value & signmask) != UINTMAX_C(0) // decide if the value shall be unsigned or negative, mask out all bits which exceed the passed type size using one-bits (negative value) or zero-bits (unsigned value)
         ? signeddecstrconv_impl((intmax_t)((uintmax_t)value | ~typemask), buffer, bufsize)
         : unsignedstrconv_impl((uintmax_t)value & typemask, buffer, bufsize, base);
}

size_t signedtowcs(intmax_t value, size_t valsize, wchar_t *buffer, size_t bufsize, int base)
{
  if (buffer == (wchar_t*)NULL || bufsize == (size_t)0)
    return (size_t)0;
  if (valsize == (size_t)0 || (valsize & (valsize - 1)) != (size_t)0 || valsize > sizeof(intmax_t))
  {
    *buffer = L'0';
    return (size_t)0;
  }
  uintmax_t signmask = UINTMAX_C(128) << ((valsize-1) << 3), typemask = (signmask << 1) - 1;
  return base == 10 && ((uintmax_t)value & signmask) != UINTMAX_C(0)
         ? signeddecwcsconv_impl((intmax_t)((uintmax_t)value | ~typemask), buffer, bufsize)
         : unsignedwcsconv_impl((uintmax_t)value & typemask, buffer, bufsize, base);
}

size_t unsignedtostr(uintmax_t value, size_t valsize, char *buffer, size_t bufsize, int base)
{
  if (buffer == (char*)NULL || bufsize == (size_t)0)
    return (size_t)0;
  if (valsize == (size_t)0 || (valsize & (valsize - 1)) != (size_t)0 || valsize > sizeof(uintmax_t))
  {
    *buffer = '0';
    return (size_t)0;
  }
  return unsignedstrconv_impl(value & ((UINTMAX_C(256) << ((valsize-1) << 3)) - 1), buffer, bufsize, base);
}

size_t unsignedtowcs(uintmax_t value, size_t valsize, wchar_t *buffer, size_t bufsize, int base)
{
  if (buffer == (wchar_t*)NULL || bufsize == (size_t)0)
    return (size_t)0;
  if (valsize == (size_t)0 || (valsize & (valsize - 1)) != (size_t)0 || valsize > sizeof(uintmax_t))
  {
    *buffer = L'0';
    return (size_t)0;
  }
  return unsignedwcsconv_impl(value & ((UINTMAX_C(256) << ((valsize-1) << 3)) - 1), buffer, bufsize, base);
}

/* ########## private worker functions that perform the actual conversion ########## */
size_t signeddecstrconv_impl(intmax_t value, char *buffer, size_t bufsize)
{ // relies on prior verification that value is negative, buffer != NULL, bufsize != 0, and base is 10
  size_t length = (size_t)0;
  if (bufsize < (size_t)3 || (length = unsignedstrconv_impl(UINTMAX_C(0) - value, buffer + 1, bufsize-1, 10)) == (size_t)0)
  {
    *buffer = '\0';
    return length;
  }
  *buffer = '-';
  return length+1;
}

size_t signeddecwcsconv_impl(intmax_t value, wchar_t *buffer, size_t bufsize)
{
  size_t length = (size_t)0;
  if (bufsize < (size_t)3 || (length = unsignedwcsconv_impl(UINTMAX_C(0) - value, buffer + 1, bufsize-1, 10)) == (size_t)0)
  {
    *buffer = L'\0';
    return length;
  }
  *buffer = L'-';
  return length+1;
}

size_t unsignedstrconv_impl(uintmax_t value, char *buffer, size_t bufsize, int base)
{ // relies on prior verification that buffer != NULL and bufsize != 0
  static const char charcodes[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  if (base < 2 || base > 36) // check if we got a qualified base
  {
    *buffer = '\0';
    return (size_t)0;
  }
  char *end = buffer; // pointer to the position to write
  do
  {
    if (bufsize-1 == (size_t)0) // decrement the remaining buffer size, check if we still have enough free space for the terminating null
    {
      *buffer = '\0';
      return (size_t)0;
    }
    uintmax_t quot = value / base, digitval = value - quot * base; // get the value of the rightmost digit (avoid modulo, it would perform another slow division)
    value = quot;
    *end++ = charcodes[digitval]; // convert it to the representing character
  } while (value != UINTMAX_C(0)); // iterate as long as digits are leftover
  size_t length = (size_t)(end - buffer); // calculate the return value before we change the buffer and end pointers
  *end-- = '\0'; // assign the terminating null and decrease the pointer in order to point to the last digit written
  while (buffer < end) // reverse the digits (only the digits) in the array because they are LTR written but we need them in RTL order
  {
    char transfer = *end;
    *end-- = *buffer;
    *buffer++ = transfer;
  }
  return length;
}

size_t unsignedwcsconv_impl(uintmax_t value, wchar_t *buffer, size_t bufsize, int base)
{
  static const wchar_t charcodes[] = L"0123456789abcdefghijklmnopqrstuvwxyz";
  if (base < 2 || base > 36)
  {
    *buffer = L'\0';
    return (size_t)0;
  }
  wchar_t *end = buffer;
  do
  {
    if (bufsize-1 == (size_t)0)
    {
      *buffer = L'\0';
      return (size_t)0;
    }
    uintmax_t quot = value / base, digitval = value - quot * base;
    value = quot;
    *end++ = charcodes[digitval];
  } while (value != UINTMAX_C(0));
  size_t length = (size_t)(end - buffer);
  *end-- = L'\0';
  while (buffer < end)
  {
    wchar_t transfer = *end;
    *end-- = *buffer;
    *buffer++ = transfer;
  }
  return length;
}
