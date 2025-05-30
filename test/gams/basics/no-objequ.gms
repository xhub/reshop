$TITLE small example with no objective function

$onText
Test the reported objective variable value and marginals when there is no objective function

Author: Olivier Huber
$offText

PARAMETER tol / 1e-6 /;

VARIABLES theta, x;

SET i / i1*i2 /;

EQUATIONS absVal(i);

absVal(i).. (x+3)$sameas(i, 'i1') - (x+3)$sameas(i, 'i2') =L= theta;

MODEL m / absVal /;

SOLVE m using lp min theta;

SET attr / 'l', 'm' /;

PARAMETERS xV(attr), thetaV(attr), absV(i, attr);

xV('l') = x.l;
xV('m') = x.m;

thetaV('l') = theta.l;
thetaV('m') = theta.m;

absV(i, 'l') = absVal.l(i);
absV(i, 'm') = absVal.m(i);

embeddedCode ReSHOP:
min theta x absVal
endEmbeddedCode

solve m using emp;

abort$[m.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'unexpected model status', m.solveStat;
abort$[m.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'unexpected model status', m.modelStat;

abort$[ abs(xV('l') - x.l) > tol ] 'wrong x.l values';
abort$[ abs(xV('m') - x.m) > tol ] 'wrong x.m values';

abort$[ abs(thetaV('l') - theta.l) > tol ] 'wrong theta.l values';
abort$[ abs(thetaV('m') - theta.m) > tol ] 'wrong theta.m values';

abort$[ smax{i, abs(absV(i, 'l') - absVal.l(i))} > tol ] 'wrong absVal.l values';

abort$[ sum{i, absVal.m(i)} + 1 > tol ] 'wrong absVal.m values';


embeddedCode ReSHOP:
n: min theta x absVal
root: Nash(n)
endEmbeddedCode

solve m using emp;

abort$[m.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'unexpected model status', m.solveStat;
abort$[m.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'unexpected model status', m.modelStat;

abort$[ abs(xV('l') - x.l) > tol ] 'wrong x.l values';
abort$[ abs(xV('m') - x.m) > tol ] 'wrong x.m values';

abort$[ abs(thetaV('l') - theta.l) > tol ] 'wrong theta.l values';
abort$[ abs(thetaV('m') - theta.m) > tol ] 'wrong theta.m values';

abort$[ smax{i, abs(absV(i, 'l') - absVal.l(i))} > tol ] 'wrong absVal.l values';

abort$[ sum{i, absVal.m(i)} + 1 > tol ] 'wrong absVal.m values';


**********************************************************************************************
* MAX version
**********************************************************************************************

EQUATIONS negabsVal(i);

negabsVal(i).. (x+3)$sameas(i, 'i1') - (x+3)$sameas(i, 'i2') =G= theta;

MODEL mmax / negabsVal /;

SOLVE mmax using lp max theta;

xV('l') = x.l;
xV('m') = x.m;

thetaV('l') = theta.l;
thetaV('m') = theta.m;

absV(i, 'l') = negabsVal.l(i);
absV(i, 'm') = negabsVal.m(i);

embeddedCode ReSHOP:
max theta x negabsVal
endEmbeddedCode

solve mmax using emp;

abort$[mmax.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'unexpected model status', mmax.solveStat;
abort$[mmax.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'unexpected model status', mmax.modelStat;

abort$[ abs(xV('l') - x.l) > tol ] 'wrong x.l values';
abort$[ abs(xV('m') - x.m) > tol ] 'wrong x.m values';

abort$[ abs(thetaV('l') - theta.l) > tol ] 'wrong theta.l values';
abort$[ abs(thetaV('m') - theta.m) > tol ] 'wrong theta.m values';

abort$[ smax{i, abs(absV(i, 'l') - negabsVal.l(i))} > tol ] 'wrong absVal.l values';

abort$[ sum{i, negabsVal.m(i)} + 1 > tol ] 'wrong absVal.m values';


embeddedCode ReSHOP:
n: max theta x negabsVal
root: Nash(n)
endEmbeddedCode

solve mmax using emp;

abort$[mmax.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'unexpected model status', mmax.solveStat;
abort$[mmax.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'unexpected model status', mmax.modelStat;

abort$[ abs(xV('l') - x.l) > tol ] 'wrong x.l values';
abort$[ abs(xV('m') - x.m) > tol ] 'wrong x.m values';

abort$[ abs(thetaV('l') - theta.l) > tol ] 'wrong theta.l values';
abort$[ abs(thetaV('m') - theta.m) > tol ] 'wrong theta.m values';

abort$[ smax{i, abs(absV(i, 'l') - negabsVal.l(i))} > tol ] 'wrong absVal.l values';

abort$[ sum{i, negabsVal.m(i)} + 1 > tol ] 'wrong absVal.m values';
