#include "reshop_config.h"

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(HAS_UNISTD) && !defined(_WIN32)
#include <unistd.h>
#endif

#include "macros.h"
#include "mathprgm.h"
#include "mdl.h"
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

#include <basetsd.h>
typedef SSIZE_T ssize_t;

static int gen_uuid(void)
{
   unsigned i = 0;
   UUID uuid;
   switch (UuidCreate(&uuid)) {
   case RPC_S_OK:
   case RPC_S_UUID_LOCAL_ONLY:
      break;
   default:
      goto _err;
   }

   RPC_CSTR str;

   if (UuidToStringA(&uuid, &str) != RPC_S_OK) { goto _err; }

   for (; i < sizeof(uuidstr) && str[i] != '\0'; ++i) {
      uuidstr[i] = (char)str[i];
   }

   if (i >= sizeof(uuidstr)) {
      error("[OS] ERROR: UUID '%s' is too long, max length is %zu. Please file bug report\n",
            str, sizeof(uuidstr));
      RpcStringFree(&str);
      return -1;
   }

   uuidstr[i] = '\0';

   RpcStringFree(&str);

_err:
   errormsg("[OS] ERROR: failed to generate a UUID\n");
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

const char* ipc_unix_domain_init(void)
{
   if (gen_uuid()) {
      errormsg("Error generating UUID\n");
      return NULL;
   }

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
   assert(strlen(uuidstr) < sizeof(sockpath) - 2);
   memcpy(&sockpath[1], uuidstr, strlen(uuidstr)+1);
   */

   strcpy(sockpath, uuidstr);

#elif defined (__APPLE__)
   strcpy(sockpath, "/tmp/reshop_ipc-");
   memcpy(&sockpath[strlen(sockpath)], uuidstr, strlen(uuidstr)+1);

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
      size_t n = snprintf(&sockpath[ret], expected_sz, "%s%s", "reshop_ipc-", uuidstr);

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

static inline bool guiackreq(rhpfd_t fd, MessageType mtyp, MessageType req) 
{
   MessageHeader header;

   ssize_t bytes_read = read(fd, &header, sizeof(header));
   if (bytes_read == -1) { goto ipc_error_read; }
   if (bytes_read != sizeof(header))  { goto ipc_error_readheader; }
   if (header.type != mtyp) { goto ipc_error_noguiack; }
   if (header.reserved[0] != req) { goto ipc_error_noreq; }

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
         msgtype2str(mtyp), msgtype2str(header.type));
   return false;
   }
ipc_error_noreq: 
   {
   error("\n[IPC] ERROR while sending model: expected GUI to request %s; got %s\n", 
         msgtype2str(req), msgtype2str(header.reserved[0]));
   return false;
   }
}

static inline bool guiack(rhpfd_t fd, MessageType mtyp) 
{
   MessageHeader header;

   ssize_t bytes_read = read(fd, &header, sizeof(header));
   if (bytes_read == -1) { goto ipc_error_read; }
   if (bytes_read != sizeof(header))  { goto ipc_error_readheader; }
   if (header.type != mtyp) { goto ipc_error_noguiack; }

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
         msgtype2str(mtyp), msgtype2str(header.type));
   return false;
   }

}

/**
 * @brief Send a Model over an FD
 *
 * @param mdl the model to send
 * @param fd  the fp
 *
 * @return    the error code
 */
