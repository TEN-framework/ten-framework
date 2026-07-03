#script (lua)

require("clingo")

Propagator = { }
Propagator.__index = Propagator

function Propagator.new()
    local self = setmetatable({}, Propagator)
    self.lits = {}
    self.step = 0
    return self
end

function Propagator:init(init)
    local a = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function("a")).literal)
    local b = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function("b")).literal)
    local c = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function("c")).literal)
    if self.step == 0 then
        l = init:add_literal()
        init:add_clause({-b, l})
        init:propagate()
        init:add_clause({-l, b})
        init:propagate()
        init:add_clause({-c, l})
        init:propagate()
        init:add_clause({-l, c})
        init:propagate()
        init:add_clause({-a, -b})
        init:propagate()
        init:add_clause({-a,  b})
        init:propagate()
        init:add_clause({-a, -a, -b})
        init:propagate()
        init:add_clause({-a, a})
        init:propagate()
    end
    if self.step == 1 then
        init:add_clause({b})
        init:propagate()
        init:add_clause({b})
        init:propagate()
    end
    if self.step == 2 then
        init:add_clause({})
        init:propagate()
        init:add_clause({a})
        init:propagate()
        init:add_clause({b})
        init:propagate()
        init:add_clause({-a})
        init:propagate()
        init:add_clause({-b})
        init:propagate()
    end
    self.step = self.step + 1
end

function main(prg)
    local p = Propagator.new()
    prg:register_propagator(p)
    prg:ground({{"base", {}}})
    prg:solve()
    prg:solve()
    prg:solve()
end

#end.

{ a; b; c }.
