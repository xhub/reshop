#ifndef WIN_DBGHELP_H
#define WIN_DBGHELP_H

#include <basetsd.h>
#include <windef.h>
#include <winver.h>
#include <processthreadsapi.h>
#include <dbghelp.h>

#ifndef WINBOOL
#define WINBOOL BOOL
#endif

typedef WINBOOL (*pSymInitialize_t)(HANDLE hProcess,PCSTR UserSearchPath,WINBOOL fInvadeProcess);
typedef DWORD   (*pSymGetOptions_t)(void);
typedef DWORD   (*pSymSetOptions_t)(DWORD);
typedef WINBOOL (*pSymFromAddr_t)(HANDLE hProcess,DWORD64 Address,PDWORD64 Displacement,PSYMBOL_INFO Symbol);
typedef DWORD64 (*pSymLoadModule64_t)(HANDLE hProcess,HANDLE hFile,PCSTR ImageName,PCSTR ModuleName,DWORD64 BaseOfDll,DWORD SizeOfDll);
typedef WINBOOL (*pSymGetModuleInfo64_t)(HANDLE hProcess,DWORD64 qwAddr,PIMAGEHLP_MODULE64 ModuleInfo);
typedef WINBOOL (*pSymGetLineFromAddr64_t)(HANDLE hProcess,DWORD64 qwAddr,PDWORD pdwDisplacement,PIMAGEHLP_LINE64 Line64);
typedef WINBOOL (*pSymCleanup_t)(HANDLE hProcess);
typedef WINBOOL (*pStackWalk64_t)(DWORD MachineType,HANDLE hProcess,HANDLE hThread,LPSTACKFRAME64 StackFrame,PVOID ContextRecord,PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,PGET_MODULE_BASE_ROUTINE64 
GetModuleBaseRoutine,PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress);
typedef PVOID    (*pSymFunctionTableAccess64_t)(HANDLE hProcess,DWORD64 AddrBase);
typedef DWORD64  (*pSymGetModuleBase64_t)(HANDLE hProcess,DWORD64 qwAddr);
typedef WINBOOL  (*pMiniDumpWriteDump_t)(HANDLE hProcess,DWORD ProcessId,HANDLE hFile,MINIDUMP_TYPE DumpType,CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

#define DBGHELP_FPTRS() \
DBGHELP_OP(SymInitialize) \
DBGHELP_OP(SymGetOptions) \
DBGHELP_OP(SymSetOptions) \
DBGHELP_OP(SymFromAddr) \
DBGHELP_OP(SymLoadModule64) \
DBGHELP_OP(SymGetModuleInfo64) \
DBGHELP_OP(SymGetLineFromAddr64) \
DBGHELP_OP(SymCleanup) \
DBGHELP_OP(StackWalk64) \
DBGHELP_OP(SymFunctionTableAccess64) \
DBGHELP_OP(SymGetModuleBase64) \
DBGHELP_OP(MiniDumpWriteDump) \


#define DBGHELP_TYPE(x) p ##x ## _t
#define DBGHELP_FIELD(x) p##x

#define DBGHELP_OP(x) DBGHELP_TYPE(x) DBGHELP_FIELD(x) ;
typedef struct {
   DBGHELP_FPTRS()
} DbgHelpFptr;

#undef DBGHELP_OP

const DbgHelpFptr * dbghelp_get_fptrs(void);
HANDLE dbghelp_get_process(void);
void cleanup_dbghelp_fptrs(void);

#endif /* WIN_DBGHELP_H */
