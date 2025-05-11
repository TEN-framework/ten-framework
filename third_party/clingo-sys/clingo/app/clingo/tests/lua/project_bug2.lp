#program a.
p.
#program b.
r :- q(_), p.
%r :- q(X), p.

#script(lua)

clingo = require("clingo")

function main(ctl)
    ctl:ground({{'a', {}}})
    ctl:solve()

    bck = ctl:backend()
    atm = bck:add_atom(clingo.Function('q', {clingo.Number(0)}))
    bck:add_rule{{atm}}
    bck:close()
    ctl:ground({{'b', {}}})
    ctl:solve()
end

#end.
