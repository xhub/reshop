/* optcc.h
 * Header file for C-style interface to the OPT library
 * 
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


#if ! defined(_OPTCC_H_)
#     define  _OPTCC_H_

#define OPTAPIVERSION 4

enum optDataType {
  optDataNone    = 0,
  optDataInteger = 1,
  optDataDouble  = 2,
  optDataString  = 3,
  optDataStrList = 4  };

enum optOptionType {
  optTypeInteger   = 0,
  optTypeDouble    = 1,
  optTypeString    = 2,
  optTypeBoolean   = 3,
  optTypeEnumStr   = 4,
  optTypeEnumInt   = 5,
  optTypeMultiList = 6,
  optTypeStrList   = 7,
  optTypeMacro     = 8,
  optTypeImmediate = 9  };

enum optOptionSubType {
  optsubRequired = 0,
  optsubNoValue  = 1,
  optsubOptional = 2,
  optsub2Values  = 3  };

enum optMsgType {
  optMsgInputEcho    = 0,
  optMsgHelp         = 1,
  optMsgDefineError  = 2,
  optMsgValueError   = 3,
  optMsgValueWarning = 4,
  optMsgDeprecated   = 5,
  optMsgFileEnter    = 6,
  optMsgFileLeave    = 7,
  optMsgTooManyMsgs  = 8,
  optMsgUserError    = 9  };

enum optVarEquMapType {
  optMapIndicator  = 0,
  optMapDefinedVar = 1  };

#if defined(_WIN32)
# define OPT_CALLCONV __stdcall
#else
# define OPT_CALLCONV
#endif

#if defined(__cplusplus)
extern "C" {
#endif

struct optRec;
typedef struct optRec *optHandle_t;

typedef int (*optErrorCallback_t) (int ErrCount, const char *msg);

/* headers for "wrapper" routines implemented in C */
/** optGetReady: load library
 *  @return false on failure to load library, true on success 
 */
int optGetReady  (char *msgBuf, int msgBufLen);

/** optGetReadyD: load library from the speicified directory
 * @return false on failure to load library, true on success 
 */
int optGetReadyD (const char *dirName, char *msgBuf, int msgBufLen);

/** optGetReadyL: load library from the specified library name
 *  @return false on failure to load library, true on success 
 */
int optGetReadyL (const char *libName, char *msgBuf, int msgBufLen);

/** optCreate: load library and create opt object handle 
 *  @return false on failure to load library, true on success 
 */
int optCreate    (optHandle_t *popt, char *msgBuf, int msgBufLen);

/** optCreateD: load library from the specified directory and create opt object handle
 * @return false on failure to load library, true on success 
 */
int optCreateD   (optHandle_t *popt, const char *dirName, char *msgBuf, int msgBufLen);

/** optCreate: load library from the specified library name and create opt object handle
 * @return false on failure to load library, true on success 
 */
int optCreateL   (optHandle_t *popt, const char *libName, char *msgBuf, int msgBufLen);

/** optFree: free opt object handle 
 * @return false on failure, true on success 
 */
int optFree      (optHandle_t *popt);

/** Check if library has been loaded
 * @return false on failure, true on success 
 */
int optLibraryLoaded(void);

/** Check if library has been unloaded
 * @return false on failure, true on success 
 */
int optLibraryUnload(void);

/** Check if API and library have the same version, Library needs to be initialized before calling this.
 * @return true  (1) on success,
 *         false (0) on failure.
 */
int  optCorrectLibraryVersion(char *msgBuf, int msgBufLen);

int  optGetScreenIndicator   (void);
void optSetScreenIndicator   (int scrind);
int  optGetExceptionIndicator(void);
void optSetExceptionIndicator(int excind);
int  optGetExitIndicator     (void);
void optSetExitIndicator     (int extind);
optErrorCallback_t optGetErrorCallback(void);
void optSetErrorCallback(optErrorCallback_t func);
int  optGetAPIErrorCount     (void);
void optSetAPIErrorCount     (int ecnt);

void optErrorHandling(const char *msg);
void optInitMutexes(void);
void optFiniMutexes(void);

#if defined(OPT_MAIN)    /* we must define some things only once */
# define OPT_FUNCPTR(NAME)  NAME##_t NAME = NULL
#else
# define OPT_FUNCPTR(NAME)  extern NAME##_t NAME
#endif

/* function typedefs and pointer definitions */
typedef int (OPT_CALLCONV *TArgvCB_t) (char *argv, int *idx);

