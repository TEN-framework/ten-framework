#const f = 3.
#const a = 1.
#const s = 2.

#include <incmode>.

#program base.

a(e(1)).
f(1).
f(2).
f(3).
i(a(e(1),2)).
i(y(z(1),3)).
i(y(c(up),1)).

dir( 1).
dir(-1).

t(F) :- i(y(z(E),F)).
u(F) :- i(y(c(D),F)).
e(F) :- u(F), not t(F).

g(e(E),F, 1) :- i(y(z(E),F)), i(a(e(E),G)), G < F.
g(e(E),F,-1) :- i(y(z(E),F)), i(a(e(E),G)), F < G.
g(A,D)              :- g(A,F,D).
g(A)                :- g(A,D).

r(A,F) :- g(A,F,D).
r(A,F) :- a(A), e(F).

l(A,F, 1) :- i(a(A,G)), F != #inf, F = #max{H : r(A,H), G < H}.
l(A,F,-1) :- i(a(A,G)), F != #sup, F = #min{H : r(A,H), H < G}.
l(A,D)    :- l(A,F,D).

p(A,-1) :- l(A,-1), l(A,1), not g(A).
p(A,-1) :- l(A,-1), g(A,1).
p(A, 1) :- g(A,-1), l(A,1), not g(A,1).

c(A,F,D) :- l(A,L,D), f(F), i(a(A,G)), H = D*F, D*G < H, H <= D*L.
c(A,F,D) :- l(A,L,-D), f(F), i(a(A,G)), H = D*F, D*L < H, H <= D*G, p(A,D).

{ a(A,F) : a(A) } = 1 :- e(F).

t(A,F) :- a(A,F).
t(e(E),F) :- i(y(z(E),F)).

o(A,F,D) :- t(A,F), i(a(A,G)), dir(D), D*G < D*F.
o(A,F,D) :- o(A,F+D,D), i(a(A,G)), c(A,F,D), D*G < D*F.
o(A,F,D) :- o(A,G+D,D), i(a(A,G)), c(A,F,D), o(A,F-D,-D), p(A,D).

x(A, 1,0) :- g(A, 1).
x(A,-1,0) :- g(A,-1), not g(A,1).
x(A, 1,0) :- i(a(A,F)), o(A,F+1, 1), not g(A).
x(A,-1,0) :- i(a(A,F)), o(A,F-1,-1), not g(A), not o(A,F+1,1).

h(X,0) :- i(X).

#program step(t).

c(A,F,t) :- h(a(A,F),t-1), t(A,F), u(F).
c(F,t)   :- c(A,F,t).

d(e(E),s,t) :- h(y(z(E),F),t-1), h(a(e(E),F),t-1).
d(A,s,t):- h(y(c(D),F),t-1), c(A,F,t), e(F).
d(A,m(D),t) :- x(A,D,t-1), not d(A,s,t).

h(y(z(E),F),t) :- h(y(z(E),F),t-1), not h(a(e(E),F),t-1).
h(y(c(D),F),t) :- h(y(c(D),F),t-1), not c(F,t).
h(a(A,F),t) :- h(a(A,F-D),t-1), d(A,m(D),t), c(A,F,D).
h(a(A,F),t) :- h(a(A,F),t-1), not h(a(A,F+1),t), not h(a(A,F-1),t).

x(A,D,t) :- h(a(A,F),t-1), o(A,F+D,D), x(A,D,t-1).
x(A,D,t) :- h(a(A,F),t-1), o(A,G+D,D), p(A,D), i(a(A,G)), D*F < D*G,
                not o(A,F-D,-D).

#program check(t).

:- query(t), t < 1.

#show.
#show h/2.



