$title missing EMPinfo file
$onText
Gracefully handles missing empinfo file
$offText

Positive Variable x;
Variable obj;
Equation objdef;

objdef.. obj =E= x;

model m / all /;

solve m using emp;
abort$[m.solveStat <> %SOLVESTAT.SYSTEM FAILURE%]       'solve failed', m.solveStat;
abort$[m.modelStat <> %MODELSTAT.ERROR NO SOLUTION%]   'solve failed', m.modelStat;
