#ifndef WIN_FSYNC_H
#define WIN_FSYNC_H

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fileapi.h>
#include <io.h>

#define fileno(X) _fileno(X)
#define fsync(X) !FlushFileBuffers((HANDLE)(uintptr_t)_get_osfhandle(X))

#endif
#endif
