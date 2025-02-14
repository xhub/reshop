#ifndef RHP_SOCKET_ERROR
#define RHP_SOCKET_ERROR

#include "reshop_config.h"

#include "macros.h"

#ifndef RHP_SOCKET_FATAL
#error "The macro RHP_SOCKET_FATAL must be defined before including this function"
#endif

#ifndef RHP_SOCKET_LOGGER
#error "The macro RHP_SOCKET_LOGGER must be defined before including this function"
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#include <winsock2.h>
#include <windows.h>

//DWORD FormatMessage(
//  [in]           DWORD   dwFlags,
//  [in, optional] LPCVOID lpSource,
//  [in]           DWORD   dwMessageId,
//  [in]           DWORD   dwLanguageId,
//  [out]          LPTSTR  lpBuffer,
//  [in]           DWORD   nSize,
//  [in, optional] va_list *Arguments
//);

#define sockerr_log(fmt) {\
   int errno_ = WSAGetLastError(); \
   char msg42[256]; \
   DWORD length = FormatMessageA( \
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
        NULL, errno_, 0, msg42, sizeof(msg42), NULL); \
\
   RHP_SOCKET_LOGGER(fmt ": errno %d '%s'\n", errno_, length > 0 ? msg42 : "UNKNOWN ERROR"); \
}

#define sockerr_log2(fmt, ...) {\
   int errno_ = WSAGetLastError(); \
   char msg42[256]; \
   DWORD length = FormatMessageA( \
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
        NULL, errno_, 0, msg42, sizeof(msg42), NULL); \
\
   RHP_SOCKET_LOGGER(fmt ": errno %d '%s'\n", __VA_ARGS__, errno_, length > 0 ? msg42 : "UNKNOWN ERROR"); \
}

#define sockerr_fatal(fmt) {\
   int errno_ = WSAGetLastError(); \
   char msg42[256]; \
   DWORD length = FormatMessageA( \
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
        NULL, errno_, 0, msg42, sizeof(msg42), NULL); \
\
   RHP_SOCKET_FATAL(fmt ": errno %d '%s'\n", errno_, length > 0 ? msg42 : "UNKNOWN ERROR"); \
}

#define sockerr_fatal2(fmt, ...) {\
   int errno_ = WSAGetLastError(); \
   char msg42[256]; \
   DWORD length = FormatMessageA( \
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
        NULL, errno_, 0, msg42, sizeof(msg42), NULL); \
\
   RHP_SOCKET_FATAL(fmt ": errno %d '%s'\n", __VA_ARGS__, errno_, length > 0 ? msg42 : "UNKNOWN ERROR"); \
}

#else
#include <errno.h>

#define sockerr_fatal(fmt) {\
   char buf[256], *msg; \
   int errno_ = errno; \
   STRERROR(errno_, buf, sizeof(buf), msg); \
   RHP_SOCKET_FATAL(fmt ": errno %d '%s'\n", errno_, msg); \
}

#define sockerr_fatal2(fmt, ...) {\
   char buf[256], *msg; \
   int errno_ = errno; \
   STRERROR(errno_, buf, sizeof(buf), msg); \
   RHP_SOCKET_FATAL(fmt ": errno %d '%s'\n", __VA_ARGS__, errno_, msg); \
}

#define sockerr_log(fmt) {\
   char buf[256], *msg; \
   int errno_ = errno; \
   STRERROR(errno_, buf, sizeof(buf), msg); \
   RHP_SOCKET_LOGGER(fmt ": errno %d '%s'\n", errno_, msg); \
}

#define sockerr_log2(fmt, ...) {\
   char buf[256], *msg; \
   int errno_ = errno; \
   STRERROR(errno_, buf, sizeof(buf), msg); \
   RHP_SOCKET_LOGGER(fmt ": errno %d '%s'\n", __VA_ARGS__, errno_, msg); \
}
#endif

#endif  //RHP_SOCKET_ERROR

