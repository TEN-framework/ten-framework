#script (lua)

function main(prg)
    on_model = function (m)
        m.context:add_clause({{clingo.Function("a"), false}, {clingo.Function("b"), false}})
        m.context:add_nogood({{clingo.Function("c"), true}, {clingo.Function("d"), true}})
    end

    prg.configuration.solve.models = 0
    prg:ground({{"base", {}}})
    prg:solve{on_model=on_model}
    print "and again"
    prg:solve()
end

#end.

2 { a; b; c; d }.
