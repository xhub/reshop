#include "asprintf.h"

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "crash_reporter.h"
#include "embcode_empinfo.h"
#include "fs_func.h"
#include "get_uuid_library.h"
#include "git_version.h"
#include "macros.h"
#include "mdl_gams.h"
#include "open_lib.h"
#include "printout.h"
#include "reshop_priv.h"
#include "tlsdef.h"

// Maximum size and limits for the report
#define MAX_FRAMES 100
#define MAX_FRAME_LEN 256

#define WIDEN(x)     L##x
#define _W(x)        WIDEN(x)

#define XSTR(x)       #x
#define STR(x)       XSTR(x)

#define API_USERAGENT     "reshop-crash-reporter/1.0"

#ifdef CRASH_REPORTER_LOCAL

#define API_HOST          "127.0.0.1"
#define API_PORT          5000
#define API_PROTO         "http://"

#define CUSTOM_OPEN_REQUEST_FLAGS 0

#else

#define API_HOST          "crash-reporter.reshop.eu"
#define API_PORT          443
#define API_PROTO         "https://"

#define CUSTOM_OPEN_REQUEST_FLAGS WINHTTP_FLAG_SECURE

#endif

#define CRASH_SERVER      API_PROTO API_HOST ":" STR(API_PORT)



#define REPORT_ENDPOINT   "/api/v1/crash_report"

#define REPORT_URL        CRASH_SERVER REPORT_ENDPOINT

#define UPLOAD_ENDPOINT   "/api/v1/"

typedef union {
   u8 uuid[16];
   struct {
      u32 seg1;
      u16 seg2;
      u16 seg3;
      u16 seg4;
      u16 seg5[3];
   };
} Uuidv7;

#define u8SCN "%02"SCNx8
#define u8PRI "%02"SCNx8

#define UUID_PRINT_FMT u8PRI u8PRI u8PRI u8PRI "-" u8PRI u8PRI "-" u8PRI u8PRI "-" u8PRI u8PRI "-" u8PRI u8PRI u8PRI u8PRI u8PRI u8PRI
#define UUID_FMTARGS(uuid) \
uuid.uuid[0], uuid.uuid[1], uuid.uuid[2], uuid.uuid[3], uuid.uuid[4], uuid.uuid[5],   \
uuid.uuid[6], uuid.uuid[7], uuid.uuid[8], uuid.uuid[9], uuid.uuid[10], uuid.uuid[11], \
uuid.uuid[12], uuid.uuid[13], uuid.uuid[14], uuid.uuid[15]

typedef struct {
   const char *gevOptStr;
   const char *upload_name;
} GamsFile;

typedef struct {
   const char *fname;
   const char *upload_name;
} ScrDirFile;

static const GamsFile gams_files_upload[] = {
   {gevNameMatrix,  "matrix"},
   {gevNameInstr,   "nlinstr"},
   {gevNameLogFile, "log"},
   {gevNameStaFile, "status"},
};

static const ScrDirFile empinfo_upload[] = {
   {"empinfo.dat", "empinfo"},
   {EMBCODE_GMDOUT_FNAME, EMBCODE_GMDOUT_FNAME_NOEXT}
};

static void print_upload_stats(double total_time, u64 speed_upload)
{
   char *prefix;
   double speed;
   if (speed_upload >= (1 << 30)) {
      speed = speed_upload / (1 << 30);
      prefix = "GB";
   } else if (speed_upload >= (1 << 20)) {
      speed = speed_upload / (1 << 20);
      prefix = "MB";
   } else if (speed_upload >= (1 << 10)) {
      speed = speed_upload / (1 << 10);
      prefix = "kB";
   } else {
      speed = speed_upload;
      prefix = "B";
   }

   fatal_error(" Done in %.1f s: speed was %.1f %s/s\n", total_time, speed, prefix);

}


#ifdef _WIN32

#if defined(__GNUC__)
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Wformat"
   #pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <winbase.h>
#include <dbghelp.h>
#include <tchar.h>

#include "win-dbghelp.h"

#ifdef _MSC_VER
#include <io.h>
#define ftello _ftelli64
#define isatty _isatty
#if !defined(STDERR_FILENO)
#define STDERR_FILENO _fileno( stderr )
#endif
#endif

#ifdef RHP_COMPILER_CL
#define stat _stat
#endif

#define CHUNK_SIZE 4096

#ifndef WINBOOL
#define WINBOOL BOOL
#endif

typedef int (*RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

typedef WINBOOL (*pWinHttpCloseHandle_t)(HINTERNET);

typedef HINTERNET (*pWinHttpConnect_t)(
  HINTERNET     hSession,
  LPCWSTR       pswzServerName,
  INTERNET_PORT nServerPort,
  DWORD         dwReserved
);

typedef HINTERNET (*pWinHttpOpen_t)(
  LPCWSTR pszAgentW,
  DWORD   dwAccessType,
  LPCWSTR pszProxyW,
  LPCWSTR pszProxyBypassW,
  DWORD   dwFlags
);

typedef HINTERNET (*pWinHttpOpenRequest_t)(
  HINTERNET hConnect,
  LPCWSTR   pwszVerb,
  LPCWSTR   pwszObjectName,
  LPCWSTR   pwszVersion,
  LPCWSTR   pwszReferrer,
  LPCWSTR   *ppwszAcceptTypes,
  DWORD     dwFlags
);

typedef BOOL (*pWinHttpQueryHeaders_t)(
  HINTERNET hRequest,
  DWORD     dwInfoLevel,
  LPCWSTR   pwszName,
  LPVOID    lpBuffer,
  LPDWORD   lpdwBufferLength,
  LPDWORD   lpdwIndex
);

typedef BOOL (*pWinHttpReceiveResponse_t)(
  HINTERNET hRequest,
  LPVOID    lpReserved
);

typedef BOOL (*pWinHttpSendRequest_t)(
  HINTERNET hRequest,
  LPCWSTR   lpszHeaders,
  DWORD     dwHeadersLength,
  LPVOID    lpOptional,
  DWORD     dwOptionalLength,
  DWORD     dwTotalLength,
  DWORD_PTR dwContext
);

typedef BOOL (*pWinHttpQueryDataAvailable_t)(
  HINTERNET hRequest,
  LPDWORD   lpdwNumberOfBytesAvailable
);

typedef BOOL (*pWinHttpReadData_t)(
  HINTERNET hRequest,
  LPVOID    lpBuffer,
  DWORD     dwNumberOfBytesToRead,
  LPDWORD   lpdwNumberOfBytesRead
);

typedef WINBOOL (*pWinHttpWriteData_t)(HINTERNET,LPCVOID,DWORD,LPDWORD);

#define WINHTTP_FPTRS() \
WINHTTP_OP(WinHttpOpen) \
WINHTTP_OP(WinHttpConnect) \
WINHTTP_OP(WinHttpOpenRequest) \
WINHTTP_OP(WinHttpSendRequest) \
WINHTTP_OP(WinHttpReceiveResponse) \
WINHTTP_OP(WinHttpQueryHeaders) \
WINHTTP_OP(WinHttpQueryDataAvailable) \
WINHTTP_OP(WinHttpReadData) \
WINHTTP_OP(WinHttpWriteData) \
WINHTTP_OP(WinHttpCloseHandle)

#define WINHTTP_TYPE(x) p ##x ## _t
#define WINHTTP_FIELD(x) p##x

#define WINHTTP_OP(x) WINHTTP_TYPE(x) WINHTTP_FIELD(x) ;
typedef struct {
   WINHTTP_FPTRS()
} WinHttpFptr;

#undef WINHTTP_OP


  // Function to gather Windows OS name and architecture
static void get_os_info(const char** os_name, const char **os_version, const char** os_arch)
{
   static tlsvar char os_version_str[128];
   *os_version = os_version_str;

   // 1. OS Name
   *os_name = "Windows";

   // 2. Architecture
#if defined(_M_AMD64)
   *os_arch = "x86_64";
#elif defined(_M_IX86)
   *os_arch = "i686";
#elif defined(_M_ARM64)
   *os_arch = "ARM64";
#elif defined(_M_ARM)
   *os_arch = "ARM";
#else
   *os_arch = "UNKNOWN";
#endif

   // 3. Version
   RTL_OSVERSIONINFOEXW osvi;
   ZeroMemory(&osvi, sizeof(RTL_OSVERSIONINFOEXW));
   osvi.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);
   osvi.szCSDVersion[0] = '\0';

   HMODULE hMod = GetModuleHandleA("ntdll.dll");
   if (hMod) {

      RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)GetProcAddress(hMod, "RtlGetVersion");

      if (fxPtr) {
         fxPtr((RTL_OSVERSIONINFOW*)&osvi);

         (void)snprintf(os_version_str, sizeof(os_version_str), "%ld.%ld %ld",
                  osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);

         return;

         /* Just for reference 
         printf("DEBUG: dwMajorVersion = %d\n", osvi.dwMajorVersion);
         printf("DEBUG: dwMinorVersion = %d\n", osvi.dwMinorVersion);
         printf("DEBUG: dwBuildNumber = %d\n", osvi.dwBuildNumber);
         printf("DEBUG: szCSDVersion = %s\n", osvi.szCSDVersion);
         printf("DEBUG: wServicePackMajor = %d\n", osvi.wServicePackMajor);
         printf("DEBUG: wServicePackMinor = %d\n", osvi.wServicePackMinor);
         */
      }

      FreeLibrary(hMod);
   }


   *os_version = "unknown version";
}