/* Prototypes for Dummy Functions */
int  OPT_CALLCONV d_optReadDefinition (optHandle_t popt, const char *fn);
int  OPT_CALLCONV d_optReadDefinitionFromPChar (optHandle_t popt, char *p);
int  OPT_CALLCONV d_optReadParameterFile (optHandle_t popt, const char *fn);
void  OPT_CALLCONV d_optReadFromStr (optHandle_t popt, const char *s);
int  OPT_CALLCONV d_optWriteParameterFile (optHandle_t popt, const char *fn);
void  OPT_CALLCONV d_optClearMessages (optHandle_t popt);
void  OPT_CALLCONV d_optAddMessage (optHandle_t popt, const char *info);
void  OPT_CALLCONV d_optGetMessage (optHandle_t popt, int NrMsg, char *info, int *iType);
void  OPT_CALLCONV d_optResetAll (optHandle_t popt);
void  OPT_CALLCONV d_optResetAllRecent (optHandle_t popt);
void  OPT_CALLCONV d_optResetRecentChanges (optHandle_t popt);
void  OPT_CALLCONV d_optShowHelp (optHandle_t popt, const char *AHlpID);
int  OPT_CALLCONV d_optResetNr (optHandle_t popt, int ANr);
int  OPT_CALLCONV d_optFindStr (optHandle_t popt, const char *AName, int *ANr, int *ARefNr);
int  OPT_CALLCONV d_optGetInfoNr (optHandle_t popt, int ANr, int *ADefined, int *ADefinedR, int *ARefNr, int *ADataType, int *AOptType, int *ASubType);
int  OPT_CALLCONV d_optGetValuesNr (optHandle_t popt, int ANr, char *ASName, int *AIVal, double *ADVal, char *ASVal);
int  OPT_CALLCONV d_optSetValuesNr (optHandle_t popt, int ANr, int AIVal, double ADVal, const char *ASVal);
int  OPT_CALLCONV d_optSetValues2Nr (optHandle_t popt, int ANr, int AIVal, double ADVal, const char *ASVal);
void  OPT_CALLCONV d_optVersion (optHandle_t popt, char *sversion);
void  OPT_CALLCONV d_optDefinitionFile (optHandle_t popt, char *sfilename);
int  OPT_CALLCONV d_optGetFromAnyStrList (optHandle_t popt, int idash, char *skey, char *sval);
int  OPT_CALLCONV d_optGetFromListStr (optHandle_t popt, const char *skey, char *sval);
int  OPT_CALLCONV d_optListCountStr (optHandle_t popt, const char *skey);
int  OPT_CALLCONV d_optReadFromListStr (optHandle_t popt, const char *skey, int iPos, char *sval);
int  OPT_CALLCONV d_optSynonymCount (optHandle_t popt);
int  OPT_CALLCONV d_optGetSynonym (optHandle_t popt, int NrSyn, char *SSyn, char *SName);
void  OPT_CALLCONV d_optEchoSet (optHandle_t popt, int AIVal);
int  OPT_CALLCONV d_optEOLOnlySet (optHandle_t popt, int AIVal);
void  OPT_CALLCONV d_optNoBoundsSet (optHandle_t popt, int AIVal);
int  OPT_CALLCONV d_optEOLChars (optHandle_t popt, char *EOLChars);
void  OPT_CALLCONV d_optErrorCount (optHandle_t popt, int *iErrors, int *iWarnings);
int  OPT_CALLCONV d_optGetBoundsInt (optHandle_t popt, int ANr, int *ilval, int *ihval, int *idval);
int  OPT_CALLCONV d_optGetBoundsDbl (optHandle_t popt, int ANr, double *dlval, double *dhval, double *ddval);
int  OPT_CALLCONV d_optGetDefaultStr (optHandle_t popt, int ANr, char *sval);
int  OPT_CALLCONV d_optGetIntNr (optHandle_t popt, int ANr, int *AIVal);
int  OPT_CALLCONV d_optGetInt2Nr (optHandle_t popt, int ANr, int *AIVal);
int  OPT_CALLCONV d_optSetIntNr (optHandle_t popt, int ANr, int AIVal);
int  OPT_CALLCONV d_optSetInt2Nr (optHandle_t popt, int ANr, int AIVal);
int  OPT_CALLCONV d_optGetStrNr (optHandle_t popt, int ANr, char *ASVal);
int  OPT_CALLCONV d_optGetOptHelpNr (optHandle_t popt, int ANr, char *AName, int *AHc, int *AGroup);
int  OPT_CALLCONV d_optGetEnumHelp (optHandle_t popt, int ANr, int AOrd, int *AHc, char *AHelpStr);
int  OPT_CALLCONV d_optGetEnumStrNr (optHandle_t popt, int ANr, char *ASVal, int *AOrd);
int  OPT_CALLCONV d_optGetEnumCount (optHandle_t popt, int ANr, int *ACount);
int  OPT_CALLCONV d_optGetEnumValue (optHandle_t popt, int ANr, int AOrd, int *AValInt, char *AValStr);
int  OPT_CALLCONV d_optGetStr2Nr (optHandle_t popt, int ANr, char *ASVal);
int  OPT_CALLCONV d_optSetStrNr (optHandle_t popt, int ANr, const char *ASVal);
int  OPT_CALLCONV d_optSetStr2Nr (optHandle_t popt, int ANr, const char *ASVal);
int  OPT_CALLCONV d_optGetDblNr (optHandle_t popt, int ANr, double *ADVal);
int  OPT_CALLCONV d_optGetDbl2Nr (optHandle_t popt, int ANr, double *ADVal);
int  OPT_CALLCONV d_optSetDblNr (optHandle_t popt, int ANr, double ADVal);
int  OPT_CALLCONV d_optSetDbl2Nr (optHandle_t popt, int ANr, double ADVal);
int  OPT_CALLCONV d_optGetValStr (optHandle_t popt, const char *AName, char *ASVal);
int  OPT_CALLCONV d_optGetVal2Str (optHandle_t popt, const char *AName, char *ASVal);
int  OPT_CALLCONV d_optGetNameNr (optHandle_t popt, int ANr, char *ASName);
int  OPT_CALLCONV d_optGetDefinedNr (optHandle_t popt, int ANr, int *AIVal);
int  OPT_CALLCONV d_optGetHelpNr (optHandle_t popt, int ANr, char *ASOpt, char *ASHelp);
int  OPT_CALLCONV d_optGetGroupNr (optHandle_t popt, int ANr, char *AName, int *AGroup, int *AHc, char *AHelp);
int  OPT_CALLCONV d_optGetGroupGrpNr (optHandle_t popt, int AGroup);
int  OPT_CALLCONV d_optGetOptGroupNr (optHandle_t popt, int ANr);
int  OPT_CALLCONV d_optGetDotOptNr (optHandle_t popt, int ANr, char *VEName, int *AObjNr, int *ADim, double *AValue);
int  OPT_CALLCONV d_optGetDotOptUel (optHandle_t popt, int ANr, int ADim, char *AUEL);
int  OPT_CALLCONV d_optGetVarEquMapNr (optHandle_t popt, int maptype, int ANr, char *EquName, char *VarName, int *EquDim, int *VarDim, int *AValue);
int  OPT_CALLCONV d_optGetEquVarEquMapNr (optHandle_t popt, int maptype, int ANr, int ADim, char *AIndex);
int  OPT_CALLCONV d_optGetVarVarEquMapNr (optHandle_t popt, int maptype, int ANr, int ADim, char *AIndex);
int  OPT_CALLCONV d_optVarEquMapCount (optHandle_t popt, int maptype, int *ANrErrors);
int  OPT_CALLCONV d_optGetIndicatorNr (optHandle_t popt, int ANr, char *EquName, char *VarName, int *EquDim, int *VarDim, int *AValue);
int  OPT_CALLCONV d_optGetEquIndicatorNr (optHandle_t popt, int ANr, int ADim, char *AIndex);
int  OPT_CALLCONV d_optGetVarIndicatorNr (optHandle_t popt, int ANr, int ADim, char *AIndex);
int  OPT_CALLCONV d_optIndicatorCount (optHandle_t popt, int *ANrErrors);
int  OPT_CALLCONV d_optDotOptCount (optHandle_t popt, int *ANrErrors);
int  OPT_CALLCONV d_optSetRefNr (optHandle_t popt, int ANr, int ARefNr);
int  OPT_CALLCONV d_optSetRefNrStr (optHandle_t popt, const char *AOpt, int ARefNr);
int  OPT_CALLCONV d_optGetConstName (optHandle_t popt, int cgroup, int cindex, char *cname);
int  OPT_CALLCONV d_optGetTypeName (optHandle_t popt, int TNr, char *sTName);
int  OPT_CALLCONV d_optLookUp (optHandle_t popt, const char *AOpt);
void  OPT_CALLCONV d_optReadFromPChar (optHandle_t popt, char *p);
void  OPT_CALLCONV d_optReadFromCmdLine (optHandle_t popt, char *p);
void  OPT_CALLCONV d_optReadFromCmdArgs (optHandle_t popt, TArgvCB_t cb);
int  OPT_CALLCONV d_optGetNameOpt (optHandle_t popt, const char *ASVal, char *solver, int *opt);
int  OPT_CALLCONV d_optResetStr (optHandle_t popt, const char *AName);
int  OPT_CALLCONV d_optGetDefinedStr (optHandle_t popt, const char *AName);
int  OPT_CALLCONV d_optGetIntStr (optHandle_t popt, const char *AName);
double  OPT_CALLCONV d_optGetDblStr (optHandle_t popt, const char *AName);
char * OPT_CALLCONV d_optGetStrStr (optHandle_t popt, const char *AName, char *buf);
void  OPT_CALLCONV d_optSetIntStr (optHandle_t popt, const char *AName, int AIVal);
void  OPT_CALLCONV d_optSetDblStr (optHandle_t popt, const char *AName, double ADVal);
void  OPT_CALLCONV d_optSetStrStr (optHandle_t popt, const char *AName, const char *ASVal);
int  OPT_CALLCONV d_optIsDeprecated (optHandle_t popt, const char *AName);
int  OPT_CALLCONV d_optCount (optHandle_t popt);
int  OPT_CALLCONV d_optMessageCount (optHandle_t popt);
int  OPT_CALLCONV d_optGroupCount (optHandle_t popt);
int  OPT_CALLCONV d_optRecentEnabled (optHandle_t popt);
void OPT_CALLCONV d_optRecentEnabledSet (optHandle_t popt, const int x);
char * OPT_CALLCONV d_optSeparator (optHandle_t popt, char *buf);
char * OPT_CALLCONV d_optStringQuote (optHandle_t popt, char *buf);


