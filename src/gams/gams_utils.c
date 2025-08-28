#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "gams_utils.h"
#include "instr.h"
#include "printout.h"

#include "gmomcc.h"
#include "gevmcc.h"

/* As of 0.4.4, icl produces broken code for the function below
 * Adding printf statement in the while loop makes the problem go away
 * No bandwidth to investigate this as ICL has been unsupported by intel
 * as of 2022
 */
#ifdef __ICL
#pragma optimize("", off)
#endif

static void sub_brackets(char *str, size_t len, char c_open, char c_close)
{
   char *end = str+len-1;
   while (*end == c_close && end > str) {
      len--;
      *end-- = '\0';
   }

   char *parenthesis = strchr(str, c_open);
   while (parenthesis) {
      // Adding this printf also unbreaks icl ...
      //printf("DEBUG: str is '%s'\n", str);
      *parenthesis++ = '_';
      while (*parenthesis != c_close && *parenthesis != '\0') {
         if (*parenthesis == ',') {
            *parenthesis = '_';
         }
         parenthesis++;
      }
      if (*parenthesis != '\0') {
         parenthesis[0] = '_';
      }

      parenthesis = strchr(str, c_open);
   }

}

#ifdef __ICL
#pragma optimize("", on)
#endif

/* Per https://www.gams.com/latest/docs/UG_GAMSPrograms.html#UG_GAMSPrograms_Identifiers */
UNUSED static bool valid_gams_identifier(const char * restrict name)
{
   const char *name_bck = name;

   /* First character must be a letter */
   char c = *name++;
   if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z')) { 
         error("[GAMS] ERROR: invalid GAMS identifier '%s': first character must "
               "be a letter, got '%c'\n", name_bck, c);
      return false;
   }

   while (*name != '\0') {
      c = *name++;

      if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
          !(c >= '0' && c <= '9') && !(c == '_')) {
         error("[GAMS] ERROR: invalid GAMS identifier '%s': character '%c' is "
               "invalid", name_bck, c);
         return false;
      }
   }

   return true;
}


void gams_fix_equvar_names(char * restrict name)
{
   assert(name && name[0] != '\0');
   size_t len = strlen(name);

   /* TODO: document why we do that complicated thing rather than just substituting
    * '[' with _ and so forth */
   sub_brackets(name, len, '(', ')');
   sub_brackets(name, len, '[', ']');

 
   /* 2023.11.10: we are currently putting the UEL in the symbol name, so the
    * above doesn't work. GITLAB #113*/
   while (*name != '\0') {

      char c = *name;
      if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
          !(c >= '0' && c <= '9') && !(c == '_')) {

         *name = '_';
      }

      name++;
   }

   //assert(valid_gams_identifier(name));
}

void gams_fix_quote_names(const char *name, char *out)
{
   size_t len = strlen(name);
   char *outptr = out;
   const char *old_name = name;

   char *parenthesis = strchr(name, '(');
   while (parenthesis) {
      ptrdiff_t cpylen = parenthesis-old_name + 1;
      memcpy(outptr, old_name, (size_t)cpylen);
      outptr += cpylen;
      parenthesis++;

      outptr[0] = '\'';
      outptr++;
      while (*parenthesis != ')') {
         if (*parenthesis == ',') {
            outptr[0] = '\'';
            outptr[1] = ',';
            outptr[2] = '\'';
            outptr += 3;
         } else {
            *outptr = *parenthesis;
            outptr++;
         }
         parenthesis++;
      }

      outptr[0] = '\'';
      outptr[1] = ')';
      outptr += 2;
      parenthesis++;

      old_name = parenthesis;
      parenthesis = strchr(old_name, '(');
   }

   if (outptr == out) {
      memcpy(out, name, len+1);
   } else {
      *outptr = '\0';
   }
}

/**
 * @brief Translate the GAMS opcode when we fix a variable to a value
 *
 * @param  opcode  the opcode we want to translate, that should involve a
 *                 variable
 *
 * @return         the corresponding opcode involving a constant (if it exists)
 *                 -1 if the argument was nlUMinV
 *                 -2 if the argument has no variable as argument
 */
int gams_opcode_var_to_cst(int opcode)
{
   switch (opcode) {
      case nlPushV:
        return nlPushI;
      case nlAddV:
        return nlAddI;
      case nlSubV:
        return nlSubI;
      case nlMulV:
        return nlMulI;
      case nlDivV:
        return nlDivI;
      case nlUMinV:
        return -1;
      default:
         error("[GAMS] ERROR: unsupported opcode %d '%s'\n", opcode,
               nlinstr2str(opcode));
         return -2;
   }
}

int gams_chk_str(const char *str, const char *fn)
{
   if (!str) {
      error("%s ERROR: the given string object is NULL!\n", fn);
      return Error_NullPointer;
   }

   size_t len = strlen(str);
   if (len >= GMS_SSSIZE) {
      error("%s ERROR: string '%s' is too long, max length is %d\n", fn, str,
            GMS_SSSIZE);
      return Error_NameTooLongForGams;
   }

   return OK;
}

const char *gams_equtype_name(int equtype)
{
   switch(equtype) {
   case gmoequ_E:
      return "equality (=E=)";
   case gmoequ_L:
      return "less-than (=L=)";
   case gmoequ_G:
      return "greater-than (=G=)";
   case gmoequ_N:
      return "free (=N=)";
   case gmoequ_X:
      return "external (=X=)";
   case gmoequ_C:
      return "conic (=C=)";
   case gmoequ_B:
      return "logic (=B=)";
   default:
      return "ERROR unknonwn type";
   }
}