static const char* get_backtrace_json(bool *free_backtrace_json)
{
   static tlsvar char json_output_buf[MAX_FRAMES*(MAX_FRAME_LEN+2)+2];
   char frame_info[MAX_FRAME_LEN];
   // We need to save space for the final ']'
   #define MAXLEN(x) (sizeof(json_output_buf)-2-(x))
   unsigned       i;
   void         * stack[MAX_FRAMES];
   unsigned short nframes;
   SYMBOL_INFO  * symbol;
   HANDLE         process;

   /* We never allocate here */
   *free_backtrace_json = false;

   const DbgHelpFptr *fptrs = dbghelp_get_fptrs();
   if (!fptrs) { return NULL; }

   process = dbghelp_get_process();
   if (!process) { return NULL; }

   DWORD symOptions = fptrs->pSymGetOptions();
   symOptions |= SYMOPT_LOAD_LINES | SYMOPT_UNDNAME;
   fptrs->pSymSetOptions(symOptions);

   nframes              = RtlCaptureStackBackTrace(0, MAX_FRAMES, stack, NULL);
   symbol               = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + MAX_FRAME_LEN * sizeof(char), 1);
   symbol->MaxNameLen   = MAX_FRAME_LEN-1;
   symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
   IMAGEHLP_LINE64 line;
   line.SizeOfStruct    = sizeof(IMAGEHLP_LINE64);

   char *json_output = json_output_buf;
   json_output[0] = '[';
   size_t curlen = 1;

   for (i = 0; i < nframes; i++) {

      if (!fptrs->pSymFromAddr( process, ( DWORD64 )( stack[ i ] ), 0, symbol )) {
         char* lpMsgBuf;
         DWORD dw = GetLastError();

         DWORD szMsg = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                      FORMAT_MESSAGE_FROM_SYSTEM |
                                      FORMAT_MESSAGE_IGNORE_INSERTS,
                                      NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      (char*)&lpMsgBuf, 0, NULL);

         if (szMsg > 0) {

            if (lpMsgBuf[szMsg-1] == '\n') { lpMsgBuf[szMsg-2] = '\0'; } /* \r\n */

            (void)snprintf(frame_info, sizeof(frame_info), "%u 0x%p (%s)", nframes-i-1,
                           stack[i], lpMsgBuf);
            LocalFree(lpMsgBuf);

         } else {

            (void)snprintf(frame_info, sizeof(frame_info), "%u 0x%p (UNKNOWN ERROR)",
                           nframes-i-1, stack[i]);

         }
 
      } else {

         /* Try to get the line number */
         if (fptrs->pSymGetLineFromAddr64(process, (DWORD64)stack[i], 0, &line)) {
            (void)snprintf(frame_info, sizeof(frame_info), "%u %s (%s:%lu)", nframes-i-1,
                           symbol->Name, line.FileName, (unsigned long)line.LineNumber);
         } else {
            (void)snprintf(frame_info, sizeof(frame_info), "%u %s+0x%0llX", nframes-i-1,
                           symbol->Name, symbol->Address);
         }
      }

      // Basic JSON string escaping (handles quotes and backslashes)
      char escaped_frame[MAX_FRAME_LEN * 2];
      size_t j = 0;
      for (size_t k = 0; k < strlen(frame_info) && j < (MAX_FRAME_LEN * 2) - 3; k++) {
         if (frame_info[k] == '"' || frame_info[k] == '\\') {
            escaped_frame[j++] = '\\';
         }
         escaped_frame[j++] = frame_info[k];
      }
      escaped_frame[j] = '\0';

      curlen += snprintf(json_output+curlen, MAXLEN(curlen), "\"%s\"%s", escaped_frame,
                         (i < nframes - 1) ? "," : "");

      if (curlen >= sizeof(json_output_buf)-2) break;
   }

   size_t finalcharpos = curlen < sizeof(json_output_buf)-2 ? curlen : sizeof(json_output_buf)-2;
   json_output[finalcharpos]   = ']';
   json_output[finalcharpos+1] = '\0';

   free(symbol);

   return json_output_buf;
}

// Helper function to print WinHTTP error messages.
static void PrintWinHttpError(const char* functionName) {
    DWORD errorCode = GetLastError();
    LPWSTR fatal_error_msg = NULL;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   errorCode,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&fatal_error_msg,
                   0,
                   NULL);
   fatal_error("[ERROR] in %s (%lu)", functionName, errorCode);
   if (fatal_error_msg) {
      fatal_error(": %S\n", fatal_error_msg);
      LocalFree(fatal_error_msg);
   } else {
      fatal_error("\n");
   }
}

