$if not set OVF_METHOD $set OVF_METHOD "equilibrium"


Scalar tol;
tol = 1e-4;

Set s / s1, s2 /;
Set r / r1, r2 /;
Set v / v1, v2 /;

alias(r,parent);

Scalar tail / 0.2 /;

Scalar hinit;
hinit = 100;

Parameter rain(s,r)  / s1.r1 20, s1.r2 40
                      s2.r1 30, s2.r2 50 /;
Parameter prob(s,r) / s1.r1 .7, s1.r2 .3
                      s2.r1 .4, s2.r2 .6 /;
Parameter vertex1(v,r) / v1.r1 1.0 , v1.r2 0.
                         v2.r1 0. , v2.r2 1. /;
Parameter vertex2(v,r) / v1.r1 1.0 , v1.r2 0.
                         v2.r1 0. , v2.r2 1. /;

SCALAR price0;
price0 = 8;
PARAMETER prices(s) / s1 9, s2 10 /;

EQUATIONS obj
          defStateCons0
          defStateCons1(r)
          defStateCons2(r,parent)
          defArgTheta1(r)
          defArgTheta2(r,parent)
          defConj1(v)
          defConj2(v,parent);


VARIABLES w
          theta1
          theta2(parent)
          argTheta1(r)
          argTheta2(r,parent);

POSITIVE VARIABLE
          x0
          x1(r)
          x2(r,parent)
          h0
          h1(r)
          h2(r,parent);

defStateCons0..                   h0    =E=  hinit - x0;
defStateCons1(r)..               h1(r)  =E=  h0 - x1(r) + rain('s1', r);
defStateCons2(r,parent).. h2(r,parent)  =E=  h1(parent) - x2(r,parent) + rain('s2', r);

obj.. w =E= -price0 * x0 + theta1;

defArgTheta1(r).. argTheta1(r) =E= - prices('s1') * x1(r) + theta2(r);

defArgTheta2(r,parent).. argTheta2(r,parent) =E= - prices('s2') * x2(r,parent);

defConj1(v).. theta1 =G= sum(r, vertex1(v,r) * ( - prices('s1') * x1(r) + theta2(r) ) ) ;

defConj2(v,parent).. theta2(parent) =G= prices('s2') * (sum(r, vertex2(v,r) * (-x2(r,parent)) )) ;



MODEL hydro_emp /obj, defStateCons0, defStateCons1, defStateCons2, defArgTheta1, defArgTheta2 /;

MODEL hydro_conj /obj, defStateCons0, defStateCons1, defStateCons2, defConj1, defConj2 /;

solve hydro_conj min w using lp;

abort$[hydro_conj.modelStat <> %MODELSTAT.OPTIMAL%]   'solve failed', hydro_conj.modelStat;
abort$[hydro_conj.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', hydro_conj.solveStat;



Scalar wRef;
wRef = w.l;

execute_unload 'empinfo.gdx' parent;

$onecho > path.opt
convergence_tolerance=1e-10
$offecho

$onecho > %gams.emp%.opt
ovf_reformulation=%OVF_METHOD%
subsolveropt 1
$offecho

$onecho > %gams.emp%.op2
ovf_reformulation=%OVF_METHOD%
EMPInfoFile=empinfo2.dat
subsolveropt 1
$offecho

* This will be to test the new empinfo
$onecho > %gams.scrdir%empinfo2.dat
gdxin empinfo.gdx
load parent

*loop(parent$(sameas('r2', parent)),
loop(parent,
  OVF cvarup theta2(parent) argTheta2(:,parent) 0.2
)
OVF cvarup theta1 argTheta1 0.2
$offecho

hydro_emp.optfile = 2
solve hydro_emp min w using emp;

abort$[hydro_emp.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', hydro_emp.modelStat;
abort$[hydro_emp.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', hydro_emp.solveStat;

abort$[ abs(w.l - wRef) > tol ] 'objective function value differ for empinfo2', w.l;



file empinfo /'%emp.info%'/
loop(parent,
  put empinfo 'OVF cvarup ' theta2(parent);
  loop(r,
    put empinfo argTheta2(r,parent);
  )
  put tail /;
  )
put empinfo 'OVF cvarup theta1 argTheta1' tail /;
putclose empinfo;


hydro_emp.optfile = 1
solve hydro_emp min w using emp;

abort$[hydro_emp.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', hydro_emp.modelStat;
abort$[hydro_emp.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', hydro_emp.solveStat;

abort$[ abs(w.l - wRef) > tol ] 'objective function value differ', w.l;

* test an empty EMPDAG. This should FAIL
putclose empinfo /' '/;
hydro_emp.optfile = 0;
solve hydro_emp min w using emp;

abort$[hydro_emp.modelStat <= %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', hydro_emp.modelStat;
abort$[hydro_emp.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', hydro_emp.solveStat;


