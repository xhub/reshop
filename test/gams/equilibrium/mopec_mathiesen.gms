$ontext

The example is taken from

  Lars Mathiesen. An algorithm based on a sequence of linear complementarity
  problems applied to a Walrasian equilibrium model: an example.
  Mathematical Programming 37 (1987) 1-18.

In the paper, an explicit inverse demand function was used.
Here, we explicitly represent utility maximization problem to formulate the
problem as a MOPEC.

The solution is y* = (3), p* = (6,1,5), and x* = (3,2,0).

Contributor: Youngdae Kim (10.27.2016)

OH: Add variants for testing purposes
$offtext

set i commodities / 1*3 /;

parameters
    ATmat(i)  'technology matrix'  / 1 1  , 2 -1 , 3 -1 /
    s(i)      'budget share'       / 1 0.9, 2 0.1, 3 0 /
    b(i)      'endowment'          / 1 0  , 2 5  , 3 3 /;

variable u    'utility of the consumer';

positive variables
    y         'activity of the producer'
    x(i)      'Marshallian demand of the consumer'
    p(i)      'prices';

equations
    mkt(i)    'constraint on excess demand'
    profit    'profit of activity'
    udef      'Cobb-Douglas utility function'
    budget    'budget constraint';

mkt(i)..
    b(i) + ATmat(i)*y - x(i) =G= 0;

profit..
    sum(i, -ATmat(i)*p(i)) =G= 0;

udef..
    u =E= sum(i, s(i)*log(x(i)));

budget..
    sum(i, p(i)*x(i)) =L= sum(i, p(i)*b(i));

model mopec / mkt, profit, udef, budget /;

* The second commodity is used as a numeraire.
p.fx('2') = 1;
p.lo(i) = 1;
x.l(i) = 1;

x.lo(i) = 1;

* Old style empinfo
file empinfo / '%emp.info%' /;
putclose empinfo
   'equilibrium' /
   'max u x udef budget' /
   'vi mkt p profit y' /;

solve mopec using emp;
abort$[mopec.modelStat <> %MODELSTAT.LOCALLY OPTIMAL%]    'bad model status', mopec.modelStat;
abort$[mopec.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]  'bad solve status';
abort$[mopec.objVal > 1e-6]                               'bad residual';

* empdag version
putclose empinfo
   'a1: max u x udef budget' /
   'v1: vi mkt p profit y' /
   'root: Nash(a1, v1)'/;

* GAMS has no concept of multiplier :(
budget.m = -budget.m;

mopec.iterlim = 0;

solve mopec using emp;
abort$[mopec.modelStat <> %MODELSTAT.LOCALLY OPTIMAL%]    'bad model status', mopec.modelStat;
abort$[mopec.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]  'bad solve status';
abort$[mopec.objVal > 1e-6]                               'bad residual';


* Variant: put Nash keyword first
putclose empinfo
   'root: Nash(a1, v1)'/
   'a1: max u x udef budget' /
   'v1: vi mkt p profit y' /
   ;

* GAMS has no concept of multiplier :(
budget.m = -budget.m;

solve mopec using emp;
abort$[mopec.modelStat <> %MODELSTAT.LOCALLY OPTIMAL%]    'bad model status';
abort$[mopec.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]  'bad solve status';
abort$[mopec.objVal > 1e-6]                               'bad residual';

* Variant: old empinfo, but no equilibrium keyword
putclose empinfo
   'max u x udef budget' /
   'vi mkt p profit y' /;

* GAMS has no concept of multiplier :(
budget.m = -budget.m;

solve mopec using emp;
abort$[mopec.modelStat <> %MODELSTAT.LOCALLY OPTIMAL%]    'bad model status';
abort$[mopec.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]  'bad solve status';
abort$[mopec.objVal > 1e-6]                               'bad residual';
