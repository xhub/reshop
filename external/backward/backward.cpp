// Pick your poison.
//
// On GNU/Linux, you have few choices to get the most out of your stack trace.
//
// By default you get:
//	- object filename
//	- function name
//
// In order to add:
//	- source filename
//	- line and column numbers
//	- source code snippet (assuming the file is accessible)

// Install one of the following library then uncomment one of the macro (or
// better, add the detection of the lib and the macro definition in your build
// system)

// - apt-get install libdw-dev ...
// - g++/clang++ -ldw ...
#define BACKWARD_HAS_DW 1

// - apt-get install binutils-dev ...
// - g++/clang++ -lbfd ...
// #define BACKWARD_HAS_BFD 1

#include "backward.hpp"
#include "bck_dotrace.h"


void* bck_regsig(void)
{
   backward::SignalHandling *sh(new backward::SignalHandling);
   return static_cast<void*>(sh);
}

void bck_clean(void* obj)
{
   delete static_cast<backward::SignalHandling*>(obj);
}

void bck_dotrace(void)
{
   using namespace backward;
   StackTrace st;

   st.load_here(32);    // load a backtrace of 32
   st.skip_n_firsts(3); // skip the internal backtrace stuff

   Printer p;
   p.object = true;
   p.color_mode = ColorMode::always;
   p.address = true;
   p.print(st, stderr);
}
