$TITLE risk-averse test with the ClearLake example

$ontext

The model is based on the ClearLake example.


Reference: Exercise, p. 44 in
Birge, R, and Louveaux, F V, Introduction to Stochastic Programming.
Springer, 1997.

Contributor: Olivier Huber <oli.huber@gmail.com>
Date:        October 2025

$offtext

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
$offEcho

SETS p Stochastic realizations (precipitation) /low, normal, high/,
     t Time periods                            /dec,jan,feb,mar/,
     n Nodes,
     succ(n,n) Successor relationship;

ALIAS (n,child,parent);

*-------------------------------------------------------------------------------
*Parameters are specified

PARAMETERS Floodcost  'Flooding penalty cost thousand CHF per mm'   /10/,
           lowcost    'Water importation cost thousand CHF per mm'  /5/,
           l_start    'Initial water level (mm)'                    /100/,
           l_max      'Maximum reservoir level (mm)'                /250/,
           r_max      'Maximum normal release in any period'        /200/;

*-----------------------------------------------------------------------------

PARAMETERS prob(n)       'Conditional probability',
           ndelta(n)     'Water inflow at each node';

$gdxin clearlaktree.gdx
$load n, succ
$load prob, ndelta
$gdxin

VARIABLE EC               'Total Cost'

*------------------------------------------------------------------------------
* Model logic:  only needs n, succ, prob, ndelta
*------------------------------------------------------------------------------

POSITIVE VARIABLES
         L(n)             'Reservoir water level -- EOP',
		   R(n)             'Normal release of water (mm)',
		   F(n)             'Floodwater released (mm)',
		   Z(n)             'Water imports (mm)';

EQUATIONS ecdef, ldef; 

SCALAR max_stage / 1 /;

PARAMETER stage(n)  'stage of each node';

SET list(n);

* Compute stage of each node
list(n)$(n.first) = yes;
stage(n)$(n.first) = max_stage;

while(card(list) > 0,

   max_stage = max_stage + 1;

   list(n) = yes$(sum(parent$(list(parent) AND succ(parent,n)), yes));

   stage(n)$(list(n)) = max_stage;
);



* PARAMETER tail / .8 /, cvar_wt / 0.5 /;
PARAMETER tail / 1 /, cvar_wt / 0.1 /;

PARAMETER beta;
beta = 1-tail;

VARIABLES          v(n)    'Var value at each non-leaf node',
                   cost(n) 'total cost at each node';

POSITIVE VARIABLES y(n)    'Contribution of the cost for each node'

EQUATIONS cvar_ineq(n,n), defcost(n), totalCost;

cvar_ineq(n,child)$(succ(n,child))..
   y(child) =G= cost(child) - v(n);

defcost(n)..
   cost(n) =G= (floodcost*F(n)+lowcost*Z(n))$(NOT n.first) + 
   (
          cvar_wt * [ v(n) + [1/(1-beta)] * sum(succ(n,child), prob(child)*y(child)) ] +
      (1-cvar_wt) * [ sum(succ(n,child), prob(child)*cost(child)) ]
   )$(sum(succ(n,child), yes));

totalCost..
   EC =E= sum(n$n.first, cost(n));


ldef(n)$[NOT n.first].. L(n) =e= sum{ succ(parent,n), L(parent)} + ndelta(n)+Z(n)-R(n)-F(n);

R.up(n) = r_max;
L.up(n) = l_max;
L.fx(n)$(n.first) = l_start;

*-------------------------------------------------------------------------------
* Reference model
*-------------------------------------------------------------------------------
MODEL mincost / totalCost, defcost, cvar_ineq, ldef /;

mincost.optfile=1;

SOLVE mincost using LP minimizing EC;

display v.l, y.l;

PARAMETERS L_L(n), L_M(n), ldef_M(n), F_L(n), F_M(n), R_L(n), R_M(n), Z_L(n), Z_M(n),
           L_Ld(n), F_Ld(n), R_Ld(n), Z_Ld(n), y_L(n), cost_L(n), v_L(n);

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


