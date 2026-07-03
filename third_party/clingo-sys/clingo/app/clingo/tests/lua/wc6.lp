#script (lua)

clingo = require("clingo")

local Propagator = { }
Propagator.__index = Propagator

function Propagator.new()
    return setmetatable({ }, Propagator)
end

function Propagator:init(init)
    local a = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function("a")).literal)
    local b = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function("b")).literal)
    local c = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function("c")).literal)
    init:add_weight_constraint(a, {{b, 1}, {c, 1}}, 1, 0, true)
end

function main(ctl)
    ctl:register_propagator(Propagator.new())
    ctl:ground({{"base", {}}})
    ctl:solve()
end

#end.

{ a;b;c }.