UNUSED static bool upload_file_via_winhttp(HINTERNET hConnect, WinHttpFptr * restrict fptrs,
                                           ScrDirFile *dat, Uuidv7 uuid)
{
   char *endpoint = NULL;
   wchar_t *endpoint_w = NULL;
   FILE *file_handle = NULL;
   HINTERNET hSession = NULL, hRequest = NULL;

   if (!file_readable_silent(dat->fname)) {
      fatal_error("Skipping '%s' as it does not exists or is not readable.\n", dat->fname);
      return true;
   }

   // 2. Open the file for reading in binary mode.
   file_handle = fopen(dat->fname, "rb");
   if (!file_handle) {
      perror("Error opening file for upload");
      goto _exit;
   }

   // 1. Get file size.
   if (fseek(file_handle, 0, SEEK_END) != 0) {
      perror("Error seeking to end of file");
      (void)fclose(file_handle);
      goto _exit;
   }

   u64 file_size = (u64)ftello(file_handle);

   if (file_size == (u64)-1) {
      perror("Error getting file size");
      (void)fclose(file_handle);
      goto _exit;
   }

   if (fseek(file_handle, 0, SEEK_SET) != 0) {
      perror("Error rewinding file");
      (void)fclose(file_handle);
      goto _exit;
   }

   int rc = asprintf(&endpoint, UPLOAD_ENDPOINT UUID_PRINT_FMT "/%s",
                     UUID_FMTARGS(uuid), dat->upload_name);

   if (rc >= 0) {
      endpoint_w = malloc(rc+1);

      size_t convertedChars = 0;
      (void)mbstowcs_s(&convertedChars, endpoint_w, rc+1, endpoint, _TRUNCATE);

   } else {
      fatal_error("[ERROR] Could not create endpoint URL\n");
      goto _exit;
   }

   hRequest = (fptrs->pWinHttpOpenRequest)(hConnect, L"POST", endpoint_w , NULL,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           CUSTOM_OPEN_REQUEST_FLAGS);

   if (!hRequest) {
      fatal_error("[WinHttp] ERROR: could not create request to endpoint\n");
      goto _exit;
   }

   const wchar_t ContentType[] = L"Content-Type: application/octet-stream";
   if (!(fptrs->pWinHttpSendRequest)(hRequest, ContentType, -1L,
                                     WINHTTP_NO_REQUEST_DATA, 0, (DWORD)file_size, 0)) {
      PrintWinHttpError("WinHttpSendRequest");
      fatal_error("[WinHttp] ERROR: could not send data\n");
      goto _exit;
   }

   fatal_error("Uploading '%s' (%zu bytes).", dat->fname, (size_t)file_size);

   // Start timing the upload.
   LARGE_INTEGER frequency, start, end;
   QueryPerformanceFrequency(&frequency);
   QueryPerformanceCounter(&start);

   // 7. Read the file and send it in chunks.
   char chunk_buffer[CHUNK_SIZE];
   size_t bytes_read = 0;
   while ((bytes_read = fread(chunk_buffer, 1, CHUNK_SIZE, file_handle)) > 0) {

      DWORD dwBytesWritten;
      if(!fptrs->pWinHttpWriteData(hRequest, chunk_buffer, bytes_read, &dwBytesWritten)) {
         PrintWinHttpError("WinHttpWriteData");
         goto _exit;
      }

   }

   // Stop timing the upload.
   QueryPerformanceCounter(&end);
   double time_taken = (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart;
   double speed_upload_bps = (time_taken > 0) ? (file_size / time_taken) : 0;

   // Finalize the request and receive the response.
   if(!fptrs->pWinHttpReceiveResponse(hRequest, NULL)) {
      PrintWinHttpError("WinHttpReceiveResponse");
      goto _exit;
   }

   // Get the HTTP response code.
   DWORD dwStatusCode = 0;
   DWORD dwSize = sizeof(dwStatusCode);
   if (!fptrs->pWinHttpQueryHeaders(hRequest,
                                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                    WINHTTP_HEADER_NAME_BY_INDEX,
                                    &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX)) {
      PrintWinHttpError("WinHttpQueryHeaders");
   } else {
      if (dwStatusCode == 200) {

         print_upload_stats(time_taken, (u64)speed_upload_bps);

      } else {
         char *response = NULL;
         if (fptrs->pWinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
            response = malloc (dwSize+1);
            DWORD dwDownloaded;
            if (response && fptrs->pWinHttpReadData(hRequest, response, dwSize, &dwDownloaded)) {
               response[dwDownloaded] = '\0';
            }
         }

         fatal_error("\n[ERROR] File '%s' was not sent successfully: the response code was %ld (should be 200). "
                     "The server response was:\n'%s'\n", dat->fname, dwStatusCode, response);

      }
   }

   if (hRequest) (fptrs->pWinHttpCloseHandle)(hRequest);
   if (hSession) (fptrs->pWinHttpCloseHandle)(hSession);

   return true;

_exit:
   if (hRequest) (fptrs->pWinHttpCloseHandle)(hRequest);
   if (hSession) (fptrs->pWinHttpCloseHandle)(hSession);

   free(endpoint);
   free(endpoint_w);
   (void)fclose(file_handle);

   return false;
}

static void send_model_instance_via_winhttp(Uuidv7 uuid)
{
   gevHandle_t gev = gmdl_get_src_gev();
   if (!gev) {
      fatal_error("\n[ERROR] Could not get initial GEV, stopping model instance upload\n");
      return;
   }

   HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
   HMODULE winhttp = LoadLibraryA("winhttp.dll");
   if (!winhttp) {
      fatal_error("[WinHttp] ERROR: could not load 'winhttp.dll'\n");
      return;
   }

   WinHttpFptr fptrs = {0};
   #define WINHTTP_OP(x) fptrs.WINHTTP_FIELD(x) = (WINHTTP_TYPE(x))GetProcAddress(winhttp, STR(x));

   WINHTTP_FPTRS();
   #undef WINHTTP_OP

   #define WINHTTP_OP(x) { if (!fptrs.WINHTTP_FIELD(x)) { fatal_error("[WinHttp] ERROR: could not load" STR(x) "\n"); return; }}
   WINHTTP_FPTRS();
   #undef WINHTTP_OP


   hSession = (fptrs.pWinHttpOpen)(L"" API_USERAGENT,
                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME,
                             WINHTTP_NO_PROXY_BYPASS, 0);

   if (!hSession) {
      fatal_error("[WinHttp] ERROR: could not create session\n");
      return;
   }

   hConnect = (fptrs.pWinHttpConnect)(hSession, _W(API_HOST), API_PORT, 0);
   if (!hConnect) {
      fatal_error("[WinHttp] ERROR: could not create connect to " API_HOST ":"STR(API_PORT)"\n");
      goto _exit;
   }

   for (unsigned i = 0, len = ARRAY_SIZE(gams_files_upload); i < len; ++i) {
      char buf[GMS_SSSIZE];
      ScrDirFile uploadDat;
      uploadDat.fname = gevGetStrOpt(gev, gams_files_upload[i].gevOptStr, buf);
      uploadDat.upload_name = gams_files_upload[i].upload_name;

      upload_file_via_winhttp(hConnect, &fptrs, &uploadDat, uuid);
   }

   for (unsigned i = 0, len = ARRAY_SIZE(empinfo_upload); i < len; ++i) {
      char buf[GMS_SSSIZE];
      ScrDirFile uploadDat;
      int rc = asprintf((char**)&uploadDat.fname, "%s%s", gevGetStrOpt(gev, gevNameScrDir, buf),
               empinfo_upload[i].fname);

      if (rc == -1) { 
         fatal_error("[ERROR] Could not create path to empinfo file '%s' with scrdir '%s'\n",
                     empinfo_upload[i].fname, buf);
         continue;
      }

      uploadDat.upload_name = empinfo_upload[i].upload_name;

      upload_file_via_winhttp(hConnect, &fptrs, &uploadDat, uuid);

      free((char*)uploadDat.fname);
   }


_exit:
   if (hRequest) (fptrs.pWinHttpCloseHandle)(hRequest);
   if (hConnect) (fptrs.pWinHttpCloseHandle)(hConnect);
   if (hSession) (fptrs.pWinHttpCloseHandle)(hSession);

   FreeLibrary(winhttp);
}

static void send_crash_report_via_winhttp(const char *json_report, u8 uuid[16])
{
   HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
   HMODULE winhttp = LoadLibraryA("winhttp.dll");
   if (!winhttp) {
      fatal_error("[WinHttp] ERROR: could not load 'winhttp.dll'\n");
      return;
   }

   WinHttpFptr fptrs;
   #define WINHTTP_OP(x) fptrs.WINHTTP_FIELD(x) = (WINHTTP_TYPE(x))GetProcAddress(winhttp, STR(x));

   WINHTTP_FPTRS();
   #undef WINHTTP_OP

   #define WINHTTP_OP(x) { if (!fptrs.WINHTTP_FIELD(x)) { fatal_error("[WinHttp] ERROR: could not load" STR(x) "\n"); return; }}
   WINHTTP_FPTRS();
   #undef WINHTTP_OP


   hSession = (fptrs.pWinHttpOpen)(L"" API_USERAGENT,
                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME,
                             WINHTTP_NO_PROXY_BYPASS, 0);

   if (!hSession) {
      fatal_error("[WinHttp] ERROR: could not create session\n");
      return;
   }

   hConnect = (fptrs.pWinHttpConnect)(hSession, _W(API_HOST), API_PORT, 0);
   if (!hConnect) {
      fatal_error("[WinHttp] ERROR: could not create connect to " API_HOST ":"STR(API_PORT)"\n");
      goto _exit;
   }


   hRequest = (fptrs.pWinHttpOpenRequest)(hConnect, L"POST", L"" REPORT_ENDPOINT, NULL,
                                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                 CUSTOM_OPEN_REQUEST_FLAGS);
   if (!hRequest) {
      fatal_error("[WinHttp] ERROR: could not create request to endpoint\n");
      goto _exit;
   }

   DWORD sz = strlen(json_report);
   const wchar_t ContentType[] = L"Content-Type:  application/json";
   if (!(fptrs.pWinHttpSendRequest)(hRequest, ContentType, -1L, (void*)json_report, sz, sz, 0)) {
      fatal_error("[WinHttp] ERROR: could not send data\n");
      goto _exit;
   }

   if (!(fptrs.pWinHttpReceiveResponse)(hRequest, NULL)) {
      fatal_error("[WinHttp] ERROR: could not get server response\n");
      goto _exit;
   }

   DWORD dwStatusCode, dwSize = sizeof(dwStatusCode);
   if (!(fptrs.pWinHttpQueryHeaders)(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            NULL, &dwStatusCode, &dwSize, NULL)) {
      fatal_error("[WinHttp] ERROR: could not query header in response\n");
      goto _exit;
   }
 
   if (dwStatusCode != 201) {
      fatal_error("[WinHttp] ERROR: expected response code to be 201, got %lu\n", dwStatusCode);
   } else {
      char *response = NULL;
      bool printed = false;

      /* Lazy way of downloading, we assume that we can get the json answer in 1 shot */
      if (fptrs.pWinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
         response = malloc (dwSize+1);
         DWORD dwDownloaded;
         if (response && fptrs.pWinHttpReadData(hRequest, response, dwSize, &dwDownloaded)) {
            response[dwDownloaded] = '\0';
            fatal_error("\n[INFO] Crash report sent successfully. Response was: %s\n", response);
            printed = true;
         }
      }

      /* Query the Location */
      WCHAR locationHeader[37];
      DWORD dwBufferLength = sizeof(locationHeader);

      if (!fptrs.pWinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                               locationHeader, &dwBufferLength, WINHTTP_NO_HEADER_INDEX)) {
         fatal_error("\n[ERROR] Could not query location of the crash report\n");
         memset(uuid, 0, sizeof(*uuid));
         goto _exit;
      } else {

         int res = swscanf(locationHeader, L"" u8SCN u8SCN u8SCN u8SCN "-" u8SCN u8SCN "-" u8SCN u8SCN "-" u8SCN u8SCN "-" 
                           u8SCN u8SCN u8SCN u8SCN u8SCN u8SCN,
                           &uuid[0], &uuid[1], &uuid[2], &uuid[3], &uuid[4], &uuid[5], &uuid[6],
                           &uuid[7], &uuid[8], &uuid[9], &uuid[10], &uuid[11], &uuid[12], &uuid[13],
                           &uuid[14], &uuid[15]);

         if (res != 16) {
            fatal_error("\n[ERROR] Parsing 'Location' HTTP Header value '%S' failed\n", locationHeader);
            memset(uuid, 0, sizeof(*uuid));
            goto _exit;
         }
      }

      if (!printed) {
         fatal_error("\n[INFO] Crash report sent successfully. Could not get server answer.\n");
      }
   }

_exit:
   if (hRequest) (fptrs.pWinHttpCloseHandle)(hRequest);
   if (hConnect) (fptrs.pWinHttpCloseHandle)(hConnect);
   if (hSession) (fptrs.pWinHttpCloseHandle)(hSession);

   FreeLibrary(winhttp);
}

