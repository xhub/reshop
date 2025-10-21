$Title Simple 4 stage dynamic example

$onText
Dynamic 4 stage 

Author: Olivier Huber, October 2025
$offText

SET t stages / t1*t4 /;

alias (t,s);

SET succ(t,s);
succ(t,s) = yes$(ord(s) = ord(t)+1);

PARAMETERS c(t);

c(t) = power(-1, ord(t))*2;

VARIABLES x(t),
          obj(t);

EQUATIONS defobj(t);

x.lo(t) = -ord(t);
x.up(t) =  ord(t);


defobj(t).. obj(t) =E= c(t)*x(t);

embeddedCode ReSHOP:
node('t1'): min obj('t1') + node('t2').valFn x('t1') defobj('t1')
node('t2'): min obj('t2') + node('t3').valFn x('t2') defobj('t2')
node('t3'): min obj('t3') + node('t4').valFn x('t3') defobj('t3')
node('t4'): min obj('t4')                    x('t4') defobj('t4')
endEmbeddedCode


model m_dynamic_scalar_lin /all/;

solve m_dynamic_scalar_lin using emp;

abort$[m_dynamic_scalar_lin.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_dynamic_scalar_lin.modelStat;
abort$[m_dynamic_scalar_lin.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_dynamic_scalar_lin.solveStat;

EQUATIONS defobjNL(t);

c(t) = ord(t);

defobjNL(t).. obj(t) =E= c(t)*power(x(t), ord(t));

embeddedCode ReSHOP:
node('t1'): min obj('t1') + node('t2').valFn x('t1') defobjNL('t1')
node('t2'): min obj('t2') + node('t3').valFn x('t2') defobjNL('t2')
node('t3'): min obj('t3') + node('t4').valFn x('t3') defobjNL('t3')
node('t4'): min obj('t4')                    x('t4') defobjNL('t4')
endEmbeddedCode

model m_dynamic_scalar_NL /defobjNL/;

solve m_dynamic_scalar_NL using emp;

abort$[m_dynamic_scalar_NL.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m_dynamic_scalar_NL.modelStat;
abort$[m_dynamic_scalar_NL.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m_dynamic_scalar_NL.solveStat;

$exit

* TODO
embeddedCode ReSHOP:
node(t): min obj(t) + node(s).valFn$(sum(succ(t,s), yes)) x(t)
endEmbeddedCode
