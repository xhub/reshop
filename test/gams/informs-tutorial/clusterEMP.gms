$ontext

Cluster s points in R^n into r groups

$offtext

$if not set seed $set seed 4
set s(*), c(*), r / k1*k15 /;
alias(r,k);
parameter data(s<,c<);

$ontext
* alternative using GAMS Connect
$onEmbeddedCode Connect:
- CSVReader:
    file: s1.txt
    name: data
    autoRow: ''
    names: ['x', 'y']
    valueColumns: [1, 2]
    fieldSeparator: '\s+'
    decimalSeparator: '.'
    header: False
    trace: 3
- GAMSWriter:
    symbols: [ {name: data} ]
$offEmbeddedCode

display data;
$offtext

$onEmbeddedCode Python:
import pandas as pd
import gams.transfer as gt

def from2dim(df, column_names=None):
    df.index = df.index + 1
    if column_names is None:
        return pd.DataFrame(df.stack()).reset_index()
    else:
        df = pd.DataFrame(df.stack()).reset_index()
        return df.rename(columns=dict(zip(df.columns, column_names)))

data = pd.read_csv('s1.txt',header=None,sep=r'\s+',names=['x', 'y'])
df = from2dim(data, column_names=['x', 'y'])
m = gt.Container()
data = m.addParameter('data',['*','*'],records=df)
m.write(gams.db, compress=False)
$offEmbeddedCode

SCALAR theta / 1e+1 /;
SCALAR scalefactor / 1e6 /, toler / 1e-6 /;
data(s,c) = data(s,c)/scalefactor;

display s, c, data;

VARIABLES x(r,c), dist(s,r);
EQUATION defdist(s,r);
defdist(s,r)..
  dist(s,r) =e= sum(c, sqr(data(s,c) - x(r,c)));

model cluster / defdist /;

option seed = %seed%;
x.l(r,c) = uniform(0,1);


SET iter / i1*i5 /;

loop(iter,
* Where does x go here?
* TODO: f(s): MP('smin', dist(s,r)) FAILS!

embeddedCode ReSHOP:
deffn dist(s,r) defdist(s,r)
f(s): MP('smin', dist(s,r))
main: min SUM(s, f(s).valfn.smooth(par=theta)) x(r,c)
endEmbeddedCode

solve cluster using emp;

abort$[cluster.modelStat > %MODELSTAT.LOCALLY OPTIMAL%]      'solve failed', cluster.modelStat;
abort$[cluster.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', cluster.solveStat;
  theta = theta*5;
);

$ontext
  How do we extract out the pieces automatically:
  F is just a function or a smin of functions
  (or could be max problem with smax of functions)
  Then we get H and h_0
$offtext
  
$exit
* Rest is just for solution

*  function LSEMinSc  'scaled smoothed min via log of sum of exp' / lselib.LSEMinSc /;
$ macro smth(theta,k,v) LSEMinSc(theta, v(s,'k1'), v(s,'k2'), v(s,'k3'), v(s,'k4'), v(s,'k5'), v(s,'k6'), v(s,'k7'), v(s,'k8'), v(s,'k9'), v(s,'k10'), v(s,'k11'), v(s,'k12'), v(s,'k13'), v(s,'k14'), v(s,'k15'))
$macro fapprox(s) smth(theta,k,dist)

* change to f(s) for nonsmooth dnlp approach
defobj..
  obj =e= sum(s, fapprox(s));
*  obj =e= sum(s, f(s));

model cluster / defobj /;
option seed = %seed%;
x.l(r,c) = uniform(0,1);
solve cluster using nlp min obj;
theta = theta*5;
solve cluster using nlp min obj;
theta = theta*5;
solve cluster using nlp min obj;
theta = theta*5;
solve cluster using nlp min obj;

parameter distances(s,r), clustsz(r), totassign, resid;
set results(s,r);
$onDotl
distances(s,r) = dist(s,r);
results(s,r) = yes$(abs(f(s) - distances(s,r)) le toler);
resid = sum(s, f(s));
$offDotl
clustsz(r) = sum(results(s,r), 1);
totassign = sum(r, clustsz(r));
display results, clustsz, totassign, resid;

Parameter mean(s), var(s), assign(s);
mean(s) = data(s,'x');
var(s) = data(s,'y');
assign(s) = smin(r$results(s,r), r.ord);

$log --- Using Python library %sysEnv.GMSPYTHONLIB%
embeddedCode Python:
  import numpy 
  import pandas

  import matplotlib
  import matplotlib.pyplot as plt
  import seaborn as sns

  sns.set(style='ticks')
  
  sns.relplot(x=gams.get('mean', keyFormat=KeyFormat.SKIP), y=gams.get('var', keyFormat=KeyFormat.SKIP), hue=[str(s) for s in gams.get('assign', keyFormat=KeyFormat.SKIP)], aspect=1.00)
  # hue_order=
  # plt.show()
  resid = next(gams.get('resid'))
  plt.title(f'Residual = {resid:.2f}')
  # plt.xlabel('return')
  # plt.ylabel('variance')
  plt.savefig('cluster.pdf')
endEmbeddedCode
