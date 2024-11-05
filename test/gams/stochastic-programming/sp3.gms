$title Simple 3 stage stochastic model
option limrow = 100, limcol=0;

$onText
3 stage stochastic model.
Only has 8 nodes in three stages.
Key issue is recursion allows us to do recursive E, and coherent risk measure
$offText

set
  t / t1*t3 /,
  i / 1*8 /;
alias(i,j,k);

set
  children(i,j) 'children of i are j' / 1.(2*3), 2.(4,5), 3.(6,7,8) /,
  tn(t,i) / t1.1, t2.(2,3), t3.(4*8) /;

$macro parent(i,j) (children(j,i))

parameter
  pr(i) 'conditional prob = prob (parent(n),n)' / 1 1, 2 0.1, 3 0.9, 4 0.4, 5 0.6, 6 0.1, 7 0.4, 8 0.5 /
  nodal_pr(i)
  tail(i) / 1 0.9, 2 0.75, 3 0.8 /
  risk_wt / 0.1 /;

nodal_pr('1') = 1;
loop(t$(not sameas(t, 't1')),
   loop(i$tn(t,i),
      nodal_pr(i) = pr(i)*sum(j$(parent(i,j)), nodal_pr(j));
   );
)

parameter
  c(i) / 1 0.5, 2 2.5, 3 1.0, 4 -1.5, 5 -0.2, 6 -2.0, 7 -0.5, 8 -3.0 /
  quad_cst /0.1/
;

VARIABLE cost(i);
POSITIVE VARIABLES x(i) 'decision variables';
EQUATIONS defcost(i), e2(i), e3(i);

e2(i)$tn('t2',i)..
  x('1') + x(i) =g= 20;

e3(i)$tn('t3',i)..
  x(i) =l= sum(j$parent(i,j), x(j));

defcost(i)..
  cost(i) =e= c(i)*x(i) + quad_cst*sqr(x(i))$tn('t3',i);




SET attrs / l, m/;

PARAMETERS x_RN(i, attrs) 'Risk-Neutral decisions'
           cost_DE_RN
           e2_RN(i,attrs)
           e3_RN(i,attrs)
           x_CVAR(i,attrs)
           cost_DE_CVAR
           e2_CVAR(i,attrs)
           e3_CVAR(i,attrs)
           x_ECVAR(i,attrs)
           cost_DE_ECVAR
           e2_ECVAR(i,attrs)
           e3_ECVAR(i,attrs)
;

* Reference risk-neutral problem

VARIABLES cost_DE;
EQUATIONS defcost_DE_RN    'cost function for risk-neutral deterministic equivalent'
          defcost_DE_CRM   'cost function for DE with non-trivial CRM';

defcost_DE_RN.. cost_DE =E= sum(i, nodal_pr(i)*c(i)*x(i)) + sum(i$tn('t3',i), nodal_pr(i)*quad_cst*sqr(x(i)));

model m_RN_ref /defcost_DE_RN, e2, e3/;
m_RN_ref.optfile = 1;

solve m_RN_ref min cost_DE using qcp;

