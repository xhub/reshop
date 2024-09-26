$onText

Simple inventory problem 

Author: Michael C. Ferris <ferris@cs.wisc.edu>

$offText

scalar rho 'units sold per year' /144/;
scalar beta 'fixed cost of an order' /400/;
scalar gamma 'inv cost per unit per year' /60/;

* target
set i / ordfix, inv /;
parameter tau(i) target /ordfix 10, inv 1000 /;
parameter alpha(i) /ordfix 5, inv 0.5 /;

variable x;

* min max (beta rho / x - target(ordfix), gamma x / 2 - target(inv))

variable z1, z2, z3, f(i);
positive variable pcost3(i);

Equations obj1, obj2(i), obj3, defpcost3(i), defcost(i);

defcost(i)..
  f(i) =e= (beta*rho/x)$sameas(i,'ordfix') + (gamma*x/2)$sameas(i,'inv');

obj1..
  z1 =e= sum(i, alpha(i)*f(i));

model bima / obj1, defcost /;
x.lo = 10;
* main: MP('sum',f, weights=alpha) x defcost  (or implicit/defvar)
solve bima using nlp min z1;

obj2(i)..
  z2 =g= f(i);

model bimm / obj2, defcost /;
* This needs main: MP('smax',f) x defcost
solve bimm using nlp min z2;

defpcost3(i)..
  pcost3(i) =g= f(i) - tau(i);

obj3..
  z3 =e= sum(i, alpha(i)*pcost3(i));
*  Commented line does max instead of weighted sum of pcost3
*  z3 =g= pcost3(i);

model bimult / obj3, defpcost3, defcost /;
* MP('sum_pos_part',"define as variable = weight*(cost-tau)")
* This can be done with OVF sum_pos_part z3 "define as variable = weight*(cost-tau)"

* Alternatively:
* implicit f defcost
* h(i): MP('plus',f(i),offset=tau(i))
* h0: MP('sum',h.valfn(i),weights=alpha)
* main: min h0.valfn x
solve bimult using nlp min z3;
