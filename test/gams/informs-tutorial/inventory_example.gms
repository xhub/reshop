
$macro check_status(m) \
abort$[not((m.solvestat = %solvestat.NormalCompletion%) and ((m.modelstat = %modelstat.Optimal%) or m.modelstat = %modelstat.locallyOptimal%))] 'Wrong status', m.solvestat, m.modelstat;\

SCALAR rho    'units sold per year'        /144/
       beta   'fixed cost of an order'     /400/
       gamma  'inv cost per unit per year' /60/;

* target
set i / ordfix, inv /;
parameter tau(i) target /ordfix 10, inv 1000 /;
parameter alpha(i) /ordfix 5, inv 0.5 /;

VARIABLES x, f(i), obj;

EQUATIONS defcost(i), defobj_sum, defobj_smax(i);

defcost(i)..
  f(i) =e= (beta*rho/x)$sameas(i,'ordfix') + (gamma*x/2)$sameas(i,'inv');

defobj_sum..
  obj =e= alpha('ordfix')*(beta*rho/x) + alpha('inv')*(gamma*x/2);

model biobjective / defcost /;
x.lo = 10;

model biobjective_sum_ref /defobj_sum/;

solve biobjective_sum_ref using nlp min obj;

PARAMETERS x_l, x_m;
SCALAR tol /1e-6/;

x_l = x.l;
x_m = x.m;

* This is the first variant in Subsection 3.1 alpha = [5, 0.5]
EmbeddedCode ReSHOP:
  deffn f(i) defcost(i)
  main: min SUM(i, alpha(i)*f(i)) x
endEmbeddedCode
 
option clear=x;
x.lo = 10;
solve biobjective using emp;

check_status(biobjective);
abort$[abs(x_l - x.l) > tol] "wrong solution", x_l, x.l;
abort$[abs(x_m - x.m) > tol] "wrong solution", x_m, x.m;

option clear=x;
x.lo = 10;

* This is the second variant in Subsection 3.1 alpha = [1 1]
defobj_smax(i)..
  obj =g= (beta*rho/x)$sameas(i,'ordfix') + (gamma*x/2)$sameas(i,'inv');

model biobjective_smax_ref / defobj_smax /;
solve biobjective_smax_ref using nlp min obj;

x_l = x.l;
x_m = x.m;

option clear=x;
x.lo = 10;

embeddedCode ReSHOP:
  deffn f(i) defcost(i)
  main: min h0.dual().valFn x
  h0: MP('smax', f(i))
endEmbeddedCode
 
solve biobjective using emp;

check_status(biobjective);
abort$[abs(x_l - x.l) > tol] "wrong solution", x_l, x.l;
abort$[abs(x_m - x.m) > tol] "wrong solution", x_m, x.m;

option clear=x;
x.lo = 10;

* Equilibrium solve
embeddedCode ReSHOP:
  deffn f(i) defcost(i)
  main: min h0.objFn x
  h0: MP('smax', f(i))
  equil: Nash(main,h0)
endEmbeddedCode
 
solve biobjective using emp;

check_status(biobjective);
abort$[abs(x_l - x.l) > tol] "wrong solution", x_l, x.l;
abort$[abs(x_m - x.m) > tol] "wrong solution", x_m, x.m;

option clear=x;
x.lo = 10;

embeddedCode ReSHOP:
  deffn f(i) defcost(i)
  main: min h0.valFn x
  h0: MP('smax', f(i))
endEmbeddedCode
 
solve biobjective using emp;

check_status(biobjective);
abort$[abs(x_l - x.l) > tol] "wrong solution", x_l, x.l;
abort$[abs(x_m - x.m) > tol] "wrong solution", x_m, x.m;

$exit

* third variant alpha = [5,0.5]
option clear=x;
x.lo = 10;

embeddedCode ReSHOP:
  deffn f(i) defcost(i)
  H(i): MP('plus',f(i),offset=tau(i))
  root: min SUM(i, alpha(i)*H(i).dual().valfn) x
endEmbeddedCode

solve biobjective using emp;
