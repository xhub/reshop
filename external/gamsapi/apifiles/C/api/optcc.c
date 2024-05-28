/* C code 
 *
 * GAMS - Loading mechanism for GAMS Expert-Level APIs
 *
 * Copyright (c) 2016-2024 GAMS Software GmbH <support@gams.com>
 * Copyright (c) 2016-2024 GAMS Development Corp. <support@gams.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

#define OPT_MAIN
#include "optcc.h"

#if defined(_WIN32)
# include <windows.h>
  static char winErr[] = "Windows error";
  typedef HINSTANCE soHandle_t;
#else
# include <unistd.h>
# include <dlfcn.h>
# include <sys/utsname.h>
  typedef void *soHandle_t;
#endif

static soHandle_t h;
static int isLoaded = 0;
static int objectCount = 0;
static int ScreenIndicator = 1;
static int ExceptionIndicator = 0;
static int ExitIndicator = 1;
static optErrorCallback_t ErrorCallBack = NULL;
static int APIErrorCount = 0;

#if !defined(GC_NO_MUTEX)
#ifndef _GCMUTEX_
#define _GCMUTEX_

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifdef _WIN32
typedef CRITICAL_SECTION GC_mutex_t;
#else
typedef pthread_mutex_t  GC_mutex_t;
#endif

#ifdef __GNUC__
__attribute__((unused))
#endif
static int GC_mutex_init(GC_mutex_t* mx)
{
#ifdef _WIN32
   InitializeCriticalSection(mx);
   return 0;
#else
   return pthread_mutex_init(mx, NULL);
#endif
}

#ifdef __GNUC__
__attribute__((unused))
#endif
static void GC_mutex_delete(GC_mutex_t* mx)
{
#ifdef _WIN32
   DeleteCriticalSection(mx);
#else
   (void) pthread_mutex_destroy(mx);
#endif
   memset(mx, 0, sizeof(*mx));
}

#ifdef __GNUC__
__attribute__((unused))
#endif
static int GC_mutex_lock(GC_mutex_t* mx)
{
#ifdef _WIN32
   EnterCriticalSection(mx);
   return 0;
#else
   return pthread_mutex_lock(mx);
#endif
}

#ifdef __GNUC__
__attribute__((unused))
#endif
static int GC_mutex_unlock(GC_mutex_t* mx)
{
#ifdef _WIN32
   LeaveCriticalSection(mx);
   return 0;
#else
   return pthread_mutex_unlock(mx);
#endif
}

#endif /* _GCMUTEX_ */
static GC_mutex_t libMutex;
static GC_mutex_t objMutex;
static GC_mutex_t exceptMutex;

static int MutexIsInitialized = 0;

void optInitMutexes(void)
{
  int rc;
  if (0==MutexIsInitialized) {
    rc = GC_mutex_init (&libMutex);     if(0!=rc) optErrorHandling("Problem initializing libMutex");
    rc = GC_mutex_init (&objMutex);     if(0!=rc) optErrorHandling("Problem initializing objMutex");
    rc = GC_mutex_init (&exceptMutex);  if(0!=rc) optErrorHandling("Problem initializing exceptMutex");
    MutexIsInitialized = 1;
  }
}

void optFiniMutexes(void)
{
  if (1==MutexIsInitialized) {
    GC_mutex_delete (&libMutex);
    GC_mutex_delete (&objMutex);
    GC_mutex_delete (&exceptMutex);
    MutexIsInitialized = 0;
  }
}
#  define lock(MUTEX)   if(MutexIsInitialized) GC_mutex_lock (&MUTEX);
#  define unlock(MUTEX) if(MutexIsInitialized) GC_mutex_unlock (&MUTEX);
#else
#  define lock(MUTEX)   ;
#  define unlock(MUTEX) ;
void optInitMutexes(void) {}
void optFiniMutexes(void) {}
#endif

#if !defined(GAMS_UNUSED)
#define GAMS_UNUSED(x) (void)x;
#endif

typedef void (OPT_CALLCONV *XCreate_t) (optHandle_t *popt);
static OPT_FUNCPTR(XCreate);
typedef void (OPT_CALLCONV *XFree_t)   (optHandle_t *popt);
static OPT_FUNCPTR(XFree);
typedef int (OPT_CALLCONV *XAPIVersion_t) (int api, char *msg, int *cl);
static OPT_FUNCPTR(XAPIVersion);
typedef int (OPT_CALLCONV *XCheck_t) (const char *ep, int nargs, int s[], char *msg);
static OPT_FUNCPTR(XCheck);
typedef void (OPT_CALLCONV *optSetLoadPath_t) (const char *s);
OPT_FUNCPTR(optSetLoadPath);
typedef void (OPT_CALLCONV *optGetLoadPath_t) (char *s);
OPT_FUNCPTR(optGetLoadPath);

