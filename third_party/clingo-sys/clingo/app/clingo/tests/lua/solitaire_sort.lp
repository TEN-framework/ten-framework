#script (lua)

function main(prg)
    step = 1
    while true do
        parts = {}
        if step > 1 then
            prg:release_external(clingo.Function("volatile", {step-1}))
        else
            parts[#parts+1] = {"base", {}}
        end
        parts[#parts+1] = {"cumulative", {step}}
        prg:ground(parts)
        prg:assign_external(clingo.Function("volatile", {step}), true)
        ret = prg:solve()
        if ret.satisfiable then
            break
        end
        step = step + 1
    end
end

#end.

#const parallel=yes. % replace by anything else than "yes" to disable parallel jumps
#const clear=32.     % (minimum) number of cleared grid cells in goal configuration

#program base.

% grid cells

gc(f(c,1..2,3..5)). gc(f(c,3..5,1..7)). gc(f(c,6..7,3..5)).

% initial configuration

                            in(f(c,3,1)). in(f(c,4,1)). in(f(c,5,1)).
                            in(f(c,3,2)). in(f(c,4,2)). in(f(c,5,2)).
in(f(c,1,3)). in(f(c,2,3)). in(f(c,3,3)). in(f(c,4,3)). in(f(c,5,3)). in(f(c,6,3)). in(f(c,7,3)).
in(f(c,1,4)). in(f(c,2,4)). in(f(c,3,4)).               in(f(c,5,4)). in(f(c,6,4)). in(f(c,7,4)).
in(f(c,1,5)). in(f(c,2,5)). in(f(c,3,5)). in(f(c,4,5)). in(f(c,5,5)). in(f(c,6,5)). in(f(c,7,5)).
                            in(f(c,3,6)). in(f(c,4,6)). in(f(c,5,6)).
                            in(f(c,3,7)). in(f(c,4,7)). in(f(c,5,7)).

% goal configurations(s)

on(f(c,4,4)).
un(clear).

% generic predicates for sorting

sort(I,J) :- sort(I), sort(J), I < J, #false : sort(K), I < K, K < J.
prec(I,J) :- sort(I,J), not lead(J).
succ(J)   :- prec(_,J).
succ(I,J) :- prec(I,J), succ(I).

% sort possible jumps

dir(-1,0). dir(0,-1). dir(0,1). dir(1,0).

sort(f(j,X-D,Y-E,X+D,Y+E)) :- gc(f(c,X,Y)), dir(D,E), gc(f(c,X-D,Y-E)), gc(f(c,X+D,Y+E)).
lead(f(j,X,Y,XX,YY))       :- sort(f(j,X,Y,XX,YY)), not sort(f(j,V,W,VV,WW),f(j,X,Y,XX,YY))
                                                      : sort(f(j,V,W,VV,WW),f(j,X,Y,XX,YY)).

% sort jumps wrt target grid cells

sort(f(i,X,Y,XX,YY)) :- sort(f(j,X,Y,XX,YY)).
lead(f(i,X,Y,XX,YY)) :- sort(f(i,X,Y,XX,YY)), not sort(f(i,X,Y,VV,WW),f(i,X,Y,XX,YY))
                                                : sort(f(i,X,Y,VV,WW),f(i,X,Y,XX,YY)).

% sort jumps wrt cleared grid cells

sort(f(k,XX,YY,X,Y,XX,YY))             :- sort(f(j,X,Y,XX,YY)).
sort(f(k,(X+XX)/2,(Y+YY)/2,X,Y,XX,YY)) :- sort(f(j,X,Y,XX,YY)).
lead(f(k,XXX,YYY,X,Y,XX,YY)) :- sort(f(k,XXX,YYY,X,Y,XX,YY)),
                            not sort(f(k,XXX,YYY,V,W,VV,WW),f(k,XXX,YYY,X,Y,XX,YY))
                              : sort(f(k,XXX,YYY,V,W,VV,WW),f(k,XXX,YYY,X,Y,XX,YY)).

% sort jumps wrt smallest cleared or target grid cells (if parallel moves disabled)

sort(f(l,0,XX,YY,X,Y,XX,YY)) :- sort(f(j,X,Y,XX,YY)), (XX,YY) < (X,Y), parallel != yes.
sort(f(l,1,-X,-Y,X,Y,XX,YY)) :- sort(f(j,X,Y,XX,YY)), (X,Y) < (XX,YY), parallel != yes.
lead(f(l,L,AA,BB,X,Y,XX,YY)) :- sort(f(l,L,AA,BB,X,Y,XX,YY)),
                            not sort(f(l,M,CC,DD,V,W,VV,WW),f(l,L,AA,BB,X,Y,XX,YY))
                              : sort(f(l,M,CC,DD,V,W,VV,WW),f(l,L,AA,BB,X,Y,XX,YY)).

% calculate grid symmetries

axis(x,M+N) :- M = #min { X : gc(f(c,X,_)) }, N = #max { X : gc(f(c,X,_)) }.
axis(y,M+N) :- M = #min { Y : gc(f(c,_,Y)) }, N = #max { Y : gc(f(c,_,Y)) }.

swap(0,f(c,X,Y),f(c,X,Y))      :- gc(f(c,X,Y)).
swap(1,f(c,X,Y),f(c,N-X,Y))    :- gc(f(c,X,Y)), axis(x,N).
swap(S+2,f(c,X,Y),f(c,XX,N-Y)) :- swap(S,f(c,X,Y),f(c,XX,Y)), axis(y,N), S < 2.
swap(S+4,f(c,X,Y),f(c,YY,XX))  :- swap(S,f(c,X,Y),f(c,XX,YY)), S < 4.

sort(f(s,S,XX,YY,X,Y)) :- swap(S,f(c,X,Y),f(c,XX,YY)), (X,Y) != (XX,YY).
lead(f(s,S,XX,YY,X,Y)) :- sort(f(s,S,XX,YY,X,Y)), not sort(f(s,S,VV,WW,V,W),f(s,S,XX,YY,X,Y))
                                                    : sort(f(s,S,VV,WW,V,W),f(s,S,XX,YY,X,Y)).

asym(f(s,S,XX,YY,X,Y)) :- sort(f(s,S,XX,YY,X,Y)), not sort(f(s,S,X,Y,XX,YY)).

less(f(s,S,XX,YY,X,Y)) :- sort(f(s,S,XX,YY,X,Y)), (XX,YY) < (X,Y).

here(I)   :- less(I), lead(I).
here(I,J) :- less(J), prec(I,J).

more(I,J) :- prec(I,J), here(J,_).
more(I,J) :- prec(I,J), more(J,_).
more(I)   :- more(I,_), lead(I).

% initial configuration

hold(C,0) :- in(C).

#program cumulative(t).

% generate jumps

poss(f(j,X,Y,XX,YY),t) :- sort(f(j,X,Y,XX,YY)), not hold(f(c,X,Y),t-1),
                          hold(f(c,XXX,YYY),t-1) : sort(f(k,XXX,YYY,X,Y,XX,YY)).

{ move(J,t) } :- poss(J,t).

% propagate effects of jumps

fill(J,t)                      :- move(J,t), succ(J).
fill(f(i,X,Y,XX,YY),t)         :- move(f(j,X,Y,XX,YY),t), succ(f(i,X,Y,XX,YY)).
fill(f(k,XXX,YYY,X,Y,XX,YY),t) :- move(f(j,X,Y,XX,YY),t), succ(f(k,XXX,YYY,X,Y,XX,YY)).
fill(f(l,L,AA,BB,X,Y,XX,YY),t) :- move(f(j,X,Y,XX,YY),t), succ(f(l,L,AA,BB,X,Y,XX,YY)).
fill(I,t)                      :- fill(J,t), succ(I,J).

grep(I,t)                      :- fill(J,t), prec(I,J), lead(I).
grep(J,t)                      :- move(J,t), lead(J).
grep(f(i,X,Y,XX,YY),t)         :- move(f(j,X,Y,XX,YY),t), lead(f(i,X,Y,XX,YY)).
grep(f(k,XXX,YYY,X,Y,XX,YY),t) :- move(f(j,X,Y,XX,YY),t), lead(f(k,XXX,YYY,X,Y,XX,YY)).

% assert application of some jump

 :- lead(f(j,X,Y,XX,YY)), not grep(f(j,X,Y,XX,YY),t).

% forbid parallel jumps (if disabled)

 :- prec(I,J), move(I,t), fill(J,t), parallel != yes.

% forbid conflicting parallel jumps (if parallel jumps not disabled)

 :- prec(f(i,X,Y,XX,YY),J), move(f(j,X,Y,XX,YY),t), fill(J,t), parallel == yes.
 :- prec(f(k,XXX,YYY,X,Y,XX,YY),J), move(f(j,X,Y,XX,YY),t), fill(J,t), parallel == yes.

% successor configuration

hold(f(c,X,Y),t)     :- grep(f(i,X,Y,_,_),t).
hold(f(c,XXX,YYY),t) :- hold(f(c,XXX,YYY),t-1), lead(f(k,XXX,YYY,X,Y,XX,YY)),
                        not grep(f(k,XXX,YYY,X,Y,XX,YY),t).

% assert order among independent jumps

 :- move(I,t), poss(I,t-1), parallel == yes.
 :- move(f(j,X,Y,XX,YY),t), poss(f(j,X,Y,XX,YY),t-1), parallel != yes,
    fill(f(l,L,AA,BB,X,Y,XX,YY),t-1) : succ(f(l,L,AA,BB,X,Y,XX,YY)).

% assert lexicographically smallest successor configuration wrt symmetric configurations

diff(S,t) :- sort(f(s,S,XX,YY,X,Y)), hold(f(c,X,Y),t-1), not hold(f(c,XX,YY),t-1).
 :- asym(f(s,S,XX,YY,X,Y)), hold(f(c,XX,YY),t-1), not hold(f(c,X,Y),t-1), not diff(S,t).

comp(S,f(s,S,XX,YY,X,Y),t) :- more(f(s,S,XX,YY,X,Y)), hold(f(c,XX,YY),t).
comp(S,f(s,S,XX,YY,X,Y),t) :- more(f(s,S,XX,YY,X,Y)), not hold(f(c,X,Y),t).
comp(S,f(s,S,XX,YY,X,Y),t) :- more(I,f(s,S,XX,YY,X,Y)), comp(S,I,t), hold(f(c,XX,YY),t).
comp(S,f(s,S,XX,YY,X,Y),t) :- more(I,f(s,S,XX,YY,X,Y)), comp(S,I,t), not hold(f(c,X,Y),t).

 :- here(f(s,S,XX,YY,X,Y)), hold(f(c,XX,YY),t), not hold(f(c,X,Y),t), not diff(S,t).
 :- here(I,f(s,S,XX,YY,X,Y)), hold(f(c,XX,YY),t), not hold(f(c,X,Y),t), not diff(S,t),
    comp(S,I,t).

% limit search space ...

:- not move(f(j,4,4,2,4),t), t == 1.
:- not move(f(j,5,3,5,1),t), t == 5.
:- not move(f(j,5,1,3,1),t), t == 6.
:- not move(f(j,5,2,5,4),t), t == 6.
:- not move(f(j,5,5,7,5),t), t == 6.
:- not move(f(j,6,5,4,5),t), t == 7.
:- not move(f(j,5,3,5,1),t), t == 7.
:- not move(f(j,4,4,4,6),t), t == 13.

#external volatile(t).
% assert goal configuration(s)

 :- on(C), not hold(C,t), volatile(t).
 :- un(N), not N { not hold(C,t) : gc(C) }, volatile(t).

#show move/2.
