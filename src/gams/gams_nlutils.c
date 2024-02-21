#include <math.h>
#include <stdio.h>
#include <stdint.h>

#include "container.h"
#include "gams_nlutils.h"
#include "instr.h"
#include "macros.h"
#include "printout.h"
#include "status.h"

#include "gmomcc.h"
#include "gevmcc.h"
#include "dctmcc.h"
#include "optcc.h"
#include "cfgmcc.h"

#define IO_READ(expr, filename) do { size_t info = expr; if (!info) { printf("IO error while reading %s\n", filename); goto _exit; } } while (0);

static uint32_t read_field(FILE* f)
{
   uint8_t len;
   IO_READ(fread(&len, sizeof(uint8_t), 1, f), "");
   switch (len)
   {
   case 0:
   {
      uint8_t buf;
      IO_READ(fread(&buf, sizeof(buf), 1, f), "");
      return buf;
   }
   case 1:
   {
      uint16_t buf;
      IO_READ(fread(&buf, sizeof(buf), 1, f), "");
      return buf;
   }
   case 2:
   {
      uint32_t buf;
      IO_READ(fread(&buf, sizeof(buf), 1, f), "");
      return buf;
   }
   default:
      printf("unexpected len %d\n", len);
   }
   return INT32_MAX;

_exit:
   return 0;
}

struct gams_opcodes_file* gams_read_opcode(const char* filename, double **pool)
{
   FILE* f = fopen(filename, "r");
   struct gams_opcodes_file* equs = NULL;

   if (!f) {
      printf("Could not open file %s\n", filename);
      return NULL;
   }

   if (setvbuf(f, NULL, _IOFBF, 1024*4)) {
      perror("setvbuf");
      printf("Error in setting larger buffer for %s\n", filename);
      goto _exit;
    }

   unsigned char opcode;
   uint32_t val, len;
   IO_READ(fread(&opcode, sizeof(opcode), 1, f), filename);

   unsigned max_equ = 10;
   unsigned max_opcodes = 10;
   unsigned equ_indx = 0;

   size_t cur_indx = 0;

   CALLOC_EXIT_NULL(equs, struct gams_opcodes_file, 1);

   MALLOC_EXIT_NULL(equs->indx_start, size_t, max_equ);
   MALLOC_EXIT_NULL(equs->instrs, int, max_opcodes);
   MALLOC_EXIT_NULL(equs->args, int, max_opcodes);
   equs->pool = NULL;
   if (!equs->indx_start) goto _exit;
   if (!equs->instrs) goto _exit;
   if (!equs->args) goto _exit;

   while (opcode != nlEnd)
   {
      if(opcode != nlHeader)
      {
         puts("malformed file, expecting Header\n");
         if (opcode == nlNoOp) //&& cur_indx > 1 && code[cur_indx-1].opcode == nlStore)
         {
            // consume 3 more ?
            IO_READ(fread(&len, sizeof(opcode), 3, f), filename);
            goto _end;
         }
         else
            goto _exit;
      }

      len = read_field(f);

      if (max_equ <= equ_indx)
      {
         max_equ *= 2;
         REALLOC_NULL(equs->indx_start, size_t, max_equ);
         if (!equs->indx_start) goto _exit;
      }

      if (max_opcodes < cur_indx + len)
      {
         max_opcodes = MAX(2*max_opcodes, cur_indx + len);
         REALLOC_NULL(equs->instrs, int, max_opcodes);
         if (!equs->instrs) goto _exit;
         REALLOC_NULL(equs->args, int, max_opcodes);
         if (!equs->args) goto _exit;
//         printf("realloc %d\n", max_opcodes);
      }
      equs->indx_start[equ_indx] = cur_indx;
      equ_indx++;

      int *instrs = equs->instrs;
      int *args = equs->args;

      instrs[cur_indx] = nlHeader;
      args[cur_indx] = len;
      cur_indx++;

      for (size_t i = 1; i < len; ++i)
      {
         IO_READ(fread(&opcode, sizeof(opcode), 1, f), filename);
         if (opcode == nlEnd || opcode == nlHeader)
         {
            puts("malformed file, unexpected Header or End");
            goto _exit;
         }
         val = read_field(f);
         instrs[cur_indx] = opcode;
         args[cur_indx] = val;

         switch (opcode) {
         case nlPushI:
         case nlAddI:
         case nlSubI:
         case nlMulI:
         case nlDivI:
         case nlMulIAdd:
            equs->min_pool_len = MAX(val, equs->min_pool_len);
            break;
         default:
            break;
         }

         cur_indx++;
      }

      IO_READ(fread(&opcode, sizeof(opcode), 1, f), filename);
   }


   // consume field of nlEnd
   read_field(f);

   IO_READ(fread(&len, sizeof(len), 1, f), filename);
   if (len) printf("Should be 0: %u :: ", len);

_end:
   IO_READ(fread(&len, sizeof(len), 1, f), filename);
   if (!len) { printf("Should be nonzero: %u\n", len); goto _exit; }

   MALLOC_EXIT_NULL((*pool), double, len);
   equs->pool = *pool;
   equs->pool_len = len;

   for (size_t i = 0; i < len; ++i)
   {
      double v;
      IO_READ(fread(&v, sizeof(v), 1,f), filename);
      (*pool)[i] = v;
   }

   while (!feof(f))
   {
      double v;
      if (!fread(&v, sizeof(v), 1,f)) {
         if (ferror(f)) {
            printf("Error reading file %s\n", filename);
         }
      } else {
      if (fabs(v) > 0.) printf("%e\n", v);
      }
   }

   fclose(f);

   equs->nb_equs = equ_indx;
   equs->len_opcode = cur_indx;

   return equs;

_exit:
   fclose(f);

   if (equs) {
      if (equs->pool) {
         FREE(equs->pool);
         equs->pool = NULL;
      }

      FREE(equs->indx_start);
      FREE(equs->instrs);
      FREE(equs->args);
      FREE(equs);
   }
   return NULL;
}

int gams_write_opcode(struct gams_opcodes_file* equs, const char* filename)
{
   TO_IMPLEMENT("");
#if 0
   FILE* f = fopen(filename, "a+");

   if (!f)
   {
      printf("Could not open/create file %s\n", filename);
      return Error_FileOpenFailed;
   }

   size_t status;

   if (!(status = fwrite(equs->nlcode, sizeof(char), 3*equs->len_nlcode, f))) {
         return Error_ExecFailed;
   }

   unsigned char end = nlEnd;
   if (!(status = fwrite(&end, sizeof(unsigned char),1, f)))
   {
      return Error_ExecFailed;
   }

   fclose(f);

   return OK;
#endif
}

void gams_opcodes_file_dealloc(struct gams_opcodes_file* equs)
{
   if (equs->instrs) { FREE(equs->instrs); equs->instrs = NULL; }
   if (equs->args) { FREE(equs->args); equs->args = NULL; }
   if (equs->indx_start) { FREE(equs->indx_start); equs->indx_start = NULL; }
   if (equs->pool) {
      FREE(equs->pool);
      equs->pool = NULL;
   }
   FREE(equs);
}
