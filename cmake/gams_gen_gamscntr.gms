VARIABLE w;
POSITIVE VARIABLE x1;

EQUATION dummyobj;

dummyobj..
  w =E= x1;

model m /dummyobj/;

solve m min w using lp;
