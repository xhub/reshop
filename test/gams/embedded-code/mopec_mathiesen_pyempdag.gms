$ontext

The example is taken from

  Lars Mathiesen. An algorithm based on a sequence of linear complementarity
  problems applied to a Walrasian equilibrium model: an example.
  Mathematical Programming 37 (1987) 1-18.

In the paper, an explicit inverse demand function was used.
Here, we explicitly represent utility maximization problem to formulate the
problem as a MOPEC.

The solution is y* = (3), p* = (6,1,5), and x* = (3,2,0).

Author: Youngdae Kim (10.27.2016)

$offtext

set i commodities / 1*3 /;

parameters
    ATmat(i)  technology matrix  / 1 1  , 2 -1 , 3 -1 /
    s(i)      budget share       / 1 0.9, 2 0.1, 3 0 /
    b(i)      endowment          / 1 0  , 2 5  , 3 3 /;

variable u    utility of the consumer;
positive variables
    y         activity of the producer
    x(i)      Marshallian demand of the consumer
    p(i)      prices;

equations
    mkt(i)    constraint on excess demand
    profit    profit of activity
    udef      Cobb-Douglas utility function
    budget    budget constraint;

mkt(i)..
    b(i) + ATmat(i)*y - x(i) =G= 0;

profit..
    sum(i, -ATmat(i)*p(i)) =G= 0;

udef..
    u =E= sum(i, s(i)*log(x(i)));

budget..
    sum(i, p(i)*x(i)) =L= sum(i, p(i)*b(i));

model mopec / mkt, profit, udef, budget /;

* The second commodity is used as a numeraire.
p.fx('2') = 1;

x.l(i) = 1;

* To change the solution
* p.lo(i) = 1;
* x.lo(i) = 1;

mopec.justscrdir = 1;
solve mopec using emp;

EmbeddedCode Python:
# This code would go into the reshop python module
def create_mp_opt(m, mp_descr, vv, ee):
    mp = rhp.empdag_newmp(m, mp_descr["sense"])
    for i, v in enumerate(vv):
        if v.split('(')[0] in mp_descr["var"]:
            rhp.mp_addvar(mp, i)
    for i, e in enumerate(ee):
        if e.split('(')[0] in mp_descr["equ"]:
            rhp.mp_addconstraint(mp, i)

    objvar = mp_descr.get("objvar")
    if not objvar:
       raise RuntimeError("Need to give an objvar")

    if objvar:
       vi = vv.index(objvar)
       rhp.mp_setobjvar(mp, vi)
    
    return mp

def create_mp_vi(m, mp_descr, vv, ee):
    mp = rhp.empdag_newmp(m, mp_descr["sense"])
    zerovars = mp_descr.get("zerovars", ())
    for zv in zerovars:
       for i in (i for i, v in enumerate(vv) if v.startswith(zv)):
          raise NotImplementedError("TODO: add zerovar")

    matchings = mp_descr.get("matchings", ())
    for vp, ep in matchings:
       sv = (i for i, v in enumerate(vv) if v.startswith(vp))
       se = (i for i, v in enumerate(ee) if v.startswith(ep))
       for vidx, eidx in zip(sv, se):
           rhp.mp_addvipair(mp, eidx, vidx)

    for i, e in enumerate(ee):
        if e.split('(')[0] in mp_descr.get("constraints", ()):
            rhp.mp_addconstraint(mp, i)
    return mp

def create_mp(m, mp_descr, vv, ee):
     sense = mp_descr["sense"]
     if sense == rhp.RHP_MAX or sense == rhp.RHP_MIN:
         return create_mp_opt(m, mp_descr, vv, ee)

     if sense == rhp.RHP_FEAS:
         return create_mp_vi(m, mp_descr, vv, ee)
     raise RuntimeError(f"Unexpecte sense {sense}")


# Start of the user-visible part

import reshop as rhp
m = rhp.Model(r"%gams.scrdir%gamscntr.%gams.scrext%")
vv = list(rhp.mdl_printvarname(m, i) for i in range(rhp.mdl_nvars(m)))
ee = list(rhp.mdl_printequname(m, i) for i in range(rhp.mdl_nequs(m)))

maxa = {"var": ("u", "x"), "equ": ("udef", 'budget'), "type": rhp.RHP_MP_OPT, "sense": rhp.RHP_MAX, "objvar": "u"}
mkt_clearing = {"matchings": (("p", "mkt"), ("y", "profit")), "type": rhp.RHP_MP_VI, "sense": rhp.RHP_FEAS}


mpe = rhp.empdag_newmpe(m)
mp_a = create_mp(m, maxa, vv, ee)
mkt_vi = create_mp(m, mkt_clearing, vv, ee)

rhp.empdag_mpeaddmp(m, mpe, mp_a)
rhp.empdag_mpeaddmp(m, mpe, mkt_vi)

m.solve(r"%gams.scrdir%savepoint.gdx")

# Uncomment to start IPython
# import IPython
# IPython.embed(colors="neutral")
endEmbeddedCode

execute_loadpoint '%gams.scrdir%savepoint.gdx';

* TODO: see if this if failing with 46?
* abort$(mopec.solvestat <> %SOLVESTAT.NORMAL COMPLETION%) "Bad solvestat"
* abort$(mopec.modelstat > 2) "Bad modelstat"

$if not set tol $set tol 1e-6
scalar tol / %tol% /;

display y.l, x.l, p.l;

abort$[ abs(y.l      - 3) > tol ] 'bad y.l', y.l;
abort$[ abs(p.l('1') - 6) > tol ] 'bad p.l("1")', p.l;
abort$[ abs(p.l('2') - 1) > tol ] 'bad p.l("2")', p.l;
abort$[ abs(p.l('3') - 5) > tol ] 'bad p.l("3")', p.l;
abort$[ abs(x.l('1') - 3) > tol ] 'bad x.l("1")', x.l;
abort$[ abs(x.l('2') - 2) > tol ] 'bad x.l("2")', x.l;
abort$[ abs(x.l('3') - 0) > tol ] 'bad x.l("3")', x.l;
