#include "reshop_config.h"

#include <errno.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

#else

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#endif

#include "macros.h"
#include "printout.h"
#include "rhp_ipc.h"
#include "rhpgui_launcher.h"
#include "status.h"

int imgui_start(const char *hint)
{
   if (gui_fd >= 0) { return OK; }
   const char *arg0, *imgui_path;

   if (hint && hint[0] != '\0') {
      arg0 = imgui_path = hint;
   } else {
      arg0 = imgui_path = "reshop_imgui";
   }

   const char *sockpath = ipc_unix_domain_init();
   if (!sockpath) {
      return Error_SystemError;
   }

#ifdef _WIN32
   // silence warning
   (void)arg0;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Create a new process
    if (!CreateProcess(
            imgui_path,        // Path to executable
      // FIXME: can this argument be modified?
            (char*)sockpath,        // Command line arguments
            NULL,        // Process handle not inheritable
            NULL,        // Thread handle not inheritable
            FALSE,       // Do not inherit handles
            0,           // Creation flags
            NULL,        // Use parent's environment
            NULL,        // Use parent's working directory
            &si,         // Pointer to STARTUPINFO
            &pi)) {      // Pointer to PROCESS_INFORMATION
        error("[GUI] ERROR: CreateProcess failed (%lu)\n", GetLastError());
        return Error_SystemError;
    }

    // Close handles to avoid resource leaks
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

#else
   int sig;

   // Block SIGUSR1 and set up the signal set
   sigset_t set;
   sigemptyset(&set);
   sigaddset(&set, SIGUSR1);
   sigprocmask(SIG_BLOCK, &set, NULL); // Block SIGUSR1

   pid_t mypid = getpid();
   char pidstr[30];
   snprintf(pidstr, sizeof(pidstr), "%d", mypid);

   pid_t pid = fork();

   if (pid < 0) {
      error_errno("[GUI] ERROR: failed to fork: %s\n");
      return Error_SystemError;
   }

   if (pid == 0) {
        // In child process: execute the program
      if (execlp(imgui_path, arg0, sockpath, pidstr, (char*)NULL) == -1) {
         error_errno("[GUI] ERROR: failed to launch GUI '%s' '%s' '%s': '%s'",
                     imgui_path, sockpath, pidstr);
         kill(getppid(), SIGUSR1); /* Otherwise parent is stalled */
         return Error_RuntimeError;
      }

      return OK;
    } else {
      // FIXME: use sigtimedwait and specify a timeout.
      sigwait(&set, &sig); // Wait for SIGUSR1
   }

#endif

   S_CHECK(unix_domain_client_init(sockpath));

   set_log_fd(gui_fd);

   return OK;
}