VARIABLES  objnode(n)

EQUATIONS defobjnode(n), deflvl(n);

defobjnode(n)..
      objnode(n) =E= (floodcost*F(n)+lowcost*Z(n))$(not n.first);

ndelta(n)$(n.first) = L.l(n);
F.fx(n)$(n.first) = 0;
Z.fx(n)$(n.first) = 0;
R.fx(n)$(n.first) = 0;

deflvl(n).. L(n) =E= sum( succ(parent,n), L(parent) ) + ndelta(n)+Z(n)-R(n)-F(n);

MODEL mincostEMP / defobjnode, deflvl /;

mincostEMP.optfile = 1;

*******************************************************************************
* 1st EMP model
*******************************************************************************

embeddedCode reshop:
nodalprob(n): min objnode(n) + CRM(n).valFn$(succ(n,child)) F(n) L(n) R(n) Z(n) defobjnode(n) deflvl(n) 
CRM(n)$(succ(n,child)): MP('ecvarup', nodalprob(child).valFn$(succ(n,child)), tail=tail, cvar_wt=cvar_wt, prob=prob(child)$(succ(n,child)))
endEmbeddedCode

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

* These don't seem to be unique for a given solution; Note that they
* have associated cost
* abort$[smax{n$(NOT n.first), L_Ld(n)} > %tol%]     'wrong L.l', L_Ld;
* abort$[smax{n$(NOT n.first), R_Ld(n)} > %tol%]     'wrong R.l', R_Ld;


VARIABLES cvarURobj;
EQUATIONS defcvarURobj, cvarURcons(n,n);

PARAMETER QL(n);

SET activeNode(n);

defcvarURobj..
   cvarURobj
   =E=
   sum(activeNode(n), v(n) + [1/(1-beta)] * sum(succ(n,child), prob(child)*y(child)));

cvarURcons(n,child)$(activeNode(n) AND succ(n,child))..
   y(child) =G= QL(child) - v(n);

model cvarUR / defcvarURobj, cvarURcons /;

* leaf: just initialize local costs
list(n)$(NOT sum(succ(n,child), yes)) = yes;

QL(list(n)) = objnode.l(n);

* TODO: why do we need this alias

ALIAS(n,nn);

list(parent) = yes$(sum(n$(list(n) AND succ(parent,n)), yes));

activeNode(nn) = no;

while(card(list),

   loop(list(nn),
      activeNode(nn) = yes;

      solve cvarUR min cvarURobj using LP;

      QL(nn) = objnode.l(nn)
             + (1-cvar_wt) * sum(succ(nn,child), prob(child)*QL(child))
             + cvar_wt * cvarURobj.l;

      activeNode(nn) = no;
   );

   list(parent) = yes$(sum(n$(list(n) AND succ(parent,n)), yes));

);

PARAMETER QLd(n), y_Ld(n), v_ld(n);

QLd(n) = abs(QL(n) - cost_L(n));
y_Ld(n) = abs(y.l(n) - y_L(n));
v_Ld(n) = abs(v.l(n) - v_L(n));

L_Ld(n) = abs(L.l(n) - L_L(n));
F_Ld(n) = abs(F.l(n) - F_L(n));
Z_Ld(n) = abs(Z.l(n) - Z_L(n));
R_Ld(n) = abs(R.l(n) - R_L(n));

display L_Ld, F_Ld, Z_Ld, R_Ld, QLd, y_Ld, v_ld;

abort$[smax{n, abs(QL(n) - cost_L(n))} > %tol%]   'wrong costs', QL, cost_L;
abort$[smax{n, abs(y.l(n) - y_L(n))} > %tol%]     'wrong probability measures', y.l, y_L;
abort$[smax{n, abs(v.l(n) - v_L(n))} > %tol%]     'wrong costs', v.l, v_L;

