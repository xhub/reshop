#ifndef GAMS_OPTION_H
#define GAMS_OPTION_H

enum gams_modelstatus {
   ModelStat_Unset                = 0,
   ModelStat_OptimalGlobal        = 1,
   ModelStat_OptimalLocal         = 2,
   ModelStat_Unbounded            = 3,   /**< The model is unbounded */
   ModelStat_InfeasibleGlobal     = 4,   /**< Model was proven to be infeasible */
   ModelStat_InfeasibleLocal      = 5,   /**< No feasible point locally found */
   ModelStat_InfeasibleIntermed   = 6,   /**< Current solution not feasible and the solver stopped for other reasons */
   ModelStat_Feasible             = 7,   /**< Feasible solution (without discrete variable) found */
   ModelStat_Integer              = 8,   /**< Feasible solution with discrete variable found */
   ModelStat_NonIntegerIntermed   = 9,   /**< Incomplete solution (not feasible) found */
   ModelStat_IntegerInfeasible    = 10,  /**< The integer model was proven infeasible */
   ModelStat_LicenseError         = 11,
   ModelStat_ErrorUnknown         = 12,  /**< A solve error occurred: no model status */
   ModelStat_ErrorNoSolution      = 13,
   ModelStat_NoSolutionReturned   = 14,  /**< No Solution has been provided. Used for instance for CONVERT */
   ModelStat_SolvedUnique         = 15,  /**< CNS specific: unique solution */
   ModelStat_Solved               = 16,  /**< CNS specific: (possibly) non-unique solution */
   ModelStat_SolvedSingular       = 17,  /**< CNS specific: Jacobian is singular */
   ModelStat_UnboundedNoSolution  = 18,
   ModelStat_InfeasibleNoSolution = 19
};

enum gams_solvestatus {
   SolveStat_Unset        = 0,
   SolveStat_Normal       = 1,
   SolveStat_Iteration    = 2,
   SolveStat_Resource     = 3,
   SolveStat_Solver       = 4,
   SolveStat_EvalError    = 5,
   SolveStat_Capability   = 6,
   SolveStat_License      = 7,
   SolveStat_User         = 8,
   SolveStat_SetupErr     = 9,
   SolveStat_SolverErr    = 10,
   SolveStat_InternalErr  = 11,
   SolveStat_Skipped      = 12,
   SolveStat_SystemErr    = 13
};

#endif /* GAMS_OPTION_H */
