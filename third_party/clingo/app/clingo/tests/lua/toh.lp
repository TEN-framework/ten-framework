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

#show put/3.

#program cumulative(k).

1{move(N,k)  : disk(N), N > 3}1.
1{where(N,k) : disk(N)}1.

on(M,N,k) :- disk(M;N), move(M,k), where(N,k).
on(M,N,k) :- disk(M;N), on(M,N,k-1), not move(M,k).

:- disk(N), move(N,k-1), move(N,k).
:- disk(M;N), on(M,N,k-1), move(N,k).
:- disk(M;N), on(M,N,k-1), where(N,k).
:- disk(M;N), on(M,N,k), M <= N.

put(M,N,k) :- disk(M;N), move(M,k), where(N,k).

#external volatile(k).

:- goal_on(X,Y), not on(X,Y,k), volatile(k).

% restrict search space a little
%:- not put(9,8,k), k==1.
%:- not put(4,3,k), k==2.
%:- not put(9,4,k), k==3.
%:- not put(8,1,k), k==4.
:- not put(9,8,k), k==5.
%:- not put(7,4,k), k==6.
%:- not put(9,6,k), k==7.
%:- not put(8,7,k), k==8.
%:- not put(9,8,k), k==9.
%:- not put(6,1,k), k==10.
%:- not put(9,6,k), k==11.
%:- not put(8,5,k), k==12.
%:- not put(9,8,k), k==13.
%:- not put(7,6,k), k==14.
%:- not put(9,4,k), k==15.
:- not put(8,7,k), k==16.
%:- not put(9,8,k), k==17.
%:- not put(5,4,k), k==18.
%:- not put(9,2,k), k==19.
%:- not put(8,5,k), k==20.
:- not put(9,8,k), k==21.
%:- not put(7,2,k), k==22.
%:- not put(9,6,k), k==23.
%:- not put(8,7,k), k==24.
%:- not put(9,8,k), k==25.
:- not put(6,5,k), k==26.
%:- not put(9,6,k), k==27.
%:- not put(8,1,k), k==28.
%:- not put(9,8,k), k==29.
:- not put(7,6,k), k==30.
%:- not put(9,2,k), k==31.
%:- not put(8,7,k), k==32.
%:- not put(9,8,k), k==33.
#program base.

on(X,Y,0) :- init_on(X,Y).
disk(1).
disk(2).
disk(3).
disk(4).
disk(5).
disk(6).
disk(7).
disk(8).
disk(9).

init_on(9,4).
init_on(4,1).
init_on(8,7).
init_on(7,6).
init_on(6,5).
init_on(5,2).

goal_on(4,3).
goal_on(5,4).
goal_on(6,5).
goal_on(7,6).
goal_on(8,7).
goal_on(9,8).
