$TITLE Test handling of invalid embcode

$onText

Contributor: Olivier Huber <oli.huber@gmail.com>
Date:        October 2025
$offText

SET i /i1*i4/, n /n1*n15/;

embeddedcode ReSHOP:
n(i)$(i.first): min obj(i) x(i) defobj(i)
endEmbeddedCode
