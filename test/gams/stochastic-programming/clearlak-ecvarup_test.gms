$TITLE risk-averse test with artificial trees

$ontext

The model is based on the ClearLake example. The model is parametric in the number of
stages


Reference: Exercise, p. 44 in
Birge, R, and Louveaux, F V, Introduction to Stochastic Programming.
Springer, 1997.

Contributor: Olivier Huber <oli.huber@gmail.com>
Date:        November 2025

$offtext

$eolcom #

$if not set tol $set tol 1e-6

$onEcho > pathnlp.opt
conv_tol 1e-12
crash_method none
prox_pert 0
crash_pert no
$offEcho

$onEcho > path.opt
conv_tol 1e-10
crash_method none
prox_pert 0
crash_pert no
factorization_method conopt
output_preprocess_level 2
output_warnings 1
path_basis_modify_tol 1000000
path_basis_modify_verbose 1
minor_iter_limit 1
major_iter_limit 1
$offEcho

$onEcho > %gams.emp%.opt
subsolveropt 1
$offEcho

$onEcho > %gams.emp%.op2
subsolveropt 1
$offEcho

option optca = 1e-12;

* We need cplex to use a simplex method
* lpmethod 2 fails
$onEcho > cplex.opt
lpmethod 1
$offEcho

$if not set nstage    $eval nstage 5
$if not set nchildren $eval nchildren 3

* init
$eval x '0' $eval i '0' $eval num_nodes '0'

* poor man's implementation of sum(i=1 to %nstage%, num_nodes += power(%nchildren%,%i%))
$label rep $eval num_nodes '%num_nodes% + power(%nchildren%,%i%)' $eval i '%i%+1'

$ifE %i%<%nstage% $goto rep
* TODO why does the following not work:
* $ifE %i% < %nstage% $goto rep

$log Scenario tree stats: stage: %nstage%; nchdilren: %nchildren%; number nodes: %num_nodes%


SETS p Stochastic realizations (precipitation) /low, normal, high/,
     t Time periods                            /dec,jan,feb,mar/,
     n set of nodes                            /n1*n%num_nodes%/,
     succ(n,n) Successor relationship,
     list(n);

ALIAS (n,child,parent);

* Define a regular tree with nchildren
succ(n,child)$(ord(child) >= %nchildren%*ord(n)-(%nchildren%-2) AND
               ord(child) <= %nchildren%*ord(n)+1) = yes;

* succ(n,child) = yes$(ord(child) >= %nchildren%*ord(n)-(%nchildren%-2) AND
*                      ord(child) <= %nchildren%*ord(n)+1);

*-------------------------------------------------------------------------------
*Parameters are specified

PARAMETERS floodCost  'Flooding penalty cost thousand CHF per mm'   /10/,
           lowCost    'Water importation cost thousand CHF per mm'  /5/,
           l_start    'Initial water level (mm)'                    /230/,
           l_max      'Maximum reservoir level (mm)'                /250/,
           r_max      'Maximum normal release in any period'        /200/;

*-----------------------------------------------------------------------------

PARAMETERS prob(n)    'Conditional probability',
           ndelta(n)  'Water inflow at each node';

SET pp;

PARAMETERS rain_vals(pp<)
$ifthenE %nchildren%=3
/ 1 250, 2 450, 3 550/
*/ 1 50, 2 250, 3 350/
$elseifE  %nchildren%=4
/ 1 25, 2 100, 3 200, 4 400/
$elseifE  %nchildren%=5
/ 1 15, 2 75, 3 150, 4 300, 5 450/
$endif
;

ALIAS(n,nn);
list(n) = yes$n.first;


prob(n)$(NOT n.first) = binomial(%nchildren%-1, mod(ord(n)+1, %nchildren%)) / power(2,%nchildren%-1);
prob(n)$n.first = 1.;

display prob;

ndelta(n)$(NOT n.first) = sum{pp$(pp.pos = 1+mod(ord(n)+1, %nchildren% )), rain_vals(pp)};

display ndelta;

* execute_unload 'clearlaktree-like-scenario-tree.gdx' n, succ, prob;


VARIABLE EC               'Total Cost';

