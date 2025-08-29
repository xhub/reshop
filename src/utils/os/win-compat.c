
#if defined(_WIN32) && !defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <rpc.h>

#ifdef _MSC_VER
#pragma comment(lib, "rpcrt4.lib")
#endif

// for malloc
#include <stdlib.h>
#include <stdio.h>

#include "asprintf.h"
#include "option_priv.h"
#include "macros.h"
#include "printout.h"
#include "reshop.h"
#include "win-compat.h"

//static int __stdcall
BOOL APIENTRY
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);
BOOL APIENTRY
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
   switch (fdwReason) {
   case DLL_PROCESS_ATTACH:
      /* To support %n in printf,
       * see https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/set-printf-count-output?view=msvc-170 */
#ifndef __MSVCRT__
       _set_printf_count_output(1);
#endif
      rhp_syncenv();
      logging_syncenv();
      option_init();
      break;
   case DLL_THREAD_ATTACH:
   case DLL_THREAD_DETACH: break;
   case DLL_PROCESS_DETACH: 
         //fprintf(stderr, "[WIN] Detaching thread, calling all destructors!\n");
      cleanup_vrepr();
      cleanup_opcode_diff();
      cleanup_path();
      cleanup_gams();
//      cleanup_snan_funcs();
   }
   return TRUE;
}

char *strndup(const char *str, size_t chars)
{
   char *copy = (char *) malloc(sizeof(char) * (chars + 1));
   if (copy) {
      size_t n;
      for (n = 0; (n < chars) && (str[n] != '\0') ; n++) copy[n] = str[n];
      copy[n] = '\0';
   }

   return copy;
}

char *win_gettmpdir(void)
{
   unsigned path_len = 260;
   char *tempDir, *tmpdir;
   MALLOC_NULL(tempDir, char, path_len);

   DWORD result = GetTempPath(path_len, tempDir), dw;

   if (result == 0) {
      goto _err;
   } else if (result > path_len) {
      path_len = result + 1;
      REALLOC_NULL(tempDir, char, path_len);
      result = GetTempPath(path_len, tempDir);
      if (result == 0 || result > path_len) {
         goto _err;
      }
   }

   UUID uuid;
   switch (UuidCreate(&uuid)) {
   case RPC_S_OK:
   case RPC_S_UUID_LOCAL_ONLY:
      break;
   default:
      dw = GetLastError();
      error("[OS] ERROR: UuidCreate() triggered error %lu\n", dw);
      return NULL;
   }

   RPC_CSTR str;

   if (UuidToStringA(&uuid, &str) != RPC_S_OK) {
      dw = GetLastError();
      error("[OS] ERROR: UuidToStringA() triggered error %lu\n", dw);
      return NULL;
   }

   if (asprintf(&tmpdir, "%s" DIRSEP "reshop_%s", tempDir, str) < 0) {
      errormsg("%s ERROR: asprintf() failed!\n");
      RpcStringFree(&str);
      return NULL;
   }

   RpcStringFree(&str);

   return tmpdir;

_err:
   dw = GetLastError();
   error("[OS] ERROR: GetTempPath() triggered error %lu\n", dw);
   return NULL;
}



#else

/* because its' easier than trying to remove a source file in cmake */
typedef int make_iso_compilers_happy;

#endif /* defined(_WIN32) && defined(_MSC_VER)  */
