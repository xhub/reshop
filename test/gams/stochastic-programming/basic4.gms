$TITLE Basic 4 stage SP

$onText
Basic 4 stage stochastic test with various syntax 

Contributor: Olivier Huber <oli.huber@gmail.com>
Date: October 2025
$offText

* Combine with CCF

SET t stages / t1*t4 /;
SET n / n1*n15 /;
ALIAS(n,child);
SET succ(n,child), leaf(n);

succ(n,child) = yes$((2*ord(n) EQ ord(child)) OR (2*ord(n) +1 EQ ord(child)) );
leaf(n) = yes$(NOT sum(child, succ(n,child)));

display succ, leaf;

PARAMETER cN(n);

cN(n) = power(-1, ord(n))*2;

VARIABLES xN(n), objN(n);

xN.lo(n) = -ord(n);
xN.up(n) =  ord(n);

EQUATIONS defobjN(n);

defobjN(n).. objN(n) =E= cN(n)*xN(n);

embeddedCode ReSHOP:
node(n)$(NOT leaf(n)): min objN(n) + m(n).valFn xN(n) defobjN(n)
m(n)$(NOT leaf(n)):    MP('smax', node(child).valFn$(succ(n,child)))
node(n)$(leaf(n)):     min objN(n) xN(n) defobjN(n)
endEmbeddedCode

model m_basic4 /defobjN/;

