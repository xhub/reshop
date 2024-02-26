#include <assert.h>
#include <stdlib.h>

#include "status.h"
#include "macros.h"
#include "reshop.h"

#define DEFINE_ERRMSGS() \
ERRMSG(OK, "OK") \
ERRMSG(Error_DimensionDifferent, "DimensionDifferent") \
ERRMSG(Error_DuplicateValue, "DuplicateValue") \
ERRMSG(Error_EMPIncorrectInput, "EMPIncorrectInput") \
ERRMSG(Error_EMPIncorrectSyntax, "EMPIncorrectSyntax") \
ERRMSG(Error_EMPRuntimeError, "EMPRuntimeError") \
ERRMSG(Error_EvaluationFailed, "EvaluationFailed") \
ERRMSG(Error_FileOpenFailed, "FileOpenFailed") \
ERRMSG(Error_GAMSIncompleteSetupInfo, "GAMS setup info is incomplete") \
ERRMSG(Error_GAMSCallFailed, "GAMS Call Failed") \
ERRMSG(Error_HDF5_IO, "HDF5 IO") \
ERRMSG(Error_HDF5_NullPointer, "HDF5 NullPointer") \
ERRMSG(Error_HDF5_Unsupported, "HDF5 Unsupported") \
ERRMSG(Error_Infeasible, "Infeasible") \
ERRMSG(Error_IndexOutOfRange, "Index Out Of Range") \
ERRMSG(Error_InsufficientMemory, "Insufficient Memory") \
ERRMSG(Error_InvalidArgument, "Invalid Argument") \
ERRMSG(Error_InvalidModel, "Invalid Model") \
ERRMSG(Error_InvalidOpCode, "Invalid OpCode") \
ERRMSG(Error_InvalidValue, "InvalidValue") \
ERRMSG(Error_ModelIncompleteMetadata, "ModelIncompleteMetadata") \
ERRMSG(Error_NameTooLong4Gams, "Name exceeds the GAMS limit (255)") \
ERRMSG(Error_NotEmpty, "NotEmpty") \
ERRMSG(Error_NotFound, "NotFound") \
ERRMSG(Error_NotImplemented, "Given operation is not implemented") \
ERRMSG(Error_NotInitialized, "NotInitialized") \
ERRMSG(Error_NotInTheRim, "NotInTheRim") \
ERRMSG(Error_NotNonLinear, "NotNonLinear") \
ERRMSG(Error_NotScalarVariable, "NotScalarVariable") \
ERRMSG(Error_NullPointer, "Unexpected null pointer") \
ERRMSG(Error_MathError, "MathError") \
ERRMSG(Error_OperationNotAllowed, "Operation is not allowed") \
ERRMSG(Error_OptionNotFound, "OptionNotFound") \
ERRMSG(Error_OvfMissingParam, "Missing parameter in OVF definition") \
ERRMSG(Error_RuntimeError, "Unexpected runtime error") \
ERRMSG(Error_SizeTooSmall, "SizeTooSmall") \
ERRMSG(Error_SolverCallFailed, "SolverCallFailed") \
ERRMSG(Error_SolverCreateFailed, "SolverCreateFailed") \
ERRMSG(Error_SolverError, "Solver error") \
ERRMSG(Error_SolverInvalidName, "SolverInvalidName") \
ERRMSG(Error_SolverModifyProblemFailed, "SolverModifyProblemFailed") \
ERRMSG(Error_SolverReadyAPIFailed, "SolverReadyAPIFailed") \
ERRMSG(Error_SymbolicDifferentiation, "SymbolicDifferentiation") \
ERRMSG(Error_SystemError, "System error encountered") \
ERRMSG(Error_Unbounded, "Unbounded") \
ERRMSG(Error_UnExpectedData, "UnExpectedData") \
ERRMSG(Error_Unconsistency, "Unconsistency") \
ERRMSG(Error_UnsupportedOperation, "UnsupportedOperation") \
ERRMSG(Error_WrongModelForFunction, "WrongModelForFunction") \
ERRMSG(Error_WrongNumericalValue, "WrongNumericalValue") \
ERRMSG(Error_WrongOptionValue, "WrongOptionValue") \
ERRMSG(Error_WrongParameterValue, "WrongParameterValue")


#define ERRMSG(id, str) char id[sizeof(str)];

typedef struct {
   union {
      struct {
      DEFINE_ERRMSGS()
   };
   char dummystr[1];
   };
} ErrMsgs;
#undef ERRMSG

#define ERRMSG(id, str) .id = str,

static const ErrMsgs errmsgs = {
DEFINE_ERRMSGS()
};
#undef ERRMSG

#define ERRMSG(id, str) [id] = offsetof(ErrMsgs, id),
static const unsigned errmsgs_offsets[Error_LastError+1] = {
DEFINE_ERRMSGS()
};

static_assert(ARRAY_SIZE(errmsgs_offsets) == Error_LastError+1,
              "Check the size of error messages");



const char *rhp_status_descr(int status)
{
   if (status < 0 || status > Error_LastError) {
      return "unknown error";
   }

   return errmsgs.dummystr + errmsgs_offsets[status];
}

const char *status_descr(int status)
{
   if (status < 0 || status > Error_LastError) {
      return "unknown error";
   }

   return errmsgs.dummystr + errmsgs_offsets[status];
}