abort$[smax{n$(NOT n.first), F_Ld(n)} > %tol%]     'wrong F.l', F_Ld;
abort$[smax{n$(NOT n.first), Z_Ld(n)} > %tol%]     'wrong Z.l', Z_Ld;

$exit

embeddedCode reshop:
nodalprob(n)$(n.first):        min objnode(n) + SUM{child$(succ(n,child)), prob(child)*nodalprob(child).valFn}
                                   L(n) defobjnode(n)
nodalprob(n)$(nonleafroot(n)): min objnode(n) + SUM{child$(succ(n,child)), prob(child)*nodalprob(child).valFn}
                                   L(n) F(n) R(n) Z(n) defobjnode(n) deflvl(n) 
nodalprob(n)$(leaf(n)):        min objnode(n)
                                   L(n) F(n) R(n) Z(n) defobjnode(n) deflvl(n)
endEmbeddedCode

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

PARAMETER deltaL_L(n), deltaL_M(n), deltaLvlDef_M(n);
deltaL_L(n) = L.l(n) - L_L(n);
deltaL_M(n) = L.m(n) - L_M(n);
deltaLvlDef_M(n) = deflvl.m(n) - ldef_M(n);

deltaL_L(n)$(abs(deltaL_L(n)) < %tol%) = 0.;
deltaL_M(n)$(abs(deltaL_M(n)) < %tol%) = 0.;
deltaLvlDef_M(n)$(abs(deltaLvlDef_M(n)) < %tol%) = 0.;


display deltaL_L, deltaL_M, deltaLvlDef_M;

abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;
abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;

*******************************************************************************
* 1bis EMP model
*******************************************************************************

embeddedCode reshop:
nodalprob(n)$(n.first):      min objnode(n) + SUM{ child$(succ(n,child)), prob(child)*nodalprob(child).valFn }
                                   L(n) defobjnode(n)
nodalprob(n)$(NOT n.first):  min objnode(n) + SUM{ child$(succ(n,child)), prob(child)*nodalprob(child).valFn }
                                   L(n) F(n) R(n) Z(n) defobjnode(n) deflvl(n) 
endEmbeddedCode

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;
abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;


*******************************************************************************
* 1ter model
*******************************************************************************

embeddedCode reshop:
nodalprob(n)$(n.first):     min objnode(n) + SUM{ succ(n,child), prob(child)*nodalprob(child).valFn}
                                   L(n) defobjnode(n)
nodalprob(n)$(NOT n.first): min objnode(n) + SUM{ succ(n,child), prob(child)*nodalprob(child).valFn}
                                   L(n) F(n) R(n) Z(n) defobjnode(n) deflvl(n) 
endEmbeddedCode

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;
abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;

mincostEMP.holdFixed = 0;

deflvlset(n)$(n.first) = yes;
ndelta(n)$(n.first) = L.l(n);
F.fx(n)$(n.first) = 0;
Z.fx(n)$(n.first) = 0;
R.fx(n)$(n.first) = 0;

display deflvlset;

*******************************************************************************
* 2nd model, purely recursive
*******************************************************************************

embeddedCode reshop:
nodalprob(n): min objnode(n) + SUM{child$(succ(n,child)), prob(child)*nodalprob(child).valFn}
                  L(n) F(n) R(n) Z(n) defobjnode(n) deflvl(n) 
endEmbeddedCode

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

PARAMETER deltaL_L(n), deltaL_M(n), deltaLvlDef_M(n);
deltaL_L(n) = L.l(n) - L_L(n);
deltaL_M(n) = L.m(n) - L_M(n);
deltaLvlDef_M(n) = deflvl.m(n) - ldef_M(n);

deltaL_L(n)$(abs(deltaL_L(n)) < %tol%) = 0.;
deltaL_M(n)$(abs(deltaL_M(n)) < %tol%) = 0.;
deltaLvlDef_M(n)$(abs(deltaLvlDef_M(n)) < %tol%) = 0.;


