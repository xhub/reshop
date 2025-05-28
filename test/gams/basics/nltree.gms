$ TITLE test nltree

$if %system.Platform%==WEX $exit

SET i /1*4/;

VARIABLE x1, x2, x3, x4, obj;

x1.lo = 1;
x1.up = 2;
x2.lo = 2;
x2.up = 3;
x3.lo = 4;
x3.up = 5;
x4.lo = 6;
x4.up = 7;


SCALAR cst / 0.5 /;

$if not set TESTTOL $set TESTTOL 1e-8
SCALAR tol / %TESTTOL% /;

put_utility 'save' / 'test-nltree';

* We can't get the subsitution to work 
$onEchoV > %gams.scrdir%subgms.in
EQUATION defobj, defobj2;

PARAMETER xvalL(i), xvalM(i), objval;

PARAMETERS xrhpL(i), xrhpM(i);

* conopt fails with iterlim=0
option nlp=pathnlp;

defobj..  obj =E= XXX; 
defobj2.. XXX =E= obj; 

model m / defobj /; 
model mEMP / defobj /; 
mEMP.iterlim = 0; 

solve m using nlp min obj;

xvalL('1')  = x1.l;
xvalL('2')  = x2.l;
xvalL('3')  = x3.l;
xvalL('4')  = x4.l;
xvalM('1')  = x1.m;
xvalM('2')  = x2.m;
xvalM('3')  = x3.m;
xvalM('4')  = x4.m;
objval      = obj.l; 

embeddedCode ReSHOP:
root: min obj x1 x2 x3 x4 defobj
endEmbeddedCode

