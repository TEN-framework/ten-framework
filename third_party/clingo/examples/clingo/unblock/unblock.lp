#include <incmode>.

#program base.
% first time step
stone(0,S,D,X,Y,L) :- stone(S,D,X,Y,L).

#program step(t).
% select possible moves
{ move(t,S,x,X2) : field(X2,Y), field(X2+L-1,Y), X1 != X2 } 1 :- stone(t-1,S,x,X1,Y,L).
{ move(t,S,y,Y2) : field(X,Y2), field(X,Y2+L-1), Y1 != Y2 } 1 :- stone(t-1,S,y,X,Y1,L).

% transition function
stone(t,S,x,X2,Y,L) :- stone(t-1,S,x,X1,Y,L), move(t,S,x,X2).
stone(t,S,y,X,Y2,L) :- stone(t-1,S,y,X,Y1,L), move(t,S,y,Y2).

% inertia
stone(t,S,D,X,Y,L) :- stone(t-1,S,D,X,Y,L), not move(t,S,_,_).

% field blocked by some stone
blocked(t,X+(0..L-1),Y) :- stone(t-1,S,x,X,Y,L).
blocked(t,X,Y+(0..L-1)) :- stone(t-1,S,y,X,Y,L).

%:- 2 { move(t,_,_,_) }.
% allow for parallel moves 
:- 2 { move(t,S,x,X) }, field(X,_).
:- 2 { move(t,S,y,Y) }, field(_,Y).
:- move(t,_,x,_), move(t,_,y,_).

% fail if move is blocked
:- blocked(t,X3,Y), stone(t-1,S,x,X1,Y,L), move(t,S,x,X2), X1 < X2, X1 + L <= X3, X3 < X2 + L.
:- blocked(t,X3,Y), stone(t-1,S,x,X1,Y,_), move(t,S,x,X2), X2 < X1, X2     <= X3, X3 < X1.
:- blocked(t,X,Y3), stone(t-1,S,y,X,Y1,L), move(t,S,y,Y2), Y1 < Y2, Y1 + L <= Y3, Y3 < Y2 + L.
:- blocked(t,X,Y3), stone(t-1,S,y,X,Y1,_), move(t,S,y,Y2), Y2 < Y1, Y2     <= Y3, Y3 < Y1.

% some domain knowledge (redundant)
:- stone(t,S1,x,X1,Y,N1), stone(t,S2,x,X2,Y,_), S1 != S2, X1 <= X2, X2 < X1 + N1.
:- stone(t,S1,y,X,Y1,N1), stone(t,S2,y,X,Y2,_), S1 != S2, Y1 <= Y2, Y2 < Y1 + N1.
:- stone(t,_,x,X1,Y1,N1), stone(t,_,y,X2,Y2,N2), X2 >= X1, X2 < X1 + N1, Y1 >= Y2, Y1 < Y2 + N2.

#program check(t).
:- target(S,X,Y), not stone(t,S,_,X,Y,_), query(t).

#show move/4.

% just for output
#show field/2.
#show stone/5.
#show target/3.
