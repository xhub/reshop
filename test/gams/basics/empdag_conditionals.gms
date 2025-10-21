$TITLE test EMPDAG conditionals

$onText

Author: Olivier Huber <oli.huber@gmail.com>
Date:   October 2025
$offText

$if not set tol $set tol 1e-5

SET i / i1*i4 /, j(i);
j('i1') = yes;

VARIABLES x(i) /#i.lo = 1./ , obj(i), xx(i);

EQUATIONS defobj(i), defobj1, defobj4;
defobj1.. x('i1') =E= obj('i1');
defobj4.. x('i4') =E= obj('i4');
defobj(i).. x(i) =E= obj(i);

model m1 / defobj1 /;
model m4 / defobj4 /;
model m / defobj /;


embeddedCode ReSHOP:
n(i)$(i.first): min obj(i) x(i) defobj1
endEmbeddedCode

solve m1 using emp;

abort$[m1.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m1.solveStat;
abort$[m1.modelStat <> %MODELSTAT.OPTIMAL%]             'solve failed', m1.modelStat;

xx.l(i) = x.l(i);
xx.m(i) = x.m(i);

embeddedCode ReSHOP:
n(i)$(sameAs(i, 'i1')): min obj(i) x(i) defobj1
endEmbeddedCode

solve m1 using emp;
abort$[m1.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m1.solveStat;
abort$[m1.modelStat <> %MODELSTAT.OPTIMAL%]   'solve failed', m1.modelStat;

abort$[smax{i, abs(x.l(i) - xx.l(i)) } > %tol% ]   'solve failed';

* m4

embeddedCode ReSHOP:
n(i)$(sameAs(i, 'i4')): min obj(i) x(i) defobj4
endEmbeddedCode

solve m4 using emp;
abort$[m4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m4.solveStat;
abort$[m4.modelStat <> %MODELSTAT.OPTIMAL%]   'solve failed', m4.modelStat;

xx.l(i) = x.l(i);
xx.m(i) = x.m(i);

display x.l;

embeddedCode ReSHOP:
n(i)$(i.last): min obj(i) x(i) defobj4
endEmbeddedCode

solve m4 using emp;

abort$[m4.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m4.solveStat;
abort$[m4.modelStat <> %MODELSTAT.OPTIMAL%]             'solve failed', m4.modelStat;

abort$[smax{i, abs(x.l(i) - xx.l(i)) } > %tol% ]   'solve failed';

$exit


SET n / n1*n15 /;

ALIAS(n,child);

SET succ(n,child), leaf(n);

succ(n,child) = yes$((2*ord(n) EQ ord(child)) OR (2*ord(n) +1 EQ ord(child)) );

loop(succ(n,child),
   leaf(n) = yes;
);
* leaf(n) = yes$(NOT sum(child, sn(n,child)));