#else /* NOT _WIN32 */

  #include <unistd.h>
  #include <sys/utsname.h>
  #include <execinfo.h> // For backtrace
  #include <limits.h>
  #include <dlfcn.h>


  // Function to gather POSIX OS name and architecture
  static void get_os_info(const char **os_name, const char **os_version, const char **os_arch)
{
    static struct utsname u_info;
    static const char os_name_failure[] = "Unknown POSIX OS";
    static const char os_version_failure[] = "Unknown version";
    static const char os_arch_failure[] = "Unknown arch";

   if (uname(&u_info) == 0) {
      *os_name = u_info.sysname;
      *os_version = u_info.release;
      *os_arch = u_info.machine;

    } else {

       *os_name = os_name_failure;
       *os_version = os_version_failure;
       *os_arch = os_arch_failure;
    }
  }

  // Function to get backtrace as a JSON array string
static const char* construct_backtrace_json_static(int nframes, char* frames[VMT(restrict static nframes)])
{
   #define BACKTRACE_JSON_SIZE 8192
   static tlsvar char json_output[BACKTRACE_JSON_SIZE];
   #define MAXLEN(x)                   (sizeof(json_output) - 2 - (x))
   size_t current_len = 1;
   json_output[0] = '[';

   for (int i = 0; i < nframes; i++) {
      // Basic JSON string escaping (handles quotes and backslashes)
      char escaped_frame[MAX_FRAME_LEN * 2];
      size_t j = 0;
      for (size_t k = 0; k < strlen(frames[i]) && j < (MAX_FRAME_LEN * 2) - 3; k++) {
         if (frames[i][k] == '"' || frames[i][k] == '\\') {
            escaped_frame[j++] = '\\';
         }
         escaped_frame[j++] = frames[i][k];
      }
      escaped_frame[j] = '\0';

      current_len += snprintf(json_output + current_len, MAXLEN(current_len),
                              "\"%s\"%s", escaped_frame, (i < nframes - 1) ? "," : "");
      if (current_len >= sizeof(json_output)-2) break;
   }

   size_t finalcharpos = current_len < sizeof(json_output)-2 ? current_len : sizeof(json_output)-2;
   json_output[finalcharpos] = ']';
   json_output[finalcharpos+1] = '\0';

   return json_output;
}

  // Function to get backtrace as a JSON array string
