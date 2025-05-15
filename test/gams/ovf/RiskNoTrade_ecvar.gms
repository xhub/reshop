$if not set init $set init 15
$if set IMPLICIT $if not set nomacro $abort "nomacro has to be set with the implicit keyword"

$if not set OVF_METHOD $set OVF_METHOD "equilibrium"

$if not set OUTPUT_VARS $set OUTPUT_VARS "RiskNoTrade"

$if not set report_profit $set report_profit ""

$onecho > jams.opt
subsolveropt=1
$offecho


* Hydro-thermal model
*
* Written by Andy Philpott 17/8/2012
* ABP edits 7/7/2013
* ABP edits 29/7/2013

* OH define cvarup over equation and not variable

$if not set noreport $set reporting 1

SETS
h   hydros     /1/
t   thermals   /1/
s   scenarios  /1*10/
n   stages /1*2/
;

alias(s,s1);
parameter prob(s)  probability ;
prob(s)= 1/card(s) ;
parameter rnprob(s)  probability ;
rnprob('1')= 1/card(s) ;
rnprob(s)$(ord(s)<>1)=1/card(s);
parameter raprob(s)  probability ;
raprob('1')= 0.28 ;
raprob(s)$(ord(s)<>1)=0.08;

parameter lambda    cvar weight (>0 and <1 chosen by user) = 0.9;
lambda = 0.2;

parameter lambdaT(t);
lambdaT(t) = lambda;
parameter lambdaH(h);
lambdaH(h) = lambda;
parameter lambdaC;
lambdaC = lambda;

scalar theta cvar tail / 0.1 /;

parameter futurecostoffset  offset to add to hydro cost-to-go;
futurecostoffset = 30.0 ;

parameter a(h)  hydro storage capacity;
a(h) = 50;
parameter x0(h) initial storage /
1     %init%
/
;

parameter b(h)  hydro release capacity  /
1     10
/
;
parameter rho(h)  hydro coefficient  /
1     1.5
/
;
parameter sigma(h)  hydro coefficient  /
1     0.03
/
;
parameter phi(h)  hydro coefficient  /
1     3
/
;
parameter psi(h)  hydro coefficient  /
1     0.05
/
;

parameter g(t)  thermal generation capacity;
g(t) = 12;
parameter c(t)  thermal generation cost coefficient  /
1        1.0
/         ;

parameter maxprice  price at zero demand   ;
maxprice = 40;
parameter pslope    slope of P(x)  ;
pslope = 4;

table demand(s,n)  demand
         1        2
1       13        13
2       13        13
3       13        13
4       13        13
5       13        13
6       13        13
7       13        13
8       13        13
9       13        13
10      13        13
;

table inflow(s,h,n)  inflow in each scenario
           1        2
1.1        4        1
2.1        4        2
3.1        4        3
4.1        4        4
5.1        4        5
6.1        4        6
7.1        4        7
8.1        4        8
9.1        4        9
10.1       4        10
;


positive variable
uh1(h)
uh2(s,h)      hydro release
eh1(h)
eh2(s,h)      hydro energy
sh1(h)
sh2(s,h)      spill
ut1(t)
ut2(s,t)      thermal generation
x2(s,h)       hydro storage period 2
x1(h)         hydro storage period 1
consume1
consume2(s)
;

variable
cost
CVaRt
CVaRh
CVaRc
yh(s)
yt(s)
yc(s)
;

equation
objective
dynamics1(h)
dynamics2(s,h)
meetdemand1
meetdemand2(s)
cvarconstrainth(s)
cvarconstraintt(s)
cvarconstraintc(s)
;

$macro Cst(x) (c(t)*power(x,2))
$macro Util(u) (rho(h)*u-0.5*sigma(h)*power(u,2))
$macro TermV(x) (10*log((x+0.1)/10.0))
$macro Dem(d) d*(maxprice-0.5*pslope*d)

objective.. cost =e=  sum(t, Cst(ut1(t))) - Dem(consume1) + (CVaRt + CVaRh + CVaRc);

dynamics1(h).. x1(h) =e=  x0(h)-uh1(h)-sh1(h)+inflow('1',h,'1');
dynamics2(s,h).. x2(s,h) =e= x1(h) -uh2(s,h)-sh2(s,h)+inflow(s,h,'2');
meetdemand1..
  sum(h, Util(uh1(h))) + sum(t, ut1(t)) =g= consume1;
