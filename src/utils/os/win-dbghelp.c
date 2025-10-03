

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "tlsdef.h"
#include "win-dbghelp.h"

static tlsvar DbgHelpFptr fptrs = {0};
static tlsvar HMODULE dbghelp = NULL;
static tlsvar HANDLE process = (HANDLE)NULL;

#define XSTR(x)       #x
#define STR(x)       XSTR(x)

static void printWinErrMsg(void)
{
   char* lpMsgBuf;
   DWORD dw = GetLastError();

   DWORD szMsg = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                FORMAT_MESSAGE_FROM_SYSTEM |
                                FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                (char*)&lpMsgBuf, 0, NULL);

   if (szMsg > 0) {
      (void)fprintf(stderr, "code: %ld; msg = %s", dw, lpMsgBuf);
      free(lpMsgBuf);
   } else {
      (void)fprintf(stderr, "code: %ld; no error message\n", dw);
   }
}

const DbgHelpFptr* dbghelp_get_fptrs(void)
{
   if (dbghelp) {
      return &fptrs;
   }

   dbghelp = LoadLibraryA("dbghelp.dll");

    if (dbghelp == NULL) {
        (void)fprintf(stderr, "[ERROR] Could not load dbghelp.dll\n");
        return NULL;
    }

   #define DBGHELP_OP(x) fptrs.DBGHELP_FIELD(x) = (DBGHELP_TYPE(x))GetProcAddress(dbghelp, STR(x));

   DBGHELP_FPTRS();
   #undef DBGHELP_OP

   #define DBGHELP_OP(x) { if (!fptrs.DBGHELP_FIELD(x)) { (void)fprintf(stderr, "[DbgHelp] ERROR: could not load" STR(x) "\n"); FreeLibrary(dbghelp); dbghelp = NULL; return NULL; }}
   DBGHELP_FPTRS();
   #undef DBGHELP_OP

   return &fptrs;
}

void cleanup_dbghelp_fptrs(void)
{
   if (process && fptrs.pSymCleanup) {
      fptrs.pSymCleanup(process);
   }

   if (dbghelp) {
      FreeLibrary(dbghelp);
      dbghelp = NULL;
   }
}

HANDLE dbghelp_get_process(void) {
   if (process) {
      return process;
   }

   const DbgHelpFptr* fptrs_ = dbghelp_get_fptrs();

   if (!fptrs_) {
      return (HANDLE)NULL;
   }

   process = GetCurrentProcess();
    if (!fptrs_->pSymInitialize(process, NULL, TRUE)) {
      (void)fprintf(stderr, "Error: SymInitialize failed: ");
      process = NULL;
      printWinErrMsg();
      return NULL;
    }

   return process;
}

#else

typedef int not_on_win32;

#endif
