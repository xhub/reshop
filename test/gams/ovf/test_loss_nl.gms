$ontext

Data is from  ``Data analysis recipes: Fitting a model to data'' by Hogg, Bovy & Lang
available on https://arxiv.org/abs/1008.4686.
$offtext

$if not set TESTTOL $set TESTTOL 1e-10
scalar tol / %TESTTOL% /;

$if not set version $set version 1
$if not dexist test_res $call mkdir test_res



$macro check_status(m) \
abort$[not((m.solvestat = %solvestat.NormalCompletion%) and ((m.modelstat = %modelstat.Optimal%) or m.modelstat = %modelstat.locallyOptimal%))] 'Wrong results', m.solvestat, m.modelstat;\


$macro check(m) \
check_status(m);\
display c1.l, d1.l, c2.l, d2.l, c3.l, d3.l, c4.l, d4.l, c5.l, d5.l;\
abort$[ smax{j, abs(c1.l(j) - c2.l(j))} > tol ] 'bad c1/c2', c1.l, c2.l;\
abort$[ smax{j, abs(c1.l(j) - c3.l(j))} > tol ] 'bad c1/c3', c1.l, c3.l;\
abort$[ smax{j, abs(c1.l(j) - c4.l(j))} > tol ] 'bad c1/c4', c1.l, c4.l;\
abort$[ smax{j, abs(c1.l(j) - c5.l(j))} > tol ] 'bad c1/c5', c1.l, c5.l;\
abort$[ abs(d1.l - d2.l) > tol ] 'bad d1/d2', d1.l, d2.l;\
abort$[ abs(d1.l - d3.l) > tol ] 'bad d1/d3', d1.l, d3.l;\
abort$[ abs(d1.l - d4.l) > tol ] 'bad d1/d4', d1.l, d4.l;\
abort$[ abs(d1.l - d5.l) > tol ] 'bad d1/d5', d1.l, d5.l;\
abort$[ smax{i, abs(fit1.l(i) - fit2.l(i))} > tol ] 'bad fit1/fit3', fit1.l, fit2.l;\
abort$[ smax{i, abs(fit1.l(i) - fit3.l(i))} > tol ] 'bad fit1/fit3', fit1.l, fit3.l;\
abort$[ smax{i, abs(fit1.l(i) - fit4.l(i))} > tol ] 'bad fit1/fit3', fit1.l, fit4.l;\
abort$[ smax{i, abs(fit1.l(i) - fit5.l(i))} > tol ] 'bad fit1/fit5', fit1.l, fit5.l;\


Set i, j;

Variables c1(j), d1;
Variables c2(j), d2;
Variables c3(j), d3;
Variables c4(j), d4;
Variables c5(j), d5;

Parameters y(i)
           x(i, j)
           sigma_y(i);

$GDXIN './hogg2010test.gdx'

$LOADIDX y x sigma_y

$GDXIN

Variables w1, rho1, fit1(i);
Variables w2, rho2, fit2(i);
Variables w3, rho3, fit3(i);
Variables w4, rho4, fit4(i);
Variables w5, rho5, fit5(i);

SCALAR kappa;
kappa = 1;
SCALAR epsilon;
epsilon = 1;
SCALAR lambda;
lambda = 1;

$if not set qs_function $set qs_function "huber"
$if not set kappa_val $set kappa_val 1
$if not set lambda_val $set lambda_val 1
$if not set epsilon_val $set epsilon_val 1

$IFTHEN %qs_function%=="huber"
$set lambda_str ""
$set epsilon_str ""
$set kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="huber_scaled"
$set lambda_str ""
$set epsilon_str ""
$set kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="l1"
$set lambda_str ""
$set epsilon_str ""
$set kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="l2"
$set lambda_str ""
$set epsilon_str ""
$set kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="elastic_net"
$set lambda_str "lambda"
$set epsilon_str ""
$set kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="hinge"
$set lambda_str ""
$set epsilon_str "epsilon"
$set kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="vapnik"
$set lambda_str ""
$set epsilon_str "epsilon"
$set kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="soft_hinge"
$set lambda_str ""
$set epsilon_str "epsilon"
$set kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="hubnik"
$set lambda_str ""
$set epsilon_str "epsilon"
$set kappa_str "kappa"
$ENDIF


$IFTHEN %qs_function%=="soft_hinge_scaled"
$set lambda_str ""
$set epsilon_str "epsilon"
$set kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="hubnik_scaled"
$set lambda_str ""
$set epsilon_str "epsilon"
$set kappa_str "kappa"
$ENDIF

EQUATIONS fit1_eqn(i);

fit1_eqn(i)..
  (y(i) - sum(j, c1(j)*x(i, j)) - d1) / sigma_y(i) =E= fit1(i);

EQUATIONS fit2_eqn(i);

fit2_eqn(i)..
  log(exp((y(i) - sum(j, c2(j)*x(i, j)) - d2) / sigma_y(i))) =E= fit2(i);

EQUATIONS fit3_eqn(i);

fit3_eqn(i)..
 fit3(i) =E= (y(i) - sum(j, c3(j)*x(i, j)) - d3) / sigma_y(i);

EQUATIONS fit4_eqn(i);

fit4_eqn(i)..
  fit4(i) =E= log(exp((y(i) - sum(j, c4(j)*x(i, j)) - d4) / sigma_y(i)));

EQUATIONS fit5_eqn(i);

fit5_eqn(i)..
  pi*(y(i) - sum(j, c5(j)*x(i, j)) - d5) / sigma_y(i) =E= pi*fit5(i);