#define printNoReturn(f,nargs) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  XCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  optErrorHandling(d_msgBuf); \
}
#define printAndReturn(f,nargs,rtype) { \
  char d_msgBuf[256]; \
  strcpy(d_msgBuf,#f " could not be loaded: "); \
  XCheck(#f,nargs,d_s,d_msgBuf+strlen(d_msgBuf)); \
  optErrorHandling(d_msgBuf); \
  return (rtype) 0; \
}


/** Read definition file.
 * @param popt opt object handle
 * @param fn File name
 */
int  OPT_CALLCONV d_optReadDefinition (optHandle_t popt, const char *fn)
{
  int d_s[]={3,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(fn)
  printAndReturn(optReadDefinition,1,int )
}

/** Read definition from array of character
 * @param popt opt object handle
 * @param p PChar: pointer to first character
 */
int  OPT_CALLCONV d_optReadDefinitionFromPChar (optHandle_t popt, char *p)
{
  int d_s[]={3,10};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(p)
  printAndReturn(optReadDefinitionFromPChar,1,int )
}

/** Read parameters from file
 * @param popt opt object handle
 * @param fn File name
 */
int  OPT_CALLCONV d_optReadParameterFile (optHandle_t popt, const char *fn)
{
  int d_s[]={3,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(fn)
  printAndReturn(optReadParameterFile,1,int )
}

/** Read options from string. In the case of errors, messages will be added to the message queue (see OptGetMessage)
 * @param popt opt object handle
 * @param s String
 */
void  OPT_CALLCONV d_optReadFromStr (optHandle_t popt, const char *s)
{
  int d_s[]={0,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(s)
  printNoReturn(optReadFromStr,1)
}

/** Write modified parameters to a file. In the case of errors, messages will be added to the message queue (see OptGetMessage)
 * @param popt opt object handle
 * @param fn File name
 */
int  OPT_CALLCONV d_optWriteParameterFile (optHandle_t popt, const char *fn)
{
  int d_s[]={3,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(fn)
  printAndReturn(optWriteParameterFile,1,int )
}

/** Clear all messages stored in the message queue.
 * @param popt opt object handle
 */
void  OPT_CALLCONV d_optClearMessages (optHandle_t popt)
{
  int d_s[]={0};
  GAMS_UNUSED(popt)
  printNoReturn(optClearMessages,0)
}

/** Add a message to the message queue
 * @param popt opt object handle
 * @param info Message string
 */
void  OPT_CALLCONV d_optAddMessage (optHandle_t popt, const char *info)
{
  int d_s[]={0,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(info)
  printNoReturn(optAddMessage,1)
}

/** Read from message queue
 * @param popt opt object handle
 * @param NrMsg Number of messages in message queue
 * @param info Message string
 * @param iType Message type value (see enumerated constants)
 */
void  OPT_CALLCONV d_optGetMessage (optHandle_t popt, int NrMsg, char *info, int *iType)
{
  int d_s[]={0,3,12,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(NrMsg)
  GAMS_UNUSED(info)
  GAMS_UNUSED(iType)
  printNoReturn(optGetMessage,3)
}

/** Reset all defined and definedR flags and clear the message queue
 * @param popt opt object handle
 */
void  OPT_CALLCONV d_optResetAll (optHandle_t popt)
{
  int d_s[]={0};
  GAMS_UNUSED(popt)
  printNoReturn(optResetAll,0)
}

/** Reset all definedR flags
 * @param popt opt object handle
 */
void  OPT_CALLCONV d_optResetAllRecent (optHandle_t popt)
{
  int d_s[]={0};
  GAMS_UNUSED(popt)
  printNoReturn(optResetAllRecent,0)
}

/** Reset all options with definedR flag set
 * @param popt opt object handle
 */
void  OPT_CALLCONV d_optResetRecentChanges (optHandle_t popt)
{
  int d_s[]={0};
  GAMS_UNUSED(popt)
  printNoReturn(optResetRecentChanges,0)
}

/** Write help for option(s)
 * @param popt opt object handle
 * @param AHlpID Help level flags (can be combined): 1 = summary by group, 2 = alphabetical listing, 4 = include large help, 8 = include deprecated and obsolete
 */
void  OPT_CALLCONV d_optShowHelp (optHandle_t popt, const char *AHlpID)
{
  int d_s[]={0,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AHlpID)
  printNoReturn(optShowHelp,1)
}

/** Reset option value to default
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 */
int  OPT_CALLCONV d_optResetNr (optHandle_t popt, int ANr)
{
  int d_s[]={3,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  printAndReturn(optResetNr,1,int )
}

/** Find option by name, return number and ref nr
 * @param popt opt object handle
 * @param AName Option name
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ARefNr Option reference number
 */
int  OPT_CALLCONV d_optFindStr (optHandle_t popt, const char *AName, int *ANr, int *ARefNr)
{
  int d_s[]={3,11,4,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ARefNr)
  printAndReturn(optFindStr,3,int )
}

/** Get information about option by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADefined Flag: 0 if not defined and not 0 if defined
 * @param ADefinedR Flag: 0 if not recently defined and not 0 if recently defined
 * @param ARefNr Option reference number
 * @param ADataType Data type
 * @param AOptType Option type
 * @param ASubType Option sub type
 */
int  OPT_CALLCONV d_optGetInfoNr (optHandle_t popt, int ANr, int *ADefined, int *ADefinedR, int *ARefNr, int *ADataType, int *AOptType, int *ASubType)
{
  int d_s[]={3,3,4,4,4,4,4,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ADefined)
  GAMS_UNUSED(ADefinedR)
  GAMS_UNUSED(ARefNr)
  GAMS_UNUSED(ADataType)
  GAMS_UNUSED(AOptType)
  GAMS_UNUSED(ASubType)
  printAndReturn(optGetInfoNr,7,int )
}

/** Read values
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASName Option name
 * @param AIVal Option integer value
 * @param ADVal Option double value
 * @param ASVal Option string value
 */
int  OPT_CALLCONV d_optGetValuesNr (optHandle_t popt, int ANr, char *ASName, int *AIVal, double *ADVal, char *ASVal)
{
  int d_s[]={3,3,12,4,14,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ASName)
  GAMS_UNUSED(AIVal)
  GAMS_UNUSED(ADVal)
  GAMS_UNUSED(ASVal)
  printAndReturn(optGetValuesNr,5,int )
}

/** Set values
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 * @param ADVal Option double value
 * @param ASVal Option string value
 */
int  OPT_CALLCONV d_optSetValuesNr (optHandle_t popt, int ANr, int AIVal, double ADVal, const char *ASVal)
{
  int d_s[]={3,3,3,13,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AIVal)
  GAMS_UNUSED(ADVal)
  GAMS_UNUSED(ASVal)
  printAndReturn(optSetValuesNr,4,int )
}

/** Set second values
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 * @param ADVal Option double value
 * @param ASVal Option string value
 */
int  OPT_CALLCONV d_optSetValues2Nr (optHandle_t popt, int ANr, int AIVal, double ADVal, const char *ASVal)
{
  int d_s[]={3,3,3,13,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AIVal)
  GAMS_UNUSED(ADVal)
  GAMS_UNUSED(ASVal)
  printAndReturn(optSetValues2Nr,4,int )
}

/** Get version number of object
 * @param popt opt object handle
 * @param sversion Version number
 */
void  OPT_CALLCONV d_optVersion (optHandle_t popt, char *sversion)
{
  int d_s[]={0,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(sversion)
  printNoReturn(optVersion,1)
}

/** Get last processed definition file
 * @param popt opt object handle
 * @param sfilename File name
 */
void  OPT_CALLCONV d_optDefinitionFile (optHandle_t popt, char *sfilename)
{
  int d_s[]={0,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(sfilename)
  printNoReturn(optDefinitionFile,1)
}

/** Read string list element name and value and remove the value. Returns 0 if no string list option found
 * @param popt opt object handle
 * @param idash Dashed names flag: 0 = do not look for dashed names, 1 = look for dashed names
 * @param skey String list
 * @param sval String list element value
 */
int  OPT_CALLCONV d_optGetFromAnyStrList (optHandle_t popt, int idash, char *skey, char *sval)
{
  int d_s[]={3,3,12,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(idash)
  GAMS_UNUSED(skey)
  GAMS_UNUSED(sval)
  printAndReturn(optGetFromAnyStrList,3,int )
}

/** Read and remove queued strings from specified option.
 * @param popt opt object handle
 * @param skey String list
 * @param sval String list element value
 */
int  OPT_CALLCONV d_optGetFromListStr (optHandle_t popt, const char *skey, char *sval)
{
  int d_s[]={3,11,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(skey)
  GAMS_UNUSED(sval)
  printAndReturn(optGetFromListStr,2,int )
}

/** Number of elements stored in list
 * @param popt opt object handle
 * @param skey String list
 */
int  OPT_CALLCONV d_optListCountStr (optHandle_t popt, const char *skey)
{
  int d_s[]={3,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(skey)
  printAndReturn(optListCountStr,1,int )
}

/** Read element iPos from list without removal
 * @param popt opt object handle
 * @param skey String list
 * @param iPos Position in list
 * @param sval String list element value
 */
int  OPT_CALLCONV d_optReadFromListStr (optHandle_t popt, const char *skey, int iPos, char *sval)
{
  int d_s[]={3,11,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(skey)
  GAMS_UNUSED(iPos)
  GAMS_UNUSED(sval)
  printAndReturn(optReadFromListStr,3,int )
}

/** Number of synonyms
 * @param popt opt object handle
 */
int  OPT_CALLCONV d_optSynonymCount (optHandle_t popt)
{
  int d_s[]={3};
  GAMS_UNUSED(popt)
  printAndReturn(optSynonymCount,0,int )
}

/** Get synonym and original option name
 * @param popt opt object handle
 * @param NrSyn Number of the synoym
 * @param SSyn Option synomyn
 * @param SName Option name
 */
int  OPT_CALLCONV d_optGetSynonym (optHandle_t popt, int NrSyn, char *SSyn, char *SName)
{
  int d_s[]={3,3,12,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(NrSyn)
  GAMS_UNUSED(SSyn)
  GAMS_UNUSED(SName)
  printAndReturn(optGetSynonym,3,int )
}

/** Set echo of input on or off
 * @param popt opt object handle
 * @param AIVal Option integer value
 */
void  OPT_CALLCONV d_optEchoSet (optHandle_t popt, int AIVal)
{
  int d_s[]={0,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AIVal)
  printNoReturn(optEchoSet,1)
}

/** Set EOLOnly and return previous value
 * @param popt opt object handle
 * @param AIVal Option integer value
 */
int  OPT_CALLCONV d_optEOLOnlySet (optHandle_t popt, int AIVal)
{
  int d_s[]={3,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AIVal)
  printAndReturn(optEOLOnlySet,1,int )
}

/** Set bound checking on or off
 * @param popt opt object handle
 * @param AIVal Option integer value
 */
void  OPT_CALLCONV d_optNoBoundsSet (optHandle_t popt, int AIVal)
{
  int d_s[]={0,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AIVal)
  printNoReturn(optNoBoundsSet,1)
}

/** Get eol characters as string returns number of eol chars
 * @param popt opt object handle
 * @param EOLChars Accepted end of line characters
 */
int  OPT_CALLCONV d_optEOLChars (optHandle_t popt, char *EOLChars)
{
  int d_s[]={3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(EOLChars)
  printAndReturn(optEOLChars,1,int )
}

/** Retrieve number of errors / warnings from message queue.
 * @param popt opt object handle
 * @param iErrors Number of errors
 * @param iWarnings Number of warnings
 */
void  OPT_CALLCONV d_optErrorCount (optHandle_t popt, int *iErrors, int *iWarnings)
{
  int d_s[]={0,4,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(iErrors)
  GAMS_UNUSED(iWarnings)
  printNoReturn(optErrorCount,2)
}

/** Bounds and default for integer option
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ilval Lower bound
 * @param ihval Upper bound
 * @param idval Default value
 */
int  OPT_CALLCONV d_optGetBoundsInt (optHandle_t popt, int ANr, int *ilval, int *ihval, int *idval)
{
  int d_s[]={3,3,4,4,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ilval)
  GAMS_UNUSED(ihval)
  GAMS_UNUSED(idval)
  printAndReturn(optGetBoundsInt,4,int )
}

/** Bounds and default for double option
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param dlval Lower bound
 * @param dhval Upper bound
 * @param ddval Default value
 */
int  OPT_CALLCONV d_optGetBoundsDbl (optHandle_t popt, int ANr, double *dlval, double *dhval, double *ddval)
{
  int d_s[]={3,3,14,14,14};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(dlval)
  GAMS_UNUSED(dhval)
  GAMS_UNUSED(ddval)
  printAndReturn(optGetBoundsDbl,4,int )
}

/** Default value for a given string option
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param sval String list element value
 */
int  OPT_CALLCONV d_optGetDefaultStr (optHandle_t popt, int ANr, char *sval)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(sval)
  printAndReturn(optGetDefaultStr,2,int )
}

/** Read integer option by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 */
int  OPT_CALLCONV d_optGetIntNr (optHandle_t popt, int ANr, int *AIVal)
{
  int d_s[]={3,3,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AIVal)
  printAndReturn(optGetIntNr,2,int )
}

/** Read second integer option by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 */
int  OPT_CALLCONV d_optGetInt2Nr (optHandle_t popt, int ANr, int *AIVal)
{
  int d_s[]={3,3,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AIVal)
  printAndReturn(optGetInt2Nr,2,int )
}

/** Set integer option by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 */
int  OPT_CALLCONV d_optSetIntNr (optHandle_t popt, int ANr, int AIVal)
{
  int d_s[]={3,3,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AIVal)
  printAndReturn(optSetIntNr,2,int )
}

/** Set second integer option by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 */
int  OPT_CALLCONV d_optSetInt2Nr (optHandle_t popt, int ANr, int AIVal)
{
  int d_s[]={3,3,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AIVal)
  printAndReturn(optSetInt2Nr,2,int )
}

/** Read string by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASVal Option string value
 */
int  OPT_CALLCONV d_optGetStrNr (optHandle_t popt, int ANr, char *ASVal)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ASVal)
  printAndReturn(optGetStrNr,2,int )
}

/** Get option name, help context and group number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AName Option name
 * @param AHc Help Context number
 * @param AGroup Group number
 */
int  OPT_CALLCONV d_optGetOptHelpNr (optHandle_t popt, int ANr, char *AName, int *AHc, int *AGroup)
{
  int d_s[]={3,3,12,4,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(AHc)
  GAMS_UNUSED(AGroup)
  printAndReturn(optGetOptHelpNr,4,int )
}

/** Get help text for enumerated value
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AOrd Ordinal position
 * @param AHc Help Context number
 * @param AHelpStr 
 */
int  OPT_CALLCONV d_optGetEnumHelp (optHandle_t popt, int ANr, int AOrd, int *AHc, char *AHelpStr)
{
  int d_s[]={3,3,3,4,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AOrd)
  GAMS_UNUSED(AHc)
  GAMS_UNUSED(AHelpStr)
  printAndReturn(optGetEnumHelp,4,int )
}

/** Get enumerated string value and ordinal
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASVal Option string value
 * @param AOrd Ordinal position
 */
int  OPT_CALLCONV d_optGetEnumStrNr (optHandle_t popt, int ANr, char *ASVal, int *AOrd)
{
  int d_s[]={3,3,12,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ASVal)
  GAMS_UNUSED(AOrd)
  printAndReturn(optGetEnumStrNr,3,int )
}

/** Number of enumerations in option
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ACount Number of enumerations
 */
int  OPT_CALLCONV d_optGetEnumCount (optHandle_t popt, int ANr, int *ACount)
{
  int d_s[]={3,3,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ACount)
  printAndReturn(optGetEnumCount,2,int )
}

/** Enumerated value by ordinal number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AOrd Ordinal position
 * @param AValInt Option value as integer
 * @param AValStr Option value as string
 */
int  OPT_CALLCONV d_optGetEnumValue (optHandle_t popt, int ANr, int AOrd, int *AValInt, char *AValStr)
{
  int d_s[]={3,3,3,4,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AOrd)
  GAMS_UNUSED(AValInt)
  GAMS_UNUSED(AValStr)
  printAndReturn(optGetEnumValue,4,int )
}

/** Read second string by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASVal Option string value
 */
int  OPT_CALLCONV d_optGetStr2Nr (optHandle_t popt, int ANr, char *ASVal)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ASVal)
  printAndReturn(optGetStr2Nr,2,int )
}

/** Set string by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASVal Option string value
 */
int  OPT_CALLCONV d_optSetStrNr (optHandle_t popt, int ANr, const char *ASVal)
{
  int d_s[]={3,3,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ASVal)
  printAndReturn(optSetStrNr,2,int )
}

/** Set second string by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASVal Option string value
 */
int  OPT_CALLCONV d_optSetStr2Nr (optHandle_t popt, int ANr, const char *ASVal)
{
  int d_s[]={3,3,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ASVal)
  printAndReturn(optSetStr2Nr,2,int )
}

/** Read double by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADVal Option double value
 */
int  OPT_CALLCONV d_optGetDblNr (optHandle_t popt, int ANr, double *ADVal)
{
  int d_s[]={3,3,14};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ADVal)
  printAndReturn(optGetDblNr,2,int )
}

/** Read second double by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADVal Option double value
 */
int  OPT_CALLCONV d_optGetDbl2Nr (optHandle_t popt, int ANr, double *ADVal)
{
  int d_s[]={3,3,14};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ADVal)
  printAndReturn(optGetDbl2Nr,2,int )
}

/** Set double by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADVal Option double value
 */
int  OPT_CALLCONV d_optSetDblNr (optHandle_t popt, int ANr, double ADVal)
{
  int d_s[]={3,3,13};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ADVal)
  printAndReturn(optSetDblNr,2,int )
}

/** Set second double by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADVal Option double value
 */
int  OPT_CALLCONV d_optSetDbl2Nr (optHandle_t popt, int ANr, double ADVal)
{
  int d_s[]={3,3,13};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ADVal)
  printAndReturn(optSetDbl2Nr,2,int )
}

/** Read value as string by option name
 * @param popt opt object handle
 * @param AName Option name
 * @param ASVal Option string value
 */
int  OPT_CALLCONV d_optGetValStr (optHandle_t popt, const char *AName, char *ASVal)
{
  int d_s[]={3,11,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(ASVal)
  printAndReturn(optGetValStr,2,int )
}

/** Read second value as string by option name
 * @param popt opt object handle
 * @param AName Option name
 * @param ASVal Option string value
 */
int  OPT_CALLCONV d_optGetVal2Str (optHandle_t popt, const char *AName, char *ASVal)
{
  int d_s[]={3,11,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(ASVal)
  printAndReturn(optGetVal2Str,2,int )
}

/** Get option name by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASName Option name
 */
int  OPT_CALLCONV d_optGetNameNr (optHandle_t popt, int ANr, char *ASName)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ASName)
  printAndReturn(optGetNameNr,2,int )
}

/** Get defined status by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal 
 */
int  OPT_CALLCONV d_optGetDefinedNr (optHandle_t popt, int ANr, int *AIVal)
{
  int d_s[]={3,3,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AIVal)
  printAndReturn(optGetDefinedNr,2,int )
}

/** Get option name and help by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASOpt 
 * @param ASHelp 
 */
int  OPT_CALLCONV d_optGetHelpNr (optHandle_t popt, int ANr, char *ASOpt, char *ASHelp)
{
  int d_s[]={3,3,12,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ASOpt)
  GAMS_UNUSED(ASHelp)
  printAndReturn(optGetHelpNr,3,int )
}

/** Get group information by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AName Option name
 * @param AGroup Group number
 * @param AHc Help Context number
 * @param AHelp Help string
 */
int  OPT_CALLCONV d_optGetGroupNr (optHandle_t popt, int ANr, char *AName, int *AGroup, int *AHc, char *AHelp)
{
  int d_s[]={3,3,12,4,4,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(AGroup)
  GAMS_UNUSED(AHc)
  GAMS_UNUSED(AHelp)
  printAndReturn(optGetGroupNr,5,int )
}

/** Get group record by group number
 * @param popt opt object handle
 * @param AGroup Group number
 */
int  OPT_CALLCONV d_optGetGroupGrpNr (optHandle_t popt, int AGroup)
{
  int d_s[]={3,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AGroup)
  printAndReturn(optGetGroupGrpNr,1,int )
}

/** Get group number of an option by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 */
int  OPT_CALLCONV d_optGetOptGroupNr (optHandle_t popt, int ANr)
{
  int d_s[]={3,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  printAndReturn(optGetOptGroupNr,1,int )
}

/** Dot option info
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param VEName Variable or equation name
 * @param AObjNr 
 * @param ADim Number of indices of variable or equation
 * @param AValue Option value
 */
int  OPT_CALLCONV d_optGetDotOptNr (optHandle_t popt, int ANr, char *VEName, int *AObjNr, int *ADim, double *AValue)
{
  int d_s[]={3,3,12,4,4,14};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(VEName)
  GAMS_UNUSED(AObjNr)
  GAMS_UNUSED(ADim)
  GAMS_UNUSED(AValue)
  printAndReturn(optGetDotOptNr,5,int )
}

/** Retrieve a single element from a dot option
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADim Number of indices of variable or equation
 * @param AUEL 
 */
int  OPT_CALLCONV d_optGetDotOptUel (optHandle_t popt, int ANr, int ADim, char *AUEL)
{
  int d_s[]={3,3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ADim)
  GAMS_UNUSED(AUEL)
  printAndReturn(optGetDotOptUel,3,int )
}

/** Variable equation mapping info
 * @param popt opt object handle
 * @param maptype Type of variable equation mapping (see enumerated constants)
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param EquName Equation name in indicator option
 * @param VarName Variable name in indicator option
 * @param EquDim Equation dimension in indicator option
 * @param VarDim Variable dimension in indicator option
 * @param AValue Option value
 */
int  OPT_CALLCONV d_optGetVarEquMapNr (optHandle_t popt, int maptype, int ANr, char *EquName, char *VarName, int *EquDim, int *VarDim, int *AValue)
{
  int d_s[]={3,3,3,12,12,4,4,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(maptype)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(EquName)
  GAMS_UNUSED(VarName)
  GAMS_UNUSED(EquDim)
  GAMS_UNUSED(VarDim)
  GAMS_UNUSED(AValue)
  printAndReturn(optGetVarEquMapNr,7,int )
}

/** Equation part of variable equation mapping
 * @param popt opt object handle
 * @param maptype Type of variable equation mapping (see enumerated constants)
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADim Number of indices of variable or equation
 * @param AIndex Domain index string
 */
int  OPT_CALLCONV d_optGetEquVarEquMapNr (optHandle_t popt, int maptype, int ANr, int ADim, char *AIndex)
{
  int d_s[]={3,3,3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(maptype)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ADim)
  GAMS_UNUSED(AIndex)
  printAndReturn(optGetEquVarEquMapNr,4,int )
}

/** Variable part of variable equation mapping
 * @param popt opt object handle
 * @param maptype Type of variable equation mapping (see enumerated constants)
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADim Number of indices of variable or equation
 * @param AIndex Domain index string
 */
int  OPT_CALLCONV d_optGetVarVarEquMapNr (optHandle_t popt, int maptype, int ANr, int ADim, char *AIndex)
{
  int d_s[]={3,3,3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(maptype)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ADim)
  GAMS_UNUSED(AIndex)
  printAndReturn(optGetVarVarEquMapNr,4,int )
}

/** Variable equation mappings available and number ignored
 * @param popt opt object handle
 * @param maptype Type of variable equation mapping (see enumerated constants)
 * @param ANrErrors Number of syntactically incorrect dot options
 */
int  OPT_CALLCONV d_optVarEquMapCount (optHandle_t popt, int maptype, int *ANrErrors)
{
  int d_s[]={3,3,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(maptype)
  GAMS_UNUSED(ANrErrors)
  printAndReturn(optVarEquMapCount,2,int )
}

/** Indicator info
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param EquName Equation name in indicator option
 * @param VarName Variable name in indicator option
 * @param EquDim Equation dimension in indicator option
 * @param VarDim Variable dimension in indicator option
 * @param AValue Option value
 */
int  OPT_CALLCONV d_optGetIndicatorNr (optHandle_t popt, int ANr, char *EquName, char *VarName, int *EquDim, int *VarDim, int *AValue)
{
  int d_s[]={3,3,12,12,4,4,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(EquName)
  GAMS_UNUSED(VarName)
  GAMS_UNUSED(EquDim)
  GAMS_UNUSED(VarDim)
  GAMS_UNUSED(AValue)
  printAndReturn(optGetIndicatorNr,6,int )
}

/** Equation part of indicator
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADim Number of indices of variable or equation
 * @param AIndex Domain index string
 */
int  OPT_CALLCONV d_optGetEquIndicatorNr (optHandle_t popt, int ANr, int ADim, char *AIndex)
{
  int d_s[]={3,3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ADim)
  GAMS_UNUSED(AIndex)
  printAndReturn(optGetEquIndicatorNr,3,int )
}

/** Variable part of indicator
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADim Number of indices of variable or equation
 * @param AIndex Domain index string
 */
int  OPT_CALLCONV d_optGetVarIndicatorNr (optHandle_t popt, int ANr, int ADim, char *AIndex)
{
  int d_s[]={3,3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ADim)
  GAMS_UNUSED(AIndex)
  printAndReturn(optGetVarIndicatorNr,3,int )
}

/** Indicators available and number ignored
 * @param popt opt object handle
 * @param ANrErrors Number of syntactically incorrect dot options
 */
int  OPT_CALLCONV d_optIndicatorCount (optHandle_t popt, int *ANrErrors)
{
  int d_s[]={3,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANrErrors)
  printAndReturn(optIndicatorCount,1,int )
}

/** Dot options available and number ignored
 * @param popt opt object handle
 * @param ANrErrors Number of syntactically incorrect dot options
 */
int  OPT_CALLCONV d_optDotOptCount (optHandle_t popt, int *ANrErrors)
{
  int d_s[]={3,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANrErrors)
  printAndReturn(optDotOptCount,1,int )
}

/** Set reference number by option number
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ARefNr Option reference number
 */
int  OPT_CALLCONV d_optSetRefNr (optHandle_t popt, int ANr, int ARefNr)
{
  int d_s[]={3,3,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ANr)
  GAMS_UNUSED(ARefNr)
  printAndReturn(optSetRefNr,2,int )
}

/** Set reference number by option name
 * @param popt opt object handle
 * @param AOpt Option name
 * @param ARefNr Option reference number
 */
int  OPT_CALLCONV d_optSetRefNrStr (optHandle_t popt, const char *AOpt, int ARefNr)
{
  int d_s[]={3,11,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AOpt)
  GAMS_UNUSED(ARefNr)
  printAndReturn(optSetRefNrStr,2,int )
}

/** Get the name of a constant
 * @param popt opt object handle
 * @param cgroup Constant group: 1 = Data types, 2 = Option types, 3 = Option sub-type, 4 = Message type
 * @param cindex Constant index
 * @param cname Constant name
 */
int  OPT_CALLCONV d_optGetConstName (optHandle_t popt, int cgroup, int cindex, char *cname)
{
  int d_s[]={3,3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(cgroup)
  GAMS_UNUSED(cindex)
  GAMS_UNUSED(cname)
  printAndReturn(optGetConstName,3,int )
}

/** Get option type name by type number
 * @param popt opt object handle
 * @param TNr Type number
 * @param sTName Type name
 */
int  OPT_CALLCONV d_optGetTypeName (optHandle_t popt, int TNr, char *sTName)
{
  int d_s[]={3,3,12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(TNr)
  GAMS_UNUSED(sTName)
  printAndReturn(optGetTypeName,2,int )
}

/** Index number of an option
 * @param popt opt object handle
 * @param AOpt Option name
 */
int  OPT_CALLCONV d_optLookUp (optHandle_t popt, const char *AOpt)
{
  int d_s[]={3,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AOpt)
  printAndReturn(optLookUp,1,int )
}

/** Read options from a PChar.  In the case of errors, messages will be added to the message queue (see OptGetMessage).
 * @param popt opt object handle
 * @param p PChar: pointer to first character
 */
void  OPT_CALLCONV d_optReadFromPChar (optHandle_t popt, char *p)
{
  int d_s[]={0,10};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(p)
  printNoReturn(optReadFromPChar,1)
}

/** Read options from a PChar.  In the case of errors, messages will be added to the message queue indicated as command line (see OptGetMessage).
 * @param popt opt object handle
 * @param p PChar: pointer to first character
 */
void  OPT_CALLCONV d_optReadFromCmdLine (optHandle_t popt, char *p)
{
  int d_s[]={0,10};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(p)
  printNoReturn(optReadFromCmdLine,1)
}

/** Read options from shortStrings returned by callback func cb.  In the case of errors, messages will be added to the message queue indicated as command line args (see OptGetMessage).
 * @param popt opt object handle
 * @param cb 
 */
void  OPT_CALLCONV d_optReadFromCmdArgs (optHandle_t popt, TArgvCB_t cb)
{
  int d_s[]={0,59};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(cb)
  printNoReturn(optReadFromCmdArgs,1)
}

/** Extract solver name and optfile number from string option.
 * @param popt opt object handle
 * @param ASVal Option string value
 * @param solver Solver name
 * @param opt Optfile number
 */
int  OPT_CALLCONV d_optGetNameOpt (optHandle_t popt, const char *ASVal, char *solver, int *opt)
{
  int d_s[]={3,11,12,4};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(ASVal)
  GAMS_UNUSED(solver)
  GAMS_UNUSED(opt)
  printAndReturn(optGetNameOpt,3,int )
}

/** Reset option to default by option name.
 * @param popt opt object handle
 * @param AName Option name
 */
int  OPT_CALLCONV d_optResetStr (optHandle_t popt, const char *AName)
{
  int d_s[]={15,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  printAndReturn(optResetStr,1,int )
}

/** Get defined status by option name.
 * @param popt opt object handle
 * @param AName Option name
 */
int  OPT_CALLCONV d_optGetDefinedStr (optHandle_t popt, const char *AName)
{
  int d_s[]={15,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  printAndReturn(optGetDefinedStr,1,int )
}

/** Read integer by option name.
 * @param popt opt object handle
 * @param AName Option name
 */
int  OPT_CALLCONV d_optGetIntStr (optHandle_t popt, const char *AName)
{
  int d_s[]={3,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  printAndReturn(optGetIntStr,1,int )
}

/** Read double by option name.
 * @param popt opt object handle
 * @param AName Option name
 */
double  OPT_CALLCONV d_optGetDblStr (optHandle_t popt, const char *AName)
{
  int d_s[]={13,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  printAndReturn(optGetDblStr,1,double )
}

/** Read string by option name.
 * @param popt opt object handle
 * @param AName Option name
 */
char * OPT_CALLCONV d_optGetStrStr (optHandle_t popt, const char *AName, char *buf)
{
  int d_s[]={12,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(buf)
  printAndReturn(optGetStrStr,1,char *)
}

/** Set integer by option name.
 * @param popt opt object handle
 * @param AName Option name
 * @param AIVal Option integer value
 */
void  OPT_CALLCONV d_optSetIntStr (optHandle_t popt, const char *AName, int AIVal)
{
  int d_s[]={0,11,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(AIVal)
  printNoReturn(optSetIntStr,2)
}

/** Set double by option name.
 * @param popt opt object handle
 * @param AName Option name
 * @param ADVal Option double value
 */
void  OPT_CALLCONV d_optSetDblStr (optHandle_t popt, const char *AName, double ADVal)
{
  int d_s[]={0,11,13};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(ADVal)
  printNoReturn(optSetDblStr,2)
}

/** Set string by option name.
 * @param popt opt object handle
 * @param AName Option name
 * @param ASVal Option string value
 */
void  OPT_CALLCONV d_optSetStrStr (optHandle_t popt, const char *AName, const char *ASVal)
{
  int d_s[]={0,11,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  GAMS_UNUSED(ASVal)
  printNoReturn(optSetStrStr,2)
}

/** Returns true, if the option is deprecated
 * @param popt opt object handle
 * @param AName Option name
 */
int  OPT_CALLCONV d_optIsDeprecated (optHandle_t popt, const char *AName)
{
  int d_s[]={15,11};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(AName)
  printAndReturn(optIsDeprecated,1,int )
}

/** Number of options
 * @param popt opt object handle
 */
int  OPT_CALLCONV d_optCount (optHandle_t popt)
{
  int d_s[]={3};
  GAMS_UNUSED(popt)
  printAndReturn(optCount,0,int )
}

/** Number of messages in buffer
 * @param popt opt object handle
 */
int  OPT_CALLCONV d_optMessageCount (optHandle_t popt)
{
  int d_s[]={3};
  GAMS_UNUSED(popt)
  printAndReturn(optMessageCount,0,int )
}

/** Number of option groups
 * @param popt opt object handle
 */
int  OPT_CALLCONV d_optGroupCount (optHandle_t popt)
{
  int d_s[]={3};
  GAMS_UNUSED(popt)
  printAndReturn(optGroupCount,0,int )
}

/** When enabled (default), Defined and DefinedR will be set when assigning to an option. When disabled, only Defined will be set
 * @param popt opt object handle
 */
int  OPT_CALLCONV d_optRecentEnabled (optHandle_t popt)
{
  int d_s[]={3};
  GAMS_UNUSED(popt)
  printAndReturn(optRecentEnabled,0,int )
}

/** When enabled (default), Defined and DefinedR will be set when assigning to an option. When disabled, only Defined will be set
 * @param popt opt object handle
 */
void OPT_CALLCONV d_optRecentEnabledSet (optHandle_t popt,const int x)
{
  int d_s[]={0,3};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(x)
  printNoReturn(optRecentEnabledSet,1)
}

/** Defined separator between option key and value
 * @param popt opt object handle
 */
char * OPT_CALLCONV d_optSeparator (optHandle_t popt, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(buf)
  printAndReturn(optSeparator,0,char *)
}

/** Defined quote string
 * @param popt opt object handle
 */
char * OPT_CALLCONV d_optStringQuote (optHandle_t popt, char *buf)
{
  int d_s[]={12};
  GAMS_UNUSED(popt)
  GAMS_UNUSED(buf)
  printAndReturn(optStringQuote,0,char *)
}


/** return dirName on success, NULL on failure */
static char *
extractFileDirFileName (const char *fileName, char *dirName, char *fName)
{
  int fileNameLen, shave=0;
  const char *end, *s;
  char *t;

  if (NULL == fileName || NULL == dirName || fName == NULL) {
    return NULL;
  }
  fileNameLen = (int) strlen(fileName);

#if defined(_WIN32)
  /* get the last delimiter */
  for (end = fileName + fileNameLen - 1;
       end >= fileName && '\\' != *end && ':' != *end;  end--);
  /* shave off the trailing delimiter if:
   *  it isn't the first char,
   *  it is a backslash, and
   *  it is not preceded by a delimiter
   */
  if (end > fileName && '\\' == *end
   && (! ('\\' == *(end-1) || ':' == *(end-1)))
     ) {
    end--; shave=1;
  }
#else
  /* non-Windows: implicitly, this is the Unix version */
  /* get the last delimiter */
  for (end = fileName + fileNameLen - 1;
       end >= fileName && '/' != *end;  end--);

  if (end > fileName && '/' == *end) {
    end--; shave=1;
  }
#endif  /* if defined(_WIN32) */

  for (s = fileName, t = dirName;  s <= end;  s++, t++)
    *t = *s;
  *t = '\0';

  if (shave) s++;
  for (t = fName;  s <= fileName + fileNameLen - 1;  s++, t++)
    *t = *s;
  *t = '\0';

  return dirName;
} /* extractFileDirFileName */

static soHandle_t
loadLib (const char *libName, char **errMsg)
{
  soHandle_t h;

#if defined(_WIN32)
#if defined(UNICODE) || defined (_UNICODE)
  h = LoadLibraryA(libName);
#else
  h = LoadLibrary(libName);
#endif
  if (NULL == h) {
    *errMsg = winErr;
  }
  else {
    *errMsg = NULL;
  }
#else
  (void) dlerror();
  h = dlopen (libName, RTLD_NOW);
  if (NULL == h) {
    *errMsg = dlerror();
  }
  else {
    *errMsg = NULL;
  }
#endif

  return h;
} /* loadLib */

static int
unLoadLib (soHandle_t hh)
{
  int rc;

#if defined(_WIN32)
  rc = FreeLibrary (hh);
  return ! rc;
#else
  rc = dlclose (hh);
#endif
  return rc;
} /* unLoadLib */

static void *
loadSym (soHandle_t h, const char *sym, char **errMsg)
{
  void *s;
  const char *from;
  char *to;
  const char *tripSym;
  char lcbuf[257];
  char ucbuf[257];
  size_t symLen;
  int trip;

  /* search in this order:
   *  1. lower
   *  2. original
   *  3. upper
   */

  symLen = 0;
  for (trip = 1;  trip <= 3;  trip++) {
    switch (trip) {
    case 1:                             /* lower */
      for (from = sym, to = lcbuf;  *from;  from++, to++) {
        *to = tolower(*from);
      }
      symLen = from - sym;
      lcbuf[symLen] = '\0';
      tripSym = lcbuf;
      break;
    case 2:                             /* original */
      tripSym = sym;
      break;
    case 3:                             /* upper */
      for (from = sym, to = ucbuf;  *from;  from++, to++) {
        *to = toupper(*from);
      }
      ucbuf[symLen] = '\0';
      tripSym = ucbuf;
      break;
    default:
      tripSym = sym;
    } /* end switch */
#if defined(_WIN32)
#  if defined(HAVE_INTPTR_T)
    s = (void *)(intptr_t)GetProcAddress (h, tripSym);
#  else
    s = (void *)GetProcAddress (h, tripSym);
#  endif
    *errMsg = NULL;
    if (NULL != s) {
      return s;
    }
#else
    (void) dlerror();
    s = dlsym (h, tripSym);
    *errMsg = dlerror();
    if (NULL == *errMsg) {
      return s;
    }
#endif
  } /* end loop over symbol name variations */

  return NULL;
} /* loadSym */

/* TNAME = type name, ENAME = exported name */
#if defined(HAVE_INTPTR_T)
#  define LOADIT(TNAME,ENAME) symName = ENAME; TNAME = (TNAME##_t) (intptr_t) loadSym (h, symName, &errMsg); if (NULL == TNAME) goto symMissing
#  define LOADIT_ERR_OK(TNAME,ENAME) symName = ENAME; TNAME = (TNAME##_t) (intptr_t) loadSym (h, symName, &errMsg)
#else
#  define LOADIT(TNAME,ENAME) symName = ENAME; TNAME = (TNAME##_t) loadSym (h, symName, &errMsg); if (NULL == TNAME) goto symMissing
#  define LOADIT_ERR_OK(TNAME,ENAME) symName = ENAME; TNAME = (TNAME##_t) loadSym (h, symName, &errMsg)
#endif

#if ! defined(GMS_DLL_BASENAME)
# define GMS_DLL_BASENAME "optdclib"
#endif
#if defined(_WIN32)
# if ! defined(GMS_DLL_PREFIX)
#  define GMS_DLL_PREFIX ""
# endif
# if ! defined(GMS_DLL_EXTENSION)
#  define GMS_DLL_EXTENSION ".dll"
# endif
# if ! defined(GMS_DLL_SUFFIX)
#  if defined(_WIN64)
#   define GMS_DLL_SUFFIX "64"
#  else
#   define GMS_DLL_SUFFIX ""
#  endif
# endif

#else  /* start non-Windows */

# if ! defined(GMS_DLL_PREFIX)
#  define GMS_DLL_PREFIX "lib"
# endif
# if ! defined(GMS_DLL_EXTENSION)
#  if defined(__APPLE__)
#   define GMS_DLL_EXTENSION ".dylib"
#  else
#   define GMS_DLL_EXTENSION ".so"
#  endif
# endif
# if ! defined(GMS_DLL_SUFFIX)
#  if defined(__WORDSIZE)
#   if 64 == __WORDSIZE
#    define GMS_DLL_SUFFIX "64"
#   else
#    define GMS_DLL_SUFFIX ""
#   endif
#  elif defined(__SIZEOF_POINTER__)
#   if 4 == __SIZEOF_POINTER__
#    define GMS_DLL_SUFFIX ""
#   elif 8 == __SIZEOF_POINTER__
#    define GMS_DLL_SUFFIX "64"
#   endif
#  endif
# endif /* ! defined(GMS_DLL_SUFFIX) */
#endif

/** XLibraryLoad: return 0 on success, ~0 on failure */
static int
XLibraryLoad (const char *dllName, char *errBuf, int errBufSize)
{
  char *errMsg;
  const char *symName;
  int rc, cl;

  if (isLoaded)
    return 0;
  h = loadLib (dllName, &errMsg);
  if (NULL == h) {
    if (NULL != errBuf) {
      int elen;
      char* ebuf;
      elen = errBufSize-1;  ebuf = errBuf;
      rc = sprintf (ebuf, "%.*s", elen, "Could not load shared library ");
      elen -= rc;  ebuf+= rc;
      rc = sprintf (ebuf, "%.*s", elen, dllName);
      elen -= rc;  ebuf+= rc;
      rc = sprintf (ebuf, "%.*s", elen, ": ");
      elen -= rc;  ebuf+= rc;
      rc = sprintf (ebuf, "%.*s", elen, errMsg);
      /* elen -= rc;  ebuf+= rc; */
      errBuf[errBufSize-1] = '\0';
    }
    return 1;
  }
  else {
     /* printf ("Loaded shared library %s successfully\n", dllName); */
    if (errBuf && errBufSize)
      errBuf[0] = '\0';
  }

  LOADIT(XCreate, "XCreate");
  LOADIT(XFree, "XFree");
  LOADIT(XCheck, "CXCheck");
  LOADIT(XAPIVersion, "CXAPIVersion");

  if (!XAPIVersion(4,errBuf,&cl))
    return 1;

  LOADIT_ERR_OK(optSetLoadPath, "CoptSetLoadPath");
  LOADIT_ERR_OK(optGetLoadPath, "CoptGetLoadPath");

#define CheckAndLoad(f,nargs,prefix) \
  if (!XCheck(#f,nargs,s,errBuf)) \
    f = &d_##f; \
  else { \
    LOADIT(f,prefix #f); \
  }
  {int s[]={3,11}; CheckAndLoad(optReadDefinition,1,"C"); }
  {int s[]={3,10}; CheckAndLoad(optReadDefinitionFromPChar,1,""); }
  {int s[]={3,11}; CheckAndLoad(optReadParameterFile,1,"C"); }
  {int s[]={0,11}; CheckAndLoad(optReadFromStr,1,"C"); }
  {int s[]={3,11}; CheckAndLoad(optWriteParameterFile,1,"C"); }
  {int s[]={0}; CheckAndLoad(optClearMessages,0,""); }
  {int s[]={0,11}; CheckAndLoad(optAddMessage,1,"C"); }
  {int s[]={0,3,12,4}; CheckAndLoad(optGetMessage,3,"C"); }
  {int s[]={0}; CheckAndLoad(optResetAll,0,""); }
  {int s[]={0}; CheckAndLoad(optResetAllRecent,0,""); }
  {int s[]={0}; CheckAndLoad(optResetRecentChanges,0,""); }
  {int s[]={0,11}; CheckAndLoad(optShowHelp,1,"C"); }
  {int s[]={3,3}; CheckAndLoad(optResetNr,1,""); }
  {int s[]={3,11,4,4}; CheckAndLoad(optFindStr,3,"C"); }
  {int s[]={3,3,4,4,4,4,4,4}; CheckAndLoad(optGetInfoNr,7,""); }
  {int s[]={3,3,12,4,14,12}; CheckAndLoad(optGetValuesNr,5,"C"); }
  {int s[]={3,3,3,13,11}; CheckAndLoad(optSetValuesNr,4,"C"); }
  {int s[]={3,3,3,13,11}; CheckAndLoad(optSetValues2Nr,4,"C"); }
  {int s[]={0,12}; CheckAndLoad(optVersion,1,"C"); }
  {int s[]={0,12}; CheckAndLoad(optDefinitionFile,1,"C"); }
  {int s[]={3,3,12,12}; CheckAndLoad(optGetFromAnyStrList,3,"C"); }
  {int s[]={3,11,12}; CheckAndLoad(optGetFromListStr,2,"C"); }
  {int s[]={3,11}; CheckAndLoad(optListCountStr,1,"C"); }
  {int s[]={3,11,3,12}; CheckAndLoad(optReadFromListStr,3,"C"); }
  {int s[]={3}; CheckAndLoad(optSynonymCount,0,""); }
  {int s[]={3,3,12,12}; CheckAndLoad(optGetSynonym,3,"C"); }
  {int s[]={0,3}; CheckAndLoad(optEchoSet,1,""); }
  {int s[]={3,3}; CheckAndLoad(optEOLOnlySet,1,""); }
  {int s[]={0,3}; CheckAndLoad(optNoBoundsSet,1,""); }
  {int s[]={3,12}; CheckAndLoad(optEOLChars,1,"C"); }
  {int s[]={0,4,4}; CheckAndLoad(optErrorCount,2,""); }
  {int s[]={3,3,4,4,4}; CheckAndLoad(optGetBoundsInt,4,""); }
  {int s[]={3,3,14,14,14}; CheckAndLoad(optGetBoundsDbl,4,""); }
  {int s[]={3,3,12}; CheckAndLoad(optGetDefaultStr,2,"C"); }
  {int s[]={3,3,4}; CheckAndLoad(optGetIntNr,2,""); }
  {int s[]={3,3,4}; CheckAndLoad(optGetInt2Nr,2,""); }
  {int s[]={3,3,3}; CheckAndLoad(optSetIntNr,2,""); }
  {int s[]={3,3,3}; CheckAndLoad(optSetInt2Nr,2,""); }
  {int s[]={3,3,12}; CheckAndLoad(optGetStrNr,2,"C"); }
  {int s[]={3,3,12,4,4}; CheckAndLoad(optGetOptHelpNr,4,"C"); }
  {int s[]={3,3,3,4,12}; CheckAndLoad(optGetEnumHelp,4,"C"); }
  {int s[]={3,3,12,4}; CheckAndLoad(optGetEnumStrNr,3,"C"); }
  {int s[]={3,3,4}; CheckAndLoad(optGetEnumCount,2,""); }
  {int s[]={3,3,3,4,12}; CheckAndLoad(optGetEnumValue,4,"C"); }
  {int s[]={3,3,12}; CheckAndLoad(optGetStr2Nr,2,"C"); }
  {int s[]={3,3,11}; CheckAndLoad(optSetStrNr,2,"C"); }
  {int s[]={3,3,11}; CheckAndLoad(optSetStr2Nr,2,"C"); }
  {int s[]={3,3,14}; CheckAndLoad(optGetDblNr,2,""); }
  {int s[]={3,3,14}; CheckAndLoad(optGetDbl2Nr,2,""); }
  {int s[]={3,3,13}; CheckAndLoad(optSetDblNr,2,""); }
  {int s[]={3,3,13}; CheckAndLoad(optSetDbl2Nr,2,""); }
  {int s[]={3,11,12}; CheckAndLoad(optGetValStr,2,"C"); }
  {int s[]={3,11,12}; CheckAndLoad(optGetVal2Str,2,"C"); }
  {int s[]={3,3,12}; CheckAndLoad(optGetNameNr,2,"C"); }
  {int s[]={3,3,4}; CheckAndLoad(optGetDefinedNr,2,""); }
  {int s[]={3,3,12,12}; CheckAndLoad(optGetHelpNr,3,"C"); }
  {int s[]={3,3,12,4,4,12}; CheckAndLoad(optGetGroupNr,5,"C"); }
  {int s[]={3,3}; CheckAndLoad(optGetGroupGrpNr,1,""); }
  {int s[]={3,3}; CheckAndLoad(optGetOptGroupNr,1,""); }
  {int s[]={3,3,12,4,4,14}; CheckAndLoad(optGetDotOptNr,5,"C"); }
  {int s[]={3,3,3,12}; CheckAndLoad(optGetDotOptUel,3,"C"); }
  {int s[]={3,3,3,12,12,4,4,4}; CheckAndLoad(optGetVarEquMapNr,7,"C"); }
  {int s[]={3,3,3,3,12}; CheckAndLoad(optGetEquVarEquMapNr,4,"C"); }
  {int s[]={3,3,3,3,12}; CheckAndLoad(optGetVarVarEquMapNr,4,"C"); }
  {int s[]={3,3,4}; CheckAndLoad(optVarEquMapCount,2,""); }
  {int s[]={3,3,12,12,4,4,4}; CheckAndLoad(optGetIndicatorNr,6,"C"); }
  {int s[]={3,3,3,12}; CheckAndLoad(optGetEquIndicatorNr,3,"C"); }
  {int s[]={3,3,3,12}; CheckAndLoad(optGetVarIndicatorNr,3,"C"); }
  {int s[]={3,4}; CheckAndLoad(optIndicatorCount,1,""); }
  {int s[]={3,4}; CheckAndLoad(optDotOptCount,1,""); }
  {int s[]={3,3,3}; CheckAndLoad(optSetRefNr,2,""); }
  {int s[]={3,11,3}; CheckAndLoad(optSetRefNrStr,2,"C"); }
  {int s[]={3,3,3,12}; CheckAndLoad(optGetConstName,3,"C"); }
  {int s[]={3,3,12}; CheckAndLoad(optGetTypeName,2,"C"); }
  {int s[]={3,11}; CheckAndLoad(optLookUp,1,"C"); }
  {int s[]={0,10}; CheckAndLoad(optReadFromPChar,1,""); }
  {int s[]={0,10}; CheckAndLoad(optReadFromCmdLine,1,""); }
  {int s[]={0,59}; CheckAndLoad(optReadFromCmdArgs,1,""); }
  {int s[]={3,11,12,4}; CheckAndLoad(optGetNameOpt,3,"C"); }
  {int s[]={15,11}; CheckAndLoad(optResetStr,1,"C"); }
  {int s[]={15,11}; CheckAndLoad(optGetDefinedStr,1,"C"); }
  {int s[]={3,11}; CheckAndLoad(optGetIntStr,1,"C"); }
  {int s[]={13,11}; CheckAndLoad(optGetDblStr,1,"C"); }
  {int s[]={12,11}; CheckAndLoad(optGetStrStr,1,"C"); }
  {int s[]={0,11,3}; CheckAndLoad(optSetIntStr,2,"C"); }
  {int s[]={0,11,13}; CheckAndLoad(optSetDblStr,2,"C"); }
  {int s[]={0,11,11}; CheckAndLoad(optSetStrStr,2,"C"); }
  {int s[]={15,11}; CheckAndLoad(optIsDeprecated,1,"C"); }
  {int s[]={3}; CheckAndLoad(optCount,0,""); }
  {int s[]={3}; CheckAndLoad(optMessageCount,0,""); }
  {int s[]={3}; CheckAndLoad(optGroupCount,0,""); }
  {int s[]={3}; CheckAndLoad(optRecentEnabled,0,""); }
  {int s[]={0,3}; CheckAndLoad(optRecentEnabledSet,1,""); }
  {int s[]={12}; CheckAndLoad(optSeparator,0,"C"); }
  {int s[]={12}; CheckAndLoad(optStringQuote,0,"C"); }

 return 0;

 symMissing:
  if (errBuf && errBufSize>0) {
    int elen;
    char* ebuf;
    elen = errBufSize;  ebuf = errBuf;
    rc = sprintf (ebuf, "%.*s", elen, "Could not load symbol '");
    elen -= rc;  ebuf+= rc;
    rc = sprintf (ebuf, "%.*s", elen, symName);
    elen -= rc;  ebuf+= rc;
    rc = sprintf (ebuf, "%.*s", elen, "': ");
    elen -= rc;  ebuf+= rc;
    rc = sprintf (ebuf, "%.*s", elen, errMsg);
    /* elen -= rc;  ebuf+= rc; */
    errBuf[errBufSize-1] = '\0';
    /* printf ("%s\n", errBuf); */
    return 2;
  }

 return 0;

} /* XLibraryLoad */

static int
libloader(const char *dllPath, const char *dllName, char *msgBuf, int msgBufSize)
{

  char dllNameBuf[512];
  int myrc = 0;

#if ! defined(GMS_DLL_PREFIX)
# error "GMS_DLL_PREFIX expected but not defined"
#endif
#if ! defined(GMS_DLL_BASENAME)
# error "GMS_DLL_BASENAME expected but not defined"
#endif
#if ! defined(GMS_DLL_EXTENSION)
# error "GMS_DLL_EXTENSION expected but not defined"
#endif
#if ! defined(GMS_DLL_SUFFIX)
# error "GMS_DLL_SUFFIX expected but not defined"
#endif

  if (NULL != msgBuf) msgBuf[0] = '\0';

  if (! isLoaded) {
    if (NULL != dllPath && '\0' != *dllPath) {
      strncpy(dllNameBuf, dllPath, sizeof(dllNameBuf)-1);
      dllNameBuf[sizeof(dllNameBuf)-1] = '\0';
#if defined(_WIN32)
      if ('\\' != dllNameBuf[strlen(dllNameBuf)])
        strcat(dllNameBuf,"\\");
#else
      if ('/' != dllNameBuf[strlen(dllNameBuf)])
        strcat(dllNameBuf,"/");
#endif
    }
    else {
      dllNameBuf[0] = '\0';
    }
    if (NULL != dllName && '\0' != *dllName) {
      strncat(dllNameBuf, dllName, sizeof(dllNameBuf)-strlen(dllNameBuf)-1);
    }
    else {
      strncat(dllNameBuf, GMS_DLL_PREFIX GMS_DLL_BASENAME, sizeof(dllNameBuf)-strlen(dllNameBuf)-1);
      strncat(dllNameBuf, GMS_DLL_SUFFIX                 , sizeof(dllNameBuf)-strlen(dllNameBuf)-1);
      strncat(dllNameBuf, GMS_DLL_EXTENSION              , sizeof(dllNameBuf)-strlen(dllNameBuf)-1);
    }
    isLoaded = ! XLibraryLoad (dllNameBuf, msgBuf, msgBufSize);
    if (isLoaded) {
       if (NULL != optSetLoadPath && NULL != dllPath && '\0' != *dllPath) {
         optSetLoadPath(dllPath);
       }
       else {                            /* no setLoadPath call found */
         myrc |= 2;
       }
    }
    else {                              /* library load failed */
      myrc |= 1;
    }
  }
  return (myrc & 1) == 0;
}

int optGetReady (char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(NULL, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* optGetReady */

int optGetReadyD (const char *dirName, char *msgBuf, int msgBufSize)
{
  int rc;
  lock(libMutex);
  rc = libloader(dirName, NULL, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* optGetReadyD */

int optGetReadyL (const char *libName, char *msgBuf, int msgBufSize)
{
  char dirName[1024],fName[1024];
  int rc;
  extractFileDirFileName (libName, dirName, fName);
  lock(libMutex);
  rc = libloader(dirName, fName, msgBuf, msgBufSize);
  unlock(libMutex);
  return rc;
} /* optGetReadyL */

int optCreate (optHandle_t *popt, char *msgBuf, int msgBufSize)
{
  int optIsReady;

  optIsReady = optGetReady (msgBuf, msgBufSize);
  if (! optIsReady) {
    return 0;
  }
  assert(XCreate);
  XCreate(popt);
  if(popt == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* optCreate */

int optCreateD (optHandle_t *popt, const char *dirName,
                char *msgBuf, int msgBufSize)
{
  int optIsReady;

  optIsReady = optGetReadyD (dirName, msgBuf, msgBufSize);
  if (! optIsReady) {
    return 0;
  }
  assert(XCreate);
  XCreate(popt);
  if(popt == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* optCreateD */

int optCreateL (optHandle_t *popt, const char *libName,
                char *msgBuf, int msgBufSize)
{
  int optIsReady;

  optIsReady = optGetReadyL (libName, msgBuf, msgBufSize);
  if (! optIsReady) {
    return 0;
  }
  assert(XCreate);
  XCreate(popt);
  if(popt == NULL)
  { strcpy(msgBuf,"Error while creating object"); return 0; }
  lock(objMutex);
  objectCount++;
  unlock(objMutex);
  return 1;                     /* return true on successful library load */
} /* optCreateL */

int optFree   (optHandle_t *popt)
{
  assert(XFree);
  XFree(popt); popt = NULL;
  lock(objMutex);
  objectCount--;
  unlock(objMutex);
  return 1;
} /* optFree */

int optLibraryLoaded(void)
{
  int rc;
  lock(libMutex);
  rc = isLoaded;
  unlock(libMutex);
  return rc;
} /* optLibraryLoaded */

int optLibraryUnload(void)
{
  lock(objMutex);
  if (objectCount > 0)
  {
    unlock(objMutex);
    return 0;
  }
  unlock(objMutex);
  lock(libMutex);
  if (isLoaded)
  {
    isLoaded = 0;
    (void) unLoadLib(h);
  }
  unlock(libMutex);
  return 1;
} /* optLibraryUnload */

int  optCorrectLibraryVersion(char *msgBuf, int msgBufLen)
{
  int cl;
  char localBuf[256];

  if (msgBuf && msgBufLen) msgBuf[0] = '\0';

  if (! isLoaded) {
    strncpy(msgBuf, "Library needs to be initialized first", msgBufLen);
    return 0;
  }

  if (NULL == XAPIVersion) {
    strncpy(msgBuf, "Function XAPIVersion not found", msgBufLen);
    return 0;
  }

  XAPIVersion(OPTAPIVERSION,localBuf,&cl);
  strncpy(msgBuf, localBuf, msgBufLen);

  if (1 == cl)
    return 1;
  else
    return 0;
}

int optGetScreenIndicator(void)
{
  return ScreenIndicator;
}

void optSetScreenIndicator(int scrind)
{
  ScreenIndicator = scrind ? 1 : 0;
}

int optGetExceptionIndicator(void)
{
   return ExceptionIndicator;
}

void optSetExceptionIndicator(int excind)
{
  ExceptionIndicator = excind ? 1 : 0;
}

int optGetExitIndicator(void)
{
  return ExitIndicator;
}

void optSetExitIndicator(int extind)
{
  ExitIndicator = extind ? 1 : 0;
}

optErrorCallback_t optGetErrorCallback(void)
{
  return ErrorCallBack;
}

void optSetErrorCallback(optErrorCallback_t func)
{
  lock(exceptMutex);
  ErrorCallBack = func;
  unlock(exceptMutex);
}

int optGetAPIErrorCount(void)
{
  return APIErrorCount;
}

void optSetAPIErrorCount(int ecnt)
{
  APIErrorCount = ecnt;
}

void optErrorHandling(const char *msg)
{
  APIErrorCount++;
  if (ScreenIndicator) { printf("%s\n", msg); fflush(stdout); }
  lock(exceptMutex);
  if (ErrorCallBack)
    if (ErrorCallBack(APIErrorCount, msg)) { unlock(exceptMutex); exit(123); }
  unlock(exceptMutex);
  assert(!ExceptionIndicator);
  if (ExitIndicator) exit(123);
}
