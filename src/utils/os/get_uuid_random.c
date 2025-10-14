#include "reshop_config.h"

#include "get_uuid_random.h"
#include "printout.h"
#include "tlsdef.h"

tlsvar char uuidv4str[37]; // uuid for socket path
 
#if defined(_WIN32) // Windows
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <objbase.h>

const char* gen_random_uuidv4(void)
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

   for (; i < sizeof(uuidv4str) && str[i] != '\0'; ++i) {
      uuidv4str[i] = (char)str[i];
   }

   if (i >= sizeof(uuidv4str)) {
      error("[OS] ERROR: UUID '%s' is too long, max length is %zu. Please file bug report\n",
            str, sizeof(uuidv4str));
      RpcStringFree(&str);
      return NULL;
   }

   uuidv4str[i] = '\0';

   RpcStringFree(&str);

   return uuidv4str;

_err:
   errormsg("[OS] ERROR: failed to generate a UUID\n");
   return NULL;
}

#elif defined(__linux__) || defined(__APPLE__) // Linux and macOS

/* This needs at least Sierra (10.12) on MacOs and glibc 2.25 / Linux 3.17 */
#include <sys/random.h>

const char* gen_random_uuidv4(void)
{
    uint8_t uuid[16];

    if (getentropy(uuid, sizeof(uuid)) != 0) {
        return NULL;
    }

    // Adjust the UUID to conform to RFC 4122
    uuid[6] = (uuid[6] & 0x0F) | 0x40; // Version 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80; // Variant 1

    // Format the UUID as a string
    snprintf(uuidv4str, sizeof(uuidv4str),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
             uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

   return uuidv4str;
}

#else

#error "Unsupported platform"

#endif
