
#if defined(_WIN32) && !defined(__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <rpc.h>

#ifdef _MSC_VER
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "dbghelp.lib")
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

#ifndef GAMS_BUILD

static LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* pExceptionInfo)
{
   fatal_error("\nReSHOP caught an exception and will terminate. Please report this issue with the following information\n");

   char buf[256];
   char *codemsg;
   DWORD code = pExceptionInfo[0].ExceptionRecord->ExceptionCode;

   unsigned len = FormatMessageA(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
      NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf), NULL);

   if (len > 0) { codemsg = buf; } else { codemsg = "UNKNOWN ERROR"; }

   fatal_error("\nException code is %0lX '%s'", code, codemsg);

   if (pExceptionInfo) {
      fatal_error("\n--- Stack Trace ---\n");

      HANDLE process = GetCurrentProcess();
      HANDLE thread = GetCurrentThread();
      CONTEXT context;
      memcpy(&context, pExceptionInfo->ContextRecord, sizeof(CONTEXT));

      // Initialize the symbol handler
      SymInitialize(process, NULL, TRUE);

      STACKFRAME64 stack_frame;
      memset(&stack_frame, 0, sizeof(STACKFRAME64));

      stack_frame.AddrStack.Mode = AddrModeFlat;
      stack_frame.AddrFrame.Mode = AddrModeFlat;
      stack_frame.AddrPC.Mode = AddrModeFlat;

#if defined(_M_X64)
      stack_frame.AddrPC.Offset = context.Rip;
      stack_frame.AddrStack.Offset = context.Rsp;
      stack_frame.AddrFrame.Offset = context.Rbp;
#elif defined(_M_ARM64)
      stack_frame.AddrPC.Offset = context.Pc;
      stack_frame.AddrStack.Offset = context.Sp;
      stack_frame.AddrFrame.Offset = context.Fp;
#elif defined(_M_ARM)
      stack_frame.AddrPC.Offset = context.Pc;
      stack_frame.AddrStack.Offset = context.Sp;
      stack_frame.AddrFrame.Offset = context.R11;
#else
      stack_frame.AddrPC.Offset = context.Eip;
      stack_frame.AddrStack.Offset = context.Esp;
      stack_frame.AddrFrame.Offset = context.Ebp;
#endif

      DWORD machine_type;
#if defined(_M_X64)
      machine_type = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_ARM64)
      machine_type = IMAGE_FILE_MACHINE_ARM64;
#elif defined(_M_ARM)
      machine_type = IMAGE_FILE_MACHINE_ARMNT;
#else
      machine_type = IMAGE_FILE_MACHINE_I386;
#endif

      PSYMBOL_INFO symbol_info = (PSYMBOL_INFO)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
      unsigned i = 0;
      while (StackWalk64(
          machine_type,
          process,
          thread,
          &stack_frame,
          &context,
          NULL,
          SymFunctionTableAccess64,
          SymGetModuleBase64,
          NULL
      )) {
          // Get the function name and offset
          symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
          symbol_info->MaxNameLen = 255;
          DWORD64 displacement = 0;

         if (stack_frame.AddrPC.Offset == 0) {
            break;
         }

          if (SymFromAddr(process, stack_frame.AddrPC.Offset, &displacement, symbol_info)) {
              fatal_error("%u %s+0x%llX\n", i, symbol_info->Name, displacement);
          } else {
              fatal_error("%u  Address: 0x%llx (Symbol not found)\n", i, stack_frame.AddrPC.Offset);
          }

          // Break out of the loop if the stack walk is complete
          if (stack_frame.AddrFrame.Offset == 0 || stack_frame.AddrReturn.Offset == 0 ||
              stack_frame.AddrPC.Offset == stack_frame.AddrReturn.Offset) {
              break;
          }
         i++;
      }

      free(symbol_info);
       SymCleanup(process);
   } else {
      fatal_error("\n no exception information ...");
   }

    // Open a file to write the minidump to.
    HANDLE hFile = CreateFileA(
        "reshop-minidump.dmp",
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        // Define the information to be included in the minidump.
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = pExceptionInfo;
        mei.ClientPointers = FALSE;

        // Write the minidump to the file.
        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            MiniDumpWithFullMemory, // This flag specifies a detailed dump.
            &mei,
            NULL,
            NULL
        );
        CloseHandle(hFile);

        fatal_error("\nA minidump was saved to: %s\n", "reshop-minidump.dmp");
    } else {
        fatal_error("\nFailed to create minidump file. Error code: %lu\n", GetLastError());
    }

    // Terminate the process. Returning EXCEPTION_EXECUTE_HANDLER
    // will prevent the default Windows crash dialog from appearing.
    return EXCEPTION_CONTINUE_SEARCH;
}

#endif

//static int __stdcall
BOOL APIENTRY
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);
BOOL APIENTRY
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
   switch (fdwReason) {
   case DLL_PROCESS_ATTACH:

#ifndef GAMS_BUILD
      SetUnhandledExceptionFilter(UnhandledExceptionHandler);
#endif

      /* To support %n in printf,
       * see https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/set-printf-count-output?view=msvc-170 */
      /* mingw with msvcrt does not define this function */
#if !defined(__USE_MINGW_ANSI_STDIO) || (__USE_MINGW_ANSI_STDIO == 0)
       _set_printf_count_output(1);
#endif
      option_init();
      rhp_syncenv();
      debug("[OS] DllMain called with DLL_PROCESS_ATTACH\n");
      logging_syncenv();
      debug("[OS] Call to DllMain with DLL_PROCESS_ATTACH successful\n");

      break;

   default:
   case DLL_THREAD_ATTACH:
   case DLL_THREAD_DETACH: break;

   case DLL_PROCESS_DETACH: 
         //fprintf(stderr, "[WIN] Detaching thread, calling all destructors!\n");
      cleanup_vrepr();
      cleanup_opcode_diff();
      cleanup_path();
      cleanup_gams();
      debug("[OS] Call to DllMain with DLL_PROCESS_DETACH successful\n");
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

static const char unknown_err[] = "unknown error";

unsigned win_strerror(unsigned sz, char buf[VMT(static sz)], const char **msg)
{
   DWORD err = GetLastError();
   unsigned length42 = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, err, 0, buf, sz, NULL);

   if (length42 > 0) { *msg = buf; } else { *msg = unknown_err; }

   return err;
}


#else

/* because its' easier than trying to remove a source file in cmake */
typedef int make_iso_compilers_happy;

#endif /* defined(_WIN32) && defined(_MSC_VER)  */