solve m_basic4 using emp;
abort$[m_basic4.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_basic4.modelStat;
abort$[m_basic4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_basic4.solveStat;

SET pow2(n,t);
pow2(n,t) = yes$(ord(n) = power(2,ord(t)-1));

display pow2;

* Simple check on the marginal values (binding constraints)
abort$[ smax(n$(sum(t, pow2(n,t))), abs( xN.m(n) ) ) < 2. - 1e-4 ] 'wrong xN.m values', xN.m;
abort$[ smax(n$(NOT sum(t, pow2(n,t))), abs( xN.m(n) ) ) > 1e-4 ] 'wrong xN.m values', xN.m;

* With this model, the values at each node are nailed down
option clear = xN;
xN.lo(n) = -ord(n);
xN.up(n) =  ord(n);

embeddedCode ReSHOP:
node(n)$(NOT leaf(n)): min objN(n) + m(n).valFn xN(n) defobjN(n)
m(n)$(NOT leaf(n)): MP('ecvarup', node(child).valFn$(succ(n,child)), risk_wt=.8, tail=.4)
node(n)$(leaf(n)): min objN(n) xN(n) defobjN(n)
endEmbeddedCode

solve m_basic4 using emp;
abort$[m_basic4.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_basic4.modelStat;
abort$[m_basic4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_basic4.solveStat;

* Odd variables are at their upper bounds
* Even at their upper bounds
abort$[ smax{n$(mod(ord(n),2) = 0), abs(xN.l(n) - xN.lo(n))} > 1e-4] 'wrong even xN values', xN.l;
abort$[ smax{n$(mod(ord(n),2) = 1), abs(xN.l(n) - xN.up(n))} > 1e-4] 'wrong even xN values', xN.l;

* Check that the absolute marginal values sum to 2 at each stage
abort$[ smax{t, abs( sum[n$(ord(n) >= power(2, ord(t)-1) AND ord(n) < power(2, ord(t))), abs(xN.m(n)) ] - 2.) } > 1e-4] 'wrong even xN values', xN.m;


****************************************************************************************************************
* Test for the syntax   '$succ(set1, set2)'
****************************************************************************************************************
option clear = xN;
xN.lo(n) = -ord(n);
xN.up(n) =  ord(n);

embeddedCode ReSHOP:
* node(n)$(sum(succ(n,child),yes)): min objN(n) + MP('smax', node(child).valFn$(succ(n,child))) xN(n) defobjN(n)
node(n)$(succ(n,child)):      min objN(n) + CRM(n).valFn xN(n) defobjN(n)
CRM(n)$(succ(n,child)):       MP('ecvarup', node(child).valFn$(succ(n,child)), risk_wt=.8, tail=.4)
node(n)$(NOT succ(n,child)):  min objN(n) xN(n) defobjN(n)
endEmbeddedCode

solve m_basic4 using emp;
abort$[m_basic4.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_basic4.modelStat;
abort$[m_basic4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_basic4.solveStat;

* Odd variables are at their upper bounds
* Even at their upper bounds
abort$[ smax{n$(mod(ord(n),2) = 0), abs(xN.l(n) - xN.lo(n))} > 1e-4] 'wrong even xN values', xN.l;
abort$[ smax{n$(mod(ord(n),2) = 1), abs(xN.l(n) - xN.up(n))} > 1e-4] 'wrong even xN values', xN.l;

* Check that the absolute marginal values sum to 2 at each stage
abort$[ smax{t, abs( sum[n$(ord(n) >= power(2, ord(t)-1) AND ord(n) < power(2, ord(t))), abs(xN.m(n)) ] - 2.) } > 1e-4] 'wrong even xN values', xN.m;

****************************************************************************************************************
* Test for the syntax  'SUM(set2, multiset(set1,set2))'
****************************************************************************************************************
option clear = xN;
xN.lo(n) = -ord(n);
xN.up(n) =  ord(n);

embeddedCode ReSHOP:
* node(n)$(sum(succ(n,child),yes)): min objN(n) + MP('smax', node(child).valFn$(succ(n,child))) xN(n) defobjN(n)
node(n)$(sum(child, succ(n,child))):      min objN(n) + CRM(n).valFn xN(n) defobjN(n)
CRM(n)$(sum(child, succ(n,child))):       MP('ecvarup', node(child).valFn$(succ(n,child)), risk_wt=.8, tail=.4)
node(n)$(NOT sum(child, succ(n,child))):  min objN(n) xN(n) defobjN(n)
endEmbeddedCode

solve m_basic4 using emp;
abort$[m_basic4.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_basic4.modelStat;
abort$[m_basic4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_basic4.solveStat;

* Odd variables are at their upper bounds
* Even at their upper bounds
abort$[ smax{n$(mod(ord(n),2) = 0), abs(xN.l(n) - xN.lo(n))} > 1e-4] 'wrong even xN values', xN.l;
abort$[ smax{n$(mod(ord(n),2) = 1), abs(xN.l(n) - xN.up(n))} > 1e-4] 'wrong even xN values', xN.l;

* Check that the absolute marginal values sum to 2 at each stage
abort$[ smax{t, abs( sum[n$(ord(n) >= power(2, ord(t)-1) AND ord(n) < power(2, ord(t))), abs(xN.m(n)) ] - 2.) } > 1e-4] 'wrong even xN values', xN.m;

****************************************************************************************************************
* Test for the syntax  'SUM(multiset(set1,set2), yes)'
****************************************************************************************************************
option clear = xN;
xN.lo(n) = -ord(n);
xN.up(n) =  ord(n);

embeddedCode ReSHOP:
* node(n)$(sum(succ(n,child),yes)): min objN(n) + MP('smax', node(child).valFn$(succ(n,child))) xN(n) defobjN(n)
node(n)$(sum(succ(n,child), yes)):      min objN(n) + CRM(n).valFn xN(n) defobjN(n)
CRM(n)$(sum(succ(n,child), yes)):       MP('ecvarup', node(child).valFn$(succ(n,child)), risk_wt=.8, tail=.4)
node(n)$(NOT sum(succ(n,child), yes)):  min objN(n) xN(n) defobjN(n)
endEmbeddedCode

solve m_basic4 using emp;
abort$[m_basic4.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_basic4.modelStat;
abort$[m_basic4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_basic4.solveStat;

* Odd variables are at their upper bounds
* Even at their upper bounds
abort$[ smax{n$(mod(ord(n),2) = 0), abs(xN.l(n) - xN.lo(n))} > 1e-4] 'wrong even xN values', xN.l;
abort$[ smax{n$(mod(ord(n),2) = 1), abs(xN.l(n) - xN.up(n))} > 1e-4] 'wrong even xN values', xN.l;

* Check that the absolute marginal values sum to 2 at each stage
abort$[ smax{t, abs( sum[n$(ord(n) >= power(2, ord(t)-1) AND ord(n) < power(2, ord(t))), abs(xN.m(n)) ] - 2.) } > 1e-4] 'wrong even xN values', xN.m;

****************************************************************************************************************
* Test for the syntax  'SUM{ multiset(set1,set2), multiset2(set1, set2) }'
****************************************************************************************************************
option clear = xN;
xN.lo(n) = -ord(n);
xN.up(n) =  ord(n);

embeddedCode ReSHOP:
* node(n)$(sum(succ(n,child),yes)): min objN(n) + MP('smax', node(child).valFn$(succ(n,child))) xN(n) defobjN(n)
node(n)$(sum(succ(n,child), succ(n,child))):      min objN(n) + CRM(n).valFn xN(n) defobjN(n)
CRM(n)$(sum(succ(n,child), succ(n,child))):       MP('ecvarup', node(child).valFn$(succ(n,child)), risk_wt=.8, tail=.4)
node(n)$(NOT sum(succ(n,child), succ(n,child))):  min objN(n) xN(n) defobjN(n)
endEmbeddedCode

solve m_basic4 using emp;
abort$[m_basic4.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_basic4.modelStat;
abort$[m_basic4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_basic4.solveStat;

* Odd variables are at their upper bounds
* Even at their upper bounds
abort$[ smax{n$(mod(ord(n),2) = 0), abs(xN.l(n) - xN.lo(n))} > 1e-4] 'wrong even xN values', xN.l;
abort$[ smax{n$(mod(ord(n),2) = 1), abs(xN.l(n) - xN.up(n))} > 1e-4] 'wrong even xN values', xN.l;

* Check that the absolute marginal values sum to 2 at each stage
abort$[ smax{t, abs( sum[n$(ord(n) >= power(2, ord(t)-1) AND ord(n) < power(2, ord(t))), abs(xN.m(n)) ] - 2.) } > 1e-4] 'wrong even xN values', xN.m;

****************************************************************************************************************
* Test for the syntax  'SUM{set2$multiset(set1,set2), yes}'
****************************************************************************************************************
option clear = xN;
xN.lo(n) = -ord(n);
xN.up(n) =  ord(n);

embeddedCode ReSHOP:
* node(n)$(sum(succ(n,child),yes)): min objN(n) + MP('smax', node(child).valFn$(succ(n,child))) xN(n) defobjN(n)
node(n)$(sum{ child$(succ(n,child)), yes }):      min objN(n) + CRM(n).valFn xN(n) defobjN(n)
CRM(n)$( sum{ child$(succ(n,child)), yes }):       MP('ecvarup', node(child).valFn$(succ(n,child)), risk_wt=.8, tail=.4)
node(n)$(NOT sum{child$(succ(n,child)), yes}):  min objN(n) xN(n) defobjN(n)
endEmbeddedCode

solve m_basic4 using emp;
abort$[m_basic4.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_basic4.modelStat;
abort$[m_basic4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_basic4.solveStat;

* Odd variables are at their upper bounds
* Even at their upper bounds
abort$[ smax{n$(mod(ord(n),2) = 0), abs(xN.l(n) - xN.lo(n))} > 1e-4] 'wrong even xN values', xN.l;
abort$[ smax{n$(mod(ord(n),2) = 1), abs(xN.l(n) - xN.up(n))} > 1e-4] 'wrong even xN values', xN.l;

* Check that the absolute marginal values sum to 2 at each stage
abort$[ smax{t, abs( sum[n$(ord(n) >= power(2, ord(t)-1) AND ord(n) < power(2, ord(t))), abs(xN.m(n)) ] - 2.) } > 1e-4] 'wrong even xN values', xN.m;

****************************************************************************************************************
* Test for the syntax  'SUM{set2$multiset(set1,set2), multiset2(set1,set2)}'
****************************************************************************************************************
option clear = xN;
xN.lo(n) = -ord(n);
xN.up(n) =  ord(n);

embeddedCode ReSHOP:
* node(n)$(sum(succ(n,child),yes)): min objN(n) + MP('smax', node(child).valFn$(succ(n,child))) xN(n) defobjN(n)
node(n)$(sum{ child$(succ(n,child)), succ(n,child) }):      min objN(n) + CRM(n).valFn xN(n) defobjN(n)
CRM(n)$( sum{ child$(succ(n,child)), succ(n,child) }):       MP('ecvarup', node(child).valFn$(succ(n,child)), risk_wt=.8, tail=.4)
node(n)$(NOT sum{child$(succ(n,child)), succ(n,child) }):  min objN(n) xN(n) defobjN(n)
endEmbeddedCode

solve m_basic4 using emp;
abort$[m_basic4.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_basic4.modelStat;
abort$[m_basic4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_basic4.solveStat;

* Odd variables are at their upper bounds
* Even at their upper bounds
abort$[ smax{n$(mod(ord(n),2) = 0), abs(xN.l(n) - xN.lo(n))} > 1e-4] 'wrong even xN values', xN.l;
abort$[ smax{n$(mod(ord(n),2) = 1), abs(xN.l(n) - xN.up(n))} > 1e-4] 'wrong even xN values', xN.l;

* Check that the absolute marginal values sum to 2 at each stage
abort$[ smax{t, abs( sum[n$(ord(n) >= power(2, ord(t)-1) AND ord(n) < power(2, ord(t))), abs(xN.m(n)) ] - 2.) } > 1e-4] 'wrong even xN values', xN.m;

****************************************************************************************************************
* Test for the syntax   'CRM(n).valFn$succ(set1, set2)'
****************************************************************************************************************
option clear = xN;
xN.lo(n) = -ord(n);
xN.up(n) =  ord(n);

embeddedCode ReSHOP:
* node(n)$(sum(succ(n,child),yes)): min objN(n) + MP('smax', node(child).valFn$(succ(n,child))) xN(n) defobjN(n)
node(n):      min objN(n) + CRM(n).valFn$(succ(n,child)) xN(n) defobjN(n)
CRM(n)$(succ(n,child)):       MP('ecvarup', node(child).valFn$(succ(n,child)), risk_wt=.8, tail=.4)
endEmbeddedCode

solve m_basic4 using emp;
abort$[m_basic4.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_basic4.modelStat;
abort$[m_basic4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_basic4.solveStat;

* Odd variables are at their upper bounds
* Even at their upper bounds
abort$[ smax{n$(mod(ord(n),2) = 0), abs(xN.l(n) - xN.lo(n))} > 1e-4] 'wrong even xN values', xN.l;
abort$[ smax{n$(mod(ord(n),2) = 1), abs(xN.l(n) - xN.up(n))} > 1e-4] 'wrong even xN values', xN.l;

* Check that the absolute marginal values sum to 2 at each stage
abort$[ smax{t, abs( sum[n$(ord(n) >= power(2, ord(t)-1) AND ord(n) < power(2, ord(t))), abs(xN.m(n)) ] - 2.) } > 1e-4] 'wrong even xN values', xN.m;

****************************************************************************************************************
* Test for the syntax   MP(..., node(...).valFn$(succ(n,child)) yielding 0
****************************************************************************************************************
option clear = xN;
xN.lo(n) = -ord(n);
xN.up(n) =  ord(n);

embeddedCode ReSHOP:
* node(n)$(sum(succ(n,child),yes)): min objN(n) + MP('smax', node(child).valFn$(succ(n,child))) xN(n) defobjN(n)
node(n):   min objN(n) + CRM(n).valFn$(succ(n,child)) xN(n) defobjN(n)
CRM(n):    MP('ecvarup', node(child).valFn$(succ(n,child)), risk_wt=.8, tail=.4)
endEmbeddedCode

solve m_basic4 using emp;
abort$[m_basic4.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_basic4.modelStat;
abort$[m_basic4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_basic4.solveStat;

* Odd variables are at their upper bounds
* Even at their upper bounds
abort$[ smax{n$(mod(ord(n),2) = 0), abs(xN.l(n) - xN.lo(n))} > 1e-4] 'wrong even xN values', xN.l;
abort$[ smax{n$(mod(ord(n),2) = 1), abs(xN.l(n) - xN.up(n))} > 1e-4] 'wrong even xN values', xN.l;

* Check that the absolute marginal values sum to 2 at each stage
abort$[ smax{t, abs( sum[n$(ord(n) >= power(2, ord(t)-1) AND ord(n) < power(2, ord(t))), abs(xN.m(n)) ] - 2.) } > 1e-4] 'wrong even xN values', xN.m;

$exit

* To implement

VARIABLE y(t);
y.fx(t) = 2.;

embeddedCode ReSHOP:
node('t1'): min obj('t1') + node('t2').valFn         x('t1') y('t1') defobj('t1')
node('t2'): min obj('t2') + y('t1')*node('t3').valFn x('t2') y('t2') defobj('t2')
node('t3'): min obj('t3') + y('t2')*node('t4').valFn x('t3') y('t3') defobj('t3')
node('t4'): min obj('t4')                            x('t4') y('t4') defobj('t4')
endEmbeddedCode


solve m_dynamic_scalar_NL using emp;


