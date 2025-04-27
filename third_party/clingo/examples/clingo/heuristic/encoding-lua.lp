#script (lua)

clingo = require("clingo")

function set_default(table, key, value)
    if table[key] == nil then
        table[key] = value
    end
    return table[key]
end

State = { }
State.__index = State

function State.new()
    local x = { degree = {} }
    setmetatable(x, State)
    return x
end

ColoringHeuristic = { }
ColoringHeuristic.__index = ColoringHeuristic

function ColoringHeuristic.new(val)
    local x = {
        graph  = {},
        assign = {},
    }
    setmetatable(x, ColoringHeuristic)
    return x
end

function ColoringHeuristic:init(init)
    states = { }
    for i = 1, init.number_of_threads do
        state = State.new()
        states[i] = state
        init:set_state(i, state)
    end
    for a in init.symbolic_atoms:by_signature("assign", 2) do
        u = tostring(a.symbol.arguments[1])
        l = init:solver_literal(a.literal)
        init:add_watch(l)
        table.insert(set_default(self.assign, l, {}), u)
    end
    for a in init.symbolic_atoms:by_signature("edge", 2) do
        u = tostring(a.symbol.arguments[1])
        v = tostring(a.symbol.arguments[2])
        table.insert(set_default(self.graph, u, {}), v)
        table.insert(set_default(self.graph, v, {}), u)
        for _, state in ipairs(states) do
            set_default(state.degree, u, 0)
            set_default(state.degree, v, 0)
            state.degree[u] = state.degree[u] + 1
            state.degree[v] = state.degree[v] + 1
        end
    end
end

function ColoringHeuristic:update_degree(changes, state, diff)
    for _, l in ipairs(changes) do
        for _, u in ipairs(self.assign[l]) do
            for _, v in ipairs(self.graph[u]) do
                state.degree[v] = state.degree[v] + diff
            end
        end
    end
end

function ColoringHeuristic:propagate(ctl, changes, state)
    self:update_degree(changes, state, -1)
end

function ColoringHeuristic:undo(thread_id, assignment, changes, state)
    self:update_degree(changes, state, 1)
end

function ColoringHeuristic:decide(thread_id, assignment, fallback, state)
    -- in practice this implementation is unusable because it is too inefficient
    -- a heap should be used to extract the vertices with maximum degree
    decision, degree = 0, 0
    for l, vertices in pairs(self.assign) do
        for _, u in ipairs(vertices) do
            if assignment:value(l) == nil and degree < state.degree[u] then
                decision, degree = l, state.degree[u]
            end
        end
    end
    return decision
end

function main(prg)
    prg:register_propagator(ColoringHeuristic.new())
    prg:ground({{"base", {}}})
    ret = prg:solve()
end

#end.

1 { assign(U,C) : color(C) } 1 :- vertex(U).
:- edge(U,V), assign(U,C), assign(V,C).