typedef int  (OPT_CALLCONV *optReadDefinition_t) (optHandle_t popt, const char *fn);
/** Read definition file.
 *
 * @param popt opt object handle
 * @param fn File name
 * @return -1 if there was a definition error
 */
OPT_FUNCPTR(optReadDefinition);

typedef int  (OPT_CALLCONV *optReadDefinitionFromPChar_t) (optHandle_t popt, char *p);
/** Read definition from array of character
 *
 * @param popt opt object handle
 * @param p PChar: pointer to first character
 * @return -1 if there was a definition error
 */
OPT_FUNCPTR(optReadDefinitionFromPChar);

typedef int  (OPT_CALLCONV *optReadParameterFile_t) (optHandle_t popt, const char *fn);
/** Read parameters from file
 *
 * @param popt opt object handle
 * @param fn File name
 */
OPT_FUNCPTR(optReadParameterFile);

typedef void  (OPT_CALLCONV *optReadFromStr_t) (optHandle_t popt, const char *s);
/** Read options from string. In the case of errors, messages will be added to the message queue (see OptGetMessage)
 *
 * @param popt opt object handle
 * @param s String
 * @return -1 if there was a definition error.
 */
OPT_FUNCPTR(optReadFromStr);

typedef int  (OPT_CALLCONV *optWriteParameterFile_t) (optHandle_t popt, const char *fn);
/** Write modified parameters to a file. In the case of errors, messages will be added to the message queue (see OptGetMessage)
 *
 * @param popt opt object handle
 * @param fn File name
 */
