$TITLE risk-averse test with the ClearLake example

$ontext

The model is based on the ClearLake example.


Reference: Exercise, p. 44 in
Birge, R, and Louveaux, F V, Introduction to Stochastic Programming.
Springer, 1997.

Contributor: Olivier Huber <oli.huber@gmail.com>
Date:        October 2025

$offtext

$if not set longtest $exit

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
     succ(n,n) Successor relationship,
     a The (fake) agents                       / a1*a2 /;

ALIAS (n,child,parent);
ALIAS (a,aa);

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
		   F(a,n)             'Floodwater released (mm)',
         L(a,n)             'Reservoir water level -- EOP',
		   R(a,n)             'Normal release of water (mm)',
		   Z(a,n)             'Water imports (mm)',
		   Fref(n)            'Floodwater released (mm)',
         Lref(n)            'Reservoir water level -- EOP',
		   Rref(n)            'Normal release of water (mm)',
		   Zref(n)            'Water imports (mm)';

R.up(a,n) = r_max;
L.up(a,n) = l_max;
L.fx(a,n)$(n.first) = l_start;

Rref.up(n) = r_max;
Lref.up(n) = l_max;
Lref.fx(n)$(n.first) = l_start;

* CVaR parameters
PARAMETER tail    'tail of the CVaR',
          cvar_wt 'weight of the CVaR';

PARAMETER beta;

SET tailvals, cvar_wt_vals;


* TODO: $genlabel(v, 'tail={tailvals(v)}')
PARAMETERS tails(tailvals<)        / 'tail=1'   1., 'tail=0.74' .74, 'tail=0.49' .49,
                                     'tail=0.3' .3/;
PARAMETERS cvar_wts(cvar_wt_vals<) / 'cvar_wt=0.9' .9, 'cvar_wt=0.75' .75, 'cvar_wt=0.5' .5,
                                     'cvar_wt=0.3' .3, 'cvar_wt=0.1'  .1/;

* Comparison
PARAMETERS L_L(n), L_M(n), ldef_M(n), F_L(n), F_M(n), R_L(n), R_M(n),
           Z_L(n), Z_M(n), L_Ld(a,n), F_Ld(a,n), R_Ld(a,n), Z_Ld(a,n), y_L(n),
           cost_L(n), v_L(n);

VARIABLES          v(a,n)     'Var value at each non-leaf node (per agent)',
                   vRef(n)    'Var value at each non-leaf node (reference)',
                   cost(a,n)  'total cost at each node (per agent)',
                   costRef(n) 'total cost at each node (reference)';

POSITIVE VARIABLES y(a,n)     'Contribution of the cost for each node (per agent)',
                   yRef(n)    'Contribution of the cost for each node (reference)';

*-------------------------------------------------------------------------------
* Model components for computing CVaR a posteriori
*-------------------------------------------------------------------------------

SET list(n);

VARIABLES cvarURobj;
EQUATIONS defcvarURobj(a), cvarURcons(a,n,n);

PARAMETER QL(a,n);

SINGLETON SET activeNode(n), activeAgent(a);

defcvarURobj(a)$(activeAgent(a))..
   cvarURobj
   =E=
   sum(activeNode(n), v(a,n) + [1/(1-beta)] * sum(succ(n,child), prob(child)*y(a,child)));

cvarURcons(a,n,child)$(activeNode(n) AND succ(n,child))..
   y(a,child) =G= QL(a,child) - v(a,n);

MODEL cvarUR 'Uryasev-Rockafellar formulation for cvar' / defcvarURobj, cvarURcons /;

PARAMETER QLd(a,n), y_Ld(a,n), v_ld(a,n);

*-------------------------------------------------------------------------------
* Reference model
*-------------------------------------------------------------------------------

EQUATIONS ecdef, ldef; 

EQUATIONS cvar_ineq(n,n), defcost(n), totalCost;

cvar_ineq(n,child)$(succ(n,child))..
   yRef(child) =G= costRef(child) - vRef(n);

defcost(n)..
   costRef(n) =G= (floodcost*Fref(n)+lowCost*Zref(n))$(NOT n.first) + 
   (
          cvar_wt * [ vRef(n) + [1/(1-beta)] * sum(succ(n,child), prob(child)*yRef(child)) ] +
      (1-cvar_wt) * [ sum(succ(n,child), prob(child)*costRef(child)) ]
   )$(sum(succ(n,child), yes));

totalCost..
   EC =E= sum(n$n.first, costRef(n));


ldef(n)$[NOT n.first].. Lref(n) =e= sum{ succ(parent,n), Lref(parent)}
                                    + ndelta(n) + Zref(n) - Rref(n) - Fref(n);

MODEL mincost / totalCost, defcost, cvar_ineq, ldef /;

mincost.optfile=1;


*-------------------------------------------------------------------------------
* EMP model
*-------------------------------------------------------------------------------

VARIABLES  objnode(a,n)     'nodal (local) cost'; 

EQUATIONS  defobjnode(a,n)  'nodal cost definition',
           deflvl(a,n)      'water level';

defobjnode(a,n)..
      objnode(a,n) =E= (floodCost*F(a,n)+lowCost*Z(a,n))$(not n.first);

