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

Scalar price0;
price0 = 10;
Parameter prices(s) / s1 9, s2 8 /;

Equations obj
          defStateCons0
          defStateCons1(r)
          defStateCons2(r,parent)
          defArgTheta1(r)
          defArgTheta2(r,parent)
          defConj1(v)
          defConj2(v,parent);


Variables w
          theta1
          theta2(parent)
          argTheta1(r)
          argTheta2(r,parent);

Positive variable
          x0
          x1(r)
          x2(r,parent)
          h0
          h1(r)
          h2(r,parent);

defStateCons0..           h0           =E=  hinit - x0;
defStateCons1(r)..        h1(r)        =E=  h0 - x1(r) + rain('s1', r);
defStateCons2(r,parent).. h2(r,parent) =E=  h1(parent) - x2(r,parent) + rain('s2', r);


obj.. w =E= price0 * x0 + theta1;


defArgTheta1(r)..        argTheta1(r)        =E= prices('s1') * x1(r) + theta2(r);

defArgTheta2(r,parent).. argTheta2(r,parent) =E= prices('s2') * x2(r,parent);


defConj1(v)..        theta1         =L=  sum(r, vertex1(v,r) * ( prices('s1') * x1(r) + theta2(r) ) ) ;

defConj2(v,parent).. theta2(parent) =L=  prices('s2') * (sum(r, vertex2(v,r) * (x2(r,parent)) )) ;


model hydro_emp  /obj, defStateCons0, defStateCons1, defStateCons2, defArgTheta1, defArgTheta2 /;

model hydro_conj /obj, defStateCons0, defStateCons1, defStateCons2, defConj1, defConj2 /;


file empinfo /'%emp.info%'/
put empinfo 'OVF cvarlo theta1 argTheta1' tail /;
loop(parent,
  put empinfo 'OVF cvarlo ' theta2(parent);
  loop(r,
    put empinfo argTheta2(r,parent);
  )
  put tail /;
  )
putclose empinfo;


$onecho > %gams.emp%.opt
*ovf_reformulation=equilibrium
*ovf_reformulation=fenchel
*ovf_reformulation=conjugate
ovf_reformulation=%OVF_METHOD%
$offecho

solve hydro_emp max w using emp;

abort$[hydro_emp.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', hydro_emp.modelStat;
abort$[hydro_emp.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', hydro_emp.solveStat;

Scalar wL;
wL = w.l;

solve hydro_conj max w using lp;

abort$[ abs(w.l - wL) > tol ] 'objective funcation value differ', w.l;

abort$[hydro_conj.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', hydro_conj.modelStat;
abort$[hydro_conj.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', hydro_conj.solveStat;


