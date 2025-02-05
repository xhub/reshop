#include "macros.h"
#include "mdl.h"
#include "reshop_config.h"

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef HAS_UNISTD
#include <unistd.h>
#endif

#include "printout.h"
#include "rhp_ipc.h"
#include "tlsdef.h"

tlsvar char uuidstr[37]; // uuid for socket path
tlsvar char sockpath[108]; //socket path


#if defined(_WIN32) // Windows
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <objbase.h>

// For AF_UNIX
#include <winsock2.h>
#include <afunix.h>
#include <io.h>

#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

static int gen_uuid(void)
{
    UUID uuid;
    if (CoCreateGuid(&uuid) == S_OK) {
        snprintf(uuidstr, sizeof(uuidstr), "%08lX-%04X-%04X-%04X-%012llX",
                 uuid.Data1, uuid.Data2, uuid.Data3,
                 (uuid.Data4[0] << 8) | uuid.Data4[1],
                 *((unsigned long long*)&uuid.Data4[2]));
      return 0;
    }

   return -1;
}

#elif defined(__linux__) || defined(__APPLE__) // Linux and macOS

/* This needs at least Sierra (10.12) on MacOs and glibc 2.25 / Linux 3.17 */
#include <sys/random.h>

static int gen_uuid(void)
{
    uint8_t uuid[16];

    if (getentropy(uuid, sizeof(uuid)) != 0) {
        return -1;
    }

    // Adjust the UUID to conform to RFC 4122
    uuid[6] = (uuid[6] & 0x0F) | 0x40; // Version 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80; // Variant 1

    // Format the UUID as a string
    snprintf(uuidstr, sizeof(uuidstr),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
             uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

   return 0;
}

#else
#error "Unsupported platform"
#endif

static const char *get_uuid(void)
{
   if (gen_uuid()) {
      errormsg("Error generating UUID\n");
      return NULL;
   }

   return uuidstr;
}

const char* ipc_unix_domain_init(void)
{
   const char *uuid = get_uuid();
   int rc = OK;

   /* We try to use abstract namespaces in Linux.
    * Microsoft claims that they work:
    * https://devblogs.microsoft.com/commandline/af_unix-comes-to-windows/
    * but not really
    * https://github.com/microsoft/WSL/issues/4240
    *
    * On MacOS, a natural temporary directory is /tmp/
    * On windows, we try a bunch to create reshop_ipc-UUID:
    * - in the directory return by GetTempPath
    * - in C:\Temp
    * - in gams.scrdir
    *   FIXME Try this with a username that contains nonlatin (CJK) characters
    */

#if defined (__linux__)
   /* FIXME: we don't test for  writting the sockpath
   sockpath[0] = '\0';
   assert(strlen(uuid) < sizeof(sockpath) - 2);
   memcpy(&sockpath[1], uuid, strlen(uuid)+1);
   */

   strcpy(sockpath, uuid);

#elif defined (__APPLE__)
   strcpy(sockpath, "/tmp/reshop_ipc-");
   memcpy(&sockpath[strlen(sockpath)], uuid, strlen(uuid)+1);

#elif defined (_WIN32)
   int method = 0, nmethods = 2; //FIXME implement other fallbacks
   DWORD ret;
   do {
      switch(method) {
      case 0:
         ret = GetTempPathA(sizeof(sockpath), sockpath);
         if (ret == 0) {
            continue;
         }
         break;
      case 1:
         strcpy(sockpath, "C:\\Temp\\");
         ret = strlen("C:\\Temp\\");
         break;
      default:
         ;
      }

      size_t expected_sz = sizeof(sockpath) - ret - 1;
      size_t n = snprintf(&sockpath[ret], expected_sz, "%s%s",
                          "reshop_ipc-", uuid);
      if (n >= expected_sz) {
         error("\n[IPC] Truncation of socket path: '%s'\n", sockpath);
         errormsg("[IPC] As a result, the communication with the GUI may not work\n");
      }

      // FIXME: check that we can write to the directory
      break;
  
   } while (++method < nmethods);

   if (method >= nmethods) {
      rc = Error_SystemError;
   }
#else
#error "Unsupported platform"
#endif

   return rc == OK ? sockpath : NULL;

}

static inline bool guiack(rhpfd_t fd)
{
   MessageHeader header;

   ssize_t bytes_read = read(fd, &header, sizeof(header));
   if (bytes_read == -1) { goto ipc_error_read; }
   if (bytes_read != sizeof(header))  { goto ipc_error_readheader; }
   if (header.type != GuiAck) { goto ipc_error_noguiack; }

   return true;

ipc_error_read: 
   {
   int errno_ = errno;
   char *errnostr, errnomsg[256];
   STRERROR(errno_, errnomsg, sizeof(errnomsg), errnostr);
   error("\n[IPC] ERROR while sending model: 'read' failed with %s'\n", errnostr);

   return false;
   }
ipc_error_readheader: 
   {
   error("\n[IPC] ERROR while sending model: expected header with size %zu; "
         "read %zd\n", sizeof(header), bytes_read);
   return false;
   }
ipc_error_noguiack: 
   {
   error("\n[IPC] ERROR while sending model: expected %s header; got %s\n", 
         msgtype2str(GuiAck), msgtype2str(header.type));
   return false;
   }

}

int ipc_send_mdl(Model *mdl, rhpfd_t fd)
{
   MessageHeader header;
   MessagePayload payload;

   payload.simple.header = (MessageHeader){ModelGuiBasicDataSize, NewModelStart, {0} };
   payload.simple.mdl.backend = mdl->backend;
   payload.simple.mdl.type = mdl->commondata.mdltype;
   payload.simple.mdl.id = mdl->id;
   payload.simple.mdl.nvars = mdl_nvars(mdl);
   payload.simple.mdl.nequs = mdl_nequs(mdl);

   if (write(fd, &payload, NewModelStartPayload) == -1) { goto ipc_error_write; }
   if (!guiack(fd)) { goto ipc_error_guiack; }

   /* Send model name */
   const char *mdlname = mdl_getname(mdl);
   size_t mdlnamelen = strlen(mdlname)+1; assert(mdlnamelen < SimpleLoadSize);
   payload.simple.header.type = NewModelName; payload.simple.header.length = mdlnamelen;
   memcpy(payload.simple.load, mdlname, mdlnamelen);

   if (!guiack(fd)) { goto ipc_error_guiack; }

   /* Send EMPDAG, if any */
   if (empinfo_hasempdag(&mdl->empinfo)) {
      const EmpDag *empdag = &mdl->empinfo.empdag;

      payload.simple.header = (MessageHeader){EmpDagGuiBasicDataSize, NewEmpDagStart, {0} };
      payload.simple.empdag.mdlid = mdl->id;
      payload.simple.empdag.type = empdag->type;
      payload.simple.empdag.stage = empdag->stage;
      payload.simple.empdag.nodestats = empdag->node_stats;
      payload.simple.empdag.arcstats = empdag->arc_stats;

      if (write(fd, &payload, NewEmpDagStartPayload) == -1) { goto ipc_error_write; }
      if (!guiack(fd)) { goto ipc_error_guiack; }

   }

   header = (MessageHeader){ 0, NewModelEnd, {0} };
   if (write(fd, &header, sizeof(header)) == -1) { goto ipc_error_write; }
   if (!guiack(fd)) { goto ipc_error_guiack; }

   return OK;

ipc_error_write: 
   {
   sockerr_log2("\n[IPC] ERROR while sending %s model '%.*s' #%u: 'write' failed",
         mdl_fmtargs(mdl));

   return Error_SystemError;
   }

ipc_error_guiack:
   error("\n[IPC] ERROR while sending %s model '%.*s' #%u: GUI didn't properly acknowledge\n",
         mdl_fmtargs(mdl));

   return Error_RuntimeError;
}

int ipc_wait(rhpfd_t fd)
{
   return OK;

}
