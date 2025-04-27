#include <incmode>.

#show jump/4.

#program base.

stone(X,Y,0) :- stone(X,Y).
field(X,Y,D) :- field(X,Y), field(X+2*DX,Y+2*DY), dir(D,DX,DY).

#program step(t).

1 { jump(X,Y,D,t) : field(X,Y,D) }.
remove(X+DX,Y+DY,t) :- jump(X,Y,D,t), dir(D,DX,DY).

 :- jump(X,Y,D,t), not stone(X,Y,t-1).
 :- jump(X,Y,D,t), stone(X+2*DX,Y+2*DY,t-1), dir(D,DX,DY).
 :- remove(X,Y,t), not stone(X,Y,t-1), dir(D,DX,DY).

 :- jump(X,Y,D,t), jump(X,Y,D',t), D != D'.
 :- jump(X,Y,D,t), jump(X',Y',D',t), dir(D,DX,DY), dir(D',DX',DY'), X+DX == X'+DX', Y+DY == Y'+DY', D != D'.
 :- jump(X,Y,D,t), jump(X',Y',D',t), dir(D,DX,DY), dir(D',DX',DY'), X+2*DX == X'+2*DX', Y+2*DY == Y'+2*DY', D != D'.
 :- jump(X,Y,D,t), remove(X,Y,t).

 :- fix_jump(X,Y,D,t), not jump(X,Y,D,t).

stone(X+2*DX,Y+2*DY,t) :- jump(X,Y,D,t), dir(D,DX,DY).
stone(X,Y,t)           :- stone(X,Y,t-1), not jump(X,Y,_,t), not remove(X,Y,t).

#program check(t).

 :- query(t), goal(X,Y), not stone(X,Y,t).
 :- query(t), not goal(X,Y), stone(X,Y,t).