OPT_FUNCPTR(optWriteParameterFile);

typedef void  (OPT_CALLCONV *optClearMessages_t) (optHandle_t popt);
/** Clear all messages stored in the message queue.
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optClearMessages);

typedef void  (OPT_CALLCONV *optAddMessage_t) (optHandle_t popt, const char *info);
/** Add a message to the message queue
 *
 * @param popt opt object handle
 * @param info Message string
 */
OPT_FUNCPTR(optAddMessage);

typedef void  (OPT_CALLCONV *optGetMessage_t) (optHandle_t popt, int NrMsg, char *info, int *iType);
/** Read from message queue
 *
 * @param popt opt object handle
 * @param NrMsg Number of messages in message queue
 * @param info Message string
 * @param iType Message type value (see enumerated constants)
 */
OPT_FUNCPTR(optGetMessage);

typedef void  (OPT_CALLCONV *optResetAll_t) (optHandle_t popt);
/** Reset all defined and definedR flags and clear the message queue
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optResetAll);

typedef void  (OPT_CALLCONV *optResetAllRecent_t) (optHandle_t popt);
/** Reset all definedR flags
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optResetAllRecent);

typedef void  (OPT_CALLCONV *optResetRecentChanges_t) (optHandle_t popt);
/** Reset all options with definedR flag set
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optResetRecentChanges);

typedef void  (OPT_CALLCONV *optShowHelp_t) (optHandle_t popt, const char *AHlpID);
/** Write help for option(s)
 *
 * @param popt opt object handle
 * @param AHlpID Help level flags (can be combined): 1 = summary by group, 2 = alphabetical listing, 4 = include large help, 8 = include deprecated and obsolete
 */
OPT_FUNCPTR(optShowHelp);

typedef int  (OPT_CALLCONV *optResetNr_t) (optHandle_t popt, int ANr);
/** Reset option value to default
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 */
OPT_FUNCPTR(optResetNr);

typedef int  (OPT_CALLCONV *optFindStr_t) (optHandle_t popt, const char *AName, int *ANr, int *ARefNr);
/** Find option by name, return number and ref nr
 *
 * @param popt opt object handle
 * @param AName Option name
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ARefNr Option reference number
 */
OPT_FUNCPTR(optFindStr);

typedef int  (OPT_CALLCONV *optGetInfoNr_t) (optHandle_t popt, int ANr, int *ADefined, int *ADefinedR, int *ARefNr, int *ADataType, int *AOptType, int *ASubType);
/** Get information about option by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADefined Flag: 0 if not defined and not 0 if defined
 * @param ADefinedR Flag: 0 if not recently defined and not 0 if recently defined
 * @param ARefNr Option reference number
 * @param ADataType Data type
 * @param AOptType Option type
 * @param ASubType Option sub type
 */
OPT_FUNCPTR(optGetInfoNr);

typedef int  (OPT_CALLCONV *optGetValuesNr_t) (optHandle_t popt, int ANr, char *ASName, int *AIVal, double *ADVal, char *ASVal);
/** Read values
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASName Option name
 * @param AIVal Option integer value
 * @param ADVal Option double value
 * @param ASVal Option string value
 */
OPT_FUNCPTR(optGetValuesNr);

typedef int  (OPT_CALLCONV *optSetValuesNr_t) (optHandle_t popt, int ANr, int AIVal, double ADVal, const char *ASVal);
/** Set values
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 * @param ADVal Option double value
 * @param ASVal Option string value
 */
OPT_FUNCPTR(optSetValuesNr);

typedef int  (OPT_CALLCONV *optSetValues2Nr_t) (optHandle_t popt, int ANr, int AIVal, double ADVal, const char *ASVal);
/** Set second values
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 * @param ADVal Option double value
 * @param ASVal Option string value
 */
OPT_FUNCPTR(optSetValues2Nr);

typedef void  (OPT_CALLCONV *optVersion_t) (optHandle_t popt, char *sversion);
/** Get version number of object
 *
 * @param popt opt object handle
 * @param sversion Version number
 */
OPT_FUNCPTR(optVersion);

typedef void  (OPT_CALLCONV *optDefinitionFile_t) (optHandle_t popt, char *sfilename);
/** Get last processed definition file
 *
 * @param popt opt object handle
 * @param sfilename File name
 */
OPT_FUNCPTR(optDefinitionFile);

typedef int  (OPT_CALLCONV *optGetFromAnyStrList_t) (optHandle_t popt, int idash, char *skey, char *sval);
/** Read string list element name and value and remove the value. Returns 0 if no string list option found
 *
 * @param popt opt object handle
 * @param idash Dashed names flag: 0 = do not look for dashed names, 1 = look for dashed names
 * @param skey String list
 * @param sval String list element value
 */
OPT_FUNCPTR(optGetFromAnyStrList);

