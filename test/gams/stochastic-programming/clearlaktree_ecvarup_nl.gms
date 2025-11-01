$TITLE risk-averse test with the ClearLake example

$ontext

The model is based on the ClearLake example.


Reference: Exercise, p. 44 in
Birge, R, and Louveaux, F V, Introduction to Stochastic Programming.
Springer, 1997.

Contributor: Olivier Huber <oli.huber@gmail.com>
Date:        October 2025

$offtext

$eolcom #

$if not set tol $set tol 1e-6

$onEcho > pathnlp.opt
conv_tol 1e-12
crash_method none
prox_pert 0
crash_pert no
$offEcho

$onEcho > path.opt
conv_tol 1e-12
crash_method none
prox_pert 0
crash_pert no
factorization_method conopt
$offEcho

$onEcho > %gams.emp%.opt
subsolveropt 1
ovf_reformulation=equilibrium
$offEcho

$onEcho > %gams.emp%.op2
ovf_reformulation=fenchel
$offEcho

SETS p Stochastic realizations (precipitation) /low, normal, high/,
     t Time periods                            /dec,jan,feb,mar/,
     n set of nodes,
     succ(n,n) Successor relationship;

ALIAS (n,child,parent);

*-------------------------------------------------------------------------------
*Parameters are specified

PARAMETERS floodCost  'Flooding penalty cost thousand CHF per mm'   /10/,
           lowCost    'Water importation cost thousand CHF per mm'  /5/,
           l_start    'Initial water level (mm)'                    /100/,
           l_max      'Maximum reservoir level (mm)'                /250/,
           r_max      'Maximum normal release in any period'        /200/;

*-----------------------------------------------------------------------------

PARAMETERS prob(n)    'Conditional probability',
           ndelta(n)  'Water inflow at each node';

$gdxin clearlaktree.gdx
$load n, succ
$load prob, ndelta
$gdxin

VARIABLE EC               'Total Cost';

*------------------------------------------------------------------------------
* Model logic:  only needs n, succ, prob, ndelta
*------------------------------------------------------------------------------

POSITIVE VARIABLES
		   F(n)             'Floodwater released (mm)',
         L(n)             'Reservoir water level -- EOP',
		   R(n)             'Normal release of water (mm)',
		   Z(n)             'Water imports (mm)';

R.up(n) = r_max;
L.up(n) = l_max;
L.fx(n)$(n.first) = l_start;



SCALAR max_stage / 1 /;

PARAMETER stage(n)  'stage of each node';

SET list(n);

* Compute stage of each node
list(n)$(n.first) = yes;
stage(n)$(n.first) = max_stage;

while(card(list),

   max_stage = max_stage + 1;

   list(n) = yes$(sum(parent$(list(parent) AND succ(parent,n)), yes));

   stage(n)$(list(n)) = max_stage;
);


SET test / t1*t4 /;


* CVaR parameters
PARAMETER tail    'tail of the CVaR',
          cvar_wt 'weight of the CVaR';

PARAMETER beta;

SET test1, test2;

PARAMETERS tails(test1<)    / '1'   1., '0.75' .75, '0.5' .5, '0.3' .3, '0.1' .1/;
PARAMETERS cvar_wts(test2<) / '0.9' .9, '0.75' .75, '0.5' .5, '0.3' .3, '0.1' .1/;

* Comparison
PARAMETERS L_L(n), L_M(n), ldef_M(n), F_L(n), F_M(n), R_L(n), R_M(n), Z_L(n), Z_M(n),
           L_Ld(n), F_Ld(n), R_Ld(n), Z_Ld(n), y_L(n), cost_L(n), v_L(n);

VARIABLES          v(n)    'Var value at each non-leaf node',
                   cost(n) 'total cost at each node';

POSITIVE VARIABLES y(n)    'Contribution of the cost for each node'

*-------------------------------------------------------------------------------
* Model components for computing CVaR a posteriori
*-------------------------------------------------------------------------------

VARIABLES cvarURobj;
EQUATIONS defcvarURobj, cvarURcons(n,n);

PARAMETER QL(n);

SINGLETON SET activeNode(n);

defcvarURobj..
   cvarURobj
   =E=
   sum(activeNode(n), v(n) + [1/(1-beta)] * sum(succ(n,child), prob(child)*y(child)));

cvarURcons(n,child)$(activeNode(n) AND succ(n,child))..
   y(child) =G= QL(child) - v(n);

MODEL cvarUR 'Uryasev-Rockafellar formulation for cvar' / defcvarURobj, cvarURcons /;

PARAMETER QLd(n), y_Ld(n), v_ld(n);

*-------------------------------------------------------------------------------
* Reference model
*-------------------------------------------------------------------------------

EQUATIONS ecdef, ldef; 

EQUATIONS cvar_ineq(n,n), defcost(n), totalCost;

cvar_ineq(n,child)$(succ(n,child))..
   y(child) =G= cost(child) - v(n);

defcost(n)..
   cost(n) =G= (floodcost*F(n)+lowCost*Z(n))$(NOT n.first) + 
   (
          cvar_wt * [ v(n) + [1/(1-beta)] * sum(succ(n,child), prob(child)*y(child)) ] +
      (1-cvar_wt) * [ sum(succ(n,child), prob(child)*cost(child)) ]
   )$(sum(succ(n,child), yes));

totalCost..
   EC =E= sum(n$n.first, cost(n));


ldef(n)$[NOT n.first].. L(n) =e= sum{ succ(parent,n), L(parent)} + ndelta(n)+Z(n)-R(n)-F(n);