solve mEMP using emp;
abort$[mEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mEMP.solveStat; 
abort$[mEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mEMP.modelStat; 

xrhpL('1')  = x1.l;
xrhpL('2')  = x2.l;
xrhpL('3')  = x3.l;
xrhpL('4')  = x4.l;
xrhpM('1')  = x1.m;
xrhpM('2')  = x2.m;
xrhpM('3')  = x3.m;
xrhpM('4')  = x4.m;
abort$[ smax{i, abs(xvalL(i) - xrhpL(i))} > tol ]    'bad x.l values'; 
abort$[ smax{i, abs(xvalM(i) - xrhpM(i))} > tol ]    'bad x.l values'; 
abort$[ abs(objval - obj.l)               > tol ]    'bad obj.l values', objval, obj.l;

embeddedCode ReSHOP:
opt: min obj x1 x2 x3 x4 defobj
root: Nash(opt)
endEmbeddedCode

solve mEMP using emp; 
abort$[mEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mEMP.solveStat; 
abort$[mEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]   'solve failed', mEMP.modelStat; 

xrhpL('1')  = x1.l;
xrhpL('2')  = x2.l;
xrhpL('3')  = x3.l;
xrhpL('4')  = x4.l;
xrhpM('1')  = x1.M;
xrhpM('2')  = x2.M;
xrhpM('3')  = x3.M;
xrhpM('4')  = x4.M;
abort$[ smax{i, abs(xvalL(i) - xrhpL(i))} > tol ]    'bad x.l values'; 
abort$[ smax{i, abs(xvalM(i) - xrhpM(i))} > tol ]    'bad x.l values'; 
abort$[ abs(objval - obj.l)               > tol ]    'bad obj.l values ' objval obj.l; 

model m2 / defobj2 /; 
model m2EMP / defobj2 /; 
m2EMP.iterlim = 0; 

solve m2 using nlp min obj;

xvalL('1')  = x1.l;
xvalL('2')  = x2.l;
xvalL('3')  = x3.l;
xvalL('4')  = x4.l;
xvalM('1')  = x1.m;
xvalM('2')  = x2.m;
xvalM('3')  = x3.m;
xvalM('4')  = x4.m;
objval      = obj.l; 


embeddedCode ReSHOP:
root: min obj x1 x2 x3 x4 defobj2
endEmbeddedCode

solve m2EMP using emp;
abort$[m2EMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m2EMP.solveStat; 
abort$[m2EMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m2EMP.modelStat; 

xrhpL('1')  = x1.l;
xrhpL('2')  = x2.l;
xrhpL('3')  = x3.l;
xrhpL('4')  = x4.l;
xrhpM('1')  = x1.m;
xrhpM('2')  = x2.m;
xrhpM('3')  = x3.m;
xrhpM('4')  = x4.m;
abort$[ smax{i, abs(xvalL(i) - xrhpL(i))} > tol ]    'NLP: bad x.l values'; 
abort$[ smax{i, abs(xvalM(i) - xrhpM(i))} > tol ]    'NLP: bad x.m values'; 
abort$[ abs(objval - obj.l)               > tol ]    'NLP: bad obj.l values: ' objval obj.l;

embeddedCode ReSHOP:
opt: min obj x1 x2 x3 x4 defobj2
root: Nash(opt)
endEmbeddedCode

solve m2EMP using emp; 
abort$[m2EMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m2EMP.solveStat; 
abort$[m2EMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m2EMP.modelStat; 

xrhpL('1')  = x1.l;
xrhpL('2')  = x2.l;
xrhpL('3')  = x3.l;
xrhpL('4')  = x4.l;
xrhpM('1')  = x1.M;
xrhpM('2')  = x2.M;
xrhpM('3')  = x3.M;
xrhpM('4')  = x4.M;
abort$[ smax{i, abs(xvalL(i) - xrhpL(i))} > tol ]    'Nash: bad x.l values'; 
abort$[ smax{i, abs(xvalM(i) - xrhpM(i))} > tol ]    'Nash: bad x.m values'; 
abort$[ abs(objval - obj.l)               > tol ]    'Nash bad obj.l values: ' objval obj.l; 

$offEcho

$macro mysolve(expr) \
put_utility 'shell.checkErrorLevel' / "sed -e 's:XXX:expr:' %gams.scrdir%subgms.in > %gams.scrdir%subgms.gms"; \
execute.checkErrorLevel "gams %gams.scrdir%subgms.gms lo=%gams.lo% r=test-nltree emp=%system.emp%"; \


mysolve((cst - x1 + x2) * x3 + x4)

mysolve((cst - x1 / x2) * x3 - sqr(x4))
mysolve((cst - x1 - x2) * x3 + x4)
mysolve((cst + x1 - x2) * x3 - x4)
mysolve((x1 - cst - x2) * x3 - x4)
mysolve((x1 + cst - x2) * x3 - log(exp(x4)))
mysolve((-x1 + cst - x2) * x3 - sqr(x4))


mysolve((cst - x1) * x3 * x2 + eps*x4)
mysolve((cst + x1) * x3 * x2 + eps*x4)
mysolve((x1 - cst) * x3 * x2 - eps*x4)
mysolve((x1 + cst) * x3 * x2 - eps*x4)
mysolve((-x1 + cst) * x3 / (x2 + eps*x4))
mysolve((-x1 - cst) * x3 / x2 - eps*x4 )
mysolve((x1 + cst) / x3 / x2 - eps*x4)

mysolve((x4 - x1) * x3 * x2)
mysolve((x4 + x1) * x3 * x2)
mysolve((x1 - x4) * x3 * x2)
mysolve((x1 + x4) * x3 * x2)
mysolve((-x1 + x4) * x3 / x2)

mysolve((-x1 - x4) * x3 / x2)

mysolve((x1 + x4) / x3 * x2)


mysolve((x2 * x4 - x1) * x3 * x2)

mysolve((x2 * x4 + x1) * x3 * x2)

mysolve((x1 - x2 * x4) * x3 * x2)

mysolve((x1 + x2 * x4) * x3 * x2)

mysolve((-x1 + x2 * x4) * x3 / x2)

mysolve((-x1 - x2 * x4) * x3 / x2)

mysolve((x1 + x2 * x4) / x3 / x2)


mysolve((log(exp(x2 * x4)) - x1) * x3 * x2)

mysolve((log(exp(x2 * x4)) + x1) * x3 * x2)

mysolve((x1 - log(exp(x2 * x4))) * x3 * x2)

mysolve((x1 + log(exp(x2 * x4))) * x3 * x2)

mysolve((-x1 + log(exp(x2 * x4))) * x3 / x2)

mysolve((-x1 - log(exp(x2 * x4))) * x3 / x2)

mysolve((x1 + log(exp(x2 * x4))) / x3 / x2)


mysolve((log(exp(x2 * x4)) - sqr(x1)) * x3 * x2)

mysolve((log(exp(x2 * x4)) + sqr(x1)) * x3 * x2)

mysolve((sqr(x1) - log(exp(x2 * x4))) * x3 * x2)

mysolve((sqr(x1) + log(exp(x2 * x4))) * x3 * x2)

mysolve((-sqr(x1) + log(exp(x2 * x4))) * x3 / x2)

mysolve((-sqr(x1) - log(exp(x2 * x4))) * x3 / x2)

mysolve((sqr(x1) + log(exp(x2 * x4))) / x3 / x2)


mysolve((sqr(x1) + sqr(x2 - x4)) / x3)


mysolve((sqr(x1) + power(x2 - x4, 5)) / x3)

