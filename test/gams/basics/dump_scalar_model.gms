$onText
Simple model to test ReSHOP options

Author: Olivier Huber <oli.huber@gmail.com>
$offText

variables x, obj;
equation  defobj;

x.lo = 1;

defobj .. obj =E= sqr(x + 3);

model m / defobj /;

file e / '%emp.info%' /;
putclose e 'root: min obj x defobj';

file or / '%gams.emp%.opt' /;
putclose or 'dump_scalar_model 1';

m.optfile = 1;

solve m using emp;

abort$[m.modelStat >  %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m.modelStat;
abort$[m.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m.solveStat;