static const char* get_backtrace_json(bool *free_backtrace_json)
{
   void *callstack[MAX_FRAMES];
   int nframes = backtrace(callstack, MAX_FRAMES);
   char **strs = backtrace_symbols(callstack, nframes);
   if (!strs) {
      *free_backtrace_json = false;
      return "backtrace_symbols() failed!";
   }

   if (nframes <= 0) {
      *free_backtrace_json = false;
      return "Couldn't get a backtrace";
   }

   /* WARNING: the following code is not correct when there is no frame (nframes <= 0)
    * It results in a heap-buffer-overflow. Hence, the above check on nframe is critical
    */
   size_t current_len = 1;

   for (int i = 0; i < nframes; i++) {
      // Basic JSON string escaping (handles quotes and backslashes)
      size_t j = 0;
      const char * restrict frame = strs[i];

      for (size_t k = 0; k < strlen(frame) && j < (MAX_FRAME_LEN * 2) - 3; k++) {
         if (frame[k] == '"' || frame[k] == '\\') {
            j++;
         }
         j++;
      }

      current_len += j + 3; /* +3 for quotes and ',' or ']' */
   }

   char *json_output = malloc(sizeof(char)*current_len+1);

   if (!json_output) {
      *free_backtrace_json = false;
      return construct_backtrace_json_static(nframes, strs);
   }

   *free_backtrace_json = true;

   char * restrict out_str = json_output;
   *out_str++ = '[';

   for (int i = 0; i < nframes; i++) {

      if (i >= 1) {
         *out_str++ = ',';
      }

      *out_str++ = '"';
      const char * restrict frame = strs[i];

      // Basic JSON string escaping (handles quotes and backslashes)
      for (size_t k = 0, klen = strlen(frame); k < klen; k++) {
         if (frame[k] == '"' || frame[k] == '\\') {
            *out_str++ = '\\';
         }
         *out_str++ = frame[k];
      }

      *out_str++ = '"';
      assert(out_str < json_output + current_len);
   }

   *out_str++ = ']';
   *out_str++ = '\0';

   free((void*)strs);

   return json_output;
  }

#endif


/**
 * @brief Checks if a character is safe to be included in a URL query string.
 * * Safe characters are: A-Z a-z 0-9 - _ . ~ 
 * All others must be percent-encoded.
 *
 * @param c The character to check.
 *
 * @return int 1 if safe, 0 otherwise.
 */
static inline int is_url_safe(char c) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
        return 1;
    }
    return 0;
}

/**
 * @brief Performs URL (Percent) encoding on a string.
 *
 * All unsafe characters are replaced by a '%' followed by their two-digit 
 * hexadecimal ASCII value. The result is a new, dynamically allocated string.
 *
 * @param src The source string to encode.
 * @return char* The newly allocated URL-encoded string. Caller must free this memory.
 */
static char* url_encode(const char * restrict src)
{
   size_t len = strlen(src);
   // Conservative estimate for required memory: three times the original length 
   // (since every character could become %XX, like space -> %20) + 1 for null terminator.
   size_t required_len = (len * 3) + 1; 
   char *url_encoded_str = (char *)malloc(required_len);

   if (url_encoded_str == NULL) {
      perror("Failed to allocate memory for report URL encoding");
      return NULL;
   }

   char *dest = url_encoded_str;
   const char *hex_digits = "0123456789ABCDEF";

   for (size_t i = 0; i < len; i++) {
      char c = src[i];

      if (is_url_safe(c)) {
         // Append safe characters directly
         *dest++ = c;
      } else if (c == ' ') {
         // Special case: Replace spaces with '+' or '%20'. 
         // We use '%20' for strict RFC 3986/GitHub issue compatibility.
         *dest++ = '%';
         *dest++ = '2';
         *dest++ = '0';
      } else {
         // Percent-encode unsafe characters
         *dest++ = '%';
         // Append the high nibble (first hex digit)
         *dest++ = hex_digits[((unsigned char) c) >> 4];
         // Append the low nibble (second hex digit)
         *dest++ = hex_digits[((unsigned char) c) & 0xF];
      }
   }

   // Null-terminate the new string
   *dest = '\0';

   return url_encoded_str;
}

/**
 * @brief Creates a JSON string containing crash report data.
 *
 * This function gathers system information, a call stack backtrace, and a
 * crash reason, and formats them into a single JSON string suitable for a REST API.
 *
 * @param sigcode A string describing the reason for the report (e.g., "SIGSEGV", "EXCEPTION_ACCESS_VIOLATION").
 *
 * @return             the json string
 */
static const char *create_crash_report_json(CODE_TYPE sigcode)
{
   const char *os_name, *os_version, *os_arch;
   char *json_buffer = NULL;

  // 1. Gather OS and Architecture Info
  get_os_info(&os_name, &os_version, &os_arch);

  // 2. Gather Backtrace
  // NOTE: The backtrace function captures the stack from this call back up.
  bool free_backtrace_json;
  const char *backtrace_json = get_backtrace_json(&free_backtrace_json);

  // 3. Format the final JSON string
  int len = asprintf(&json_buffer, 
    "{\n"
    "  \"git_hash\": \"%s\",\n"
    "  \"uuid_binary\": \"%s\",\n"
    "  \"crash_reason\": \"" CODE_TYPE_FMT "\",\n"
    "  \"backend_source\": \"%s\",\n"
    "  \"os\": {\n"
    "    \"name\": \"%s\",\n"
    "    \"version\": \"%s\",\n"
    "    \"architecture\": \"%s\"\n"
    "  },\n"
    "  \"backtrace\": %s\n"
    "}\n",
    rhp_git_hash,
    get_uuid_library(),
    sigcode,
    get_userinfo(),
    os_name,
    os_version,
    os_arch,
    backtrace_json
  );

   if (free_backtrace_json) { free((char*)backtrace_json); }

  if (len < 0 || !json_buffer) {
    fatal_error("\nERROR: Could not get enough memory for the JSON buffer\n");
    return NULL;
  }

  return json_buffer;
}

// Callback function for writing received data to the MemoryStruct
static size_t curlResponseCallback(void *contents, size_t size, size_t nmemb, void *userp) {
   size_t realsize = size * nmemb;
   char **response_ptr = (char **)userp;

   *response_ptr = realloc(*response_ptr, realsize + 1);
   if(!(*response_ptr)) {
      fatal_error("[ERROR] WriteMemoryCallback: Failed to reallocate memory.\n");
      return 0;
   }

   memcpy(*response_ptr, contents, realsize);
   (*response_ptr)[realsize] = '\0';

   return realsize;
}

