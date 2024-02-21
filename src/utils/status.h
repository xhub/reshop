#ifndef STATUS_H
#define STATUS_H

const char *status_descr(int status);


enum status {
   OK                          = 0,
   Error_DimensionDifferent,
   Error_DuplicateValue,
   Error_EMPIncorrectInput,
   Error_EMPIncorrectSyntax,
   Error_EMPRuntimeError,
   Error_EvaluationFailed,
   Error_FileOpenFailed,
   Error_GAMSIncompleteSetupInfo,
   Error_GAMSCallFailed,
   Error_HDF5_IO,
   Error_HDF5_NullPointer,
   Error_HDF5_Unsupported,
   Error_Infeasible,
   Error_IndexOutOfRange,
   Error_InsufficientMemory,
   Error_InvalidArgument,
   Error_InvalidModel,
   Error_InvalidOpCode,
   Error_InvalidValue,
   Error_ModelIncompleteMetadata,
   Error_NameTooLong4Gams,
   Error_NotEmpty,
   Error_NotFound,
   Error_NotImplemented,
   Error_NotInitialized,
   Error_NotInTheRim,
   Error_NotNonLinear,
   Error_NotScalarVariable,
   Error_NullPointer,
   Error_MathError,
   Error_OperationNotAllowed,
   Error_OptionNotFound,
   Error_OvfMissingParam,
   Error_RuntimeError,
   Error_SizeTooSmall,
   Error_SolverCallFailed,
   Error_SolverCreateFailed,
   Error_SolverError,
   Error_SolverInvalidName,
   Error_SolverModifyProblemFailed,
   Error_SolverReadyAPIFailed,
   Error_SymbolicDifferentiation,
   Error_SystemError,
   Error_Unbounded,
   Error_UnExpectedData,
   Error_Unconsistency,
   Error_UnsupportedOperation,
   Error_WrongModelForFunction,
   Error_WrongNumericalValue,
   Error_WrongOptionValue,
   Error_WrongParameterValue,
   __Error_NumErrors
};

static inline enum status status_update(enum status status, enum status error)
{
   return status != OK ? status : error;
}

#endif
