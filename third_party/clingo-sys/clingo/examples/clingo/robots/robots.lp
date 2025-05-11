#program base.
dir(-1,0;1,0;0,-1;0,1).

stop( DX, DY,X,   Y   ) :- barrier(X,Y,DX,DY).
stop(-DX,-DY,X+DX,Y+DY) :- stop(DX,DY,X,Y).

#external target(R,X,Y) : available_target(R,M,X,Y).
#external pos(R,X,Y,0) : dim(X), dim(Y), robot(R).

#program trans(t).

1 { move(R,DX,DY,t) : robot(R), dir(DX,DY) } 1.

halt(DX,DY,X-DX,Y-DY,t) :- pos(_,X,Y,t-1), dir(DX,DY), dim(X-DX), dim(Y-DY), not stop(-DX,-DY,X,Y).

goto(R,DX,DY,X,Y,t) :- pos(R,X,Y,t-1), dir(DX,DY).
goto(R,DX,DY,X+DX,Y+DY,t) :- goto(R,DX,DY,X,Y,t), dim(X+DX), dim(Y+DY), not stop(DX,DY,X,Y), not halt(DX,DY,X,Y,t).

pos(R,X,Y,t) :- move(R,DX,DY,t), goto(R,DX,DY,X,Y,t), not goto(R,DX,DY,X+DX,Y+DY,t).
pos(R,X,Y,t) :- pos(R,X,Y,t-1), not move(R,_,_,t).

:- move(R,DX,DY,t-1), not goon(t-1), not move(R,DX,DY,t).

#program state(t).

goon(t) :- target(R,X,Y), not pos(R,X,Y,t).
#minimize{ 1,t : goon(t) }.

#program check(t).

#external horizon(t).

:- goon(t), horizon(t).

#show move/4.
