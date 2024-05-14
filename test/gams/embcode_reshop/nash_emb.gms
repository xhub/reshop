$ontext
Basic test for embeddedCode ReSHOP

Author: Olivier Huber, based on a previous work of Jiajie Shen
Date: May 2024
$offtext

set
  t /0*1/,
  a /a1*a2/;
scalar
  beta / 7 /,
  alpha / 6 /;
variables x(a,t);
variables obj(a);
equations defobj(a), cons(a);

defobj(a)..
  obj(a) =e=
  ((beta/2*sqr(x('a1','0')) - alpha*x('a1','0'))
    + (1/2*sqr(x('a1','1')) + 3*x('a1','1')*x('a2','1') - 4*x('a1','1'))
  )$sameas(a,'a1')
  + (x('a2','0')
    + (1/2*sqr(x('a2','1')) + x('a1','1')*x('a2','1') - 3*x('a2','1'))
  )$sameas(a,'a2');

x.lo('a1','0') = 0;
x.fx('a2','0') = 0;

cons(a)..
  (x('a1','1') - x('a1','0'))$sameas(a,'a1') + (x('a2','1'))$sameas(a,'a2') =g= 0;

model nash / defobj, cons /;

$onEmbeddedCode ReSHOP:
n(a): min obj(a) x(a,'*') defobj(a) cons(a)
root: Nash(n(a))
$offEmbeddedCode

solve nash using emp;

abort$[nash.modelStat > %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', nash.modelStat;
abort$[nash.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', nash.solveStat;
