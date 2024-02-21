$title Simple Equilibrium with external constraint (SIMPEQUIL2,SEQ=63)

$ontext
From SIMPEQUIL2.63 by Michael Ferris

-- 
Olivier Huber
$offtext


option limrow=0, limcol = 0;

variables y;
positive variables x;
equations optcons, vicons;

optcons.. x + y =l= 1;

vicons.. -3*x + y =e= 0.5;

model comp / optcons, vicons /;

file info / '%emp.info%' /;
putclose info /
 '* this is a test' /
 '   ' /
 '         * another test' /
'equilibrium' /
' *now define the opt problem' /
 'max x optcons' /
 '* and now the VI' /
 'vi vicons y';

solve comp using emp;
abort$[comp.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'modelstat is wrong';
abort$[comp.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed';
abort$[comp.objVal     > 1e-6]                            'residual is wrong', comp.objVal;
