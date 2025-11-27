$ontext

The example is taken from

  Lars Mathiesen. An algorithm based on a sequence of linear complementarity
  problems applied to a Walrasian equilibrium model: an example.
  Mathematical Programming 37 (1987) 1-18.

In the paper, an explicit inverse demand function was used.
Here, we explicitly represent utility maximization problem to formulate the
problem as a MOPEC.

The solution is y* = (3), p* = (6,1,5), and x* = (3,2,0).

Author: Youngdae Kim (10.27.2016)

$offtext

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

* The second commodity is used as a numeraire.
p.fx('2') = 1;

x.l(i) = 1;

* To change the solution
* p.lo(i) = 1;
* x.lo(i) = 1;

mopec.justscrdir = 1;
solve mopec using emp;

EmbeddedCode ReSHOP:
producer: max u x udef budget
cc: vi mkt p profit y
root: Nash(producer, cc)
endEmbeddedCode

EmbeddedCode Python:
import reshop as rhp
import IPython
IPython.embed(colors="neutral")


m = rhp.Model(r"%gams.scrdir%gamscntr.%gams.scrext%")
m.solve(r"%gams.scrdir%savepoint.gdx")

# Uncomment to start IPython
endEmbeddedCode

execute_loadpoint '%gams.scrdir%savepoint.gdx';

* abort$(mopec.solvestat <> %SOLVESTAT.NORMAL COMPLETION%) "Bad solvestat"
* abort$(mopec.modelstat > 2)                              "Bad modelstat"

$if not set tol $set tol 1e-6
scalar tol / %tol% /;

display y.l, x.l, p.l;

abort$[ abs(y.l      - 3) > tol ] 'bad y.l', y.l;
abort$[ abs(p.l('1') - 6) > tol ] 'bad p.l("1")', p.l;
abort$[ abs(p.l('2') - 1) > tol ] 'bad p.l("2")', p.l;
abort$[ abs(p.l('3') - 5) > tol ] 'bad p.l("3")', p.l;
abort$[ abs(x.l('1') - 3) > tol ] 'bad x.l("1")', x.l;
abort$[ abs(x.l('2') - 2) > tol ] 'bad x.l("2")', x.l;
abort$[ abs(x.l('3') - 0) > tol ] 'bad x.l("3")', x.l;
