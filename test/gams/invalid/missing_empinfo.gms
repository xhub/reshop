$title missing EMPinfo file
$onText
Gracefully handles missing empinfo file
$offText

Positive Variable x;
Variable obj;
Equation objdef;

objdef.. obj =E= x;

model m / all /;

solve m using emp;
