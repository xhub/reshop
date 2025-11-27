$ontext

Simple example to illustrate how to model the CVaR as a support function

author: Olivier Huber <oli.huber@gmail.com>

$offtext

* START PROBLEM DATA DEFINITION

* Allow to set another name for reshop
* Useful when one has the solver provided by the GAMS distributions and a local development version 
$if not set reformulation $set reformulation fenchel
$if not set nSamples $set nSamples 950

SETS i 'realizations set' /1*%nSamples%/
     j 'dimension of x'   /1*2/
     k 'number of distributions' /1*4/;




PARAMETERS xi(k,i) 'normal distributions',
           mean(k)   /1 250,
           2  125, 3 2500, 4 40000/,
           stddev(k) /1  75, 2 62.5, 3  500, 4  4000/;


embeddedCode Python:

import numpy as np
np.random.seed(0)
k = list(gams.get("k"))
i = list(gams.get("i"))

si = len(i)
sk = len(k)

means   = dict(gams.get("mean"))
stddevs = dict(gams.get("stddev"))


xi = []
for kk in k:
   dvec = [kk]*si
   xi.extend(zip(dvec, i, np.random.normal(loc=means[kk], scale=stddevs[kk], size=si)))

# import IPython
# IPython.embed(colors="neutral")
gams.set("xi", xi)

endEmbeddedCode xi


SCALAR tail;
tail = 1-.05;

* END PROBLEM DATA DEFINITION

* START OPTIMIZATION PROBLEM DEFINITION

VARIABLES obj, h0v, phi(i), x(j);

x.lo('1') = 0.1;
x.up('1') = 0.2;
x.lo('2') = 0.1;
x.up('2') = 0.6;

x.l(j) = x.lo(j);

Equation defobj, defphi(i);

defobj..       obj =E= h0v;

defphi(i).. phi(i) =E= 4 * xi('1', i) / (x('1')*x('2')*xi('4', i)) + 4*xi('2', i) * x('2') / (sqr(x('1')) * xi('4', i)) + sqr(xi('3', i)) / (sqr(x('1')) * sqr(xi('4', i)));
 
* END OPTIMIZATION PROBLEM DEFINITION

file empinfo /'%emp.info%'/;
empinfo.nd = 10;
putclose empinfo /'OVF cvarup h0v phi ' tail;

$onecho > %gams.emp%.op3
ovf_reformulation=%reformulation%
subsolveropt=1
output_subsolver_log=1
empinfofile=empdag.txt
$offecho

$onecho > %gams.emp%.opt
ovf_reformulation=%reformulation%
subsolveropt=1
output_subsolver_log=1
$offecho

$onecho > path.opt
convergence_tolerance=1e-11
$offecho

$onecho > conopt.opt
RTREDG=1e-12
$offecho

$macro reset(x) x.l(j) = 0.; x.m(j) = 0.;


model superquantile /all/;

solve superquantile min obj using emp;

PARAMETERS x_l(j), x_m(j);
SCALAR tol /1e-6/;

x_l(j) = x.l(j);
x_m(j) = x.m(j);

EmbeddedCode ReSHOP:
cvar: MP("cvarup", phi, tail=tail)
main: min cvar.valFn x
endEmbeddedCode

* defobj is not needed
model superquantile_EC /superquantile-defobj/;

reset(x);
solve superquantile_EC using emp;

abort$[smax{j, abs(x_l(j) - x.l(j)) > tol}] "wrong solution", x_l, x.l;
abort$[smax{j, abs(x_m(j) - x.m(j)) > tol}] "wrong solution", x_m, x.m;

* This is the EC of Listing 2 
EmbeddedCode ReSHOP:
deffn phi(i) defphi(i)
cvar: MP("cvarup", phi(i), tail=tail)
main: min cvar.valfn x(j)
endEmbeddedCode

reset(x)
solve superquantile_EC using emp;

abort$[smax{j, abs(x_l(j) - x.l(j))} > tol] "wrong solution", x_l, x.l;
abort$[smax{j, abs(x_m(j) - x.m(j))} > tol] "wrong solution", x_m, x.m;

* This is the EC of Listing 2 
EmbeddedCode ReSHOP:
deffn phi(i) defphi(i)
cvar: MP("cvarup", phi(i), tail=tail)
main: min cvar.dual().valfn x(j)
endEmbeddedCode

reset(x)
solve superquantile_EC using emp;

abort$[smax{j, abs(x_l(j) - x.l(j)) > tol}] "wrong solution", x_l, x.l;
abort$[smax{j, abs(x_m(j) - x.m(j)) > tol}] "wrong solution", x_m, x.m;

EmbeddedCode ReSHOP:
deffn phi(i) defphi(i)
cvar: MP("cvarup", phi(i), tail=tail)
main: min cvar.objfn x(j)
equil: vi main.kkt() cvar.kkt()
endEmbeddedCode

reset(x)
solve superquantile_EC using emp;

abort$[smax{j, abs(x_l(j) - x.l(j)) > tol}] "wrong solution", x_l, x.l;
abort$[smax{j, abs(x_m(j) - x.m(j)) > tol}] "wrong solution", x_m, x.m;

EmbeddedCode ReSHOP:
deffn phi(i) defphi(i)
cvar: MP("cvarup", phi(i), tail=tail)
main: min cvar.objfn x(j)
equil: Nash(main,cvar)
endEmbeddedCode

reset(x)
solve superquantile_EC using emp;

abort$[smax{j, abs(x_l(j) - x.l(j)) > tol}] "wrong solution", x_l, x.l;
abort$[smax{j, abs(x_m(j) - x.m(j)) > tol}] "wrong solution", x_m, x.m;

$exit


* TODO: where is %gams.emp% explained?
$onecho > %gams.emp%.op2
ovf_reformulation=equilibrium
subsolveropt=2
output_subsolver_log=1
$offecho

$onecho > path.op2
convergence_tolerance=1e-11
major_iteration_limit=0
crash_meth none
prox_pert 0
crash_pert no
$offecho


superquantile.optfile=2

solve superquantile min obj using emp;


* loop(iter,
* param)