*------------------------------------------------------------------------------
* Model logic:  only needs n, succ, prob, ndelta
*------------------------------------------------------------------------------

POSITIVE VARIABLES
		   F(n)             'Floodwater released (mm)',
         L(n)             'Reservoir water level -- EOP',
		   R(n)             'Normal release of water (mm)',
		   Z(n)             'Water imports (mm)';

R.up(n) = r_max;
L.up(n) = l_max;
L.fx(n)$(n.first) = l_start;

* CVaR parameters
PARAMETER tail    'tail of the CVaR',
          cvar_wt 'weight of the CVaR';

PARAMETER beta;

SET test1, test2;

PARAMETERS tails(test1<)    / '1'   1., '0.75' .75, '0.5' .5, '0.3' .3, '0.1' .1/;
PARAMETERS cvar_wts(test2<) / '0.9' .9, '0.75' .75, '0.5' .5, '0.3' .3, '0.1' .1/;

* Comparison
PARAMETERS L_L(n), L_M(n), ldef_M(n), F_L(n), F_M(n), R_L(n), R_M(n), Z_L(n), Z_M(n),
           L_Ld(n), F_Ld(n), R_Ld(n), Z_Ld(n), y_L(n), cost_L(n), v_L(n);

VARIABLES          v(n)    'Var value at each non-leaf node',
                   cost(n) 'total cost at each node';

POSITIVE VARIABLES y(n)    'Contribution of the cost for each node'

*-------------------------------------------------------------------------------
* Model components for computing CVaR a posteriori
*-------------------------------------------------------------------------------

VARIABLES cvarURobj;
EQUATIONS defcvarURobj, cvarURcons(n,n);

PARAMETER QL(n);

SINGLETON SET activeNode(n);

defcvarURobj..
   cvarURobj
   =E=
   sum(activeNode(n), v(n) + [1/(1-beta)] * sum(succ(n,child), prob(child)*y(child)));

cvarURcons(n,child)$(activeNode(n) AND succ(n,child))..
   y(child) =G= QL(child) - v(n);

MODEL cvarUR 'Uryasev-Rockafellar formulation for cvar' / defcvarURobj, cvarURcons /;

PARAMETER QLd(n), y_Ld(n), v_ld(n);

*-------------------------------------------------------------------------------
* Reference model
*-------------------------------------------------------------------------------

EQUATIONS ecdef, ldef; 

EQUATIONS cvar_ineq(n,n), defcost(n), totalCost;

cvar_ineq(n,child)$(succ(n,child))..
   y(child) =G= cost(child) - v(n);

defcost(n)..
   cost(n) =G= (floodcost*F(n)+lowCost*Z(n))$(NOT n.first) + 
   (
          cvar_wt * [ v(n) + [1/(1-beta)] * sum(succ(n,child), prob(child)*y(child)) ] +
      (1-cvar_wt) * [ sum(succ(n,child), prob(child)*cost(child)) ]
   )$(sum(succ(n,child), yes));

totalCost..
   EC =E= sum(n$n.first, cost(n));


ldef(n)$[NOT n.first].. L(n) =e= sum{ succ(parent,n), L(parent)} + ndelta(n)+Z(n)-R(n)-F(n);

MODEL mincost / totalCost, defcost, cvar_ineq, ldef /;

mincost.optfile=1;


*-------------------------------------------------------------------------------
* EMP model
*-------------------------------------------------------------------------------

VARIABLES  objnode(n)     'nodal (local) cost'; 

EQUATIONS  defobjnode(n)  'nodal cost definition',
           deflvl(n)      'water level';

defobjnode(n)..
      objnode(n) =E= (floodCost*F(n)+lowCost*Z(n))$(not n.first);

deflvl(n).. L(n) =E= sum( succ(parent,n), L(parent) ) + ndelta(n)+Z(n)-R(n)-F(n);

MODEL mincostEMP 'EMP stochastic model' / defobjnode, deflvl /;

mincostEMP.optfile = 1;


SCALAR optfileVal / 1 /;

tail = .4;
cvar_wt = .3;

if( %gams.optfile% > 1, optfileVal = %gams.optfile% );

display tail, cvar_wt;

beta = 1-tail;