display deltaL_L, deltaL_M, deltaLvlDef_M;

abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;
abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;


*******************************************************************************
* 2.bis model, purely recursive
*******************************************************************************

embeddedCode reshop:
nodalprob(n): min objnode(n) + SUM{succ(n,child), prob(child)*nodalprob(child).valFn}
                  L(n) F(n) R(n) Z(n) defobjnode(n) deflvl(n) 
endEmbeddedCode

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

PARAMETER deltaL_L(n), deltaL_M(n), deltaLvlDef_M(n);
deltaL_L(n) = L.l(n) - L_L(n);
deltaL_M(n) = L.m(n) - L_M(n);
deltaLvlDef_M(n) = deflvl.m(n) - ldef_M(n);

deltaL_L(n)$(abs(deltaL_L(n)) < %tol%) = 0.;
deltaL_M(n)$(abs(deltaL_M(n)) < %tol%) = 0.;
deltaLvlDef_M(n)$(abs(deltaLvlDef_M(n)) < %tol%) = 0.;


display deltaL_L, deltaL_M, deltaLvlDef_M;

abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;
abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;

* This is not working
* abort$[smax{n$(NOT n.first), abs(L.m(n) - L_M(n))} > %tol%]     'wrong L.m', L.m;
* abort$[smax{n$(NOT n.first), abs(deflvl.m(n) - ldef_M(n))} > %tol%]     'wrong lvldef.m', deflvl.m;

*******************************************************************************
* 3rd model, with CRM
*******************************************************************************

embeddedCode reshop:
nodalprob(n)$(succ(n,child)): min objnode(n) + CRM(n).valFn F(n) L(n) R(n) Z(n) defobjnode(n) deflvl(n) 
nodalprob(n)$(NOT succ(n, child)): min objnode(n) F(n) L(n) R(n) Z(n) defobjnode(n) deflvl(n)
CRM(n)$(succ(n,child)): MP('expectation', nodalprob(child).valFn$(succ(n,child)), prob=prob(child)$(succ(n,child)))
endEmbeddedCode

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;
abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;

*******************************************************************************
* 3.bis model, with CRM
*******************************************************************************

embeddedCode reshop:
nodalprob(n): min objnode(n) + CRM(n).valFn$(succ(n,child)) F(n) L(n) R(n) Z(n) defobjnode(n) deflvl(n) 
CRM(n)$(succ(n,child)): MP('expectation', nodalprob(child).valFn$(succ(n,child)), prob=prob(child)$(succ(n,child)))
endEmbeddedCode

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;
abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;

*******************************************************************************
* 3.ter model, with CRM
*******************************************************************************

embeddedCode reshop:
nodalprob(n)$(n.first): min objnode(n) + CRM(n).valFn F(n) L(n) R(n) Z(n) deflvl(n) defobjnode(n) 
nodalprob(n)$((NOT n.first) AND (NOT leaf(n))): min objnode(n) + CRM(n).valFn F(n) L(n) R(n) Z(n) defobjnode(n) deflvl(n)
nodalprob(n)$((leaf(n) AND n.first) OR leaf(n)): min objnode(n) F(n) L(n) R(n) Z(n) defobjnode(n) deflvl(n)
CRM(n): MP('expectation', nodalprob(child).valFn$(succ(n,child)), prob=prob(child)$(succ(n,child)))
endEmbeddedCode

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;
abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;

$exit
embeddedCode reshop:
nodalprob(n)$(n.first): min objnode(n) + CRM(n).valFn F(n) L(n) R(n) Z(n) deflvl(n) defobjnode(n) 
nodalprob(n)$(NOT n.first AND (NOT leaf(n))): min objnode(n) + CRM(n).valFn defobjnode(n) deflvl(n)
CRM(n): MP('expectation', nodalprob(child).valFn$(succ(n,child)), prob=prob(child)$(succ(n,child)))
endEmbeddedCode


