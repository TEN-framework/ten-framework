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
    l = init:add_literal()
    init:add_weight_constraint( l, {{a, 1}, {b, 1}}, 2)
    init:add_weight_constraint(-l, {{b, 1}, {c, 1}}, 2)
end

function main(prg)
    prg:ground({ {"base", {}} })
    prg:register_propagator(Propagator.new())
    prg:solve()
end

#end.

{ a; b; c }.

