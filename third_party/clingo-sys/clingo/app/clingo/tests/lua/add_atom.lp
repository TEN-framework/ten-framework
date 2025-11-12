#script (lua)

clingo = require("clingo")

function main(ctl)
    backend = ctl:backend()
    a = backend:add_atom(clingo.Function("a", {2}))
    b = backend:add_atom(clingo.Function("b", {1}))
    backend:add_rule{{a}, {}, true}
    backend:add_rule{{b}, {}, true}
    backend:close()

    ctl:ground({{"base", {}}})

    backend = ctl:backend()
    e = backend:add_atom(clingo.Function("e", {3}))
    backend:add_rule{{e}, {}, false}
    backend:close()

    ctl:solve()

    ctl:ground({{"multi", {}}})
    ctl:solve()
end

#end.

#program base.
#show a/1.
#show c/1.
#show d/1.
a(1).
c(X) :- a(X).
d(X) :- b(X).

#program multi.
a(X) :- e(X).