meetdemand2(s)..
  sum(h,Util(uh2(s,h))) + sum(t, ut2(s,t)) =g= consume2(s);

* Random variable calculation
cvarconstrainth(s).. yh(s) =g= ( - sum(h,TermV(x2(s,h))));
cvarconstraintt(s).. yt(s) =g= sum(t,Cst(ut2(s,t)));
cvarconstraintc(s).. yc(s) =g= ( - Dem(consume2(s))  );

model hydro /all / ;

file empinfo /'%emp.info%'/
put empinfo 'OVF ecvarup CVaRt yt ' theta lambda /;
put empinfo 'OVF ecvarup CVaRh yh ' theta lambda /;
put empinfo 'OVF ecvarup CVaRc yc ' theta lambda /;
putclose empinfo;

uh1.up(h)=b(h);
ut1.up(t)=g(t);
x1.up(h)=a(h);
uh2.up(s,h)=b(h);
ut2.up(s,t)=g(t);
x2.up(s,h)=a(h);
*sh2.fx(s,h)=0;
*sh2.fx('2','2')=2.64;
*sh2.fx('7','2')=2.23;

$onecho > %gams.emp%.op7
ovf_reformulation=%OVF_METHOD%
output_subsolver_log=1
subsolveropt=1
export_models=1
$offecho

$onecho > conopt.opt
RTREDG=1e-12
$offecho


hydro.optfile=7

solve hydro min cost using emp;



parameter fuelcost(s);
fuelcost(s) = sum(t, Cst(ut1.l(t))) + sum(t, Cst(ut2.l(s,t)));
parameter futurecost(s);
futurecost(s) = -sum(h, TermV(x2.l(s,h)));
parameter demandcost(s);
demandcost(s) = -Dem(consume1.l)- Dem(consume2.l(s))
parameter totalcost(s);
totalcost(s)= fuelcost(s) +  futurecost(s)+ demandcost(s) ;
parameter secondstagecost(s);
secondstagecost(s)= sum(t, Cst(ut2.l(s,t)))+ futurecost(s) ;
parameter expectedsecondcost;
expectedsecondcost =   sum(s, prob(s)* secondstagecost(s) )
parameter firstcost;
firstcost =  sum(t, Cst(ut1.l(t)));

parameter expectedcost;
expectedcost =  sum(t, Cst(ut1.l(t)))
 - Dem(consume1.l)
 - sum (s, prob(s)*Dem(consume2.l(s)))
 + sum(s, prob(s)* sum(t, Cst(ut2.l(s,t))) )
 - sum(s, prob(s)* sum(h, TermV(x2.l(s,h))));