abort$[m_RN_ref.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', m_RN_ref.modelStat;
abort$[m_RN_ref.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_RN_ref.solveStat;

x_RN(i, 'l') = x.l(i);
x_RN(i, 'm') = x.m(i);
cost_DE_RN = cost_DE.l;
e2_RN(i, 'l') = e2.l(i);
e2_RN(i, 'm') = e2.m(i);
e3_RN(i, 'l') = e3.l(i);
e3_RN(i, 'm') = e3.m(i);

SCALAR err;

$macro check(mdl, x, x_ref, cost, cost_ref, e2, e2_ref, e3, e3_ref) \
abort$[mdl.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', mdl.modelStat; \
abort$[mdl.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mdl.solveStat; \
err = smax(i, abs(x.l(i) - x_ref(i, 'l'))); \
abort$[err > 1e-6]  'different x levels', x.l, x_ref, err; \
err = smax(i, abs(x.m(i) - x_ref(i, 'm'))); \
abort$[err > 1e-6]  'different x marginals', x.m, x_ref, err; \
err = smax(i, abs(e2.l(i) - e2_ref(i, 'l'))); \
abort$[err > 1e-6]  'different e2 levels', e2.l, e2_ref, err; \
err = smax(i, abs(e2.m(i) - e2_ref(i, 'm'))); \
abort$[err > 1e-6]  'different e2 marginals', e2.m, e2_ref, err; \
err = smax(i, abs(e3.l(i) - e3_ref(i, 'l'))); \
abort$[err > 1e-6]  'different e3 levels', e3.l, e3_ref, err; \
err = smax(i, abs(e3.m(i) - e3_ref(i, 'm'))); \
abort$[err > 1e-6]  'different e3 marginals', e3.m, e3_ref, err; \


$onEcho > %gams.emp%.opt
subsolveropt 1
$offEcho

$onEcho > path.opt
conv_tol 1e-10
$offEcho


model m /defcost, e2, e3/;
m.optfile = 1;

set O_1(i) /2*3/, O_2(i) /4*5/, O_3(i) /6*8/, L(i) / 4*8 /;

* Default is to do min-max as equilibrium

EmbeddedCode ReSHOP:
n('1'): min cost('1') + cv('1').valfn x('1') defcost('1')
cv('1'): MP('expectation',n(O_1).valfn,prob=pr(O_1))
n(O_1): min cost(O_1) + cv(O_1).valfn x(O_1) defcost(O_1) e2(O_1)
cv('2'): MP('expectation',n(O_2).valfn,prob=pr(O_2))
cv('3'): MP('expectation',n(O_3).valfn,prob=pr(O_3))
n(L): min cost(L) x(L) e3(L) defcost(L)  
endEmbeddedCode

solve m using emp;

check(m, x, x_RN, cost('1'), cost_DE_RN, e2, e2_RN, e3, e3_RN)

* Create equilibrium reference problem for risk-neutral

VARIABLES y(i,j) 'coherent risk measure variables';

EQUATIONS normY(i);

normY(i)$(not L(i)).. sum(j$children(i,j), y(i,j)) =E= 1.;

* TODO: support this
$onText
EmbeddedCode ReSHOP:
n('1'): min cost('1') + y('1', '2')*n('2').valFn + y('1', '3')*n('3').valFn x('1') defcost('1')
n(O_1): min cost(O_1) + SUM(i$(children(O_1,i)), y(O_1, j)*n(j)) x(O_1) defcost(O_1) e2(O_1)
n(L): min cost(L) x(L) e3(L) defcost(L)  

crm(i)$(not L(i)): max SUM(j$(children(i,j), y(i,j)*n(j).objFn) y(i,j) normY
root: Nash(n(i), crm(i$(not L(i))))
endEmbeddedCode
$offText

* TODO: for now, this just tests emb mode
EmbeddedCode ReSHOP:
n(i)$(not L(i)): min cost(i) + SUM(j$children(i,j), y(i, j)*n(j).valFn) x(i) defcost(i) e2(i)$(tn('t2',i))
n(L): min cost(L) x(L) e3(L) defcost(L)  

crm(i)$(not L(i)): max SUM(j$children(i,j), y(i,j)*n(j).objFn) y(i,j) normY
* TODO: root: Nash(n(i), crm(i$(not L(i))))
root: Nash(n, crm)
endEmbeddedCode

**** works
***root: Nash(n(i), crm(notL))
**** FAILS since crm(leaf) is not defined
***root: Nash(n(i), crm(i))

* TODO: this fails since we don't support multiset yet in VM mode
* solve m using emp;

* check(m, x, x_RN, cost('1'), cost_DE_RN, e2, e2_RN, e3, e3_RN)


*******************************************************************************
* Start CRM tests
*******************************************************************************

* Default is to do min-max as equilibrium

defcost_DE_CRM..
  cost_DE =E= c('1')*x('1')
                + sum{ i$children('1',i), y('1',i) * (c(i)*x(i) + sum{j$children(i,j), y(i,j) * (c(j)*x(j) + quad_cst*sqr(x(j)))} ) };

* CMEX does not allow cost('1') to be used as objective function. TODO: open an issue to either support it or improve doc to say that it should be DECLARED as scalar

*******************************************************************************
* CRM test 1: redo risk neutral
*******************************************************************************

y.fx(i,j)$(children(i,j)) = pr(j);

model m_crm_ref / defcost_DE_CRM, e2, e3 /;
m_crm_ref.optfile = 1;

m_crm_ref.holdfixed = 1;

* TODO: There is a complain about a general NL equation, despute the holdfix ...
* solve m_crm_ref using qcp min cost_DE;
solve m_crm_ref using nlp min cost_DE;

* Check that this is equivalent to the one before
check(m_crm_ref, x, x_RN, cost_DE, cost_DE_RN, e2, e2_RN, e3, e3_RN)

*******************************************************************************
* CRM test 2: SAVE CVAR results
*******************************************************************************

y.lo(i,j)$(children(i,j)) = 0;
y.up(i,j)$(children(i,j)) = pr(j)/tail(i);

EQUATIONS crm_objequ(i) 'Objective equations for the CRMs';
VARIABLES crm_objvar(i) 'Objective variables for the CRMs';

crm_objequ(i)$(not L(i)).. crm_objvar(i) =E=
(
   sum{j$children(i,j), y(i,j)*(c(j)*x(j) + sum{k$children(j,k), y(j,k)*(c(k)*x(k) + quad_cst*sqr(x(k)))} ) }
)$tn('t1',i)
+ 
(
   sum{j$children(i,j), y(i,j)*(c(j)*x(j) + quad_cst*sqr(x(j)))}
)$tn('t2',i);


* This would be one way to define it, using cost to hold the expression
* TODO FIX IT!
$onText
EmbeddedCode ReSHOP:
primal: min cost_DE x e2 e3 defcost_DE_CRM

deffn cost(i) defcost(i)
crm(i)$(not L(i)): max SUM(j$children(i,j), y(i,j) * cost(j) ) normY(i)
equil: Nash(primal, crm)
endEmbeddedCode

solve m_crm_ref using qcp min cost_DE;
$offText



EmbeddedCode ReSHOP:
primal: min cost_DE x e2 e3 defcost_DE_CRM

* Should this work?
*crm(i)$(not L(i)): max crm_objvar(i) y(i,j) normY(i) crm_objequ(i)

crm(i)$(not L(i)): max crm_objvar(i) y(i,'*') normY(i) crm_objequ(i)
* TODO: fix the following
* equil: Nash(primal, crm)
equil: Nash(primal, crm(i)$(not L(i)))
endEmbeddedCode

EQUATION defcost_DE_CRMtmp;

model m_crm_equil_ref / e2 e3 defcost_DE_CRM normY crm_objequ /;
m_crm_equil_ref.optfile = 1;

solve m_crm_equil_ref using emp;

abort$[m_crm_equil_ref.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_crm_equil_ref.modelStat;
abort$[m_crm_equil_ref.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_crm_equil_ref.solveStat;

x_CVAR(i, 'l') = x.l(i);
x_CVAR(i, 'm') = x.m(i);
* cost_DE_CVAR = cost_DE.l;
e2_CVAR(i, 'l') = e2.l(i);
e2_CVAR(i, 'm') = e2.m(i);
e3_CVAR(i, 'l') = e3.l(i);
e3_CVAR(i, 'm') = e3.m(i);

EmbeddedCode ReSHOP:
n('1'): min cost('1') + cv('1').valfn x('1') defcost('1')
cv('1'): MP('cvarup',n(O_1).valfn,tail=tail('1'),prob=pr(O_1))
n(O_1): min cost(O_1) + cv(O_1).valfn x(O_1) defcost(O_1) e2(O_1)
cv('2'): MP('cvarup',n(O_2).valfn,tail=tail('2'),prob=pr(O_2))
cv('3'): MP('cvarup',n(O_3).valfn,tail=tail('3'),prob=pr(O_3))
n(L): min cost(L) x(L) e3(L) defcost(L) 
endEmbeddedCode


* m.optfile = 1;

solve m using emp;

* Note: if tail values are too small, then it is difficult to compare the solutions
check(m, x, x_CVAR, cost_DE, cost_DE_CVAR, e2, e2_CVAR, e3, e3_CVAR)

* m.optfile = 0;

*******************************************************************************
* CRM test 2: SAVE ECVAR results
*******************************************************************************

y.lo(i,j)$(children(i,j)) = (1-risk_wt)*pr(j);
y.up(i,j)$(children(i,j)) = pr(j) * ((1-risk_wt) + risk_wt/tail(i));

EmbeddedCode ReSHOP:
primal: min cost_DE x e2 e3 defcost_DE_CRM

* Should this work?
*crm(i)$(not L(i)): max crm_objvar(i) y(i,j) normY(i) crm_objequ(i)

crm(i)$(not L(i)): max crm_objvar(i) y(i,'*') normY(i) crm_objequ(i)
* TODO: fix the following
* equil: Nash(primal, crm)
equil: Nash(primal, crm(i)$(not L(i)))
endEmbeddedCode

solve m_crm_equil_ref using emp;

abort$[m_crm_equil_ref.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_crm_equil_ref.modelStat;
abort$[m_crm_equil_ref.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_crm_equil_ref.solveStat;

x_ECVAR(i, 'l') = x.l(i);
x_ECVAR(i, 'm') = x.m(i);
* cost_DE_ECVAR = cost_DE.l;
e2_ECVAR(i, 'l') = e2.l(i);
e2_ECVAR(i, 'm') = e2.m(i);
e3_ECVAR(i, 'l') = e3.l(i);
e3_ECVAR(i, 'm') = e3.m(i);

EmbeddedCode ReSHOP:
n('1'): min cost('1') + cv('1').valfn x('1') defcost('1')
cv('1'): MP('ecvarup',n(O_1).valfn,tail=tail('1'),risk_wt=risk_wt,prob=pr(O_1))
n(O_1): min cost(O_1) + cv(O_1).valfn x(O_1) defcost(O_1) e2(O_1)
cv('2'): MP('ecvarup',n(O_2).valfn,tail=tail('2'),risk_wt=risk_wt,prob=pr(O_2))
cv('3'): MP('ecvarup',n(O_3).valfn,tail=tail('3'),risk_wt=risk_wt,prob=pr(O_3))
n(L): min cost(L) x(L) e3(L) defcost(L) 
endEmbeddedCode


solve m using emp;

check(m, x, x_ECVAR, cost_DE, cost_DE_ECVAR, e2, e2_ECVAR, e3, e3_ECVAR)

$exit

* Has dual operator encoded in ReSHOP

EmbeddedCode ReSHOP:
n('1'): min cost('1') + cv('1').dual().valfn x('1') defcost('1')
cv('1'): MP('ecvarup',n(O_1).valfn,tail=0.9,risk_wt=risk_wt,prob=pr(O_1))
n(O_1): min cost(O_1) + cv(O_1).dual().valfn x(O_1) defcost(O_1) e2(O_1)
cv('2'): MP('ecvarup',n(O_2).valfn,tail=0.75,risk_wt=risk_wt,prob=pr(O_2))
cv('3'): MP('ecvarup',n(O_3).valfn,tail=0.8,risk_wt=risk_wt,prob=pr(O_3))
n(L): min cost(L) x(L) e3(L) defcost(L)  
endEmbeddedCode

solve m using emp;

abort$[m.modelStat <> %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', m.modelStat;
abort$[m.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m.solveStat;


