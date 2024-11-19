$title Simple example of soft constraints  (SIMPENLP,SEQ=60)

$ontext
This model shows how to model soft constraints in EMP

min -x2 + 2 * max ( x1 + x2 + 0.5, 0)
s.t. x1 >= 0, x1 - 3*x2 = 2

Contributor: Michael C. Ferris, February 2011
$offtext


variables obj,x1,x2;
equations f0, f2;
variables maxz, v;
equations supp;

f0.. obj =e= -x2;
supp.. v*(x1 + x2 + 0.5) =e= maxz;
f2.. x1 - 3*x2 =e= 2;

x1.lo = 0;
v.lo = 0; v.up = 1;

embeddedCode ReSHOP:
n1: min obj + n2.valFn x1 x2 f0 f2
* support function
n2: max maxz v supp
root: Nash(n1,n2)
endembeddedCode

model enlpemp /all/;
solve enlpemp using emp;

positive variables w;
equation defobj, deflow;

defobj.. obj =e= -x2 + 2*w;
deflow.. w =g= x1 + x2 + 0.5;

model equivlp /defobj,f2,deflow/;

solve equivlp using lp min obj;

abort$(enlpemp.objval <> equivlp.objval) 'Objval differs', enlpemp.objval, equivlp.objval;
