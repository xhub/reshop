#include "reshop_config.h"

#include <stdlib.h>

#include "get_uuid_library.h"
#include "tlsdef.h"

#ifdef __linux__

/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <dlfcn.h>
#include <link.h>
#include <stddef.h>
#include <string.h>

#include "macros.h"

#ifndef NT_GNU_BUILD_ID
#define NT_GNU_BUILD_ID 3
#endif

#ifndef ElfW
#define ElfW(type) Elf_##type
#endif

#define ALIGN(val, align)       (((val) + (align) - 1) & ~((align) - 1))

struct build_id_note {
    ElfW(Nhdr) nhdr;

    char name[4];
    uint8_t build_id[];
};

enum tag { BY_NAME, BY_SYMBOL };

struct callback_data {
    union {
        const char *name;

        /* Base address of shared object, taken from Dl_info::dli_fbase */
        const void *dli_fbase;
    };
    enum tag tag;
    struct build_id_note *note;
};

static int
build_id_find_nhdr_callback(struct dl_phdr_info *info, UNUSED size_t size, void *data_)
{
    struct callback_data *data = data_;

    if (data->tag == BY_NAME) {

      if (!info->dlpi_name) { return 0; }

      /* dlpi_name might be a full path. We can't be that precise and are just happy
       * with the library name */

      const char * basename = strrchr(info->dlpi_name, '/');
      const char * dlpi_name = basename ? basename+1 : info->dlpi_name;

      if (strcmp(dlpi_name, data->name) != 0) { return 0; }
   }

    if (data->tag == BY_SYMBOL) {
       /* Calculate address where shared object is mapped into the process space.
        * (Using the base address and the virtual address of the first LOAD segment)
        */
       void *map_start = NULL;
       for (unsigned i = 0; i < info->dlpi_phnum; i++) {
          if (info->dlpi_phdr[i].p_type == PT_LOAD) {
             map_start = (void *)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
             break;
          }
       }

       if (map_start != data->dli_fbase) { return 0; }
    }

    for (unsigned i = 0; i < info->dlpi_phnum; i++) {
        if (info->dlpi_phdr[i].p_type != PT_NOTE) {
            continue;
        }

        struct build_id_note *note = (void *)(info->dlpi_addr +
                                              info->dlpi_phdr[i].p_vaddr);
        ptrdiff_t len = info->dlpi_phdr[i].p_filesz;

        while (len >= sizeof(struct build_id_note)) {
            if (note->nhdr.n_type == NT_GNU_BUILD_ID &&
                note->nhdr.n_descsz != 0 &&
                note->nhdr.n_namesz == 4 &&
                memcmp(note->name, "GNU", 4) == 0) {
                data->note = note;
                return 1;
            }

            size_t offset = sizeof(ElfW(Nhdr)) +
                            ALIGN(note->nhdr.n_namesz, 4) +
                            ALIGN(note->nhdr.n_descsz, 4);
            note = (struct build_id_note *)((uintptr_t)note + offset);
            len -= offset;
        }
    }

    return 0;
}

static const struct build_id_note *
build_id_find_nhdr_by_name(const char *name)
{
    struct callback_data data = {
        .tag = BY_NAME,
        .name = name,
        .note = NULL,
    };

    if (!dl_iterate_phdr(build_id_find_nhdr_callback, &data)) {
        return NULL;
    }

    return data.note;
}

UNUSED static const struct build_id_note *
build_id_find_nhdr_by_symbol(const void *symbol)
{
    Dl_info info;

    if (!dladdr(symbol, &info)) { return NULL; }

    if (!info.dli_fbase) { return NULL; }

    struct callback_data data = {
        .tag = BY_SYMBOL,
        .dli_fbase = info.dli_fbase,
        .note = NULL,
    };

    if (!dl_iterate_phdr(build_id_find_nhdr_callback, &data)) { return NULL; }

    return data.note;
}

static ElfW(Word)
build_id_length(const struct build_id_note *note)
{
    return note->nhdr.n_descsz;
}

