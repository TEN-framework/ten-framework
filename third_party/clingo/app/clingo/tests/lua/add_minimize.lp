#script (lua)

require("clingo")

local Propagator = { }
Propagator.__index = Propagator

function Propagator.new()
    local self = setmetatable({ }, Propagator)
    return self
end

function Propagator:init(init)
    a = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function("a")).literal)
    b = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function("b")).literal)
    c = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function("c")).literal)
    d = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function("d")).literal)
    init:add_minimize(a, 1)
    init:add_minimize(b, 1)
    init:add_minimize(c, 1)
    init:add_minimize(d, 1)
end

function main(prg)
    prg:ground({ {"base", {}} })
    prg:register_propagator(Propagator.new())
    prg:solve()
end

#end.

2 { a; b; c }.
2 { b; c; d }.

