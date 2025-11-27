$TITLE PIES Energy Equilibrium (PIES,SEQ=56)

$ontext
A linear program with a variable rhs in the constraint system
is expressed as a complementarity problem.

LP:     min     <c,x>
               Ax >= q(p),
        s.t.   Bx  = b,
                x >= 0

where the prices p are the duals to the first constraint.

MCP:           A'p + B'v + c >= 0, x >= 0, comp.
        -Ax + q(p)           >= 0, p >= 0, comp.
        -Bx              + b  = 0, v free, comp.

Of course, the variables x and v are going to be
split up further in the GAMS model.

References:
William W. Hogan, Energy Policy Models for Project Independence,
Computers \& Operations Research (2), 1975.

N. Josephy, A Newton Method for the PIES energy model,
Tech Report, Mathematics Research Center, UW-Madison, 1979.

S.P. Dirkse and M.C. Ferris, "MCPLIB: A Collection of Nonlinear Mixed
Complementarity Problems", Optimization Methods in Software 5 (1995), 319-345.

Contributor: Michael Ferris & Steven Dirkse, October 2010
$offtext


SETS
  comod /
    C  'coal'
    L  'light oil'
    H  'heavy oil'
  /
  R  'resources' /
    C  'capital'
    S  'steel'
  /
  creg   'coal producing regions'         / 1 * 2 /
  oreg   'crude oil producing regions'    / 1 * 2 /
  ctyp   'increments of coal production'  / 1 * 3 /
  otyp   'increments of oil production'   / 1 * 2 /
  refin  'refineries'                     / 1 * 2 /
  users  'consumption regions'            / 1 * 2 /
  ;
alias (comod,cc);

PARAMETERS
  rmax(R) 'maximum resource usage' /
    C       35000
    S       12000
  /
  cmax(creg,ctyp) 'coal prod. limits' /
    1.1     300
    1.2     300
    1.3     400
    2.1     200
    2.2     300
    2.3     600
  /
  omax(oreg,otyp) 'oil prod. limits' /
    1.1     1100
    1.2     1200
    2.1     1300
    2.2     1100
  /
  rcost(refin) 'refining cost' /
    1       6.5
    2       5
  /
  q0(comod) 'base demand for commodities' /
    C       1000
    L       1200
    H       1000
  /
  p0(comod) 'base prices for commodities' /
    C       12
    L       16
    H       12
  /
  demand(comod,users) 'computed at optimality'
  output(refin,*)  '% output of light/heavy oil' /
    1.L     .6
    1.H     .4
    2.L     .5
    2.H     .5
  /
  ;
TABLE esub(comod,cc)    'cross-elasticities of substitution'
        C       L       H
C      -.75     .1      .2
L       .1     -.5      .2
H       .2      .1     -.5 ;

TABLE cruse(R,creg,ctyp) 'resource use in coal prod'
        1       2       3
C.1     1       5      10
C.2     1       5       6
S.1     1       2       3
S.2     1       4       5 ;

table oruse(R,oreg,otyp) 'resource use in oil prod'
        1       2
C.1     0      10
C.2     0      15
S.1     0       4
S.2     0       2 ;

table ccost(creg,ctyp)  'coal prod. cost'
        1       2       3
1       5       6       8
2       4       5       7 ;

table ocost(oreg,otyp)  'oil prod. cost'
        1       2
1       1       1.5
2       1.25    1.5 ;

table ctcost(creg,users)
        1       2
1       1       2.5
2       .75     2.75 ;

table otcost(oreg,refin)
        1       2
1       2       3
2       4       2 ;

table ltcost(refin,users)  'light oil trans costs'
        1       2
1       1       1.2
2       1       1.5 ;

table htcost(refin,users)  'heavy oil trans costs'
        1       2
1       1       1.2
2       1       1.5 ;


positive variables
  c(creg,ctyp)           'coal production'
  o(oreg,otyp)           'oil production'
  ct(creg,users)         'coal transportation levels'
  ot(oreg,refin)         'crude oil transportation levels'
  lt(refin,users)        'light transportation levels'
  ht(refin,users)        'heavy transportation levels'
  p(comod,users)         'commodity prices'
  ;
c.up(creg,ctyp) = cmax(creg,ctyp);
o.up(oreg,otyp) = omax(oreg,otyp);

equations
  dembal(comod,users)  'excess supply of product'
  cmbal(creg)          'coal material balance'
  ombal(oreg)          'oil material balance'
  lmbal(refin)         'light material balance'
  hmbal(refin)         'heavy material balance'
  ruse(R)              'resource use constraints'
  ;