static const uint8_t *
build_id_data(const struct build_id_note *note)
{
    return note->build_id;
}

const char *get_uuid_library(void)
{
   static tlsvar char uuid[128];
   const struct build_id_note *note = build_id_find_nhdr_by_name("libreshop.so");

   if (!note) {
      strcpy(uuid, "No build-id note found");
      return uuid;
   }

   ElfW(Word) len = build_id_length(note);

   const uint8_t *build_id = build_id_data(note);

   size_t curlen = 0;
    for (ElfW(Word) i = 0; i < len; i++) {
        curlen += snprintf(uuid + curlen, sizeof(uuid)-curlen, "%02x", build_id[i]);
    }

   return uuid;
}

#elif defined(__APPLE__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid/uuid.h> // For uuid_unparse
#include <mach-o/loader.h> // Definitions for Mach-O headers and load commands
#include <mach-o/dyld.h>   // Functions for accessing loaded images

/**
 * @brief Retrieves the UUID of a specified loaded Mach-O image (executable or library).
 *
 * This function iterates through all loaded Mach-O images in the current
 * process's address space. If 'image_name' is NULL, it returns the UUID of the
 * main executable (image index 0). Otherwise, it searches for a matching name.
 *
 * @param image_name Optional name of the library or executable (e.g., "libcurl.dylib"). Pass NULL for the main executable.
 * @param uuid_str Output buffer (must be at least 37 bytes long) to store the UUID string.
 * @return int 0 on success, -1 on failure.
 */
static int get_process_uuid(const char *image_name, char *uuid_str) {
    uint32_t image_count = _dyld_image_count();
    
    // Determine target image name
    const char *target_name = image_name;
    int search_all = (target_name != NULL);
    
    // 1. Iterate through all loaded images
    for (uint32_t image_index = 0; image_index < image_count; ++image_index) {
        
        // Skip image if we are only searching the main executable and this isn't it
        if (!search_all && image_index != 0) {
            continue;
        }

        const char *current_image_path = _dyld_get_image_name(image_index);
        const struct mach_header *header = _dyld_get_image_header(image_index);

        if (header == NULL) continue;

        // If searching a specific name, check if the current path contains that name
        if (search_all) {
            if (current_image_path == NULL || strstr(current_image_path, target_name) == NULL) {
                continue;
            }
        }
        
        // Determine the header size based on 32-bit or 64-bit magic number
        const uint32_t cmd_count = header->ncmds;
        size_t header_size = (header->magic == MH_MAGIC ? sizeof(struct mach_header) : sizeof(struct mach_header_64));
        const char *p = (const char *)header + header_size;

        // 2. Iterate through all load commands (LC_...) for the matched image
        for (uint32_t i = 0; i < cmd_count; ++i) {
            const struct load_command *lc = (const struct load_command *)p;

            // 3. Check for the LC_UUID command
            if (lc->cmd == LC_UUID) {
                const struct uuid_command *uuid_cmd = (const struct uuid_command *)lc;

                // 4. Extract and format the UUID
                uuid_unparse(uuid_cmd->uuid, uuid_str);
                
                if (search_all) {
                    printf("Found UUID for '%s' at path: %s\n", target_name, current_image_path);
                }
                return 0; // Success
            }

            // Move the pointer to the next load command
            p += lc->cmdsize;
        }
        
        // If we were only looking for the main executable (index 0), we can stop here
        if (!search_all && image_index == 0) {
            break;
        }
    }

    fprintf(stderr, "Error: LC_UUID command not found for the specified image or main executable.\n");
    return -1;
}

const char *get_uuid_library(void)
{
   static tlsvar char uuid_buffer[37]; // UUID string is 36 chars + null terminator

    if (get_process_uuid("libreshop.dylib", uuid_buffer) == 0) {
        return uuid_buffer;
    }

   strcpy(uuid_buffer, "Failed to retrieve libreshop UUID");

   return uuid_buffer;
}


#elif defined (_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <stdio.h>
#include <tchar.h>

#include "win-dbghelp.h"