deflvl(a,n).. L(a,n) =E= sum( succ(parent,n), L(a,parent) ) + ndelta(n) + Z(a,n)
                         - R(a,n) - F(a,n);

MODEL mincostEMP 'EMP stochastic model' / defobjnode, deflvl /;

SCALAR optfileVal;

*-------------------------------------------------------------------------------
* Start of tests
*-------------------------------------------------------------------------------
SET s     'reformulation scheme' / 'equilibrium', 'fenchel' /;
*SET s     'reformulation scheme' / 'fenchel' /;

PARAMETER QLdiff(a,n)    'Q values difference';

loop((s,tailvals,cvar_wt_vals),

tail    = tails(tailvals);
cvar_wt = cvar_wts(cvar_wt_vals);
optfileVal = ord(s);

$label test_start

display tail, cvar_wt;

beta = 1-tail;

SOLVE mincost using LP minimizing EC;

display vRef.l, yRef.l;

* Save reference values
L_L(n) = Lref.l(n);
L_M(n) = Lref.m(n);
ldef_M(n)$(NOT n.first) = ldef.m(n);

F_L(n) = Fref.l(n);
F_M(n) = Fref.m(n);
R_L(n) = Rref.l(n);
R_M(n) = Rref.m(n);
Z_L(n) = Zref.l(n);
Z_M(n) = Zref.m(n);

y_L(n) = yRef.l(n);
v_L(n) = vRef.l(n);
cost_L(n) = costRef.l(n);


* Reset
L.l(a,n) = 0.;
L.fx(a,n)$(n.first) = l_start;
R.l(a,n) = 0.;
F.l(a,n) = 0.;
Z.l(a,n) = 0.;
L.m(a,n) = 0.;
R.m(a,n) = 0.;
F.m(a,n) = 0.;
Z.m(a,n) = 0.;

ndelta(n)$(n.first) = l_start;
F.fx(a,n)$(n.first) = 0;
Z.fx(a,n)$(n.first) = 0;
R.fx(a,n)$(n.first) = 0;

*******************************************************************************
* 1st EMP model
*******************************************************************************

embeddedCode reshop:
nodalprob(a,n): min objnode(a,n) + CRM(a,n).valFn$(succ(n,child)) F(a,n) L(a,n) R(a,n) Z(a,n) defobjnode(a,n) deflvl(a,n) 
CRM(a,n)$(succ(n,child)): MP('ecvarup', nodalprob(a,child).valFn$(succ(n,child)), tail=tail, cvar_wt=cvar_wt, prob=prob(child)$(succ(n,child)))
main: Nash(nodalprob(a,n)$(n.first))
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

QL(a,list(n)) = objnode.l(a,n);

* Select leaf node parents
list(parent) = yes$(sum(n$(list(n) AND succ(parent,n)), yes));

while(card(list),

   loop(list,
      activeNode(list) = yes;

      loop(aa,
      activeAgent(aa) = yes;

         solve cvarUR min cvarURobj using LP;

         QL(aa,list) = objnode.l(aa,list)
                + (1-cvar_wt) * sum(succ(list,child), prob(child)*QL(aa,child))
                + cvar_wt * cvarURobj.l;

      );

   );

   list(parent) = yes$(sum(n$(list(n) AND succ(parent,n)), yes));

);

QLdiff(a,n) = abs(QL(a,n) - cost_L(n));
QLdiff(a,n)$(QLdiff(a,n) <= %tol%) = 0.;

y_Ld(a,n) = abs(y.l(a,n) - y_L(n));
v_Ld(a,n) = abs(v.l(a,n) - v_L(n));

L_Ld(a,n) = abs(L.l(a,n) - L_L(n));
F_Ld(a,n) = abs(F.l(a,n) - F_L(n));
Z_Ld(a,n) = abs(Z.l(a,n) - Z_L(n));
R_Ld(a,n) = abs(R.l(a,n) - R_L(n));

display L_Ld, F_Ld, Z_Ld, R_Ld, QLdiff, y_Ld, v_Ld;

* With pure cvar (cvar_wt = 1), then many variables are not nailed down
* Unless tail = 1 (expectation case) it only make sense to compare the root node cost
if (cvar_wt = 1. AND tail < 1.,
   abort$[smax{(a,n)$n.first, abs(QLdiff(a,n))} > %tol%]   'wrong costs', QLdiff, QL, cost_L;
else

   abort$[smax{(a,n), abs(QLdiff(a,n))} > %tol% AND {smax[(a,n)$n.first, QLdiff(a,n)/QL(a,n)] > %tol%} ]   'wrong costs', QLdiff, QL, cost_L;
* TODO: (.75, .75) yields different solutions. 
*   abort$[smax{n, abs(y_Ld(n))} > %tol%]  'wrong probability measures', y_Ld, y.l, y_L;
*   abort$[smax{n, abs(v_Ld(n))} > %tol%]  'wrong VaR', v_Ld, v.l, v_L;

   abort$[smax{(a,n)$(NOT n.first), F_Ld(a,n)} > %tol%]     'wrong F.l', F_Ld;
   abort$[smax{(a,n)$(NOT n.first), Z_Ld(a,n)} > %tol%]     'wrong Z.l', Z_Ld;
)
* end of loop
); 

