FILE RESEMP2 / equil.out /;
PUT RESEMP2;
RESEMP2.nd=5;
RESEMP2.nw=10;
RESEMP2.ap=0;


put " Weight on CVaR (lambda) = ";
put lambda;
put /;
put " Initial storage = ";
put x0('1');
put /;

put " Expected Total Cost = ";
put expectedcost;
put /;

put " First stage cost = ";
put firstcost;
put /;
put " Expected second stage cost = ";
put expectedsecondcost;
put /;
put " Contract price = 0";
put /;


put "Costs ";
put /;
put "scenario   Fuel Cost    FutureCost   DemandCost  TotalCost  Stage2Cost  y(s)   ";
put /;
loop (s,
put ord(s);
put "  ";
put fuelcost(s);
put "  ";
put futurecost(s);
put "  ";
put demandcost(s);
put "  ";
put totalcost(s);
put "  ";
put secondstagecost(s);
put "  ";

put /;
)
put /;
put /;

put "Profits ";
put /;
put "scenario     Thermal     Hydro     ConsumerWelfare  Totalwelfare ";
put /;
loop (s,
put ord(s);
put "  ";
$if set report_profit put thermalprofit('1',s);
put "  ";
$if set report_profit put hydroprofit('1',s);
put "  ";
$if set report_profit put consumerbenefit(s);
put "  ";
put totalwelfare(s);
put "  ";

put /;
)

put /;
put /;
put "Demand ";
put /;
put "scenario   demand1           demand2(s)     ";
put /;
loop (s,
put ord(s);
put "  ";
put consume1.l;
put "  ";
put consume2.l(s)  ;
put "  ";

put /;
)
put /;
put /;



put "Electricity Prices ";
put /;
put "scenario   Pi(1)           Pi(2)         ";
put /;
loop (s,
put ord(s);
put "  ";
put pi1.l;
put "  ";
put pi2.l(s);
put "  ";



put /;
)
put /;
put /;

put "RiskContracts ";
put /;
put "scenario     tradet     tradeh    tradec   tradeprice(s)  ";
put /;
loop (s,
put ord(s);
put "  ";
put "  ";
put "  ";
put "  ";
put tradeprice.l(s);
put "  ";


put /;
)
put /;
put /;


put "Storage              Hydro 1               Hydro 2    ";
put /;
put "scenario        n = 1        n = 2       n = 1      n = 2    ";
put /;
loop (s,
put ord(s);
put "  ";
put x1.l('1');
put "  ";
put x2.l(s,'1');
put "  ";
put "  ";
put /;
)
put /;
put /;

put "Inflow              Hydro 1               Hydro 2    ";
put /;
put "scenario        n = 1        n = 2       n = 1      n = 2    ";
put /;
loop (s,
put ord(s);
put "  ";
put inflow(s,'1','1');
put "  ";
put inflow(s,'1','2');
put "  ";
put "  ";
put /;
)
put /;
put /;


put "Release              Hydro 1               Hydro 2    ";
put /;
put "scenario        n = 1        n = 2       n = 1      n = 2    ";
put /;
loop (s,
put ord(s);
put "  ";

put uh1.l('1');
put "  ";
put uh2.l(s,'1');
put "  ";
put "  ";


put /;
)
put /;
put /;

put "Spill              Hydro 1               Hydro 2    ";
put /;
put "scenario        n = 1        n = 2       n = 1      n = 2    ";
put /;
loop (s,
put ord(s);
put "  ";

put sh1.l('1');
put "  ";
put sh2.l(s,'1');
put "  ";
put "  ";

put /;
)
put /;
put /;


put "Thermals              Thermal 1               Thermal 2    ";
put /;
put "scenario        n = 1        n = 2       n = 1      n = 2    ";
put /;
loop (s,
put ord(s);
put "  ";

put ut1.l('1');
put "  ";
put ut2.l(s,'1');
put "  ";
put "  ";


put /;
)
put /;


put /;
put "Objective values       Thermal                Hydro     ";
put /;
put "                  n = 1        n = 2       n = 1      n = 2    ";
put /;
put "               ";
put  tpobj.l('1');
put "  ";
put "  ";
put "  ";
put hpobj.l('1');
put "  ";
put "  ";
put /;

putclose /;

