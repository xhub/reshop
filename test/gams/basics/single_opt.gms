$TITLE single optimization problem

$onText
Basic tests with single optimization problem
$offText

VARIABLES obj, x, v, z;

x.lo = 1;
v.lo = 2;
z.lo = 3;

EQUATIONS defobjL, defobjNL, defobjNLonly;

SCALAR c;

defobjL.. c*obj =E= 4*x + 8*v + z - 4;

defobjNL.. c*obj =E= sqr(x) + power(v, 3) + z - 8;

defobjNLonly.. c*obj =E= sqr(x) + power(z+v, 3) + 12;

model mL       / defobjL /;
model mNLonly  / defobjNLonly /;
model mNL      / defobjNL /;

SET i;
PARAMETERS cvals(i<) / 
   1 -8
   2 -1
   3  1
   4  8
/;

file empinfo / '%emp.info%' /;

SCALAR tol / 1e-6 /;

$macro checkL \
abort$[ abs(c*obj.l - 19)      > tol ] 'wrong obj.l'; \
abort$[ abs(obj.m - 0.)        > tol ] 'wrong obj.m'; \
abort$[ abs(x.l   - 1)         > tol ] 'wrong x.l';   \
abort$[ abs(c*x.m - 4)         > tol ] 'wrong x.m';   \
abort$[ abs(v.l   - 2)         > tol ] 'wrong v.l';   \
abort$[ abs(c*v.m - 8)         > tol ] 'wrong v.m';   \
abort$[ abs(z.l   - 3)         > tol ] 'wrong z.l';   \
abort$[ abs(c*z.m - 1)         > tol ] 'wrong z.m';   \
abort$[ abs(defobjL.l + 4)     > tol ] 'wrong defobjL.l' \
abort$[ abs(c*defobjL.m - 1.)  > tol ] 'wrong defobjL.m'

$macro checkNLonly \
abort$[ abs(c*obj.l - 138)       > tol ] 'wrong obj.l'; \
abort$[ abs(obj.m - 0.)          > tol ] 'wrong obj.m'; \
abort$[ abs(x.l   - 1)           > tol ] 'wrong x.l';   \
abort$[ abs(c*x.m - 2)           > tol ] 'wrong x.m';   \
abort$[ abs(v.l   - 2)           > tol ] 'wrong v.l';   \
abort$[ abs(c*v.m - 75)          > tol ] 'wrong v.m';   \
abort$[ abs(z.l   - 3)           > tol ] 'wrong z.l';   \
abort$[ abs(c*z.m - 75)          > tol ] 'wrong z.m';   \
abort$[ abs(defobjNLonly.l - 12) > tol ] 'wrong defobjNLonly.l' \
abort$[ abs(c*defobjNLonly.m - 1.) > tol ] 'wrong defobjNLonly.m'

$macro checkNL \
abort$[ abs(c*obj.l - 4)       > tol ] 'wrong obj.l'; \
abort$[ abs(obj.m - 0.)        > tol ] 'wrong obj.m'; \
abort$[ abs(x.l   - 1)         > tol ] 'wrong x.l';   \
abort$[ abs(c*x.m - 2)         > tol ] 'wrong x.m';   \
abort$[ abs(v.l   - 2)         > tol ] 'wrong v.l';   \
abort$[ abs(c*v.m - 12)        > tol ] 'wrong v.m';   \
abort$[ abs(z.l   - 3)         > tol ] 'wrong z.l';   \
abort$[ abs(c*z.m - 1)         > tol ] 'wrong z.m';   \
abort$[ abs(defobjNL.l + 8)    > tol ] 'wrong defobjNL.l' \
abort$[ abs(c*defobjNL.m - 1.) > tol ] 'wrong defobjNL.m'


loop(i,
   c = cvals(i);
   if {c > 0, 
      putclose empinfo 'equilibrium min obj x v z defobjL';
   else
      putclose empinfo 'equilibrium max obj x v z defobjL';
   };

   solve mL using emp;

   checkL

   if {c > 0, 
      putclose empinfo 'equilibrium min obj x v z defobjNLonly';
   else
      putclose empinfo 'equilibrium max obj x v z defobjNLonly';
   };

   solve mNLonly using emp;

   checkNLonly

   if {c > 0, 
      putclose empinfo 'equilibrium min obj x v z defobjNL';
   else
      putclose empinfo 'equilibrium max obj x v z defobjNL';
   };

   solve mNL using emp;

   checkNL
);

$if %system.emp%==JAMS $exit

loop(i,
   c = cvals(i);
   if {c > 0, 
      embeddedCode ReSHOP:
      min obj x v z defobjL
      endEmbeddedCode
   else
      embeddedCode ReSHOP:
      max obj x v z defobjL
      endEmbeddedCode
   };

   solve mL using emp;

   checkL

   if {c > 0, 
      embeddedCode ReSHOP:
      min obj x v z defobjNLonly
      endEmbeddedCode
   else
      embeddedCode ReSHOP:
      max obj x v z defobjNLonly
      endEmbeddedCode
   };

   solve mNLonly using emp;

   checkNLonly

   if {c > 0, 
      embeddedCode ReSHOP:
      min obj x v z defobjNL
      endEmbeddedCode
   else
      embeddedCode ReSHOP:
      max obj x v z defobjNL
      endEmbeddedCode
   };

   solve mNL using emp;

   checkNL
);
