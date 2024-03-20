$ontext

Simple example to illustrate how to model the CVaR as a support function

author: Olivier Huber <oli.huber@gmail.com>

$offtext

$if not set ovf_method $set ovf_method equilibrium

* START PROBLEM DATA DEFINITION

Set i /1*4/;

* Events
Parameter d(i);

d(i) = i.val;

Variable z(i);

* Probabilities
Parameter p(i) /1 .125, 2 .25, 3 .5, 4 .125/;

SCALAR theta;
theta = 1-.15;

* END PROBLEM DATA DEFINITION

* START OPTIMIZATION PROBLEM DEFINITION

Variables w, rho;

Equation obj;
obj..
  w =E= rho;

EQUATION equ_z(i);
equ_z(i).. z(i) =E= d(i);

* END OPTIMIZATION PROBLEM DEFINITION

model cvar_simple /all/;

file empinfo /'%emp.info%'/;
empinfo.nd = 10;
put empinfo /'OVF cvarup rho z ' theta; loop(i, put p(i));
putclose /;

option emp = reshop

$onecho > reshop.opt
ovf_reformulation=%ovf_method%
subsolveropt=1
output_subsolver_log=1
$offecho

$onecho > jams.opt
subsolveropt=1
$offecho

$onecho > path.opt
convergence_tolerance=1e-11
$offecho

$onecho > conopt.opt
RTREDG=1e-12
$offecho

solve cvar_simple min w using emp;

$if not dexist test_res $call mkdir test_res
file res / "test_res/simple_OVF.out" /;
put res;
res.nd=4;
res.nw=10;
res.ap=0;
putclose w.l;

$exit

* TODO cleanup the rest

w.l = 0.;

$onecho > reshop.op2
ovf_reformulation=fenchel
subsolveropt=1
output_subsolver_log=1
$offecho

cvar_simple.optfile=2

solve cvar_simple min w using emp;

w.l = 0.;

$onecho > jams.opt
subsolveropt=1
$offecho

$onecho > path.opt
convergence_tolerance=1e-11
$offecho

w.l = 0.;

$onecho > reshop.op4
ovf_reformulation=conjugate
convergence_tolerance=1e-11
subsolveropt=1
output_subsolver_log=1
$offecho

$onecho > jams.opt
subsolveropt=1
$offecho

cvar_simple.optfile=4

solve cvar_simple min w using emp;

w.l = 0.;


