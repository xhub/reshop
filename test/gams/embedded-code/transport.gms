$title A Transportation Problem with multiple version LP/MIP/MINLP
$onText
This problem finds a least cost shipping schedule that meets
requirements at markets and supplies at factories.


Dantzig, G B, Chapter 3.3. In Linear Programming and Extensions.
Princeton University Press, Princeton, New Jersey, 1963.

$offText

Set
   i      'canning plants'
   j      'markets'
   t      'available model types' / lp, mip, minlp /
   locHdr 'location data header'  / lat, lng /;

Parameter
   a(i<) 'capacity of plant i in cases'
        / Seattle     350
          San-Diego   600 /

   b(j<) 'demand at market j in cases'
        / New-york   325
          Chicago   300
          Topeka   275 /;

Table d(i,j) 'distance in thousands of miles'
              New-York  Chicago  Topeka
Seattle         2.5       1.7     1.8
San-Diego       2.5       1.8     1.4;

Scalar f 'freight in dollars per case per thousand miles' / 90 /
       minS 'minimum shipment (MIP- and MINLP-only)' / 100 /
       beta 'beta (MINLP-only)' / 0.95 /;

Table ilocData(i,locHdr) 'Plant location information'
               lat           lng     
Seattle     47.608013  -122.335167
San-Diego   32.715736  -117.161087;

Table jlocData(j,locHdr) 'Market location information'
           lat           lng     
New-York   40.730610  -73.935242
Chicago    41.881832  -87.623177
Topeka     39.056198  -95.695312;

Parameter
c(i,j) 'transport cost in thousands of dollars per case';
c(i,j) = f*d(i,j)/1000;


Variable
   x(i,j) 'shipment quantities in cases'
   z      'total transportation costs in thousands of dollars';

Positive Variable x;

Equation
   cost        'define objective function'
   supply(i)   'observe supply limit at plant i'
   demand(j)   'satisfy demand at market j';

cost ..        z  =e=  sum((i,j), c(i,j)*x(i,j));

supply(i).. sum(j, x(i,j)) =l= a(i);

demand(j).. sum(i, x(i,j)) =g= b(j);

Model transport / all /;

transport.justScrDir = 1;
solve transport using lp minimizing z;

* The rest requires the reshop module being installed
$exit

* Q: gamscntr could be changed via solverCntr cmdline option?

embeddedCode Python:
import reshop as rhp
m = rhp.Model(r"%gams.scrdir%gamscntr.%gams.scrext%")
m.solve(r"%gams.scrdir%savepoint.gdx")
endEmbeddedCode

execute_loadpoint '%gams.scrdir%savepoint.gdx';

abort$(transport.modelstat < 2) "No feasible solution found"

display x.l;
