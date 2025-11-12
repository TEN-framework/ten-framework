#external a.
a :- b.
b :- a.

x :- not x.
x :- y.
y :- x.

u :- not v.
v :- not u.

r.
r :- s.
s :- r.
