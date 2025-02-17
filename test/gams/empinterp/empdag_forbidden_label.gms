$onText
Simple model to test a few failures
1) keywords and UELs cannot be used as node names
2) Exercise a few code path reporting errors to the user
   1) variables after equations in MP declaration

Author: Olivier Huber <oli.huber@gmail.com>
Date: October 2024
$offText

* Do not print error on on stderr
$setenv RHP_DRIVER_SILENT 1

SET i /'one'/;

variables x(i), obj;
equation  defobj;

x.lo(i) = 1;

defobj.. obj =E= sqr(x('one') + 3);

model m / defobj /;

*****************************************************************************************
* 1) Test that keywords and UELs cannot be used as node names
*****************************************************************************************

file e / '%emp.info%' /;
putclose e 'dual: min obj x defobj';

solve m using emp;

abort$[m.modelStat <= %MODELSTAT.LOCALLY OPTIMAL%]      'solve should fail', m.modelStat;
abort$[m.solveStat  = %SOLVESTAT.NORMAL COMPLETION%]    'solve should fail', m.solveStat;

embeddedCode ReSHOP:
dual: min obj x defobj
endEmbeddedCode

* The embeddedCode should fail
abort$[execerror <> 1] 'embeddedCode should fail'; execerror = 0;

solve m using emp;

abort$[m.modelStat <= %MODELSTAT.LOCALLY OPTIMAL%]      'solve should fail', m.modelStat;
abort$[m.solveStat  = %SOLVESTAT.NORMAL COMPLETION%]   'solve should fail', m.solveStat;

embeddedCode ReSHOP:
kkt: min obj x defobj
endEmbeddedCode

* The embeddedCode should fail
abort$[execerror <> 1] 'embeddedCode should fail'; execerror = 0;

solve m using emp;

abort$[m.modelStat <= %MODELSTAT.LOCALLY OPTIMAL%]      'solve should fail', m.modelStat;
abort$[m.solveStat  = %SOLVESTAT.NORMAL COMPLETION%]   'solve should fail', m.solveStat;

putclose e 'one: min obj x defobj';

solve m using emp;

abort$[m.modelStat <= %MODELSTAT.LOCALLY OPTIMAL%]      'solve should fail', m.modelStat;
abort$[m.solveStat  = %SOLVESTAT.NORMAL COMPLETION%]   'solve should fail', m.solveStat;

embeddedCode ReSHOP:
one: min obj x defobj
endEmbeddedCode

* The embeddedCode should fail
abort$[execerror <> 1] 'embeddedCode should fail'; execerror = 0;

solve m using emp;

abort$[m.modelStat <= %MODELSTAT.LOCALLY OPTIMAL%]      'solve should fail', m.modelStat;
abort$[m.solveStat  = %SOLVESTAT.NORMAL COMPLETION%]   'solve should fail', m.solveStat;

*****************************************************************************************
* Tests small failures
*****************************************************************************************


* This should print a nice error message
putclose e 'node: min obj defobj x';

solve m using emp;

abort$[m.modelStat <= %MODELSTAT.LOCALLY OPTIMAL%]      'solve should fail', m.modelStat;
abort$[m.solveStat  = %SOLVESTAT.NORMAL COMPLETION%]   'solve should fail', m.solveStat;

embeddedCode ReSHOP:
node: min obj defobj x
endEmbeddedCode

* The embeddedCode should fail
abort$[execerror <> 1] 'embeddedCode should fail'; execerror = 0;

solve m using emp;

abort$[m.modelStat <= %MODELSTAT.LOCALLY OPTIMAL%]      'solve should fail', m.modelStat;
abort$[m.solveStat  = %SOLVESTAT.NORMAL COMPLETION%]   'solve should fail', m.solveStat;