dembal(comod,users) ..
      (sum(creg,ct(creg,users)))$[sameas(comod,'C')]
  + (sum(refin,lt(refin,users)))$[sameas(comod,'L')]
  + (sum(refin,ht(refin,users)))$[sameas(comod,'H')]
  =g=
  q0(comod) * prod(cc, (p(cc,users)/p0(cc))**esub(comod,cc));

cmbal(creg) ..
  sum(ctyp,c(creg,ctyp))  =e=  sum(users,ct(creg,users));

ombal(oreg) ..
  sum(otyp,o(oreg,otyp))  =e=  sum(refin,ot(oreg,refin));

lmbal(refin) ..
  sum(oreg, ot(oreg,refin)) * output(refin,"L") =e=
  sum(users,lt(refin,users));

hmbal(refin) ..
  sum(oreg, ot(oreg,refin)) * output(refin,"H") =e=
  sum(users,ht(refin,users));

ruse(R) ..
  rmax(R) =g=
  sum(creg, sum(ctyp, c(creg,ctyp)*cruse(R,creg,ctyp)))
  + sum(oreg, sum(otyp, o(oreg,otyp)*oruse(R,oreg,otyp)));

option limrow = 0;
option limcol = 0;
option iterlim = 1000;
option reslim = 120;

table i_c(creg,ctyp)
        1       2       3
1       300     300     400
2       200     300     600 ;

table i_o(oreg,otyp)
        1       2
1       1100    1000
2       1300    1000 ;

table i_ct(creg,users)  'initial trans'
        1       2
1       0       828
2       1016    84 ;

table i_ot(oreg,refin)  'initial trans'
        1       2
1       2075    0
2       0       2358 ;

table i_lt(refin,users) 'initial trans'
        1       2
1       22      1223
2       1179    0 ;

table i_ht(refin,users) 'initial trans'
        1       2
1       0       830
2       998     180 ;

table iprice(comod,users) 'initial price estimate'
        1       2
C       11.7    13.7
L       15.8    16.0
H       11.9    12.4 ;

c.l(creg,ctyp) = i_c(creg,ctyp);
o.l(oreg,otyp) = i_o(oreg,otyp);
ct.l(creg,users) = i_ct(creg,users);
ot.l(oreg,refin) = i_ot(oreg,refin);
lt.l(refin,users) = i_lt(refin,users);
ht.l(refin,users) = i_ht(refin,users);
p.lo(comod,users) = .1;
* p.fx(comod,users) = iprice(comod,users);
p.l(comod,users) = iprice(comod,users);

* initialize the multipliers.....
cmbal.m(creg) = 1;
ombal.m(oreg) = 1;
lmbal.m(refin) = 1;
hmbal.m(refin) = 1;
ruse.m(R) = 1;

variables obj;
equation defobj;

defobj.. obj =E= sum((creg,ctyp), ccost(creg,ctyp)*c(creg,ctyp))
         + sum((oreg,otyp), ocost(oreg,otyp)*o(oreg,otyp))
         + sum((creg,users), ctcost(creg,users)*ct(creg,users))
         + sum((oreg,refin), (otcost(oreg,refin) + rcost(refin))*ot(oreg,refin))
         + sum((refin,users), ltcost(refin,users)*lt(refin,users))
         + sum((refin,users), htcost(refin,users)*ht(refin,users));

model piesemp / defobj, dembal, cmbal, ombal, lmbal, hmbal, ruse /;

file myinfo /'%emp.info%'/;
putclose myinfo 'dualVar p dembal';

file jams_opt /'jams.opt'/;
putclose jams_opt 
                  'fileName PIESmin.gms' /
                  'dict PIESminDict.txt';

solve piesemp using emp min obj;
display  p.l, dembal.m;
abort$[piesemp.solveStat <> %SOLVESTAT.NORMAL COMPLETION%] 'Bad solvestat';
abort$[piesemp.modelStat > %MODELSTAT.LOCALLY OPTIMAL%] 'bad modelstat';
abort$[smax((comod,users), abs(p.l(comod,users)-dembal.m(comod,users))) > 1e-10] 'ERROR in piesemp: p.l != dembal.m';

* ----------------------------------------------------------------------
* OH: let's test the MIN formulation with flipping
* ----------------------------------------------------------------------


EQUATION dembalFlipped 'negative of dembal';