typedef struct {
    char *location_url;
    size_t size;
} LocationHeaderData;

// --- 2. Callback Function ---
static size_t curlLocationHeaderCallback(char *buffer, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    LocationHeaderData *data = (LocationHeaderData *)userp;
    char *line = (char *)buffer;
    const char *header_name = "Location:";
    size_t header_len = strlen(header_name);

    // Check if the line starts with "Location:" (case-insensitive search is best practice)
    if (strncasecmp(line, header_name, header_len) == 0) {
        // Find the start of the value, skipping the header name and any leading whitespace
        char *value_start = line + header_len;
        while (*value_start == ' ' || *value_start == '\t') {
            value_start++;
        }
  
        // Find the end of the value (end of line, minus CR/LF)
        size_t value_len = total_size - (value_start - line);
   
        // Trim trailing CR/LF characters
        while (value_len > 0 && (value_start[value_len - 1] == '\r' || value_start[value_len - 1] == '\n')) {
            value_len--;
        }

        // Dynamically allocate memory for the URL
        data->location_url = (char *)malloc(value_len + 1);
        if (data->location_url) {
            strncpy(data->location_url, value_start, value_len);
            data->location_url[value_len] = '\0';
            data->size = value_len;
        } else {
         fatal_error("[CURL] could not allocate memory for 'Location' http header");
      }
    }
 
    // cURL requires that we always return the total size of the block
    return total_size;
}

static const char curl_libname[] =
#ifdef _WIN32
   "curl.dll";
#elif defined(__APPLE__)
   "libcurl.dylib";
#else
   "libcurl.so";
#endif

#define CURL_CHECK(EXPR) { int rc42 = EXPR; if (rc42) { error("CURL ERROR while executing " #EXPR " rc = %d\n", rc42); goto _exit; } }

// Function pointers storage
typedef void (*CURL_GLOBAL_INIT)(long flags);
typedef void *(*CURL_EASY_INIT)(void);
typedef int (*CURL_EASY_SETOPT)(void *curl, int option, ...);
typedef int (*CURL_EASY_GETINFO)(void *curl, int info, ... );
typedef int (*CURL_EASY_PERFORM)(void *curl);
typedef void (*CURL_EASY_CLEANUP)(void *curl);
typedef void (*CURL_GLOBAL_CLEANUP)(void);
typedef void *(*CURL_SLIST_APPEND)(void *slist, const char *string);
typedef void (*CURL_SLIST_FREE_ALL)(void *slist);

tlsvar void *curl_lib = NULL;

tlsvar struct curl_fptr {
   CURL_GLOBAL_INIT    curl_global_init;
   CURL_EASY_INIT      curl_easy_init;
   CURL_EASY_SETOPT    curl_easy_setopt;
   CURL_EASY_GETINFO   curl_easy_getinfo;
   CURL_EASY_PERFORM   curl_easy_perform;
   CURL_EASY_CLEANUP   curl_easy_cleanup;
   CURL_GLOBAL_CLEANUP curl_global_cleanup;
   CURL_SLIST_APPEND   curl_slist_append;
   CURL_SLIST_FREE_ALL curl_slist_free_all;
} curlFptr = {0};

typedef struct {
   char *response;
   void* curl;
} CurlData;

UNUSED static bool load_libcurl(void)
{
   curl_lib = open_library(curl_libname, 0);

   if (!curl_lib) {
      fatal_error("[INFO] Please ensure libcurl is installed and discoverable (e.g., ldconfig).\n");
      return false;
   }

   // 2. Resolve function addresses
   *(void **)&curlFptr.curl_global_init    = get_function_address(curl_lib, "curl_global_init");
   *(void **)&curlFptr.curl_easy_init      = get_function_address(curl_lib, "curl_easy_init");
   *(void **)&curlFptr.curl_easy_setopt    = get_function_address(curl_lib, "curl_easy_setopt");
   *(void **)&curlFptr.curl_easy_getinfo   = get_function_address(curl_lib, "curl_easy_getinfo");
   *(void **)&curlFptr.curl_easy_perform   = get_function_address(curl_lib, "curl_easy_perform");
   *(void **)&curlFptr.curl_easy_cleanup   = get_function_address(curl_lib, "curl_easy_cleanup");
   *(void **)&curlFptr.curl_global_cleanup = get_function_address(curl_lib, "curl_global_cleanup");
   *(void **)&curlFptr.curl_slist_append   = get_function_address(curl_lib, "curl_slist_append");
   *(void **)&curlFptr.curl_slist_free_all = get_function_address(curl_lib, "curl_slist_free_all");

   if (!curlFptr.curl_easy_init || !curlFptr.curl_easy_setopt || !curlFptr.curl_easy_perform || !curlFptr.curl_slist_append) {
      fatal_error("\n[ERROR] CURL_DLSYM_ERROR: Failed to find essential curl functions.\n");
      close_library(curl_lib);
      return false;
   }

   return true;
}

/**
 * @brief Common initialization function
 *
 * @param curlDat the curl data structure
 *
 * @return  true if the initizliatoin was successful, false otherwise
 */
UNUSED static bool libcurl_init(CurlData *curlDat)
{
    curlFptr.curl_global_init(3L); // 3L corresponds to CURL_GLOBAL_ALL
    void *curl = curlFptr.curl_easy_init();

   if (!curl) { return false; }

   curlDat->curl = curl;

   // Tell curl to be verbose for debugging (1L = CURLOPT_VERBOSE)
   if (getenv("RHP_CURL_VERBOSE")) {
      CURL_CHECK(curlFptr.curl_easy_setopt(curl, 41, 1L));
   }

   CURL_CHECK(curlFptr.curl_easy_setopt(curl, 10018, API_USERAGENT));

   // --- Set up callback for server response ---
   CURL_CHECK(curlFptr.curl_easy_setopt(curl, 20011, curlResponseCallback)); // 20011 = CURLOPT_WRITEFUNCTION
   CURL_CHECK(curlFptr.curl_easy_setopt(curl, 10001, (void *)&curlDat->response));       // 10005 = CURLOPT_WRITEDATA

   return true;

_exit:

   if (curl) {
      curlFptr.curl_easy_cleanup(curl);
   }

   return false;
}

