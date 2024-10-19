#ifndef STATUS_H
#define STATUS_H

const char *status_descr(int status);

enum status {
   OK                          = 0,
   Error_BugPleaseReport,
   Error_DimensionDifferent,
   Error_DuplicateValue,
   Error_EMPIncorrectInput,
   Error_EMPIncorrectSyntax,
   Error_EMPRuntimeError,
   Error_EvaluationFailed,
   Error_FileOpenFailed,
   Error_GamsIncompleteSetupInfo,
   Error_GamsCallFailed,
   Error_GamsSolverCallFailed,
   Error_HDF5_IO,
   Error_HDF5_NullPointer,
   Error_HDF5_Unsupported,
   Error_IncompleteModelMetadata,
   Error_Inconsistency,
   Error_IndexOutOfRange,
   Error_InsufficientMemory,
   Error_InvalidArgument,
   Error_InvalidModel,
   Error_InvalidOpCode,
   Error_InvalidValue,
   Error_MathError,
   Error_ModelInfeasible,
   Error_ModelUnbounded,
   Error_NameTooLongForGams,
   Error_NotEmpty,
   Error_NotFound,
   Error_NotImplemented,
   Error_NotInitialized,
   Error_NotScalarVariable,
   Error_NullPointer,
   Error_OperationNotAllowed,
   Error_OptionNotFound,
   Error_OvfMissingParam,
   Error_RuntimeError,
   Error_SizeTooSmall,
   Error_SizeTooLarge,
   Error_SolverCreateFailed,
   Error_SolverError,
   Error_SolverInvalidName,
   Error_SolverModifyProblemFailed,
   Error_SymbolicDifferentiation,
   Error_SymbolNotInTheGamsRim,
   Error_SystemError,
   Error_UnExpectedData,
   Error_UnsupportedOperation,
   Error_WrongModelForFunction,
   Error_WrongNumericalValue,
   Error_WrongOptionValue,
   Error_WrongParameterValue,
   Error_LastError
};

static inline enum status status_update(enum status status, enum status error)
{
   return status != OK ? status : error;
}

#endif
