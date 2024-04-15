*Title Stochastic portfolio model (PORTFOLIO,SEQ=80)

$ontext

Portfolio composition
3 assets, 12 scenarios
max expected return using emp and keyword ExpectedValue

$offtext

Set j assets    / ATT, GMC, USX /
    s scenarios / s1*s12 /

Table vs(s,j) scenario returns from assets
     att     gmc     usx
 s1  1.300   1.225   1.149
 s2  1.103   1.290   1.260
 s3  1.216   1.216   1.419
 s4  0.954   0.728   0.922
 s5  0.929   1.144   1.169
 s6  1.056   1.107   0.965
 s7  1.038   1.321   1.133
 s8  1.089   1.305   1.732
 s9  1.090   1.195   1.021
s10  1.083   1.390   1.131
s11  1.035   0.928   1.006
s12  1.176   1.715   1.908;

Alias (j,jj);
Parameter
    mean(j)      mean return
    dev(s,j)     deviations
    covar(j,jj)  covariance matrix of returns
    totmean      total mean return;

mean(j)     = sum(s, vs(s,j))/card(s);
dev(s,j)    = vs(s,j) - mean(j);
covar(j,jj) = sum(s, dev(s,j)*dev(s,jj))/(card(s)-1);
totmean     = sum(j, mean(j))/card(j);

display mean, dev, covar, totmean;

Parameter
    p(s)   probability / #s [1/card(s)] /
    v(j)   return from assets; v(j) = mean(j);
Scalar
    theta  relative volume / [1-0.9] /
    lambda weight EV versus CVaR / 0.2 /;
    
display p, theta;

Variables
    r      value of portfolio under each scenario
    w(j)   portfolio selection
    EV_r	expected value of r
    objective	objective variable;

Positive variables w;

Equations
    defr     return of portfolio
    budget   budget constraint
    obj_eq	 objective eqn;

defr..     r =e= sum(j, v(j)*w(j));

budget..   sum(j, w(j)) =e= 1;

obj_eq..	objective =e= EV_r;

model portfolio / all /;

file emp / '%emp.info%' /; 
emp.nd=4;
put emp '* problem %gams.i%'
      / 'ExpectedValue r EV_r'
      / '* cvarlo r EV_r .2'
      / 'stage 2 v defr r'
      / 'stage 1 objective obj_eq EV_r'
      / "jrandvar v('att') v('gmc') v('usx')"
loop(s,
  put / p(s) vs(s,"att") vs(s,"gmc") vs(s,"usx"));
putclose emp;

Parameter
    s_v(s,j)     return from assets by scenario /s1.att 1/
    s_r(s)       return from portfolio by scenario 
    data(s,*)   scenario probability        / #s.prob 0/;

Set dict / s     .scenario.''
           v     .randvar. s_v
           r     .level.   s_r 
           ''    .opt     .data
           /;

option emp=de;
* portfolio.iterlim = 0;
solve portfolio using emp max objective scenario dict;

display s_r, s_v, w.l, EV_r.l, objective.l, data;

Parameters obj_l, EV_r_l;
obj_l = objective.l;
EV_r_l = EV_r.l;

variables rs(s);
equations defr_full(s), obj_full;

defr_full(s)..     rs(s) =e= sum(j, s_v(s,j)*w(j));

obj_full..	objective =e= sum(s, data(s,'prob')*s_r(s));

model portfull / defr_full, obj_full, budget /;
solve portfull using lp max objective;

display objective.l, w.l, s_r, rs.l;

variables EV_r;
equations obj_emp;

obj_emp..	objective =e= EV_r;

* lambda = 1 is cvar, lambda = 0 is ev
*put emp 'OVF ecvarlo EV_r rs 0.2 1.' /;
put emp 'OVF expectedvalue EV_r rs '/;
putclose emp;

$onecho > %gams.emp%.opt
ovf_reformulation=equilibrium
output_subsolver_log=1
subsolveropt=1
export_models=1
$offecho

model portemp / defr_full, obj_emp, budget /;
solve portemp using emp max objective;
abort$[portemp.modelStat <> %MODELSTAT.LOCALLY OPTIMAL%]    'bad model status', portemp.modelStat;
abort$[portemp.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]  'bad solve status';

$if not set tol $set tol 1e-6
scalar tol / %tol% /;

abort$[abs(objective.l - obj_L) > tol] 'wrong objective value', objective.l;
abort$[abs(EV_r.l - EV_r_L) > tol] 'wrong objective value', EV_r.l;

display objective.l, w.l;
display rs.l, EV_r.l;
