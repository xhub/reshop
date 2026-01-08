$onText

Contributor: Olivier Huber
Date:        January 2026

$offText


SET i, j, jj(j);

VARIABLE x(i), y(j);

EQUATION defF(i), defG(j);

embeddedCode reshop:
a(i): vi defF(i) x(i)
b(j)$(jj(j)): vi defG(j) y(j)
endEmbeddedCode