MODEL mincost / totalCost, defcost, cvar_ineq, ldef /;

mincost.optfile=1;


*-------------------------------------------------------------------------------
* EMP model
*-------------------------------------------------------------------------------

VARIABLES  objnode(n)     'nodal (local) cost'; 

EQUATIONS  defobjnode(n)  'nodal cost definition',
           deflvl(n)      'water level';

defobjnode(n)..
      objnode(n) =E= (floodCost*log(exp(F(n))) + lowCost*Z(n))$(not n.first);

deflvl(n).. L(n) =E= sum{ succ(parent,n), L(parent) } + ndelta(n)+log(exp(Z(n))) - R(n) - F(n);

MODEL mincostEMP 'EMP stochastic model' / defobjnode, deflvl /;

SCALAR optfileVal;

*-------------------------------------------------------------------------------
* Start of tests
*-------------------------------------------------------------------------------
SET s     'reformulation scheme' / 'equilibrium', 'fenchel' /;

PARAMETER QLdiff(n)    'Q values difference';

loop((s,test1,test2),

tail    = tails(test1);
cvar_wt = cvar_wts(test2);
optfileVal = ord(s);

$label test_start

display tail, cvar_wt;

beta = 1-tail;

SOLVE mincost using LP minimizing EC;

display v.l, y.l;

* Save values
L_L(n) = L.l(n);
L_M(n) = L.m(n);
ldef_M(n)$(NOT n.first) = ldef.m(n);

F_L(n) = F.l(n);
F_M(n) = F.m(n);
R_L(n) = R.l(n);
R_M(n) = R.m(n);
Z_L(n) = Z.l(n);
Z_M(n) = Z.m(n);

y_L(n) = y.l(n);
v_L(n) = v.l(n);
cost_L(n) = cost.l(n);


* Reset
L.l(n) = 0.;
L.fx(n)$(n.first) = l_start;
R.l(n) = 0.;
F.l(n) = 0.;
Z.l(n) = 0.;
L.m(n) = 0.;
R.m(n) = 0.;
F.m(n) = 0.;
Z.m(n) = 0.;

ndelta(n)$(n.first) = L.l(n);
F.fx(n)$(n.first) = 0;
Z.fx(n)$(n.first) = 0;
R.fx(n)$(n.first) = 0;

*******************************************************************************
* 1st EMP model
*******************************************************************************

embeddedCode reshop:
nodalprob(n): min objnode(n) + CRM(n).valFn$(succ(n,child)) F(n) L(n) R(n) Z(n) defobjnode(n) deflvl(n) 
CRM(n)$(succ(n,child)): MP('ecvarup', nodalprob(child).valFn$(succ(n,child)), tail=tail, cvar_wt=cvar_wt, prob=prob(child)$(succ(n,child)))
endEmbeddedCode

mincostEMP.optfile = optfileVal;

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

* These don't seem to be unique for a given solution; Note that they
* have associated cost
* abort$[smax{n$(NOT n.first), L_Ld(n)} > %tol%]     'wrong L.l', L_Ld;
* abort$[smax{n$(NOT n.first), R_Ld(n)} > %tol%]     'wrong R.l', R_Ld;


* leaf: just initialize local costs
list(n)$(NOT sum(succ(n,child), yes)) = yes;

QL(list(n)) = objnode.l(n);

* Select leaf node parents
list(parent) = yes$(sum(n$(list(n) AND succ(parent,n)), yes));

while(card(list),

   loop(list,
      activeNode(list) = yes;

      solve cvarUR min cvarURobj using LP;

      QL(list) = objnode.l(list)
             + (1-cvar_wt) * sum(succ(list,child), prob(child)*QL(child))
             + cvar_wt * cvarURobj.l;

   );

   list(parent) = yes$(sum(n$(list(n) AND succ(parent,n)), yes));

);

QLdiff(n) = abs(QL(n) - cost_L(n));
QLdiff(n)$(QLdiff(n) <= %tol%) = 0.;

y_Ld(n) = abs(y.l(n) - y_L(n));
v_Ld(n) = abs(v.l(n) - v_L(n));

L_Ld(n) = abs(L.l(n) - L_L(n));
F_Ld(n) = abs(F.l(n) - F_L(n));
Z_Ld(n) = abs(Z.l(n) - Z_L(n));
R_Ld(n) = abs(R.l(n) - R_L(n));

display L_Ld, F_Ld, Z_Ld, R_Ld, QLdiff, y_Ld, v_Ld;

* With pure cvar (cvar_wt = 1), then many variables are not nailed down
* Unless tail = 1 (expectation case) it only make sense to compare the root node cost
if (cvar_wt = 1. AND tail < 1.,
   abort$[smax{n$n.first, abs(QLdiff(n))} > %tol%]   'wrong costs', QLdiff, QL, cost_L;
else

   abort$[smax{n, abs(QLdiff(n))} > %tol% AND {sum[n$n.first, QLdiff(n)/QL(n)] > %tol%} ]   'wrong costs', QLdiff, QL, cost_L;
* TODO: (.75, .75) yields different solutions. 
*   abort$[smax{n, abs(y_Ld(n))} > %tol%]  'wrong probability measures', y_Ld, y.l, y_L;
*   abort$[smax{n, abs(v_Ld(n))} > %tol%]  'wrong VaR', v_Ld, v.l, v_L;

   abort$[smax{n$(NOT n.first), F_Ld(n)} > %tol%]     'wrong F.l', F_Ld;
   abort$[smax{n$(NOT n.first), Z_Ld(n)} > %tol%]     'wrong Z.l', Z_Ld;
)
* end of loop
); 

