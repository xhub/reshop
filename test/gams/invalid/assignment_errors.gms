$TITLE test whether assignment errors are detected

$onText

Basic tests that unassigned variables and equations are assigned

Author: Olivier Huber <oli.huber@gmail.com>
Date:   October 2025
$offText

$if not set tol $set tol 1e-5

SET i / i1*i4 /;

VARIABLES x(i) /#i.lo = 1./ , obj(i);

EQUATIONS defobj(i), defobj1, defobj4;
defobj(i).. x(i) =E= obj(i);

model m / defobj /;

* Only obj('i1') x('i1') defobj('i1') are assigned
embeddedCode ReSHOP:
n(i)$(i.first): min obj(i) x(i) defobj(i)
endEmbeddedCode

solve m using emp;

abort$[m.solveStat <> %SOLVESTAT.SETUP FAILURE%]   'solve failed', m.solveStat;
abort$[m.modelStat <> %MODELSTAT.ERROR NO SOLUTION%]   'solve failed', m.modelStat;

* x is not assigned
embeddedCode ReSHOP:
n(i): min obj(i) defobj(i)
root: Nash(n(i))
endEmbeddedCode

solve m using emp;

abort$[m.solveStat <> %SOLVESTAT.SETUP FAILURE%]   'solve failed', m.solveStat;
abort$[m.modelStat <> %MODELSTAT.ERROR NO SOLUTION%]   'solve failed', m.modelStat;

* defobj is not assigned
embeddedCode ReSHOP:
n(i): min obj(i) x(i) 
root: Nash(n(i))
endEmbeddedCode

solve m using emp;

abort$[m.solveStat <> %SOLVESTAT.SETUP FAILURE%]   'solve failed', m.solveStat;
abort$[m.modelStat <> %MODELSTAT.ERROR NO SOLUTION%]   'solve failed', m.modelStat;
