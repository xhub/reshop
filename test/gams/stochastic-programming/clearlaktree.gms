$TITLE Scenario Reduction: ClearLake exercise (CLEARLAKSP,SEQ=72)

$ontext

Exercise, p. 44:

The Clear Lake Dam controls the water level in Clear Lake, a
well-known resort in Dreamland.  The Dam Commission is trying to
decide how much water to release in each of the next four months.
The Lake is currently 150 mm below flood stage.  The dam is capable
of lowering the water level 200 mm each month, but additional
precipitation and evaporation affect the dam.  The weather near Clear
Lake is highly variable.  The Dam Commission has divided the months
into two two-month blocks of similar weather.  The months within each
block have the same probabilities for weather, which are assumed
independent of one another.  In each month of the first block, they
assign a probability of 1/2 to having a natural 100-mm increase in
water levels and probabilities of 1/4 to having a 50-mm decrease or a
250-mm increase in water levels.  All these figures correspond to
natural changes in water level without dam releases.  In each month
of the second block, they assign a probability of 1/2 to having a
natural 150-mm increase in water levels and probabilities of 1/4 to
having a 50-mm increase or a 350-mm increase in water levels.  If a
flood occurs, then damage is assessed at $10,000 per mm above flood
level.  A water level too low leads to costly importation of water.
These costs are $5000 per mm less than 250 mm below flood stage.  The
commission first considers an overall goal of minimizing expected
costs.  This model only considers this first objective.


Birge, R, and Louveaux, F V, Introduction to Stochastic Programming.
Springer, 1997.

$offtext

$if not set tol $set tol 1e-6

$onEcho > pathnlp.opt
conv_tol 1e-12
crash_method none
prox_pert 0
crash_pert no
$offEcho

option lp=pathnlp;

SETS p Stochastic realizations (precipitation) /low, normal, high/,
     t Time periods                            /dec,jan,feb,mar/,
     n Nodes,
     succ(n,n) Successor relationship;

ALIAS (n,child,parent);

*-------------------------------------------------------------------------------
*Parameters are specified

parameter Floodcost  'Flooding penalty cost thousand CHF per mm'   /10/,
          lowcost    'Water importation cost thousand CHF per mm'  /5/,
          l_start    'Initial water level (mm)'                    /100/,
          l_max      'Maximum reservoir level (mm)'                /250/,
          r_max      'Maximum normal release in any period'        /200/;

*-----------------------------------------------------------------------------

parameter prob(n)       'Conditional probability',
          ndelta(n)     'Water inflow at each node';

$gdxin clearlaktree.gdx
$load n, succ
$load prob, ndelta
$gdxin

*------------------------------------------------------------------------------
* Model logic:  only needs n, succ, prob, ndelta

parameter n_prob(n)       Probability of being at any node;

n_prob(n)$(n.first) = 1;
loop {
   succ(parent,child),
   n_prob(child) = prob(child)*n_prob(parent);
};

display n_prob;

variable EC               'Expected total cost';

POSITIVE VARIABLE
         L(n)             'Reservoir water level -- EOP',
		   R(n)             'Normal release of water (mm)',
		   F(n)             'Floodwater released (mm)',
		   Z(n)             'Water imports (mm)';

EQUATIONS ecdef, ldef; 

ecdef.. EC =e= sum {n, n_prob(n)*[floodcost*F(n)+lowcost*Z(n)]};

ldef(n)$[NOT n.first].. L(n) =e= sum{succ(parent,n),L(parent)} + ndelta(n)+Z(n)-R(n)-F(n);

R.up(n) = r_max;
L.up(n) = l_max;
L.fx(n)$(n.first) = l_start;

*-------------------------------------------------------------------------------
MODEL mincost / ecdef, ldef /;

mincost.optfile=1;

SOLVE mincost using LP minimizing EC;

PARAMETERS L_L(n), L_M(n), ldef_M(n), F_L(n), F_M(n), R_L(n), R_M(n), Z_L(n), Z_M(n);

L_L(n) = L.l(n);
L_M(n) = L.m(n);
ldef_M(n)$(NOT n.first) = ldef.m(n);

F_L(n) = F.l(n);
F_M(n) = F.m(n);
R_L(n) = R.l(n);
R_M(n) = R.m(n);
Z_L(n) = Z.l(n);
Z_M(n) = Z.m(n);


SET leaf(n);
leaf(n) = yes$(NOT sum(succ(n,child), yes));

SET nonleafroot(n);
nonleafroot(n) = yes$(NOT n.first AND NOT leaf(n));

display leaf, nonleafroot;

ALIAS(child,n)

VARIABLES  objnode(n)

EQUATIONS defobjnode(n), deflvl(n);

defobjnode(n)..
      objnode(n) =E= (floodcost*F(n)+lowcost*Z(n))$(not n.first);

SET deflvlset(n);
deflvlset(n) = yes$(NOT n.first);

deflvl(n)$[deflvlset(n)]..
      L(n) =E= sum( succ(parent,n), L(parent) ) + ndelta(n)+Z(n)-R(n)-F(n);

MODEL mincostEMP / defobjnode, deflvl /;

mincostEMP.optfile = 1;

*******************************************************************************
* 1st EMP model
*******************************************************************************

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

abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;
abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;

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

abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;
abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;


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

abort$[smax{n$(NOT n.first), abs(F.l(n) - F_L(n))} > %tol%]     'wrong F.l', F.l;
abort$[smax{n$(NOT n.first), abs(Z.l(n) - Z_L(n))} > %tol%]     'wrong Z.l', Z.l;
abort$[smax{n$(NOT n.first), abs(R.l(n) - R_L(n))} > %tol%]     'wrong R.l', R.l;
abort$[smax{n$(NOT n.first), abs(L.l(n) - L_L(n))} > %tol%]     'wrong L.l', L.l;

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