UNUSED static void send_crash_report_via_curl(const char *json_report, u8 uuid[16])
{
   void *headers = NULL, *curl = NULL;
   CurlData curlDat = {0};
 
   if (!load_libcurl()) {
      fatal_error("\n[ERROR] Could not load libcurl function, exiting\n");
      return;
   }

   /* Common curl init */
   if (!libcurl_init(&curlDat)) {
      fatal_error("\n[ERROR] CURL_INIT_ERROR: Failed to initialize CURL easy handle.\n");
      return;
   }

   curl = curlDat.curl; assert(curl);

   // Set the URL (10002)
   CURL_CHECK(curlFptr.curl_easy_setopt(curl, 10002, REPORT_URL)); 

   // Set POST data (10015) and size (47)
   CURL_CHECK(curlFptr.curl_easy_setopt(curl, 10015, json_report)); 
   // No need for CURLOPT_POSTFIELDSIZE (60) as strlen works
   //CURL_CHECK(curlFptr.curl_easy_setopt(curl, 60, (long)strlen(json_report))); 

   headers = curlFptr.curl_slist_append(headers, "Content-Type: application/json");
   if (!headers) {
      fatal_error("CURL ERROR while adding Content-Type header\n");
      goto _exit;
   }

   // Set Content-Type header (20023)
   CURL_CHECK(curlFptr.curl_easy_setopt(curl, 10023, headers)); 

   LocationHeaderData header_data = { .location_url = NULL, .size = 0 };
   curlFptr.curl_easy_setopt(curl, 20079, curlLocationHeaderCallback); // 20079 = CURLOPT_HEADERFUNCTION
   curlFptr.curl_easy_setopt(curl, 10029, &header_data); // 10029 = CURLOPT_HEADERDATA

   // Execute the transfer
   int res = curlFptr.curl_easy_perform(curl);

   if (res != 0) {
      error("[ERROR] CURL_PERFORM_ERROR: curl_easy_perform failed with code %d. Check server logs.\n", res);
   } else {
      long httpcode;
      curlFptr.curl_easy_getinfo(curl, 0x200002, &httpcode);

      /* Looks like we might have a GCC bug with %s argument */
      const char *response = curlDat.response ? curlDat.response : "NULL";
      if (httpcode == 201) {
         error("\n[INFO] Crash report sent successfully. Response was: '%s'\n", response);
      } else {
         fatal_error("\n[ERROR] Crash report was not sent successfully: the response code was %ld (should be 201). "
                     "The server response was:\n'%s'\n", httpcode, response);
      }

      if (!header_data.size) {
         fatal_error("\n[ERROR] Could not parse 'Location' HTTP Header\n");
      } else if (header_data.size != 36) {
         fatal_error("\n[ERROR] 'Location' HTTP Header has wrong size %zu; expecting %u\n", header_data.size, 36);
      } else {
         res = sscanf(header_data.location_url, u8SCN u8SCN u8SCN u8SCN "-" u8SCN u8SCN "-" u8SCN u8SCN "-" u8SCN u8SCN "-" 
                      u8SCN u8SCN u8SCN u8SCN u8SCN u8SCN,
                      &uuid[0], &uuid[1], &uuid[2], &uuid[3], &uuid[4], &uuid[5], &uuid[6],
                      &uuid[7], &uuid[8], &uuid[9], &uuid[10], &uuid[11], &uuid[12], &uuid[13],
                      &uuid[14], &uuid[15]);

         if (res != 16) {
            fatal_error("\n[ERROR] Parsing 'Location' HTTP Header value '%s' failed\n", header_data.location_url);
         }

      }

      free(header_data.location_url);

   }
_exit:
   // 4. Cleanup
   if (headers) {
      curlFptr.curl_slist_free_all(headers);
   }

   if (curl) {
      curlFptr.curl_easy_cleanup(curl);
   }

   if (curlFptr.curl_global_cleanup) {
      curlFptr.curl_global_cleanup();
   }

   if (curlDat.response) {
      free(curlDat.response);
   }

   if (curl_lib) {
      close_library(curl_lib);
      curl_lib = NULL;
   }
}


UNUSED static bool upload_file_via_curl(void *curl, const char *response, ScrDirFile *dat,
                                        Uuidv7 uuid)
{
   char *url = NULL;
   FILE *file_handle = NULL;

   if (!file_readable_silent(dat->fname)) {
      fatal_error("Skipping '%s' as it does not exists or is not readable.\n", dat->fname);
      return true;
   }

   // 2. Open the file for reading in binary mode.
   file_handle = fopen(dat->fname, "rb");
   if (!file_handle) {
      perror("Error opening file for upload");
      goto _exit;
   }

    // 1. Get file size.
    if (fseek(file_handle, 0, SEEK_END) != 0) {
        perror("Error seeking to end of file");
        (void)fclose(file_handle);
        goto _exit;
    }

    u64 file_size = ftello(file_handle);

    if (fseek(file_handle, 0, SEEK_SET) != 0) {
        perror("Error rewinding file");
        (void)fclose(file_handle);
        goto _exit;
    }

   IO_PRINT(asprintf(&url, CRASH_SERVER UPLOAD_ENDPOINT UUID_PRINT_FMT "/%s",
                     UUID_FMTARGS(uuid), dat->upload_name));

   // Set the URL (10002)
   CURL_CHECK(curlFptr.curl_easy_setopt(curl, 10002, url)); 

   fatal_error("Uploading '%s' (%zu bytes).", dat->fname, (size_t)file_size);

   // Set the file handle as the source for the request body.
   // CURLOPT_READDATA = 10009
   curlFptr.curl_easy_setopt(curl, 10009, file_handle);
   // Set the size of the data to be sent.
   // CURLOPT_INFILESIZE_LARGE = 30115
   curlFptr.curl_easy_setopt(curl, 30115, file_size);

   // Execute the transfer
   int res = curlFptr.curl_easy_perform(curl);

   if (res != 0) {
      error("\n[ERROR] CURL_PERFORM_ERROR: curl_easy_perform failed with code %d. Check server logs.\n", res);
   } else {
      long httpcode;
      curlFptr.curl_easy_getinfo(curl, 0x200002, &httpcode);

      if (httpcode == 200) {
         u64 speed_upload;
         double total_time;
         curlFptr.curl_easy_getinfo(curl, 0x200002, &total_time);
         curlFptr.curl_easy_getinfo(curl, 0x600010, &speed_upload);
         print_upload_stats(total_time, speed_upload);

      } else {
         fatal_error("\n[ERROR] File '%s' was not sent successfully: the response code was %ld (should be 200). "
                     "The server response was:\n'%s'\n", dat->fname, httpcode, response);
      }

   }

   free(url);
   (void)fclose(file_handle);
   return true;

_exit:
   free(url);
   (void)fclose(file_handle);
   return false;
}