dembalFlipped(comod,users) ..
  q0(comod) * prod(cc, (p(cc,users)/p0(cc))**esub(comod,cc))
  =l=
  (sum(creg,ct(creg,users)))$[sameas(comod,'C')]
  + (sum(refin,lt(refin,users)))$[sameas(comod,'L')]
  + (sum(refin,ht(refin,users)))$[sameas(comod,'H')] ;

MODEL piesempFlipped /piesemp+dembalFlipped-dembal/;

putclose myinfo 'dualVar p -dembalFlipped';


putclose jams_opt 
                  'fileName PIESminFlipped.gms' /
                  'dict PIESminFlippedDict.txt';
piesempFlipped.iterlim = 0;
solve piesempFlipped using emp min obj;
abort$[piesempFlipped.solveStat <> %SOLVESTAT.NORMAL COMPLETION%] 'Bad solvestat';
abort$[piesempFlipped.modelStat > %MODELSTAT.LOCALLY OPTIMAL%] 'bad modelstat';
abort$[smax((comod,users), abs(p.l(comod,users)+dembalFlipped.m(comod,users))) > 1e-10] 'ERROR piesempFlipped: p.l != -dembalFlipped.m (dembalFlipped was flipped)';

* ----------------------------------------------------------------------
* OH: let's test the MAX formulation
* ----------------------------------------------------------------------

VARIABLE objMax;
EQUATION defobjMax;

defobjMax.. -objMax =E= sum((creg,ctyp), ccost(creg,ctyp)*c(creg,ctyp))
         + sum((oreg,otyp), ocost(oreg,otyp)*o(oreg,otyp))
         + sum((creg,users), ctcost(creg,users)*ct(creg,users))
         + sum((oreg,refin), (otcost(oreg,refin) + rcost(refin))*ot(oreg,refin))
         + sum((refin,users), ltcost(refin,users)*lt(refin,users))
         + sum((refin,users), htcost(refin,users)*ht(refin,users));

model piesempMax / piesemp+defobjMax-defobj /;


putclose myinfo 'dualVar p dembal';


putclose jams_opt 
                  'fileName PIESmax.gms' /
                  'dict PIESmaxDict.txt';

objMax.l = -obj.l;
*p.m(comod,users) = -p.m(comod,users);

c.m(creg,ctyp) = -c.m(creg,ctyp);
o.m(oreg,otyp) = -o.m(oreg,otyp);
ct.m(creg,users) = -ct.m(creg,users);
ot.m(oreg,refin) = -ot.m(oreg,refin);
lt.m(refin,users) = -lt.m(refin,users);
ht.m(refin,users) = -ht.m(refin,users);



*$ifThenI %system.emp% == "jams"
cmbal.m(creg) = -cmbal.m(creg);
ombal.m(oreg) = -ombal.m(oreg);
lmbal.m(refin) = -lmbal.m(refin);
hmbal.m(refin) = -hmbal.m(refin);
ruse.m(R) = -ruse.m(R);
*$endIf


* option nlp = examiner;
* Solve piesempMax using nlp max objMax;


* TODO: fix this
*piesempMax.iterlim = 0;
*option mcp=convert;
solve piesempMax using emp max objMax;
abort$[piesempMax.solveStat <> %SOLVESTAT.NORMAL COMPLETION%] 'Bad solvestat';
abort$[piesempMax.modelStat > %MODELSTAT.LOCALLY OPTIMAL%] 'bad modelstat';

abort$[smax((comod,users), abs(p.l(comod,users)+dembal.m(comod,users))) > 1e-10] 'ERROR in piesempMax: p.l != -dembal.m (dembal was flipped)';


* ----------------------------------------------------------------------
* now, a max version without flipping
* ----------------------------------------------------------------------

* smart init
dembalFlipped.m(comod,users) = -dembal.m(comod,users);

model piesempMaxFlipped / piesempMax+dembalFlipped-dembal /;
putclose myinfo 'dualVar p dembalFlipped';


putclose jams_opt 
                  'fileName PIESmaxDembalFlipped.gms' /
                  'dict PIESmaxDembalFlippedDict.txt';


piesempMaxFlipped.iterlim = 0;
solve piesempMaxFlipped using emp max objMax;
abort$[piesempMaxFlipped.solveStat <> %SOLVESTAT.NORMAL COMPLETION%] 'Bad solvestat';
abort$[piesempMaxFlipped.modelStat > %MODELSTAT.LOCALLY OPTIMAL%] 'bad modelstat';

display  p.l, dembalFlipped.m;

abort$[smax((comod,users), abs(p.l(comod,users)-dembalFlipped.m(comod,users))) > 1e-10] 'ERROR in piesempMaxFlipped: p.l != dembalFlipped.m';
