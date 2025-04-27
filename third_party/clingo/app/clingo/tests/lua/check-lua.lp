#script (lua)

require("clingo")

Propagator = { }
Propagator.__index = Propagator

function Propagator.new()
    local self = setmetatable({}, Propagator)
    self.lits = {}
    return self
end

function Propagator:init(init)
    for atom in init.symbolic_atoms:by_signature("p", 1) do
        local sym = atom.symbol
        self.lits[#self.lits+1] = init:solver_literal(atom.literal)
    end
    print(init.check_mode, clingo.PropagatorCheckMode.Total)
    assert(init.check_mode == clingo.PropagatorCheckMode.Total)
    assert(init.check_mode ~= clingo.PropagatorCheckMode.Fixpoint)
    assert(init.check_mode ~= clingo.PropagatorCheckMode.Off)
    init.check_mode = clingo.PropagatorCheckMode.Fixpoint
    assert(init.check_mode == clingo.PropagatorCheckMode.Fixpoint)
end

function Propagator:check(ctl)
    -- assert(not ctl.assignment.is_total)
    for _, lit in ipairs(self.lits) do
        if ctl.assignment:value(lit) == nil then
            ctl:add_clause{{lit}}
            break
        end
    end
end

function main(prg)
    local p = Propagator.new()
    prg:register_propagator(p)
    prg:ground({{"base", {}}})
    prg:solve()
end

#end.

{ p(1..10) }.