int ipc_send_mdl(Model *mdl, rhpfd_t fd)
{
  /* ----------------------------------------------------------------------
   * Protocol:
   * --->    ReSHOP sends a ModelGui struct
   * <---    GUI ACKs and ask for a model name
   * --->    ReSHOP sends the model name
   * <---    GUI ACKs
   *
   * If there is an EMPDAG:
   * --->    ReSHOP sends a basic EmpDagGui
   * The function newempdag is called to receive it
   *
   * If there is no EMPDAG:
   * --->    ReSHOP sends an end of transmission
   * The processing is done and we return to the callee
   * ---------------------------------------------------------------------- */

   MessageHeader header;
   MessagePayload payload;

   payload.load.header = (MessageHeader){ModelGuiBasicDataSize, NewModelStart, {0}, {0} };
   payload.load.mdl.backend = mdl->backend;
   payload.load.mdl.type = mdl->commondata.mdltype;
   payload.load.mdl.id = mdl->id;
   payload.load.mdl.nvars = mdl_nvars(mdl);
   payload.load.mdl.nequs = mdl_nequs(mdl);

   if (write(fd, &payload, NewModelStartPayload) == -1) { goto ipc_error_write; }
   if (!guiackreq(fd, GuiAckReq, NewModelName)) { goto ipc_error_guiack; }

   /* Send model name */
   const char *mdlname = mdl_getname(mdl);
   size_t mdlnamelen = strlen(mdlname)+1; assert(mdlnamelen < SimpleLoadSize);
   payload.load.header.type = NewModelName; payload.load.header.length = mdlnamelen;
   memcpy(payload.load.load, mdlname, mdlnamelen);

   if (write(fd, &payload, sizeof(MessageHeader)+mdlnamelen) == -1) { goto ipc_error_write; }
   if (!guiack(fd, GuiAck)) { goto ipc_error_guiack; }

   /* Send EMPDAG, if any */
   if (empinfo_hasempdag(&mdl->empinfo)) {
      const EmpDag *empdag = &mdl->empinfo.empdag;

      /* ----------------------------------------------------------------------
       * Protocol:
       * --->    ReSHOP sends a EmpDagGui struct
       * <---    GUI acks
       *
       * --->    ReSHOP sends the MathPrgms
       * <---    Gui acks
       * --->    ReSHOP sends the MathPrgm names
       * <---    GUI acks
       *
       * --->    ReSHOP sends the Nashs
       * <---    GUI acks
       * --->    ReSHOP sends the Nash names
       * <---    GUI acks
       *
       * ---------------------------------------------------------------------- */

      payload.load.header = (MessageHeader){EmpDagGuiBasicDataSize, NewEmpDagStart, {0}, {0} };
      payload.load.empdag.mdlid = mdl->id;
      payload.load.empdag.type = empdag->type;
      payload.load.empdag.stage = empdag->stage;
      payload.load.empdag.nodestats = empdag->node_stats;
      payload.load.empdag.arcstats = empdag->arc_stats;

      if (write(fd, &payload, NewEmpDagStartPayload) == -1) { goto ipc_error_write; }
      if (!guiack(fd, GuiAck)) { goto ipc_error_guiack; }

      u32 num_mps = empdag->node_stats.num_mps;

      /* -------------------------------------------------------------------
       * Sends the MPs
       * ------------------------------------------------------------------- */

      MathPrgm **mps_arr = empdag->mps.arr;
      u32 i = 0;
      do {
         u32 nmps = 0, nmps_max = MIN(MathPrgmGuiMax, num_mps-i);

         for (; nmps < nmps_max; ++nmps, ++i) {
            MathPrgm *mp = mps_arr[i];
            payload.load.mps[nmps].id = mp->id;
            payload.load.mps[nmps].sense = mp->sense;
            payload.load.mps[nmps].type = mp->type;
            payload.load.mps[nmps].status = mp->status;
            payload.load.mps[nmps].nparents =  empdag->mps.rarcs->len;
            payload.load.mps[nmps].nchildren = empdag->mps.Carcs[i].len + empdag->mps.Varcs[i].len;
            payload.load.mps[nmps].status = mp->status;

            memcpy(&payload.load.mps[nmps], mps_arr[nmps], MpGuiBasicDataSize);
         }

         payload.load.header.type = i < num_mps ? MathPrgmGuiData : MathPrgmGuiDataEnd;
         payload.load.header.length = nmps;

         size_t sz = offsetof(MessagePayload, load.mps[nmps_max]);
         assert(sz + (uintptr_t)(&payload) == (uintptr_t)(&payload.load.mps[nmps_max]));

         if (write(fd, &payload, sizeof(MessageHeader)+sz) == -1) { goto ipc_error_write; }
         if (!guiack(fd, GuiAck)) { goto ipc_error_guiack; }

      } while (i < num_mps);

      /* FIXME: transmit the name struct rather than each name individually */
      /* We transmit this serially */

      for (i = 0; i < num_mps; ++i) {
         payload.load.header.type = MathPrgmGuiName;
         const char *mpname = empdag_getmpname(empdag, i);
         size_t namelen = strlen(mpname) + 1;
         assert(namelen <= StrMaxLen);
         payload.load.header.length = namelen;
         memcpy(payload.load.name, mpname, namelen);

         if (write(fd, &payload, sizeof(MessageHeader)+namelen) == -1) { goto ipc_error_write; }
         if (!guiack(fd, GuiAck)) { goto ipc_error_guiack; }
      }

      /* -------------------------------------------------------------------
       * Sends the Nashs
       * ------------------------------------------------------------------- */

      u32 num_nashs = empdag->node_stats.num_nashs;

      Nash **nashs_arr = empdag->nashs.arr;
      i = 0;
      while (i < num_nashs) {
         u32 nnashs = 0, nnashs_max = MIN(NashGuiMax, num_nashs-i);

         for (; nnashs < nnashs_max; ++nnashs, ++i) {
            Nash *nash = nashs_arr[i];
            payload.load.nashs[nnashs].id = nash->id;
            payload.load.nashs[nnashs].nparents  = empdag->nashs.rarcs[i].len;
            payload.load.nashs[nnashs].nchildren = empdag->nashs.arcs[i].len;
         }

         payload.load.header.type = i < num_nashs ? NashGuiData : NashGuiDataEnd;
         payload.load.header.length = nnashs;

         size_t sz = offsetof(MessagePayload, load.nashs[nnashs_max]);
         assert(sz + (uintptr_t)(&payload) == (uintptr_t)(&payload.load.nashs[nnashs_max]));

         if (write(fd, &payload, sizeof(MessageHeader)+sz) == -1) { goto ipc_error_write; }
         if (!guiack(fd, GuiAck)) { goto ipc_error_guiack; }

      }
   }

   header = (MessageHeader){ 0, NewModelEnd, {0}, {0} };
   if (write(fd, &header, sizeof(header)) == -1) { goto ipc_error_write; }
   if (!guiack(fd, GuiAck)) { goto ipc_error_guiack; }

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
