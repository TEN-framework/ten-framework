#script (lua)

function main(prg)
    prg:ground({{"base", {}}})
    prg:solve{assumptions={{clingo.Function("a"), true}}}
end

#end.

{ a }.
b :- a.
