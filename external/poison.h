/* Copyright 2013 - Leaf Security Research
http://leafsr.com

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

poison.h - A C header file for poisoning unsafe C/C++
functions. This is far from complete, you will need to
add your own in-house deprecated and insecure APIs for
it to be very effective */

#ifndef _POISON_H
#define _POISON_H

#ifdef __GNUC__

#define MYprintf printf
#define MYsprintf sprintf
/* String handling functions */
#	pragma GCC poison strcpy wcscpy stpcpy wcpcpy
#	pragma GCC poison scanf sscanf vscanf fwscanf swscanf wscanf
#	pragma GCC poison gets puts
#	pragma GCC poison strcat wcscat
#	pragma GCC poison wcrtomb wctob
#	pragma GCC poison sprintf vsprintf vfprintf
#	pragma GCC poison asprintf vasprintf
#	pragma GCC poison strncpy wcsncpy
#	pragma GCC poison strtok wcstok
#	pragma GCC poison strdupa strndupa

/* Signal related */
#	pragma GCC poison longjmp siglongjmp
#	pragma GCC poison setjmp sigsetjmp

/* Memory allocation */
#	pragma GCC poison alloca
#	pragma GCC poison mallopt

/* File API's */
#	pragma GCC poison remove
#	pragma GCC poison mktemp tmpnam tempnam
#	pragma GCC poison getwd

/* Misc */
#	pragma GCC poison getlogin getpass cuserid
#	pragma GCC poison rexec rexec_af

/* Your custom insecure APIs here */
#pragma GCC poison printf
#pragma GCC poison malloc realloc free
#pragma GCC poison puts

#endif

#endif
