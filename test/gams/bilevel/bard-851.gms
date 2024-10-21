Positive Variables x, y1, y2;
Variables          objout, objin;

Equations defout, defin, e1, e2, e3, e4;

defout..  objout =e= sqr(x-1) + 2*sqr(y1) - 2*x;
defin..   objin  =e= sqr(2*y1-4) + sqr(2*y2-1) + x*y1;

e1..      4*x + 5*y1 + 4*y2 =l= 12;
e2..    - 4*x - 5*y1 + 4*y2 =l= -4;
e3..      4*x - 4*y1 + 5*y2 =l=  4;
e4..    - 4*x + 4*y1 + 5*y2 =l=  4;

Model bard / all /;

option mpec=knitro

$echo bilevel x min objin y1 y2 defin e1 e2 e3 e4 > "%emp.info%"

solve bard use emp min objout;

$onEcho > %gams.emp%.opt
empinfoFile=empinfo_dag.dat
$offEcho

$onEcho > %gams.scrdir%/empinfo_dag.dat
lower: MIN objin y1 y2 defin e1 e2 e3 e4
upper: MIN objout x defout lower
$offEcho

bard.optfile=1;
bard.iterlim=0;
solve bard using emp;

$onEcho > %gams.emp%.op2
empinfoFile=empinfo_dag2.dat
$offEcho

$onEcho > %gams.scrdir%/empinfo_dag2.dat
upper: MIN objout x defout lower
lower: MIN objin y1 y2 defin e1 e2 e3 e4
$offEcho

bard.optfile=2;

solve bard using emp;

$onEcho > %gams.emp%.op3
empinfoFile=empinfo_dag3.dat
$offEcho

$onEcho > %gams.scrdir%/empinfo_dag3.dat
upper: MIN objout x defout dummyNash
lower: MIN objin y1 y2 defin e1 e2 e3 e4
dummyNash: Nash(lower)
$offEcho

bard.optfile=3;

solve bard using emp;
