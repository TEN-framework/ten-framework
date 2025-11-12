#show queen/2.

#program board(n).
#external attack(n,1..n,h).
#external attack(1..n,n,v).

target(n,  X,  X,  n,  b,n) :- X = 1..n-1.              % diagonal b
target(Y,  n-1,n,  Y-1,b,n) :- Y = 2..n-1.              % diagonal b
target(X,  n-1,X+1,n,  f,n) :- X = 1..n-1.              % diagonal f
target(n-1,Y,  n,  Y+1,f,n) :- Y = 1..n-2.              % diagonal f
target(X,  n,  X-1,n,  h,n) :- X = 2..n.                % horizontal
target(n,  Y,  n-1,Y,  h,n) :- Y = 1..n-1.              % horizontal
target(Y,  X,  Y,  X-1,v,n) :- target(X,Y,X-1,Y,h,n).   % vertical

{ queen(1..n,n); queen(n,1..n-1) }.

attack(X',Y',D) :- target(X,Y,X',Y',D,n), queen(X,Y).
attack(X',Y',D) :- target(X,Y,X',Y',D,n), attack(X,Y,D).

:- target(X,Y,X',Y',D,n), attack(X',Y',D), queen(X',Y').

:- not queen(1,n), not attack(1,n,h).
:- not queen(n,1), not attack(n,1,v).
