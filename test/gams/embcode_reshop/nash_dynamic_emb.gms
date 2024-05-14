$ontext
$offtext

set
  t /0*1/,
  a /a1*a2/;
scalar
  beta / 7 /,
  alpha / 6 /;
variables x(a,t);
variables obj(a,t);
equations defobj(a,t), cons(a);

defobj(a,t)..
  obj(a,t) =e=
  ((beta/2*sqr(x('a1','0')) - alpha*x('a1','0'))$sameas(t,'0')
    + (1/2*sqr(x('a1','1')) + 3*x('a1','1')*x('a2','1') - 4*x('a1','1'))$sameas(t,'1')
  )$sameas(a,'a1')
  + (x('a2','0')$sameas(t,'0')
    + (1/2*sqr(x('a2','1')) + x('a1','1')*x('a2','1') - 3*x('a2','1'))$sameas(t,'1')
  )$sameas(a,'a2');

x.lo('a1','0') = 0;
x.fx('a2','0') = 0;

cons(a)..
  (x('a1','1') - x('a1','0'))$sameas(a,'a1') + (x('a2','1'))$sameas(a,'a2') =g= 0;

model nash / defobj, cons /;

$onEmbeddedCode ReSHOP:
n(a,'0'): min obj(a,'0')+n.valfn(a,'1') x(a,'0') defobj(a,'0')
n(a,'1'): min obj(a,'1') x(a,'1') defobj(a,'1') cons(a)
root: Nash(n(a,'0'))
$offEmbeddedCode

nash.justscrdir = 1;
solve nash using emp;

abort$[nash.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', nash.solveStat;
* abort$[nash.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', nash.modelStat;

$exit

cons(a)..
  x('a1','1') - x('a1','0') =g= 0;

model nash / defobj, cons /;

$onEmbeddedCode ReSHOP:
n(a,'0'): min obj(a,'0')+n.valfn(a,'1') x(a,'0') defobj(a,'0')
n(a,'1'): min obj(a,'1') x(a,'1') defobj(a,'1') cons(a)$sameas(a,'a1')
root: Nash(n(a,'0'))
$offEmbeddedCode

