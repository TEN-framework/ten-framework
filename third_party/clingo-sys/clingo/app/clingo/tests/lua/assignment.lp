#script (lua)

require("clingo")

Propagator = { }
Propagator.__index = Propagator

function Propagator.new()
    local self = setmetatable({}, Propagator)
    return self
end

function check_lit(assignment, l)
    local trail = assignment.trail
    local t = assignment:value(l)
    if t ~= nil then
        local s = 1
        if not t then
            s = -1
        end
        local n = 0
        for k in trail:iter() do
            if k == s * l then
                n = n + 1
            end
        end
        assert(n == 1)
    end
end

function check_trail(assignment)
    local trail = assignment.trail
    local n = 0
    for i = 0, assignment.decision_level do
        check = false
        for j = trail:first(i), trail:last(i) do
            check = true
            assert(assignment:is_true(trail[j]))
            n = n + 1
        end
        assert(check)
    end
    assert(n == #trail)
    n = 0
    for l in assignment:iter() do
        n = n + 1
        check_lit(assignment, l)
    end
    assert(n == #assignment)
    n = 0
    for i, l in assignment:__pairs() do
        n = n + 1
        assert(assignment[i] == l)
    end
    assert(n == #assignment)
    n = 0
    for i, l in assignment:__ipairs() do
        n = n + 1
        assert(assignment[i] == l)
    end
    assert(n == #assignment)
    n = 0
    for i, k in trail:__pairs() do
        assert(trail[i] == k)
        n = n + 1
    end
    assert(n == #trail)
    n = 0
    for i, k in trail:__ipairs() do
        assert(trail[i] == k)
        n = n + 1
    end
    assert(n == #trail)
end


function Propagator:init(init)
    check_trail(init.assignment)

    init.check_mode = clingo.PropagatorCheckMode.Fixpoint
end

function Propagator:check(control)
    check_trail(control.assignment)
end

function main(prg)
    local p = Propagator.new()
    prg:register_propagator(p)
    prg:ground({{"base", {}}})
    prg:solve()
end

#end.

{ a; b; c }.
