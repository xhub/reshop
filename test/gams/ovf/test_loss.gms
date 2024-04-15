$ontext



$offtext

$if not set TESTTOL $set TESTTOL 1e-10
scalar tol / %TESTTOL% /;
scalar told / 1e-8 /;



option decimals = 8;


$macro check(m) \
abort$[not((m.solvestat = %solvestat.NormalCompletion%) and ((m.modelstat = %modelstat.Optimal%) or (m.modelstat = %modelstat.locallyOptimal%)))] 'Wrong results', m.solvestat, m.modelstat;\
display c1.l, d1.l, c2.l, d2.l, c3.l, d3.l, c4.l, d4.l, c5.l, d5.l, rho1.l, rho2.l, rho3.l, rho4.l, rho5.l;\
abort$[ smax{j, abs(c1.l(j) - c2.l(j))} > tol ] 'bad c1/c2', c1.l, c2.l;\
abort$[ smax{j, abs(c1.l(j) - c3.l(j))} > tol ] 'bad c1/c3', c1.l, c3.l;\
abort$[ smax{j, abs(c1.l(j) - c4.l(j))} > tol ] 'bad c1/c4', c1.l, c4.l;\
abort$[ smax{j, abs(c1.l(j) - c5.l(j))} > tol ] 'bad c1/c5', c1.l, c5.l;\
abort$[ abs(d1.l - d2.l) > told ] 'bad d1/d2', d1.l, d2.l;\
abort$[ abs(d1.l - d3.l) > told ] 'bad d1/d3', d1.l, d3.l;\
abort$[ abs(d1.l - d4.l) > told ] 'bad d1/d4', d1.l, d4.l;\
abort$[ abs(d1.l - d5.l) > told ] 'bad d1/d5', d1.l, d5.l;\
abort$[ smax{i, abs(fit1.l(i) - fit2.l(i))} > tol ] 'bad fit1/fit3', fit1.l, fit2.l;\
abort$[ smax{i, abs(fit1.l(i) - fit3.l(i))} > tol ] 'bad fit1/fit3', fit1.l, fit3.l;\
abort$[ smax{i, abs(fit1.l(i) - fit4.l(i))} > tol ] 'bad fit1/fit3', fit1.l, fit4.l;\
abort$[ smax{i, abs(fit1.l(i) - fit5.l(i))} > tol ] 'bad fit1/fit5', fit1.l, fit5.l;\
abort$[ abs(rho1.l - rho2.l) > tol ] 'bad rho1/rho2', rho1.l, rho2.l;\
abort$[ abs(rho1.l - rho3.l) > tol ] 'bad rho1/rho3', rho1.l, rho3.l;\
abort$[ abs(rho1.l - rho4.l) > tol ] 'bad rho1/rho4', rho1.l, rho4.l;\
abort$[ abs(rho1.l - rho5.l) > tol ] 'bad rho1/rho5', rho1.l, rho5.l;\


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

equations fit1_eqn(i);

$BATINCLUDE default_loss.inc

fit1_eqn(i)..
  (y(i) - sum(j, c1(j)*x(i, j)) - d1) / sigma_y(i) =E= fit1(i);

equations fit2_eqn(i);

fit2_eqn(i)..
  log(exp((y(i) - sum(j, c2(j)*x(i, j)) - d2) / sigma_y(i))) =E= fit2(i);

equations fit3_eqn(i);

fit3_eqn(i)..
 fit3(i) =E= (y(i) - sum(j, c3(j)*x(i, j)) - d3) / sigma_y(i);

equations fit4_eqn(i);

fit4_eqn(i)..
  fit4(i) =E= log(exp((y(i) - sum(j, c4(j)*x(i, j)) - d4) / sigma_y(i)));

equations fit5_eqn(i);

fit5_eqn(i)..
  pi*(y(i) - sum(j, c5(j)*x(i, j)) - d5) / sigma_y(i) =E= pi*fit5(i);

file empfile /'%emp.info%'/;
put empfile /'equilibrium'/;
put empfile /'min w1 rho1 c1 d1 obj1'/
put empfile /'min w2 rho2 c2 d2 obj2'/
put empfile /'min w3 rho3 c3 d3 obj3'/
put empfile /'min w4 rho4 c4 d4 obj4'/
put empfile /'min w5 rho5 c5 d5 obj5'/
put empfile /'OVF' '%qs_function%' 'rho1 fit1' %lambda_str% %epsilon_str% %kappa_str%
put empfile /'OVF' '%qs_function%' 'rho2 fit2' %lambda_str% %epsilon_str% %kappa_str%
put empfile /'OVF' '%qs_function%' 'rho3 fit3' %lambda_str% %epsilon_str% %kappa_str%
put empfile /'OVF' '%qs_function%' 'rho4 fit4' %lambda_str% %epsilon_str% %kappa_str%
put empfile /'OVF' '%qs_function%' 'rho5 fit5' %lambda_str% %epsilon_str% %kappa_str%
putclose empfile

equations obj1, obj2, obj3, obj4, obj5;

obj1.. w1 =E= rho1;
obj2.. w2 =E= rho2;
obj3.. w3 =E= rho3;
obj4.. w4 =E= rho4;
obj5.. w5 =E= rho5;

model huber_fit / all /;


$onecho > %gams.emp%.opt
ovf_reformulation=equilibrium
subsolveropt=1
output_subsolver_log=1
$offecho

$onecho > jams.opt
subsolveropt=1
$offecho

$onecho > path.opt
convergence_tolerance=1e-11
$offecho

solve huber_fit using emp;

check(huber_fit)

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

huber_fit.optfile=2

solve huber_fit using emp;

check(huber_fit)

$exit

rho1.l = 0.;
rho2.l = 0.;
rho3.l = 0.;
rho4.l = 0.;
rho5.l = 0.;

$onecho > %gams.emp%.op3
ovf_reformulation=gauge
subsolveropt=1
output_subsolver_log=1
$offecho

$onecho > jams.opt
subsolveropt=1
$offecho

$onecho > path.opt
convergence_tolerance=1e-11
$offecho

solve huber_fit using emp;

check(huber_fit)

$ontext

$onecho > %gams.emp%.op3
ovf_reformulation=conjugate
$offecho

huber_fit.optfile=3

solve huber_fit using emp;

check(huber_fit)

$offtext

file res / "%qs_function%.out" /;
put res;
res.nd=15;
res.nw=15;
res.ap=0;
loop(j, put c1.l(j) ) ;
putclose /d1.l;


file fit_res / "%qs_function%_fit.out" /;
put fit_res;
fit_res.nd=15;
fit_res.nw=15;
fit_res.ap=0;
loop(i, put fit1.l(i)/ ) ;
putclose /;
