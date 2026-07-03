#script (lua)

function main(prg)
    step = 1
    stop = 10
    while true do
        if step > 1 then
            prg:release_external(clingo.Function("volatile", {step-1}))
        end
        prg:ground({{"cumulative", {step}}})
        prg:assign_external(clingo.Function("volatile", {step}), true)
        ret = prg:solve()
        if step == stop then
            break
        end
        step = step + 1
    end
end

#end.

#program cumulative(k).

dim(k).

{q(X,k) : dim(X)}.
{q(k,Y) : dim(Y), Y<k}.

:- q(X1,k), q(X2,Y), k-Y == |X1-X2|, Y < k.
:- q(k,Y1), q(X,Y2), k-X == |Y1-Y2|, Y1 < k, Y2 < k, X < k.

#external volatile(k).

:-   {q(X,Y) : dim(Y)} 0, dim(X), volatile(k).
:- 2 {q(X,Y) : dim(Y)},   dim(X), volatile(k).
:-   {q(X,Y) : dim(X)} 0, dim(Y), volatile(k).
:- 2 {q(X,Y) : dim(X)},   dim(Y), volatile(k).

#show q/2.
