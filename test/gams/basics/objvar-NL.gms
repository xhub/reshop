$TITLE simple example where the objective variable appears nonlinearly in the objective equation

SET k;

VARIABLES obj, x;

EQUATIONS defobj;

PARAMETERS c, cplus(k<) /
   1    1
   8    8
  /, cminus(k) /
   8  -8
   1  -1
 /;


defobj.. c*obj =E= sqrt(power(x, 4));

MODEL m / defobj /;

x.lo = 1.5e-1;

PARAMETERS xL, xM, objL, objM, defobjL, defobjM, tol / 1e-6 /;

option nlp=pathnlp;

loop(k,
   c = cplus(k);

   solve m min obj using NLP;

   xL = x.l; xM = x.m;
   objL = obj.l; objM = obj.m;
   defobjL = defobj.l; defobjM = defobj.m;

   embeddedCode ReSHOP:
   min obj x defobj
   endEmbeddedCode

   solve m using emp;

   abort$[abs(x.l - xL) > tol] 'wrong x.l value', x.l, xL;
   abort$[abs(x.m - xM) > tol] 'wrong x.m value', x.m, xM;

   abort$[abs(obj.l - objL) > tol] 'wrong obj.l value', obj.l, objL;
   abort$[abs(obj.m - objM) > tol] 'wrong obj.m value', obj.m, objM;

   abort$[abs(defobj.l - defobjL) > tol] 'wrong defobj.l value', defobj.l, defobjL;
   abort$[abs(defobj.m - defobjM) > tol] 'wrong defobj.m value', defobj.m, defobjM;

   embeddedCode ReSHOP:
   n: min obj x defobj
   root: Nash(n)
   endEmbeddedCode

   solve m using emp;

   abort$[abs(x.l - xL) > tol] 'wrong x.l value', x.l, xL;
   abort$[abs(x.m - xM) > tol] 'wrong x.m value', x.m, xM;

   abort$[abs(obj.l - objL) > tol] 'wrong obj.l value', obj.l, objL;
   abort$[abs(obj.m - objM) > tol] 'wrong obj.m value', obj.m, objM;

   abort$[abs(defobj.l - defobjL) > tol] 'wrong defobj.l value', defobj.l, defobjL;
   abort$[abs(defobj.m - defobjM) > tol] 'wrong defobj.m value', defobj.m, defobjM;

**************************************************************************************
* MAX case
**************************************************************************************

   c = cminus(k);

   solve m max obj using NLP;
   abort$[m.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m.solveStat;
   abort$[m.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m.modelStat;

   xL = x.l; xM = x.m;
   objL = obj.l; objM = obj.m;
   defobjL = defobj.l; defobjM = defobj.m;

   embeddedCode ReSHOP:
   max obj x defobj
   endEmbeddedCode

   solve m using emp;
   abort$[m.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m.solveStat;
   abort$[m.modelStat  > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m.modelStat;

   abort$[abs(x.l - xL) > tol] 'wrong x.l value', x.l, xL;
   abort$[abs(x.m - xM) > tol] 'wrong x.m value', x.m, xM;

   abort$[abs(obj.l - objL) > tol] 'wrong obj.l value', obj.l, objL;
   abort$[abs(obj.m - objM) > tol] 'wrong obj.m value', obj.m, objM;

   abort$[abs(defobj.l - defobjL) > tol] 'wrong defobj.l value', defobj.l, defobjL;
   abort$[abs(defobj.m - defobjM) > tol] 'wrong defobj.m value', defobj.m, defobjM;

   embeddedCode ReSHOP:
   n: max obj x defobj
   root: Nash(n)
   endEmbeddedCode

   solve m using emp;
   abort$[m.solveStat <> %SOLVESTAT.NORMAL COMPLETION%]   'solve failed', m.solveStat;
   abort$[m.modelStat > %MODELSTAT.LOCALLY OPTIMAL%]     'solve failed', m.modelStat;

   abort$[abs(x.l - xL) > tol] 'wrong x.l value', x.l, xL;
   abort$[abs(x.m - xM) > tol] 'wrong x.m value', x.m, xM;

   abort$[abs(obj.l - objL) > tol] 'wrong obj.l value', obj.l, objL;
   abort$[abs(obj.m - objM) > tol] 'wrong obj.m value', obj.m, objM;

   abort$[abs(defobj.l - defobjL) > tol] 'wrong defobj.l value', defobj.l, defobjL;
   abort$[abs(defobj.m - defobjM) > tol] 'wrong defobj.m value', defobj.m, defobjM;

)