static void printWinErrMsg(void)
{
   char* lpMsgBuf;
   DWORD dw = GetLastError();

   DWORD szMsg = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                FORMAT_MESSAGE_FROM_SYSTEM |
                                FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                (char*)&lpMsgBuf, 0, NULL);

   if (szMsg > 0) {
      (void)fprintf(stderr, "code: %ld; msg = %s", dw, lpMsgBuf);
      free(lpMsgBuf);
   } else {
      (void)fprintf(stderr, "code: %ld; no error message\n", dw);
   }
}

/**
 * @brief Retrieves the CodeView GUID and Age of a specified loaded module (DLL or EXE).
 *
 * This function locates the loaded module in memory and queries its debug information
 * using the DbgHelp API, which is necessary for symbol server lookups (PDB files).
 *
 * @param module_name The name of the module (e.g., "kernel32.dll" or the executable's name).
 * @param guid_str Output buffer (must be at least 37 bytes long) for the formatted GUID string.
 * @param age Output pointer for the PDB 'Age' value.
 * @return int 0 on success, -1 on failure.
 */
static int get_dll_codeview_guid(const char *module_name, unsigned len, char *guid_str, DWORD *age)
{
    HMODULE hModule = NULL;

    // 1. Locate the module in memory
    hModule = GetModuleHandleA(module_name);
    if (hModule == NULL) {
        (void)fprintf(stderr, "Error: Module '%s' not loaded or not found.\n", module_name);
        return -1;
    }
 
   const DbgHelpFptr* fptrs = dbghelp_get_fptrs();
   if (!fptrs) {
      return -1;
   }

   HANDLE process = dbghelp_get_process();

    // 3. Load the module into the symbol handler's context
    // This step is often necessary to ensure DbgHelp has the freshest module info
    if ((fptrs->pSymLoadModule64(process, NULL, (PCSTR)module_name, NULL, (DWORD64)hModule, 0) == 0)
      && GetLastError() != ERROR_SUCCESS) {
        // SymLoadModule64 failure is often non-fatal for this specific check, continue...
      (void)fprintf(stderr, "Warning: SymLoadModule64 failed: ");
      printWinErrMsg();
    }

    // 4. Use SymGetModuleInfo64 to query the module details
    IMAGEHLP_MODULE64 moduleDetails = {0};
    moduleDetails.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);

    if (fptrs->pSymGetModuleInfo64(process, (DWORD64)hModule, &moduleDetails)) {
      // PDBSig70 is a 16-byte structure (GUID). Convert it to string.
      // The format string "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX" is complex.
      // Using a standard Win32 function like StringFromGUID2 is safer, 
      // but for simple C, we stick to snprintf if possible, or manually formatting the PDB structure.

      // The PDB structure is usually a raw GUID plus an Age.
      // We'll use the GUID returned in the PDB's debug header block.

      // Format the string as GUID,Age
      // GUID is stored as moduleDetails.PdbSig70 (a GUID struct).
      GUID* guid = &moduleDetails.PdbSig70;

      (void)snprintf(guid_str, len, 
                     "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX,%lu",
                     guid->Data1, guid->Data2, guid->Data3, 
                     guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3], 
                     guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7],
                     moduleDetails.PdbAge);

      *age = moduleDetails.PdbAge;
      return 0; // Success
   }

   (void)fprintf(stderr, "Error: SymGetModuleInfo64 failed for module '%s': ", module_name);
   printWinErrMsg();

   return -1;
}

#ifdef _MSCVER
#define RESHOPDLL "reshop.dll"
#else
#define RESHOPDLL "libreshop.dll"
#endif

const char *get_uuid_library(void)
{
    static tlsvar char guid_buffer[37 + 20]; 
    DWORD age = 0;
    const char *target_module = RESHOPDLL;

    if (!get_dll_codeview_guid(target_module, sizeof(guid_buffer), guid_buffer, &age)) {
      return guid_buffer;
    }

   strcpy(guid_buffer, "Failed to retrieve CodeView GUID for " RESHOPDLL);

    return guid_buffer;
}

#else

#endif