typedef int  (OPT_CALLCONV *optGetFromListStr_t) (optHandle_t popt, const char *skey, char *sval);
/** Read and remove queued strings from specified option.
 *
 * @param popt opt object handle
 * @param skey String list
 * @param sval String list element value
 * @return -2 if option not found, -1 if option is not a string list, 0 if string list is empty, 1 if item returned and remove
 */
OPT_FUNCPTR(optGetFromListStr);

typedef int  (OPT_CALLCONV *optListCountStr_t) (optHandle_t popt, const char *skey);
/** Number of elements stored in list
 *
 * @param popt opt object handle
 * @param skey String list
 */
OPT_FUNCPTR(optListCountStr);

typedef int  (OPT_CALLCONV *optReadFromListStr_t) (optHandle_t popt, const char *skey, int iPos, char *sval);
/** Read element iPos from list without removal
 *
 * @param popt opt object handle
 * @param skey String list
 * @param iPos Position in list
 * @param sval String list element value
 */
OPT_FUNCPTR(optReadFromListStr);

typedef int  (OPT_CALLCONV *optSynonymCount_t) (optHandle_t popt);
/** Number of synonyms
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optSynonymCount);

typedef int  (OPT_CALLCONV *optGetSynonym_t) (optHandle_t popt, int NrSyn, char *SSyn, char *SName);
/** Get synonym and original option name
 *
 * @param popt opt object handle
 * @param NrSyn Number of the synoym
 * @param SSyn Option synomyn
 * @param SName Option name
 */
OPT_FUNCPTR(optGetSynonym);

typedef void  (OPT_CALLCONV *optEchoSet_t) (optHandle_t popt, int AIVal);
/** Set echo of input on or off
 *
 * @param popt opt object handle
 * @param AIVal Option integer value
 */
OPT_FUNCPTR(optEchoSet);

typedef int  (OPT_CALLCONV *optEOLOnlySet_t) (optHandle_t popt, int AIVal);
/** Set EOLOnly and return previous value
 *
 * @param popt opt object handle
 * @param AIVal Option integer value
 */
OPT_FUNCPTR(optEOLOnlySet);

typedef void  (OPT_CALLCONV *optNoBoundsSet_t) (optHandle_t popt, int AIVal);
/** Set bound checking on or off
 *
 * @param popt opt object handle
 * @param AIVal Option integer value
 */
OPT_FUNCPTR(optNoBoundsSet);

typedef int  (OPT_CALLCONV *optEOLChars_t) (optHandle_t popt, char *EOLChars);
/** Get eol characters as string returns number of eol chars
 *
 * @param popt opt object handle
 * @param EOLChars Accepted end of line characters
 */
OPT_FUNCPTR(optEOLChars);

typedef void  (OPT_CALLCONV *optErrorCount_t) (optHandle_t popt, int *iErrors, int *iWarnings);
/** Retrieve number of errors / warnings from message queue.
 *
 * @param popt opt object handle
 * @param iErrors Number of errors
 * @param iWarnings Number of warnings
 */
OPT_FUNCPTR(optErrorCount);

typedef int  (OPT_CALLCONV *optGetBoundsInt_t) (optHandle_t popt, int ANr, int *ilval, int *ihval, int *idval);
/** Bounds and default for integer option
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ilval Lower bound
 * @param ihval Upper bound
 * @param idval Default value
 */
OPT_FUNCPTR(optGetBoundsInt);

typedef int  (OPT_CALLCONV *optGetBoundsDbl_t) (optHandle_t popt, int ANr, double *dlval, double *dhval, double *ddval);
/** Bounds and default for double option
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param dlval Lower bound
 * @param dhval Upper bound
 * @param ddval Default value
 */
OPT_FUNCPTR(optGetBoundsDbl);

typedef int  (OPT_CALLCONV *optGetDefaultStr_t) (optHandle_t popt, int ANr, char *sval);
/** Default value for a given string option
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param sval String list element value
 */
OPT_FUNCPTR(optGetDefaultStr);

typedef int  (OPT_CALLCONV *optGetIntNr_t) (optHandle_t popt, int ANr, int *AIVal);
/** Read integer option by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 */
OPT_FUNCPTR(optGetIntNr);

typedef int  (OPT_CALLCONV *optGetInt2Nr_t) (optHandle_t popt, int ANr, int *AIVal);
/** Read second integer option by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 */
OPT_FUNCPTR(optGetInt2Nr);

typedef int  (OPT_CALLCONV *optSetIntNr_t) (optHandle_t popt, int ANr, int AIVal);
/** Set integer option by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 */
OPT_FUNCPTR(optSetIntNr);

typedef int  (OPT_CALLCONV *optSetInt2Nr_t) (optHandle_t popt, int ANr, int AIVal);
/** Set second integer option by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal Option integer value
 */
OPT_FUNCPTR(optSetInt2Nr);

typedef int  (OPT_CALLCONV *optGetStrNr_t) (optHandle_t popt, int ANr, char *ASVal);
/** Read string by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASVal Option string value
 */
OPT_FUNCPTR(optGetStrNr);

typedef int  (OPT_CALLCONV *optGetOptHelpNr_t) (optHandle_t popt, int ANr, char *AName, int *AHc, int *AGroup);
/** Get option name, help context and group number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AName Option name
 * @param AHc Help Context number
 * @param AGroup Group number
 */
OPT_FUNCPTR(optGetOptHelpNr);

typedef int  (OPT_CALLCONV *optGetEnumHelp_t) (optHandle_t popt, int ANr, int AOrd, int *AHc, char *AHelpStr);
/** Get help text for enumerated value
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AOrd Ordinal position
 * @param AHc Help Context number
 * @param AHelpStr 
 */
