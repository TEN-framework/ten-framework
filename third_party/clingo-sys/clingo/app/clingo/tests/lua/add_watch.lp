#script (lua)

clingo = require("clingo")

local Propagator = { }
Propagator.__index = Propagator

function Propagator.new()
    local self = setmetatable({ }, Propagator)
    self.__a = nil
    return self
end

function Propagator:init(init)
    a = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function('a')).literal)
    b = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function('b')).literal)
    c = init:solver_literal(init.symbolic_atoms:lookup(clingo.Function('c')).literal)
    init:add_watch(a, 1)
    ass = init.assignment
    assert(ass:value(a) == nil)
    assert(ass:is_false(b))
    assert(ass:is_true(c))
    self.__a = a
end

function Propagator:propagate(ctl, changes)
    assert(ctl.thread_id == 1)
    for _, lit in ipairs(changes) do
        assert(lit == self.__a)
    end
end

function main(prg)
    prg:register_propagator(Propagator.new())
    prg:ground({{"base", {}}})
    prg:solve()
end

#end.

{a;b}.
:- b.
c.