mincost.optfile = optfileVal;

SOLVE mincost using LP minimizing EC;

display v.l, y.l;

* Save values
L_L(n) = L.l(n);
L_M(n) = L.m(n);
ldef_M(n)$(NOT n.first) = ldef.m(n);

F_L(n) = F.l(n);
F_M(n) = F.m(n);
R_L(n) = R.l(n);
R_M(n) = R.m(n);
Z_L(n) = Z.l(n);
Z_M(n) = Z.m(n);

y_L(n) = y.l(n);
v_L(n) = v.l(n);
cost_L(n) = cost.l(n);


* Reset
L.fx(n)$(n.first) = l_start;

ndelta(n)$(n.first) = L.l(n);
F.fx(n)$(n.first) = 0;
Z.fx(n)$(n.first) = 0;
R.fx(n)$(n.first) = 0;

*******************************************************************************
* EMP model
*******************************************************************************

embeddedCode reshop:
nodalprob(n): min objnode(n) + CRM(n).valFn$(succ(n,child)) F(n) L(n) R(n) Z(n) defobjnode(n) deflvl(n) 
CRM(n)$(succ(n,child)): MP('ecvarup', nodalprob(child).valFn$(succ(n,child)), tail=tail, cvar_wt=cvar_wt, prob=prob(child)$(succ(n,child)))
endEmbeddedCode

mincostEMP.optfile = optfileVal;

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;

SOLVE mincostEMP using EMP;

