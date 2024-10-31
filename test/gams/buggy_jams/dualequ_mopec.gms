$ontext

Usage: gams mopec_mathiesen.gms

A general equilibrium model is formulated as a MOPEC. The example is taken
from the following paper:

  Lars Mathiesen. An algorithm based on a sequence of linear complementarity
  problems applied to a Walrasian equilibrium model: an example.
  Mathematical Programming 37 (1987) 1-18.

In the paper, an explicit inverse demand function was used.
Here, we explicitly represent utility maximization problem to formulate the
problem as a MOPEC.

The solution is y* = (3), p* = (6,1,5), and x* = (3,2,0).

Contributors: Youngdae Kim (10.27.2016):   initial version
              Olivier Huber:               add invalid empinfos

$offtext

* Do not report failures on stderr
$setenv RHP_DRIVER_SILENT 1

set i commodities / 1*3 /;

parameters
    ATmat(i)  technology matrix  / 1 1  , 2 -1 , 3 -1 /
    s(i)      budget share       / 1 0.9, 2 0.1, 3 0 /
    b(i)      endowment          / 1 0  , 2 5  , 3 3 /;

variable u    utility of the consumer;
positive variables
    y         activity of the producer
    x(i)      Marshallian demand of the consumer
    p(i)      prices;

equations
    mkt(i)    constraint on excess demand
    profit    profit of activity
    udef      Cobb-Douglas utility function
    budget    budget constraint;

mkt(i)..
    b(i) + ATmat(i)*y - x(i) =G= 0;

profit..
    sum(i, -ATmat(i)*p(i)) =G= 0;

udef..
    u =E= sum(i, s(i)*log(x(i)));

budget..
    sum(i, p(i)*x(i)) =L= sum(i, p(i)*b(i));

model mopec / mkt, profit, udef, budget /;

file empinfo / '%emp.info%' /;
putclose empinfo
   'equilibrium' /
   'max u x udef budget' /
   'vi mkt p profit y' /;

* The second commodity is used as a numeraire.
p.fx('2') = 1;
p.lo(i) = 1;
x.l(i) = 1;

x.lo(i) = 1;

solve mopec using emp;

putclose empinfo
   'dualequ mkt p profit y' /;

mopec.iterlim = 0;
solve mopec using emp maximizing u;

putclose empinfo
   'dualequ 0 p profit y' /;

mopec.iterlim = 0;
solve mopec using emp maximizing u;

* Two issues: u is the objective variables and invalid dualequ statement
putclose empinfo
   'dualequ u mkt p profit y' /;
solve mopec using emp maximizing u;
abort$[mopec.modelStat = %MODELSTAT.LOCALLY OPTIMAL%] 'bad empinfo should be detected';
abort$[mopec.solveStat = %SOLVESTAT.NORMAL COMPLETION%] 'bad empinfo should be detected';

* invalid dualequ statement
putclose empinfo
   'dualequ u mkt p profit y' /;
solve mopec using emp maximizing u;
abort$[mopec.modelStat = %MODELSTAT.LOCALLY OPTIMAL%] 'bad empinfo should be detected';
abort$[mopec.solveStat = %SOLVESTAT.NORMAL COMPLETION%] 'bad empinfo should be detected';

* Dualequ is invalid in an equilibrium
putclose empinfo
   'equilibrium' /
   'max u x udef budget' /
   'dualequ mkt p profit y' /;
solve mopec using emp;
abort$[mopec.modelStat = %MODELSTAT.LOCALLY OPTIMAL%] 'bad empinfo should be detected';
abort$[mopec.solveStat = %SOLVESTAT.NORMAL COMPLETION%] 'bad empinfo should be detected';

* invalid dualequ statement: trailing equations
putclose empinfo
   'dualequ mkt p profit y udef' /;

solve mopec using emp;
abort$[mopec.modelStat = %MODELSTAT.LOCALLY OPTIMAL%] 'bad empinfo should be detected';
abort$[mopec.solveStat = %SOLVESTAT.NORMAL COMPLETION%] 'bad empinfo should be detected';

