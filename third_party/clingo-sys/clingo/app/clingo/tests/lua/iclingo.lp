#program step(k).

#external vol(k).
p(k).
:- vol(k), not 5 { p(K) : K <= k }.

#script (lua)

function main(prg)
    step = 0
    while true do
        if step > 0 then
            prg:release_external(clingo.Function("vol", {step-1}))
        end
        prg:ground({{"step", {step}}})
        prg:assign_external(clingo.Function("vol", {step}), true)
        ret = prg:solve()
        if ret.satisfiable then
            break
        end
        step = step + 1
    end
end

#end.
