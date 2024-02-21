#include "timings.h"

#if defined(_WIN32) && defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <stdio.h>
#include <processthreadsapi.h>
#include "asnan.h"

double get_thrdtime(void)
{
    FILETIME a,b,c,d;
    if (GetThreadTimes(GetCurrentThread(),&a,&b,&c,&d) != 0){
        return
            (double)(d.dwLowDateTime |
            ((unsigned long long)d.dwHighDateTime << 32)) * 0.0000001;
    }else{
        return NAN;
    }
}

double get_proctime(void)
{
    FILETIME a,b,c,d;
    if (GetProcessTimes(GetCurrentProcess(),&a,&b,&c,&d) != 0){
        return
            (double)(d.dwLowDateTime |
            ((unsigned long long)d.dwHighDateTime << 32)) * 0.0000001;
    }else{
        return NAN;
    }
}

double get_walltime(void)
{
   return GetTickCount64() * 1e-3;
}

#else /* POSIX */
#include "reshop_config.h"
#include <math.h>
#include <time.h>
#include <stdio.h>


double get_proctime(void)
{
   struct timespec tp;
   if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp) == -1) {
      perror("clock_gettime");
      return NAN;
   }

   return tp.tv_sec + tp.tv_nsec * 1e-9;
}

double get_thrdtime(void)
{
   struct timespec tp;
   if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tp) == -1) {
      perror("clock_gettime");
      return NAN;
   }

   return tp.tv_sec + tp.tv_nsec * 1e-9;
}

double get_walltime(void)
{
   struct timespec tp;
   if (clock_gettime(CLOCK_MONOTONIC, &tp) == -1) {
      perror("clock_gettime");
      return NAN;
   }

   return tp.tv_sec + tp.tv_nsec * 1e-9;
}

#endif
