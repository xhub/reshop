/** Copyright (C) GAMS Development and others
  * All Rights Reserved.
  * This code is published under the Eclipse Public License.
  *
  * @file optreshop.cpp
  *
  * @author Olivier Huber
 */

#include <cstdarg>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include "GamsOptionsSpecWriter.hpp"

#ifndef OPTRESHOP_CPP
extern "C" {
#endif

#include "rhp_options.h"
#include "status.h"
#include "option_priv.h"


void printstr(unsigned mode, const char *str);
void printout(unsigned mode, const char *format, ...);
void backtrace_(const char *expr, int status);

void printstr(unsigned mode, const char *str)
{
   puts(str);
}

void printout(unsigned mode, const char *format, ...)
{
      va_list ap;
      char *buf = NULL;
      int rc = 0;

      va_start(ap, format);
      rc = vsnprintf(buf, rc, format, ap);
      va_end(ap);

      if (rc <= 0) {
         return;
      }
      rc++; /* for '\0' */
      buf = (char*)malloc(rc);
      if (!buf) return;

      va_start(ap, format);
      rc = vsnprintf(buf, rc, format, ap);
      va_end(ap);

      if (rc <= 0) {
         free(buf);
         return;
      }


      free(buf);
}

void backtrace_(const char *expr, int status)
{
   return;
}

#ifndef OPTRESHOP_CPP
} // end of extern "C"
#endif


// Note that we keep the advanced stuff to make it easier to implement that in
// the future

int main(int argc, char** argv)
{
   /* It is necessary to initialize the options in a platform without C constructor support */
   option_init();

   GamsOptions gmsopt("ReSHOP");
   gmsopt.setSeparator("=");
   gmsopt.setGroup("reshop");
   gmsopt.setEolChars("#");

   OptIterator iter = {0};
   struct option *opt;

   while ((opt = opt_iter(&iter)), opt != nullptr) {

      std::string defaultlongdescr;

      switch (opt->type) {

      case OptBoolean:
         gmsopt.collect(opt->name, opt->description, defaultlongdescr,
            opt->value.b).advanced = false;
         break;

      case OptChoice: {

         GamsOption::EnumVals enumval;
         unsigned len;
         const char *const (*strs)[2];

         optchoice_getopts(opt, &strs, &len);

         for (unsigned i = 0; i < len; ++i) {
               enumval.append(strs[i][0], strs[i][1]);
         }

         gmsopt.collect(opt->name, opt->description, defaultlongdescr,
            optchoice_getdefaultstr(opt), enumval).advanced = false;
         break;
      }

      case OptInteger: {

         int min, max;
         if (optint_getrange(opt, &min, &max) != OK) { 
               std::cerr << "ERROR: fail to get range for option '" << opt->name << "'\n";
               exit(EXIT_FAILURE);
            }

         gmsopt.collect(opt->name, opt->description, defaultlongdescr,
            opt->value.i, min, max).advanced = false;
         break;
      }

      case OptDouble:

         gmsopt.collect(opt->name, opt->description, defaultlongdescr,
            opt->value.d, 0., std::numeric_limits<double>::infinity()).advanced = false;
         break;

      case OptString:

         gmsopt.collect(opt->name, opt->description, defaultlongdescr,
            std::string(opt->value.s)).advanced = false;
         break;
      }
   }

   gmsopt.finalize();

   gmsopt.writeDef();

   gmsopt.writeDoxygen(true);
   gmsopt.writeMarkdown();
}
