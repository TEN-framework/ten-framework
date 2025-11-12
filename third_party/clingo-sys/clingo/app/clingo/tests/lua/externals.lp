#script (lua)

Function = clingo.Function

function main(prg)
    prg:ground({{"base", {}}})
    prg:assign_external(Function("a"), true)
    prg:solve()
    prg:assign_external(Function("a"), false)
    prg:solve()
    prg:assign_external(Function("a"), nil)
    prg:solve()
    prg:release_external(Function("a"))
    prg:solve()
end

#end.

#external a.