parameter thermalprofit(t,s);
parameter hydroprofit(h,s);
parameter demandwelfare(s);
parameter totalwelfare(s);
parameter price2(s);
price2(s) = meetdemand2.m(s)/raprob(s);
thermalprofit(t,s) = meetdemand1.m*ut1.l(t)              - Cst(ut1.l(t)
                  + price2(s)*(ut2.l(s,t)       ) - Cst(ut2.l(s,t));
hydroprofit(h,s) = meetdemand1.m*Util(uh1.l(h))
                 + price2(s)*(Util(uh2.l(s,h))       )+ TermV(x2.l(s,h));
demandwelfare(s) = -meetdemand1.m*consume1.l -price2(s)*consume2.l(s)+ Dem(consume1.l)
                 +Dem(consume2.l(s));

totalwelfare(s) = sum(t,thermalprofit(t,s))+ sum(h,hydroprofit(h,s))+ demandwelfare(s);

parameter optcvart;
optcvart = (CVaRt.l - (1-lambda)*sum(s, prob(s)*(yt.l(s))))/lambda;
parameter optcvarh;
optcvarh = (CVaRh.l - (1-lambda)*sum(s, prob(s)*(yh.l(s))))/lambda;
parameter optcvar;
optcvar = optcvart + optcvarh ;


$include "Demandresponse.inc";

execute '=diff -bBw optInd.out ref/notrade15/optInd.out';
abort$errorlevel 'hydro solve failed';

* $exit

parameter md1(h), md2(s,h);
md1(h) = dynamics1.m(h);
md2(s,h) = dynamics2.m(s,h);

* define emp model
equations tpobjdef(t),hpobjdef(h),cpobjdef, tpcvar(s,t),hpcvar(s,h),cpcvar(s);
variables tpobj(t), hpobj(h), cpobj, CVaRhp(h), CVaRtp(t);

positive variables pi1, pi2(s);
positive variable tradeprice(s);
tradeprice.fx(s)=0;

$ifthen.macro_control_def set nomacro

variables  ytp(s,t), yhp(s,h), ycp(s);
cpcvar(s).. ycp(s) =e= pi2(s)*consume2(s) - Dem(consume2(s));
tpcvar(s,t).. ytp(s,t) =e= pi2(s)*(-ut2(s,t)) + Cst(ut2(s,t));
hpcvar(s,h).. yhp(s,h) =e= pi2(s)*(-Util(uh2(s,h))) - TermV(x2(s,h));

$else.macro_control_def

$macro YCP(s)   (pi2(s)*consume2(s) - Dem(consume2(s)))
$macro YHP(s,h) (pi2(s)*(-Util(uh2(s,h))) - TermV(x2(s,h)))
$macro YTP(s,t) (pi2(s)*(-ut2(s,t)) + Cst(ut2(s,t)))

cpcvar(s).. YCP(s) =n= 0.;
tpcvar(s,t).. YTP(s,t) =n= 0.;
hpcvar(s,h).. YHP(s,h) =n= 0.;

$endif.macro_control_def

cpobjdef..
  cpobj =e= pi1*consume1 - Dem(consume1) + CVaRc;


tpobjdef(t)..
  tpobj(t) =e= -pi1*ut1(t) + Cst(ut1(t)) + CVaRtp(t);


hpobjdef(h)..
  hpobj(h) =e= -pi1*Util(uh1(h)) + CVaRhp(h);


model hydroemp /tpobjdef,hpobjdef,cpobjdef,
                tpcvar,hpcvar,cpcvar,
                dynamics1,dynamics2,
              meetdemand1, meetdemand2
               /;

put empinfo / 'equilibrium';
empinfo.pw = 25500;
$ifthen set IMPLICIT
put / 'implicit';
loop((s,t),
    put ytp(s,t), tpcvar(s,t);
);
loop((s,h),
    put yhp(s,h), hpcvar(s,h);
);
loop(s,
    put ycp(s), cpcvar(s);
);
$endif
put / 'min ' cpobj;
put consume1;
$ifthen.macro_control_c set nomacro
 loop(s, put consume2(s) );
$else.macro_control_c
 loop(s, put consume2(s));
$endif.macro_control_c
 put CVaRc cpobjdef ;
$ifthen not set IMPLICIT
* loop(s, put cpcvar(s));
$endif

loop(h,
 put / 'min ' hpobj(h);
 put x1(h) uh1(h) sh1(h) CVaRhp(h);
$ifthen.macro_control_h set nomacro
 loop(s, put x2(s,h) uh2(s,h) sh2(s,h));
$else.macro_control_h
 loop(s, put x2(s,h) uh2(s,h) sh2(s,h))
$endif.macro_control_h
put hpobjdef(h) dynamics1(h);
$ifthen not set IMPLICIT
* loop(s, put hpcvar(s,h));
$endif
loop(s, put dynamics2(s,h));
);

loop(t,
 put / 'min ' tpobj(t);
 put ut1(t);
$ifthen.macro_control_t set nomacro
 loop(s, put ut2(s,t));
$else.macro_control_t
 loop(s, put ut2(s,t));
$endif.macro_control_t
 put CVaRtp(t) tpobjdef(t);
$ifthen not set IMPLICIT
* loop(s, put tpcvar(s,t));
$endif
);


put / 'vi meetdemand1 pi1';
put / 'vi meetdemand2 pi2';

$ifthen.macro_control_c_qsf set nomacro
 put / 'OVF ecvarup CVaRc ycp ' theta lambdaC/;
$else.macro_control_c_qsf
 put / 'OVF ecvarup CVaRc cpcvar ' theta lambdaC/;
$endif.macro_control_c_qsf
loop(h,
$ifthen.macro_control_h_qsf set nomacro
 put / "OVF ecvarup " CVaRhp(h);
 loop(s, put yhp(s,h));
$else.macro_control_h_qsf
 put / "OVF ecvarup " CVaRhp(h);
 loop(s, put hpcvar(s,h));
$endif.macro_control_h_qsf
 put theta lambdaH(h)/;
)

loop(t,
$ifthen.macro_control_t_qsf set nomacro
 put / "OVF ecvarup " CVaRtp(t);
 loop(s, put ytp(s,t));
$else.macro_control_t_qsf
 put / "OVF ecvarup " CVaRtp(t);
 loop(s, put tpcvar(s,t));
$endif.macro_control_t_qsf
 put theta lambdaT(t)/;
);
putclose empinfo /;

* Hot start from risk neutral
$onecho > %gams.emp%.op8
ovf_reformulation=%OVF_METHOD%
output_subsolver_log=1
subsolveropt=1
$offecho

$onecho > path.opt
convergence_tolerance=1e-12
$offecho

hydroemp.optfile=8

solve hydroemp using emp;

fuelcost(s) = sum(t, Cst(ut1.l(t))) + sum(t, Cst(ut2.l(s,t)));
futurecost(s) =  -sum(h, TermV(x2.l(s,h)));
totalcost(s)= fuelcost(s) +  futurecost(s) ;
secondstagecost(s)= sum(t, Cst(ut2.l(s,t)))+ futurecost(s) ;
expectedsecondcost =   sum(s, prob(s)* secondstagecost(s) ) ;
firstcost =  sum(t, Cst(ut1.l(t)));
expectedcost =  sum(t, Cst(ut1.l(t)))
 + sum(s, prob(s)* sum(t, Cst(ut2.l(s,t))) )
 - sum(s, prob(s)* sum(h, TermV(x2.l(s,h))));

parameter thermalprofit(t,s);
parameter hydroprofit(h,s);
parameter consumerbenefit(s);
thermalprofit(t,s) = pi1.l*ut1.l(t)              - Cst(ut1.l(t)
                  + pi2.l(s)*(ut2.l(s,t)       ) - Cst(ut2.l(s,t));
hydroprofit(h,s) = pi1.l*Util(uh1.l(h))
                 + pi2.l(s)*(Util(uh2.l(s,h))       )+ TermV(x2.l(s,h));
consumerbenefit(s) = -pi1.l*consume1.l - pi2.l(s)*consume2.l(s)+ Dem(consume1.l)
                   +Dem(consume2.l(s))   ;

parameter demandcost(s);
demandcost(s) = -Dem(consume1.l)- Dem(consume2.l(s));

parameter demandwelfare(s);
parameter totalwelfare(s);
parameter price1;
parameter price2(s);
price1 = pi1.l;
price2(s) = pi2.l(s);
thermalprofit(t,s) = price1*ut1.l(t)              - Cst(ut1.l(t)
                  + price2(s)*(ut2.l(s,t)       ) - Cst(ut2.l(s,t));
hydroprofit(h,s) = price1*Util(uh1.l(h))
                 + price2(s)*(Util(uh2.l(s,h))       )+ TermV(x2.l(s,h));
demandwelfare(s) = -price1*consume1.l -price2(s)*consume2.l(s)+ Dem(consume1.l)
                 +Dem(consume2.l(s));

totalwelfare(s) = sum(t,thermalprofit(t,s))+ sum(h,hydroprofit(h,s))+ demandwelfare(s);

* Few checks
PARAMETER hpobjL(h);
hpobjL(h) = pi1.l*Util(uh1.l(h)) + CVaRhp.l(h);
abort$[ smax{h, abs(hpObjL(h) - hpobj.l(h))} > 1e-10 ]  'bad hpobj', hpobjL;


parameter tsale(s);
parameter hpurchase(s);
parameter cpurchase(s);
tsale(s) =  0   ;
hpurchase(s) =  0   ;
cpurchase(s) =  0;

$include  "outputtrade05.inc";

execute '=diff -bBw equil.out ref/notrade15/equil.out';
abort$errorlevel 'hydroemp solve failed';

$exit

$set GDX_BASENAME '%OUTPUT_VARS%_%formulation%_%init%'
$if set nomacro $set GDX_BASENAME '%GDX_BASENAME%_nomacro'
$if set implicit $set GDX_BASENAME '%GDX_BASENAME%_implicit'

$ifthen.clean_cvar set clean_cvar
CVaRc.m = 0.;
CVaRhp.m(h) = 0.;
CVaRtp.m(t) = 0.;
$endif.clean_cvar

execute_unload '%GDX_BASENAME%.gdx' uh1, uh2, eh1, eh2, sh1, sh2, ut1, ut2, x2, x1, consume1, consume2, tpobj, hpobj, cpobj, CVaRc, CVaRhp, CVaRtp, pi1, pi2;
