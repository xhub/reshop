#include "empinterp_checks.h"
#include "empinterp_priv.h"

static int raise_error(Interpreter *interp)
{
   interp_erase_savedtok(interp);

   return Error_EMPIncorrectSyntax;
}

int bilevel_sanity_check(Interpreter *interp)
{
   if (bilevel_once(interp)) {
      errormsg("[empinterp] ERROR: only one bilevel statement is allows per "
               "empinfo file\n");
      return raise_error(interp);
   }

   if (_has_equilibrium(interp)) {
      errormsg("[empinterp] ERROR: the bilevel and equilibrium statements are "
               "mutually exclusive\n");
      return raise_error(interp);
   }

   if (_has_dag(interp)) {
      errormsg("[empinterp] ERROR: Using the old bilevel statement in an EMPDAG "
               "is not allowed. Please use the EMPDAG syntax for bilevel/MPEC "
               "node\n");
      return raise_error(interp);
   }

   if (_has_single_mp(interp)) {
      errormsg("[empinterp] ERROR: unexpected bilevel keyword after single "
               "problem definition\n");
      return raise_error(interp);

   }

   if (_has_implicit_Nash(interp)) {
      errormsg("[empinterp] ERROR: unexpected bilevel keyword after (implicit) "
               "Nash equilibrium definition\n");
      return raise_error(interp);

   }
   return OK;
}

int empdag_sanity_check(Interpreter *interp)
{
   if (bilevel_once(interp)) {
      errormsg("[empinterp] ERROR: the bilevel statement and an EMPDAG are "
               "mutually exclusive\n");
      return raise_error(interp);
   }

   if (_has_equilibrium(interp)) {
      errormsg("[empinterp] ERROR: the equilibrium statement and an EMPDAG are "
               "mutually exclusive\n");
      return raise_error(interp);
   }

   if (_has_single_mp(interp)) {
      errormsg("[empinterp] ERROR: unexpected EMPDAG after single "
               "problem definition\n");
      return raise_error(interp);

   }

   if (_has_implicit_Nash(interp)) {
      errormsg("[empinterp] ERROR: unexpected EMPDAG after (implicit) "
               "Nash equilibrium definition\n");
      return raise_error(interp);

   }
   return OK;
}

int dualequ_sanity_check(Interpreter *interp)
{
   if (_has_bilevel(interp)) {
      errormsg("[empinterp] ERROR: the bilevel and dualequ statements can only "
               "to used together when the dualequ is in the bilevel\n");
      return raise_error(interp);
   }

   if (_has_equilibrium(interp)) {
      errormsg("[empinterp] ERROR: the dualequ and equilibrium statements "
               "are mutually exclusive\n");
      return raise_error(interp);
   }

   if (_has_dag(interp)) {
      errormsg("[empinterp] ERROR: Using the old dualequ statement in an EMPDAG "
               "is not allowed. Please use the EMPDAG syntax for a VI node\n");
      return raise_error(interp);
   }

   if (_has_single_mp(interp)) {
      errormsg("[empinterp] ERROR: unexpected dualequ keyword after single "
               "problem definition\n");
      return raise_error(interp);

   }

   if (_has_implicit_Nash(interp)) {
      errormsg("[empinterp] ERROR: unexpected dualequ keyword after (implicit) "
               "Nash equilibrium definition, use a VI keyword\n");
      return raise_error(interp);

   }
   return OK;
}

int equilibrium_sanity_check(Interpreter *interp)
{
   if (bilevel_once(interp)) {
      errormsg("[empinterp] ERROR: the bilevel and equilibrium statements are "
               "mutually exclusive\n");
      return raise_error(interp);
   }

   if (_has_equilibrium(interp)) {
      errormsg("[empinterp] ERROR: only one equilibrium statement is allows per "
               "empinfo file\n");
      return raise_error(interp);
   }

   if (_has_dag(interp)) {
      errormsg("[empinterp] ERROR: Using the old-style equilibrium statement in "
               "an EMPDAG is not allowed. Please use the EMPDAG syntax "
               "exclusively\n");
      return raise_error(interp);
   }

   if (_has_single_mp(interp)) {
      errormsg("[empinterp] ERROR: unexpected equilibrium keyword after single "
               "problem definition\n");
      return raise_error(interp);

   }

   if (_has_implicit_Nash(interp)) {
      errormsg("[empinterp] ERROR: unexpected equilibrium keyword after (implicit) "
               "Nash equilibrium definition\n");
      return raise_error(interp);

   }
   return OK;
}

int mp_sanity_check(Interpreter *interp)
{
   if (_has_bilevel(interp)) {
      error("[empinterp] ERROR: the bilevel and %s keyword are only compatible"
            "when the latter is inside the former\n", toktype2str(interp->cur.type));
      return raise_error(interp);
   }

   if (_has_dualequ(interp)) {
      error("[empinterp] ERROR: the dualequ and %s keyword are mutually "
            "exclusive\n", toktype2str(interp->cur.type));
      return raise_error(interp);
   }

   return OK;
}

/**
 * @brief Check that the file processed so far is of the OLD JAMS flavor
 *
 * @param interp the EMP interpreter
 *
 * @return       the error code
 */
int old_style_check(Interpreter *interp)
{
   if (_has_dag(interp)) {
      error("[empinterp] ERROR line %u: old style empinfo and EMPDAG are mutually "
            "exclusive", interp->linenr);
      return Error_EMPIncorrectSyntax;
   }

   return OK;
}
