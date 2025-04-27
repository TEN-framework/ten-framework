#include <incmode>.
#show show/0.
#const imin=4.

1 { p(1..4) } 1.

#show p(X) : p(X).

#program step(k).

q(k+1), k < 3.
:- p(X), q(X), k < 3.

{show} :- k == 3.
#show p(1..4) : show, k == 3.