OPT_FUNCPTR(optGetEnumHelp);

typedef int  (OPT_CALLCONV *optGetEnumStrNr_t) (optHandle_t popt, int ANr, char *ASVal, int *AOrd);
/** Get enumerated string value and ordinal
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASVal Option string value
 * @param AOrd Ordinal position
 */
OPT_FUNCPTR(optGetEnumStrNr);

typedef int  (OPT_CALLCONV *optGetEnumCount_t) (optHandle_t popt, int ANr, int *ACount);
/** Number of enumerations in option
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ACount Number of enumerations
 */
OPT_FUNCPTR(optGetEnumCount);

typedef int  (OPT_CALLCONV *optGetEnumValue_t) (optHandle_t popt, int ANr, int AOrd, int *AValInt, char *AValStr);
/** Enumerated value by ordinal number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AOrd Ordinal position
 * @param AValInt Option value as integer
 * @param AValStr Option value as string
 */
OPT_FUNCPTR(optGetEnumValue);

typedef int  (OPT_CALLCONV *optGetStr2Nr_t) (optHandle_t popt, int ANr, char *ASVal);
/** Read second string by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASVal Option string value
 */
OPT_FUNCPTR(optGetStr2Nr);

typedef int  (OPT_CALLCONV *optSetStrNr_t) (optHandle_t popt, int ANr, const char *ASVal);
/** Set string by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASVal Option string value
 */
OPT_FUNCPTR(optSetStrNr);

typedef int  (OPT_CALLCONV *optSetStr2Nr_t) (optHandle_t popt, int ANr, const char *ASVal);
/** Set second string by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASVal Option string value
 */
OPT_FUNCPTR(optSetStr2Nr);

typedef int  (OPT_CALLCONV *optGetDblNr_t) (optHandle_t popt, int ANr, double *ADVal);
/** Read double by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADVal Option double value
 */
OPT_FUNCPTR(optGetDblNr);

typedef int  (OPT_CALLCONV *optGetDbl2Nr_t) (optHandle_t popt, int ANr, double *ADVal);
/** Read second double by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADVal Option double value
 */
OPT_FUNCPTR(optGetDbl2Nr);

typedef int  (OPT_CALLCONV *optSetDblNr_t) (optHandle_t popt, int ANr, double ADVal);
/** Set double by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADVal Option double value
 */
OPT_FUNCPTR(optSetDblNr);

typedef int  (OPT_CALLCONV *optSetDbl2Nr_t) (optHandle_t popt, int ANr, double ADVal);
/** Set second double by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADVal Option double value
 */
OPT_FUNCPTR(optSetDbl2Nr);

typedef int  (OPT_CALLCONV *optGetValStr_t) (optHandle_t popt, const char *AName, char *ASVal);
/** Read value as string by option name
 *
 * @param popt opt object handle
 * @param AName Option name
 * @param ASVal Option string value
 */
OPT_FUNCPTR(optGetValStr);

typedef int  (OPT_CALLCONV *optGetVal2Str_t) (optHandle_t popt, const char *AName, char *ASVal);
/** Read second value as string by option name
 *
 * @param popt opt object handle
 * @param AName Option name
 * @param ASVal Option string value
 */
OPT_FUNCPTR(optGetVal2Str);

typedef int  (OPT_CALLCONV *optGetNameNr_t) (optHandle_t popt, int ANr, char *ASName);
/** Get option name by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASName Option name
 */
OPT_FUNCPTR(optGetNameNr);

typedef int  (OPT_CALLCONV *optGetDefinedNr_t) (optHandle_t popt, int ANr, int *AIVal);
/** Get defined status by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AIVal 
 */
OPT_FUNCPTR(optGetDefinedNr);

typedef int  (OPT_CALLCONV *optGetHelpNr_t) (optHandle_t popt, int ANr, char *ASOpt, char *ASHelp);
/** Get option name and help by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ASOpt 
 * @param ASHelp 
 */
OPT_FUNCPTR(optGetHelpNr);

typedef int  (OPT_CALLCONV *optGetGroupNr_t) (optHandle_t popt, int ANr, char *AName, int *AGroup, int *AHc, char *AHelp);
/** Get group information by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param AName Option name
 * @param AGroup Group number
 * @param AHc Help Context number
 * @param AHelp Help string
 */
OPT_FUNCPTR(optGetGroupNr);

typedef int  (OPT_CALLCONV *optGetGroupGrpNr_t) (optHandle_t popt, int AGroup);
/** Get group record by group number
 *
 * @param popt opt object handle
 * @param AGroup Group number
 */
OPT_FUNCPTR(optGetGroupGrpNr);

typedef int  (OPT_CALLCONV *optGetOptGroupNr_t) (optHandle_t popt, int ANr);
/** Get group number of an option by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 */
OPT_FUNCPTR(optGetOptGroupNr);

typedef int  (OPT_CALLCONV *optGetDotOptNr_t) (optHandle_t popt, int ANr, char *VEName, int *AObjNr, int *ADim, double *AValue);
/** Dot option info
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param VEName Variable or equation name
 * @param AObjNr 
 * @param ADim Number of indices of variable or equation
 * @param AValue Option value
 */
OPT_FUNCPTR(optGetDotOptNr);