abort$[mincostEMP.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', mincostEMP.solveStat;
abort$[mincostEMP.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', mincostEMP.modelStat;


* leaf: just initialize local costs
list(n)$(NOT sum(succ(n,child), yes)) = yes;

QL(list(n)) = objnode.l(n);

* Select leaf node parents
list(parent) = yes$(sum(n$(list(n) AND succ(parent,n)), yes));

while(card(list),

   loop(list,
      activeNode(list) = yes;

      solve cvarUR min cvarURobj using LP;

      QL(list) = objnode.l(list)
             + (1-cvar_wt) * sum(succ(list,child), prob(child)*QL(child))
             + cvar_wt * cvarURobj.l;

   );

   list(parent) = yes$(sum(n$(list(n) AND succ(parent,n)), yes));

);

QLd(n) = abs(QL(n) - cost_L(n));
QLd(n)$(QLd(n) <= %tol%) = 0.;

y_Ld(n) = abs(y.l(n) - y_L(n));
v_Ld(n) = abs(v.l(n) - v_L(n));

L_Ld(n) = abs(L.l(n) - L_L(n));
F_Ld(n) = abs(F.l(n) - F_L(n));
Z_Ld(n) = abs(Z.l(n) - Z_L(n));
R_Ld(n) = abs(R.l(n) - R_L(n));

display QLd, F_Ld, Z_Ld;



abort$[smax{n$n.first, abs(QLd(n))} > %tol%]   'wrong costs', QLd, QL, cost_L;
abort$[smax{n, abs(y_Ld(n))} > %tol%]  'wrong probability measures', y_Ld, y.l, y_L;
abort$[smax{n, abs(v_Ld(n))} > %tol%]  'wrong VaR', v_Ld, v.l, v_L;

abort$[smax{n$(NOT n.first), F_Ld(n)} > %tol%]     'wrong F.l', F_Ld;
abort$[smax{n$(NOT n.first), Z_Ld(n)} > %tol%]     'wrong Z.l', Z_Ld;

$exit

embeddedCode python:
import graphviz

def drawTree(dot, q, localCost, f, z, y, diff, deltaStr):
    dot.attr('node', shape='circle', style='filled', fontname='Helvetica')
    dot.attr('edge', color='gray40', arrowsize='0.8')
    
    
    # Add all nodes to the graph
    for node in nodes:
        costToGo = q.get(node, 0)
        lCost = localCost.get(node, 0)
        label = "\n".join((
                             node,
                             f"objval: {costToGo:,.5g}",
                             f"local cost: {lCost:,.5g}",
                             f"Q: {costToGo-lCost:,.5g}",
                             f"F: {f.get(node,0):,.5g}",
                             f"Z: {z.get(node,0):,.5g}",
                             deltaStr.get(node, ""),
                             ))
        dot.node(node, label=label, fillcolor=diff.get(node, 'lightblue'))
    
    # Add all directed edges (arcs) to the graph
    for start_node, end_node in arcs:
        dot.edge(start_node, end_node, label=f"{y.get(end_node,0)}")


db = gams.db
nodes = [rec.keys[0] for rec in db["n"]]
arcs = [(rec.keys[0], rec.keys[1]) for rec in db["succ"]]

floodCost = db["floodCost"].first_record().value
lowCost = db["lowCost"].first_record().value

yRef = {}
varRef = {}
qRef = {}
fRef = {}
zRef = {}
localCostRef = {}

y = {}
var = {}
q = {}
f = {}
z = {}
localCost = {}

for rec in db["y"]: y[rec.keys[0]] = rec.level
for rec in db["v"]: var[rec.keys[0]] = rec.level
for rec in db["f"]: f[rec.keys[0]] = rec.level
for rec in db["z"]: z[rec.keys[0]] = rec.level
for rec in db["objnode"]: localCost[rec.keys[0]] = rec.level
for rec in db["QL"]: q[rec.keys[0]] = rec.value


for rec in db["y_L"]: yRef[rec.keys[0]] = rec.value
for rec in db["v_L"]: varRef[rec.keys[0]] = rec.value
for rec in db["F_L"]: fRef[rec.keys[0]] = rec.value
for rec in db["Z_L"]: zRef[rec.keys[0]] = rec.value
for n in nodes: localCostRef[n] = (floodCost*fRef.get(n, 0) + lowCost*zRef.get(n,0))
for rec in db["cost_L"]: qRef[rec.keys[0]] = rec.value

tol = 1e-6
diff = {}
deltaStr = {}
for k in q.keys():
   Dq = abs(q.get(k,0) - qRef.get(k,0))
   Df = abs(f.get(k,0) - fRef.get(k,0))
   Dz = abs(z.get(k,0) - zRef.get(k,0))
   if (Dq > tol) or (Df > tol) or (Dz > tol):

      diff[k] = 'red'
      deltas = []
      if Dq > tol:
         deltas.append(f"Δq = {Dq:.2g}")
      if Df > tol:
         deltas.append(f"Δf = {Df:.2g}")
      if Dz > tol:
         deltas.append(f"Δz = {Dz:.2g}")

      deltaStr[k] = "\n".join(deltas)

print(f"Found {len(nodes)} nodes and {len(arcs)} arcs.")

# --- Create the Graphviz Graph ---
print("Building the graph with Graphviz...")
dot = graphviz.Digraph('GamsTree', comment='GAMS Tree Visualization')

# Add some global graph attributes for styling
dot.attr(rankdir='LR', size='10,8', label=f'Tree with ReSHOP solution', fontsize='20')
drawTree(dot, q, localCost, f, z, y, diff, deltaStr)
 
# --- Render and Save the Graph ---
output_filename = 'reshop_solution'
try:
    # This will create a DOT source file and render it to a PDF
    dot.render(output_filename, view=True, format='pdf', cleanup=True)
    print(f"\nSuccessfully created graph visualization: '{output_filename}.pdf'")
    print("Your PDF viewer should open automatically.")
except Exception as e:
    print(f"\nError during Graphviz rendering: {e}")
    print("Please ensure the Graphviz software is installed and in your system's PATH.")


dotRef = graphviz.Digraph('GamsTree', comment='GAMS Tree Visualization')

# Add some global graph attributes for styling
dotRef.attr(rankdir='LR', size='10,8', label=f'Tree with reference solution', fontsize='20')
drawTree(dotRef, qRef, localCostRef, fRef, zRef, yRef, diff, deltaStr)
    
# --- Render and Save the Graph ---
output_filename = 'reference_solution'
try:
    # This will create a DOT source file and render it to a PDF
    dotRef.render(output_filename, view=True, format='pdf', cleanup=True)
except Exception as e:
    print(f"\nError during Graphviz rendering: {e}")
    print("Please ensure the Graphviz software is installed and in your system's PATH.")

endEmbeddedCode


