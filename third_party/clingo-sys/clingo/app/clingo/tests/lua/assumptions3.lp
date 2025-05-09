#script (lua)

function main(prg)
    prg:ground({{"base", {}}})
    prg:solve{assumptions={{clingo.Function("a"), true}, {clingo.Function("b"), false}, {clingo.Function("d"), true}}}
    prg:solve{assumptions={{clingo.Function("a"), false}, {clingo.Function("c"), true}}}
end

#end.

#external d.
{ a; b; c }.
