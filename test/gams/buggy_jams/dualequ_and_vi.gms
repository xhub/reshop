$title test bogus dualequ and vi statement

$ontext


Contributor: Olivier Huber, from the EMPLIB example zerofunc.18

$offtext

* Do not report failures on stderr
$setenv RHP_DRIVER_SILENT 1

free variables y, z;
free variable obj;
equation objDef, gCons;

objDef..  obj =E= power(y,3) / 3;
gCons..   y   =G= sqr(z);

equations
  F_y 'dfdy'
  F_z 'dfdz'
  ;

F_y.. sqr(y) =N= 0;
F_z.. 0 =N= 0;

model mVIbug / F_z, F_y, gCons, objDef /;

file empinfo / '%emp.info%' /;

* invalid VI: do not start with equation
putclose empinfo  'vi  gCons F_z z  gCons';
solve mVIbug using emp minimizing obj;
abort$[mVIbug.modelStat = %MODELSTAT.LOCALLY OPTIMAL%] 'bad empinfo should be detected';
abort$[mVIbug.solveStat = %SOLVESTAT.NORMAL COMPLETION%] 'bad empinfo should be detected';

* invalid VI: do not end with variable
putclose empinfo  'vi  F_z z  gCons y obj';
solve mVIbug using emp minimizing obj;
abort$[mVIbug.modelStat = %MODELSTAT.LOCALLY OPTIMAL%] 'bad empinfo should be detected';
abort$[mVIbug.solveStat = %SOLVESTAT.NORMAL COMPLETION%] 'bad empinfo should be detected';


* invalid VI: do not end with variable
putclose empinfo  'vi equilibrium F_z z  gCons y obj';
solve mVIbug using emp minimizing obj;
abort$[mVIbug.modelStat = %MODELSTAT.LOCALLY OPTIMAL%] 'bad empinfo should be detected';
abort$[mVIbug.solveStat = %SOLVESTAT.NORMAL COMPLETION%] 'bad empinfo should be detected';

* vi then dualequ
putclose empinfo  'vi  F_z z  gCons objDef' /
                  'dualequ F_y y';
solve mVIbug using emp minimizing obj;
abort$[mVIbug.modelStat = %MODELSTAT.LOCALLY OPTIMAL%] 'bad empinfo should be detected';
abort$[mVIbug.solveStat = %SOLVESTAT.NORMAL COMPLETION%] 'bad empinfo should be detected';

* dualequ then vi
putclose empinfo  'dualequ F_y y' /
                  'vi  F_z z  gCons objDef';
solve mVIbug using emp minimizing obj;
abort$[mVIbug.modelStat = %MODELSTAT.LOCALLY OPTIMAL%] 'bad empinfo should be detected';
abort$[mVIbug.solveStat = %SOLVESTAT.NORMAL COMPLETION%] 'bad empinfo should be detected';