FILE empfile /'%emp.info%'/;
$ifthen %version% == "1"
PUT empfile /'OVF' '%qs_function%' 'rho1 fit1' %lambda_str% %epsilon_str% %kappa_str%
$elseif %version% == "2"
put empfile /'OVF' '%qs_function%' 'rho2 fit2' %lambda_str% %epsilon_str% %kappa_str%
$elseif %version% == "3"
put empfile /'OVF' '%qs_function%' 'rho3 fit3' %lambda_str% %epsilon_str% %kappa_str%
$elseif %version% == "4"
put empfile /'OVF' '%qs_function%' 'rho4 fit4' %lambda_str% %epsilon_str% %kappa_str%
$elseif %version% == "5"
put empfile /'OVF' '%qs_function%' 'rho5 fit5' %lambda_str% %epsilon_str% %kappa_str%
$else
$error "version %version% not supported"
$endif
putclose empfile

EQUATIONS obj1, obj2, obj3, obj4, obj5;

obj1.. w1 =E= rho1;
obj2.. w2 =E= rho2;
obj3.. w3 =E= rho3;
obj4.. w4 =E= rho4;
obj5.. w5 =E= rho5;

MODEL mfit / obj%version% fit%version%_eqn /;

$onecho > jams.opt
SubSolverOpt=1
$offecho

$onecho > path.opt
convergence_tolerance=1e-13
$offecho

$onecho > CONOPT.opt
RTREDG=1e-12
$offecho

$onecho > cplex.opt
epagap=1e-12
$offecho

$onecho > %gams.emp%.opt
ovf_reformulation=equilibrium
subsolveropt=1
output_subsolver_log=1
$offecho

mfit.optfile = 1

solve mfit using emp min w%version%;

check_status(mfit)

file res / "test_res/mcp_%qs_function%_v%version%.out" /;
put res;
res.nd=15;
res.nw=15;
res.ap=0;
loop(j, put c%version%.l(j) ) ;
put /d%version%.l;
put /w%version%.l;
putclose /rho%version%.l;

file fit_res / "test_res/mcp_%qs_function%_v%version%_fit.out" /;
put fit_res;
fit_res.nd=15;
fit_res.nw=15;
fit_res.ap=0;
loop(i, put fit%version%.l(i)/ ) ;
putclose /;

*check(fit1)

d1.l = 0.;
d2.l = 0.;
d3.l = 0.;
d4.l = 0.;
d5.l = 0.;

c1.l(j) = 0.;
c2.l(j) = 0.;
c3.l(j) = 0.;
c4.l(j) = 0.;
c5.l(j) = 0.;

rho1.l = 0.;
rho2.l = 0.;
rho3.l = 0.;
rho4.l = 0.;
rho5.l = 0.;

$onecho > %gams.emp%.op2
ovf_reformulation=fenchel
subsolveropt=1
output_subsolver_log=1
$offecho

$onecho > conopt.opt
RTREDG=1e-12
$offecho

mfit.optfile = 2

solve mfit using emp min w%version%;

check_status(mfit);

file res2 / "test_res/fenchel_%qs_function%_v%version%.out" /;
put res2;
res2.nd=15;
res2.nw=15;
res2.ap=0;
loop(j, put c%version%.l(j) ) ;
put /d%version%.l;
put /w%version%.l;
putclose /rho%version%.l;

file fit_res2 / "test_res/fenchel_%qs_function%_v%version%_fit.out" /;
put fit_res2;
fit_res2.nd=15;
fit_res2.nw=15;
fit_res2.ap=0;
loop(i, put fit%version%.l(i)/ ) ;
putclose /;

*check(fit1)
$IFTHEN %qs_function%=="huber"
$exit
$ENDIF

$IFTHEN %qs_function%=="huber_scaled"
$exit
$ENDIF

$IFTHEN %qs_function%=="l2"
$exit
$ENDIF

$IFTHEN %qs_function%=="elastic_net"
$exit
$ENDIF

$IFTHEN %qs_function%=="hubnik"
$exit
$ENDIF

$IFTHEN %qs_function%=="hubnik_scaled"
$exit
$ENDIF

$exit

d1.l = 0.;
d2.l = 0.;
d3.l = 0.;
d4.l = 0.;
d5.l = 0.;

c1.l(j) = 0.;
c2.l(j) = 0.;
c3.l(j) = 0.;
c4.l(j) = 0.;
c5.l(j) = 0.;

rho1.l = 0.;
rho2.l = 0.;
rho3.l = 0.;
rho4.l = 0.;
rho5.l = 0.;

$onecho > %gams.emp%.op3
ovf_reformulation=gauge
subsolveropt=1
output_subsolver_log=1
export_models=1
$offecho

$onecho > path.opt
convergence_tolerance=1e-13
$offecho

mfit.optfile = 3

solve mfit using emp min w%version% ;

check_status(mfit);

file res3 / "test_res/gauge_%qs_function%_v%version%.out" /;
put res3;
res3.nd=15;
res3.nw=15;
res3.ap=0;
loop(j, put c%version%.l(j) ) ;
put /d%version%.l;
put /w%version%.l;
putclose /rho%version%.l;

file fit_res3 / "test_res/gauge_%qs_function%_v%version%_fit.out" /;
put fit_res3;
fit_res3.nd=15;
fit_res3.nw=15;
fit_res3.ap=0;
loop(i, put fit%version%.l(i)/ ) ;
putclose /;

rho1.l = 0.;
$ontext
rho2.l = 0.;
rho3.l = 0.;
rho4.l = 0.;
rho5.l = 0.;
$offtext


$ontext
$onecho > %gams.emp%.opt
ovf_reformulation=conjugate
write_agent_model=no
$offecho

solve fit1 using emp;

*check(fit1)
$offtext