typedef int  (OPT_CALLCONV *optGetDotOptUel_t) (optHandle_t popt, int ANr, int ADim, char *AUEL);
/** Retrieve a single element from a dot option
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADim Number of indices of variable or equation
 * @param AUEL 
 */
OPT_FUNCPTR(optGetDotOptUel);

typedef int  (OPT_CALLCONV *optGetVarEquMapNr_t) (optHandle_t popt, int maptype, int ANr, char *EquName, char *VarName, int *EquDim, int *VarDim, int *AValue);
/** Variable equation mapping info
 *
 * @param popt opt object handle
 * @param maptype Type of variable equation mapping (see enumerated constants)
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param EquName Equation name in indicator option
 * @param VarName Variable name in indicator option
 * @param EquDim Equation dimension in indicator option
 * @param VarDim Variable dimension in indicator option
 * @param AValue Option value
 */
OPT_FUNCPTR(optGetVarEquMapNr);

typedef int  (OPT_CALLCONV *optGetEquVarEquMapNr_t) (optHandle_t popt, int maptype, int ANr, int ADim, char *AIndex);
/** Equation part of variable equation mapping
 *
 * @param popt opt object handle
 * @param maptype Type of variable equation mapping (see enumerated constants)
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADim Number of indices of variable or equation
 * @param AIndex Domain index string
 */
OPT_FUNCPTR(optGetEquVarEquMapNr);

typedef int  (OPT_CALLCONV *optGetVarVarEquMapNr_t) (optHandle_t popt, int maptype, int ANr, int ADim, char *AIndex);
/** Variable part of variable equation mapping
 *
 * @param popt opt object handle
 * @param maptype Type of variable equation mapping (see enumerated constants)
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADim Number of indices of variable or equation
 * @param AIndex Domain index string
 */
OPT_FUNCPTR(optGetVarVarEquMapNr);

typedef int  (OPT_CALLCONV *optVarEquMapCount_t) (optHandle_t popt, int maptype, int *ANrErrors);
/** Variable equation mappings available and number ignored
 *
 * @param popt opt object handle
 * @param maptype Type of variable equation mapping (see enumerated constants)
 * @param ANrErrors Number of syntactically incorrect dot options
 */
OPT_FUNCPTR(optVarEquMapCount);

typedef int  (OPT_CALLCONV *optGetIndicatorNr_t) (optHandle_t popt, int ANr, char *EquName, char *VarName, int *EquDim, int *VarDim, int *AValue);
/** Indicator info
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param EquName Equation name in indicator option
 * @param VarName Variable name in indicator option
 * @param EquDim Equation dimension in indicator option
 * @param VarDim Variable dimension in indicator option
 * @param AValue Option value
 */
OPT_FUNCPTR(optGetIndicatorNr);

typedef int  (OPT_CALLCONV *optGetEquIndicatorNr_t) (optHandle_t popt, int ANr, int ADim, char *AIndex);
/** Equation part of indicator
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADim Number of indices of variable or equation
 * @param AIndex Domain index string
 */
OPT_FUNCPTR(optGetEquIndicatorNr);

typedef int  (OPT_CALLCONV *optGetVarIndicatorNr_t) (optHandle_t popt, int ANr, int ADim, char *AIndex);
/** Variable part of indicator
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ADim Number of indices of variable or equation
 * @param AIndex Domain index string
 */
OPT_FUNCPTR(optGetVarIndicatorNr);

typedef int  (OPT_CALLCONV *optIndicatorCount_t) (optHandle_t popt, int *ANrErrors);
/** Indicators available and number ignored
 *
 * @param popt opt object handle
 * @param ANrErrors Number of syntactically incorrect dot options
 */
OPT_FUNCPTR(optIndicatorCount);

typedef int  (OPT_CALLCONV *optDotOptCount_t) (optHandle_t popt, int *ANrErrors);
/** Dot options available and number ignored
 *
 * @param popt opt object handle
 * @param ANrErrors Number of syntactically incorrect dot options
 */
OPT_FUNCPTR(optDotOptCount);

typedef int  (OPT_CALLCONV *optSetRefNr_t) (optHandle_t popt, int ANr, int ARefNr);
/** Set reference number by option number
 *
 * @param popt opt object handle
 * @param ANr Ordinal option number - an integer between 1..optCount
 * @param ARefNr Option reference number
 */
OPT_FUNCPTR(optSetRefNr);

typedef int  (OPT_CALLCONV *optSetRefNrStr_t) (optHandle_t popt, const char *AOpt, int ARefNr);
/** Set reference number by option name
 *
 * @param popt opt object handle
 * @param AOpt Option name
 * @param ARefNr Option reference number
 */
OPT_FUNCPTR(optSetRefNrStr);

typedef int  (OPT_CALLCONV *optGetConstName_t) (optHandle_t popt, int cgroup, int cindex, char *cname);
/** Get the name of a constant
 *
 * @param popt opt object handle
 * @param cgroup Constant group: 1 = Data types, 2 = Option types, 3 = Option sub-type, 4 = Message type
 * @param cindex Constant index
 * @param cname Constant name
 */
OPT_FUNCPTR(optGetConstName);

typedef int  (OPT_CALLCONV *optGetTypeName_t) (optHandle_t popt, int TNr, char *sTName);
/** Get option type name by type number
 *
 * @param popt opt object handle
 * @param TNr Type number
 * @param sTName Type name
 */
OPT_FUNCPTR(optGetTypeName);

typedef int  (OPT_CALLCONV *optLookUp_t) (optHandle_t popt, const char *AOpt);
/** Index number of an option
 *
 * @param popt opt object handle
 * @param AOpt Option name
 */