UNUSED static void send_model_instance_via_curl(Uuidv7 uuid)
{
   void *headers = NULL;
   CurlData curlDat = {0};
 
   if (!load_libcurl()) {
      fatal_error("\n[ERROR] Could not load libcurl function, exiting\n");
      return;
   }

   gevHandle_t gev = gmdl_get_src_gev();
   if (!gev) {
      fatal_error("\n[ERROR] Could not get initial GEV, exiting\n");
      return;
   }

   /* Common curl init */
   if (!libcurl_init(&curlDat)) {
      fatal_error("\n[ERROR] CURL_INIT_ERROR: Failed to initialize CURL easy handle.\n");
      return;
   }

   void *curl = curlDat.curl;

      headers = curlFptr.curl_slist_append(headers, "Content-Type: application/octet-stream");
   if (!headers) {
      fatal_error("CURL ERROR while adding Content-Type header\n");
      goto _exit;
   }

   // Set Content-Type header (10023)
   CURL_CHECK(curlFptr.curl_easy_setopt(curl, 10023, headers)); 

   // We are sending data, so this is a POST request.
   // CURLOPT_POST = 47
   curlFptr.curl_easy_setopt(curl, 47, 1L);

   for (unsigned i = 0, len = ARRAY_SIZE(gams_files_upload); i < len; ++i) {
      char buf[GMS_SSSIZE];
      ScrDirFile uploadDat;
      uploadDat.fname = gevGetStrOpt(gev, gams_files_upload[i].gevOptStr, buf);
      uploadDat.upload_name = gams_files_upload[i].upload_name;

      upload_file_via_curl(curl, curlDat.response, &uploadDat, uuid);
   }

   for (unsigned i = 0, len = ARRAY_SIZE(empinfo_upload); i < len; ++i) {
      char buf[GMS_SSSIZE];
      ScrDirFile uploadDat;
      int rc = asprintf((char**)&uploadDat.fname, "%s%s", gevGetStrOpt(gev, gevNameScrDir, buf),
               empinfo_upload[i].fname);

      if (rc == -1) { 
         fatal_error("[ERROR] Could not create path to empinfo file '%s' with scrdir '%s'\n",
                     empinfo_upload[i].fname, buf);
         continue;
      }

      uploadDat.upload_name = empinfo_upload[i].upload_name;

      upload_file_via_curl(curl, curlDat.response, &uploadDat, uuid);

      free((char*)uploadDat.fname);
   }

_exit:
   // 4. Cleanup
   if (headers) {
      curlFptr.curl_slist_free_all(headers);
   }

   if (curl) {
      curlFptr.curl_easy_cleanup(curl);
   }

   if (curlFptr.curl_global_cleanup) {
      curlFptr.curl_global_cleanup();
   }

   if (curlDat.response) {
      free(curlDat.response);
   }

   if (curl_lib) {
      close_library(curl_lib);
      curl_lib = NULL;
   }
}

static void send_crash_report(const char *json_report, u8 uuid[16])
{
#ifdef _WIN32
   send_crash_report_via_winhttp(json_report, uuid);
#else
   send_crash_report_via_curl(json_report, uuid);
#endif
}

static void send_model_instance(const Uuidv7 uuid)
{
#ifdef _WIN32
   send_model_instance_via_winhttp(uuid);
#else
   send_model_instance_via_curl(uuid);
#endif
}

static bool yesnoanswer(void)
{
   unsigned cnt = 0;

   if (getenv("RHP_REPORT_CRASH")) { return true; }

   while (cnt < 100) {
      int answer = getchar();
      if (answer == 'Y' || answer == 'y' || (cnt == 0 && answer == '\n')) {
         fatal_error("%c", answer);
         return true;
      }
      if (answer == 'N' || answer == 'n') {
         fatal_error("%c", answer);
         return false;
      } 
      if (answer != '\n') {
         fatal_error("\nAnswer '%c' not understood, please type 'y' or 'n': ", answer);
      }
      cnt++;
   }

   return false;
}

static bool rerun_continue_answer(void)
{
   unsigned cnt = 0;

   if (getenv("RHP_REPORT_CRASH")) { return true; }

   while(cnt < 100) {
      int answer = getchar();
      if (answer == 'R' || answer == 'r' || (cnt == 0 && answer == '\n')) {
         fatal_error("%c", answer);
         return true;
      }
      if (answer == 'C' || answer == 'c') {
         fatal_error("%c", answer);
         return  false;
      } 
      if (answer != '\n') {
         fatal_error("\nAnswer '%c' not understood, please type 'r' or 'c': ", answer);
      }
      cnt++;
   }

   return false;
}

/**
 * @brief Crash handler function
 *
 * This function tries to create and upload a JSON crash report
 *
 * @param sigcode  the signal value
 */
void crash_handler(CODE_TYPE sigcode)
{

   fatal_error("\nReSHOP has unfortunately crashed!.\n");

   /* If we have a GAMS source model, try to upload it */
   if (!gmdl_check_solvelink_crash_report()) {
      fatal_error("To help us fix the issue, running the gams with the solvelink option (shortname: sl) set to either 2 or 0 would be helpful.\n");
      fatal_error("\nDo you wish to rerun gams with that option solve or continue? Enter r for rerun or c for continue: [R]/c : ");
      bool rerun = rerun_continue_answer();

      if (rerun) { return; }
   }

  const char *json_report = create_crash_report_json(sigcode);

  if (json_report) {

      if (isatty(STDERR_FILENO)) {
         fatal_error("To help us fix the issue, the following information would be helpful:\n");
         fatal_error("%s", json_report);
         fatal_error("\nDo you agree to send the above crash report to our crash reporting system: [Y]/n : ");

         bool upload_report = yesnoanswer();

         if (upload_report) {

            Uuidv7 uuid = {0}, uuid_invalid = {0};
            send_crash_report(json_report, uuid.uuid);
            char *url_encoded_report = url_encode(json_report);
 
            char uuid_str[37]; //32 + 4 + 1
            bool report_upload_success;

            if (memcmp(uuid.uuid, uuid_invalid.uuid, sizeof(uuid)) == 0) {
               strcpy(uuid_str, "Invalid-UUID");
               report_upload_success = false;
               fatal_error("\nCrash report could not be uploaded, see error above.\n");
            } else {
               report_upload_success = true;
               DBGUSED int res = snprintf(uuid_str, sizeof(uuid_str), UUID_PRINT_FMT,
                                          UUID_FMTARGS(uuid));

               assert(res == 36); //32 + 4

            }

            if (report_upload_success) {
               fatal_error("\nUploading the GMO (binary model instance) and empinfo information would greatly help fix this bug.\n");
               fatal_error("\nDo you agree to send the model instance to our crash reporting system: [Y]/n : \n");

               bool upload_model_instance = yesnoanswer();

               if (upload_model_instance) {
                  send_model_instance(uuid);
               }
            }

            fatal_error("\nPlease consider opening an issue on github by opening the "
                        "following URL:\n"
                        "https://github.com/xhub/reshop/issues/new?title=Crash&"
                        "body=ReSHOP%%20experienced%%20a%%20major%%20failure.%%0A"
                        "Crash%%20report%%20UUID%%20is%%20%s.%%0AReport%%20follows%%0A"
                        "%%60%%60%%60%%0A%s%%0A%%60%%60%%60%%0A&"
                        "labels=crash,bug\n",
                        uuid_str, url_encoded_report);

#ifdef _WIN32
            Sleep(5000);
#else
            sleep(5);
#endif

            free(url_encoded_report);
         }
      } else {

         char *url_encoded_report = url_encode(json_report);
         if (url_encoded_report) {
            fatal_error("\nPlease consider opening an issue on github by opening the "
                        "following URL:\n"
                        "https://github.com/xhub/reshop/issues/new?title=Crash&"
                        "body=ReSHOP%%20experienced%%20a%%20major%%20failure.%%0A"
                        "%%0AReport%%20follows%%0A"
                        "%%60%%60%%60%%0A%s%%0A%%60%%60%%60%%0A&"
                        "labels=crash,bug\n", url_encoded_report);
            free(url_encoded_report);
         } else {
            fatal_error("\nPlease consider opening an issue on github by opening the "
                        "following URL:\n"
                        "https://github.com/xhub/reshop/issues/new?title=Crash&"
                        "body=ReSHOP%%20experienced%%20a%%20major%%20failure.&"
                        "labels=crash,bug\n");
         }

      }


      free((char*)json_report);
   }
}
