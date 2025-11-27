
#ifdef SWIGPYTHON

%inline %{
#define SWIG_Object PyObject *
%}

#endif



%wrapper %{

void set_option(const char *name, SWIG_Object o)
{
   unsigned typ;
   RHP_FAIL(rhp_opt_gettype(name, &typ), "Could not query the option type");

   switch (typ) {
      case RhpOptBoolean: {
         bool b;
         int ecode = SWIG_AsVal_bool(o, &b);
         if (!SWIG_IsOK(ecode)) {
            SWIG_exception_fail(SWIG_ArgError(ecode), "in method '" "set_option" "', argument " "2"" of type '" "bool""'");
         } 

         RHP_FAIL(rhp_opt_setb(name, b), "Call to set option failed");
         break; 
      }
      case RhpOptChoice: {
         char *c;
         int alloc = 0;
         int res = SWIG_AsCharPtrAndSize(o, &c, NULL, &alloc);
         if (!SWIG_IsOK(res)) {
            SWIG_exception_fail(SWIG_ArgError(res), "in method '" "set_option" "', argument " "2"" of type '" "char const *""'");
         }

         RHP_FAIL(rhp_opt_setc(name, c), "Call to set option failed");

         if (alloc == SWIG_NEWOBJ) free(c);

         break;
      }
      case RhpOptDouble: {
         double d;
         int ecode = SWIG_AsVal_double(o, &d);
         if (!SWIG_IsOK(ecode)) {
            SWIG_exception_fail(SWIG_ArgError(ecode), "in method '" "set_option" "', argument " "2"" of type '" "double""'");
         } 

         RHP_FAIL(rhp_opt_setd(name, d), "Call to set option failed");
         break;
      }
      case RhpOptInteger: {
         int i;
         int ecode = SWIG_AsVal_int(o, &i);
         if (!SWIG_IsOK(ecode)) {
            SWIG_exception_fail(SWIG_ArgError(ecode), "in method '" "set_option" "', argument " "2"" of type '" "int""'");
         } 

         RHP_FAIL(rhp_opt_seti(name, i), "Call to set option failed");
         break;
      }
      case RhpOptString: {
         char *c;
         int alloc = 0;
         int res = SWIG_AsCharPtrAndSize(o, &c, NULL, &alloc);
         if (!SWIG_IsOK(res)) {
            SWIG_exception_fail(SWIG_ArgError(res), "in method '" "set_option" "', argument " "2"" of type '" "char const *""'");
         }

         RHP_FAIL(rhp_opt_sets(name, c), "Call to set option failed");

         if (alloc == SWIG_NEWOBJ) free(c);

         break;
      }
      default:
         SWIG_exception_fail(SWIG_RuntimeError, "Unhandled option type");
   }


fail: rhp_exception_code = -1;
}

void mdl_set_option(struct rhp_mdl *mdl, const char *name, SWIG_Object o)
{
   unsigned typ;
   RHP_FAIL(rhp_mdl_getopttype(mdl, name, &typ), "Could not query the option type");

   switch (typ) {
      case RhpOptBoolean: {
         bool b;
         int ecode = SWIG_AsVal_bool(o, &b);
         if (!SWIG_IsOK(ecode)) {
            SWIG_exception_fail(SWIG_ArgError(ecode), "in method '" "set_option" "', argument " "2"" of type '" "bool""'");
         } 

         RHP_FAIL(rhp_mdl_setopt_b(mdl, name, b), "Call to set option failed");
         break; 
      }
      case RhpOptChoice: {
         char *c;
         int alloc = 0;
         int res = SWIG_AsCharPtrAndSize(o, &c, NULL, &alloc);
         if (!SWIG_IsOK(res)) {
            SWIG_exception_fail(SWIG_ArgError(res), "in method '" "set_option" "', argument " "2"" of type '" "char const *""'");
         }

         RHP_FAIL(rhp_mdl_setopt_c(mdl, name, c), "Call to set option failed");

         if (alloc == SWIG_NEWOBJ) free(c);

         break;
      }
      case RhpOptDouble: {
         double d;
         int ecode = SWIG_AsVal_double(o, &d);
         if (!SWIG_IsOK(ecode)) {
            SWIG_exception_fail(SWIG_ArgError(ecode), "in method '" "set_option" "', argument " "2"" of type '" "double""'");
         } 

         RHP_FAIL(rhp_mdl_setopt_d(mdl, name, d), "Call to set option failed");
         break;
      }
      case RhpOptInteger: {
         int i;
         int ecode = SWIG_AsVal_int(o, &i);
         if (!SWIG_IsOK(ecode)) {
            SWIG_exception_fail(SWIG_ArgError(ecode), "in method '" "set_option" "', argument " "2"" of type '" "int""'");
         } 

         RHP_FAIL(rhp_mdl_setopt_i(mdl, name, i), "Call to set option failed");
         break;
      }
      case RhpOptString: {
         char *c;
         int alloc = 0;
         int res = SWIG_AsCharPtrAndSize(o, &c, NULL, &alloc);
         if (!SWIG_IsOK(res)) {
            SWIG_exception_fail(SWIG_ArgError(res), "in method '" "set_option" "', argument " "2"" of type '" "char const *""'");
         }

         RHP_FAIL(rhp_mdl_setopt_s(mdl, name, c), "Call to set option failed");

         if (alloc == SWIG_NEWOBJ) free(c);

         break;
      }
      default:
         SWIG_exception_fail(SWIG_RuntimeError, "Unhandled option type");
   }

   return;

   fail: rhp_exception_code = -1;
}

%}