OPT_FUNCPTR(optLookUp);

typedef void  (OPT_CALLCONV *optReadFromPChar_t) (optHandle_t popt, char *p);
/** Read options from a PChar.  In the case of errors, messages will be added to the message queue (see OptGetMessage).
 *
 * @param popt opt object handle
 * @param p PChar: pointer to first character
 */
OPT_FUNCPTR(optReadFromPChar);

typedef void  (OPT_CALLCONV *optReadFromCmdLine_t) (optHandle_t popt, char *p);
/** Read options from a PChar.  In the case of errors, messages will be added to the message queue indicated as command line (see OptGetMessage).
 *
 * @param popt opt object handle
 * @param p PChar: pointer to first character
 */
OPT_FUNCPTR(optReadFromCmdLine);

typedef void  (OPT_CALLCONV *optReadFromCmdArgs_t) (optHandle_t popt, TArgvCB_t cb);
/** Read options from shortStrings returned by callback func cb.  In the case of errors, messages will be added to the message queue indicated as command line args (see OptGetMessage).
 *
 * @param popt opt object handle
 * @param cb 
 */
OPT_FUNCPTR(optReadFromCmdArgs);

typedef int  (OPT_CALLCONV *optGetNameOpt_t) (optHandle_t popt, const char *ASVal, char *solver, int *opt);
/** Extract solver name and optfile number from string option.
 *
 * @param popt opt object handle
 * @param ASVal Option string value
 * @param solver Solver name
 * @param opt Optfile number
 */
OPT_FUNCPTR(optGetNameOpt);

typedef int  (OPT_CALLCONV *optResetStr_t) (optHandle_t popt, const char *AName);
/** Reset option to default by option name.
 *
 * @param popt opt object handle
 * @param AName Option name
 */
OPT_FUNCPTR(optResetStr);

typedef int  (OPT_CALLCONV *optGetDefinedStr_t) (optHandle_t popt, const char *AName);
/** Get defined status by option name.
 *
 * @param popt opt object handle
 * @param AName Option name
 */
OPT_FUNCPTR(optGetDefinedStr);

typedef int  (OPT_CALLCONV *optGetIntStr_t) (optHandle_t popt, const char *AName);
/** Read integer by option name.
 *
 * @param popt opt object handle
 * @param AName Option name
 */
OPT_FUNCPTR(optGetIntStr);

typedef double  (OPT_CALLCONV *optGetDblStr_t) (optHandle_t popt, const char *AName);
/** Read double by option name.
 *
 * @param popt opt object handle
 * @param AName Option name
 */
OPT_FUNCPTR(optGetDblStr);

typedef char * (OPT_CALLCONV *optGetStrStr_t) (optHandle_t popt, const char *AName, char *buf);
/** Read string by option name.
 *
 * @param popt opt object handle
 * @param AName Option name
 */
OPT_FUNCPTR(optGetStrStr);

typedef void  (OPT_CALLCONV *optSetIntStr_t) (optHandle_t popt, const char *AName, int AIVal);
/** Set integer by option name.
 *
 * @param popt opt object handle
 * @param AName Option name
 * @param AIVal Option integer value
 */
OPT_FUNCPTR(optSetIntStr);

typedef void  (OPT_CALLCONV *optSetDblStr_t) (optHandle_t popt, const char *AName, double ADVal);
/** Set double by option name.
 *
 * @param popt opt object handle
 * @param AName Option name
 * @param ADVal Option double value
 */
OPT_FUNCPTR(optSetDblStr);

typedef void  (OPT_CALLCONV *optSetStrStr_t) (optHandle_t popt, const char *AName, const char *ASVal);
/** Set string by option name.
 *
 * @param popt opt object handle
 * @param AName Option name
 * @param ASVal Option string value
 */
OPT_FUNCPTR(optSetStrStr);

typedef int  (OPT_CALLCONV *optIsDeprecated_t) (optHandle_t popt, const char *AName);
/** Returns true, if the option is deprecated
 *
 * @param popt opt object handle
 * @param AName Option name
 */
OPT_FUNCPTR(optIsDeprecated);

typedef int  (OPT_CALLCONV *optCount_t) (optHandle_t popt);
/** Number of options
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optCount);

typedef int  (OPT_CALLCONV *optMessageCount_t) (optHandle_t popt);
/** Number of messages in buffer
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optMessageCount);

typedef int  (OPT_CALLCONV *optGroupCount_t) (optHandle_t popt);
/** Number of option groups
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optGroupCount);

typedef int  (OPT_CALLCONV *optRecentEnabled_t) (optHandle_t popt);
/** When enabled (default), Defined and DefinedR will be set when assigning to an option. When disabled, only Defined will be set
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optRecentEnabled);

typedef void (OPT_CALLCONV *optRecentEnabledSet_t) (optHandle_t popt, const int x);
OPT_FUNCPTR(optRecentEnabledSet);

typedef char * (OPT_CALLCONV *optSeparator_t) (optHandle_t popt, char *buf);
/** Defined separator between option key and value
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optSeparator);

typedef char * (OPT_CALLCONV *optStringQuote_t) (optHandle_t popt, char *buf);
/** Defined quote string
 *
 * @param popt opt object handle
 */
OPT_FUNCPTR(optStringQuote);

#if defined(__cplusplus)
}
#endif
#endif /* #if ! defined(_OPTCC_H_) */
