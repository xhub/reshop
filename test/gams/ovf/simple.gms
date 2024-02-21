$ontext

Simple example of QS functions for machine learning

$offtext

option emp = reshop;
option decimals = 8;

Set i, j;

Variables c1(j), d1;

Parameters y(i)
           x(i, j)
           sigma_y(i);

$GDXIN './hogg2010test.gdx'

$LOADIDX y x sigma_y

$GDXIN

Variables w1, rho1, fit1(i);

equations fit1_eqn(i);

SCALAR kappa;
kappa = 1;
SCALAR epsilon;
epsilon = 1;
SCALAR lambda;
lambda = 1;

$if not set qs_function $setglobal qs_function "huber"

$IFTHEN %qs_function%=="huber"
$setglobal lambda_str ""
$setglobal epsilon_str ""
$setglobal kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="huber_scaled"
$setglobal lambda_str ""
$setglobal epsilon_str ""
$setglobal kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="l1"
$setglobal lambda_str ""
$setglobal epsilon_str ""
$setglobal kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="l2"
$setglobal lambda_str ""
$setglobal epsilon_str ""
$setglobal kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="elastic_net"
$setglobal lambda_str "lambda"
$setglobal epsilon_str ""
$setglobal kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="hinge"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="vapnik"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str ""
$ENDIF

$IFTHEN %qs_function%=="soft_hinge"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="soft_hinge_scaled"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="hubnik"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str "kappa"
$ENDIF

$IFTHEN %qs_function%=="hubnik_scaled"
$setglobal lambda_str ""
$setglobal epsilon_str "epsilon"
$setglobal kappa_str "kappa"
$ENDIF

fit1_eqn(i)..
  (y(i) - sum(j, c1(j)*x(i, j)) - d1) / sigma_y(i) =E= fit1(i);

file empfile /'%emp.info%'/;
empfile.nr = 2
put empfile /'equilibrium'/;
put empfile /'min w1 rho1 c1 d1 fit1 obj1 fit1_eqn'/
put empfile /'OVF' '%qs_function%' 'rho1 fit1' %lambda_str% %epsilon_str% %kappa_str%
putclose empfile

equations obj1;

obj1.. w1 =E= rho1;

model huber_fit / all /;


$onecho > reshop.opt
ovf_reformulation=equilibrium
convergence_tolerance=1e-11
subsolveropt=1
output_subsolver_log=1
export_models=1
$offecho

$onecho > jams.opt
subsolveropt=1
$offecho

$onecho > path.opt
convergence_tolerance=1e-6
$offecho

solve huber_fit using emp;

