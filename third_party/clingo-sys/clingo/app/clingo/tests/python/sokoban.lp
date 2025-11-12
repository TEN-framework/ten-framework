#script(python)

from clingo import Function, Number, SolveResult
from sys import stderr

def main(prg):
    prg.configuration.solve.opt_mode = "optN"
    prg.configuration.solve.models  = "0"
    t = 0
    e = 2
    parts = []
    parts.append(("base", []))
    parts.append(("state", [Number(t)]))
    while True:
        prg.ground(parts)
        prg.release_external(Function("volatile", [Number(t-1)]))
        prg.cleanup()
        prg.assign_external(Function("volatile", [Number(t)]), True)
        ret = prg.solve()
        if ret.satisfiable is False and t > 20: break
        if ret.satisfiable is True:
            e -= 1
            if e == 0: break
        t += 1
        parts = []
        parts.append(("trans", [Number(t)]))
        parts.append(("state", [Number(t)]))

#end.

#program base.

dir(u, 0, 1).
dir(d, 0,-1).
dir(r, 1, 0).
dir(l,-1, 0).

box(X,Y,0) :- initial_box(X,Y).
at(X,Y,0)  :- initial_at(X,Y).

#program trans(t).

% reachable places
route(X,Y,t)       :- at(X,Y,t-1).
route(X+DX,Y+DY,t) :- route(X,Y,t), dir(D,DX,DY), square(X+DX,Y+DY), not box(X+DX,Y+DY,t-1).

% select place to move from
1 { push_from(X,Y,D,t) : route(X,Y,t), dir(D,DX,DY), square(X+DX,Y+DY), box(X+DX,Y+DY,t-1) } 1.

% move horizontally or vertically
move(X,Y,D,t)           :- push_from(X,Y,D,t).
{ move(X+DX,Y+DY,D,t) } :- move(X,Y,D,t), dir(D,DX,DY), square(X+2*DX,Y+2*DY), not box(X+2*DX,Y+2*DY,t-1).

% target location
move_to(X,Y,D,t)     :- move(X,Y,D,t), not move(X+DX,Y+DY,D,t), dir(D,DX,DY).
push_to(X+DX,Y+DY,t) :- move_to(X,Y,D,t), dir(D,DX,DY).

% state transition
at(X-DX,Y-DY,t) :- move_to(X,Y,D,t), dir(D,DX,DY).
box(X,Y,t)      :- push_to(X,Y,t).
box(X,Y,t)      :- box(X,Y,t-1), not push_from(X-DX,Y-DY,D,t) : dir(D,DX,DY).

#show push_from(X,Y,t) : push_from(X,Y,D,t).
#show move_to(X,Y,t) : move_to(X,Y,D,t).
#show push_to/3.

#program state(t).

#minimize { X@1,Y,t : box(X,Y,t) }.
#minimize { Y@2,X,t : box(X,Y,t) }.

#external volatile(t).

 :- target_square(X,Y), not box(X,Y,t), volatile(t).

#program base.

square(1, 1).
target_square(1, 1).
square(2, 1).
target_square(2, 1).
square(1, 2).
target_square(1, 2).
square(2, 2).
target_square(2, 2).
initial_at(5,3).
square(1, 3).
square(2, 3).
initial_box(2, 3).
square(3, 3).
square(4, 3).
initial_box(4, 3).
square(5, 3).
square(1, 4).
square(2, 4).
square(3, 4).
initial_box(3, 4).
square(4, 4).
initial_box(4, 4).
square(5, 4).
square(1, 5).
square(2, 5).
square(3, 5).
square(4, 5).
square(5, 5).
