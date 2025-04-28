#script (lua)

require("clingo")

-- {{{1 Aux

function max(a, b) 
    if a < b then
        return b
    else
        return a
    end
end

function table.unique(t)
    local res, n = 1, #t
    for i = 2, n do
        if t[i] ~= t[i-1] then
            res = res + 1
            t[res] = t[i]
        end
    end
    for i = res+1, n do
        t[i] = nil
    end
end

function table.copy(t)
    local u = {}
    for i, v in pairs(t) do
        u[i] = v
    end
    return u
end

function table.find(t, k)
    for _, v in pairs(t) do
        if v == k then
            return true
        end
    end
    return false
end

function table.tostring(t)
    local r, c = "", false
    for _, k in ipairs(t) do
        if c then
            r = r .. ","
        else
            c = true
        end
        r = r .. tostring(k)
    end
    return r
end

function table.append(t, v)
    t[#t+1] = v
end

function table.setdefault(t, k, d)
    local x = t[k]
    if x == nil then
        x = d
        t[k] = x
    end
    return x
end

function table.keys(t)
    local keys = {}
    for k, _ in pairs(t) do
        table.append(keys, k)
    end
    return keys
end

-- {{{1 State

local State = { }
State.__index = State

function State.new(pat_len, seq_len)
    local self = setmetatable({ }, State)
    self.seq_active = {}
    self.stack = {}
    self.trail = {}
    self.pat = {}
    self.pat_assigned = 0
    self.pat_len = pat_len
    self.seq_len = seq_len
    for i = 1, seq_len do
        self.seq_active[i] = true
    end
    return self
end

-- {{{1 Propagator

local Propagator = { }
Propagator.__index = Propagator

-- {{{2 Initialization

function Propagator.new()
    local self = setmetatable({ }, Propagator)
    self.__seq_atoms = {}
    self.__pat_atoms = {}
    self.__occ_list = {}
    self.__item_map = {}
    self.__item_num = 0
    self.__pat_len = 0
    return self
end

function Propagator:__map_item(item)
    local mapped = self.__item_map[item]
    if mapped == nil then
        self.__item_num = self.__item_num + 1
        mapped = self.__item_num
        self.__item_map[item] = mapped
    end
    return mapped
end

function Propagator:__add_seq_atom(init, sid, atom)
    assert(self.__seq_atoms[sid] == nil)
    self.__seq_atoms[sid] = {init:solver_literal(atom.literal), {}}
    for _, elem in ipairs(atom.elements) do
        local index, item = elem.terms[1].number + 1, elem.terms[2]
        assert(index >= 1)
        local item = self:__map_item(tostring(item))
        self.__seq_atoms[sid][2][index] = item
    end
end

function Propagator:__add_pat_atoms(init, atom)
    for _, elem in ipairs(atom.elements) do
        local lit = init:solver_literal(elem.condition_id)
        local index, item = elem.terms[1].number + 1, elem.terms[2]
        assert(index >= 1)
        local item = self:__map_item(tostring(item))
        local pat = {index, item}
        local pat_atom = self.__pat_atoms[lit]
        if pat_atom == nil then
            init:add_watch(lit)
            pat_atom = {}
            self.__pat_atoms[lit] = pat_atom
        end
        pat_atom[#pat_atom + 1] = pat
        self.__pat_len = max(self.__pat_len, index)
    end
end

function Propagator:__init_occ_list()
    for i = 1, self.__item_num do
        self.__occ_list[#self.__occ_list + 1] = {}
    end
    local sid = 1
    for _, seq in ipairs(self.__seq_atoms) do
        local set = table.copy(seq[2])
        table.sort(set)
        table.unique(set)
        for _, item in ipairs(set) do
            assert(item ~= nil)
            table.append(self.__occ_list[item], sid)
        end
        sid = sid + 1
    end
end

function Propagator:init(init)
    for atom in init.theory_atoms do
        term = atom.term
        args = term.arguments
        if term.name == "seq" and #args == 1 then
            self:__add_seq_atom(init, args[1].number + 1, atom)
        end
        if term.name == "pat" and #args == 0 then
            self:__add_pat_atoms(init, atom)
        end
    end
    self:__init_occ_list()
    for i = 1, init.number_of_threads do
        init:set_state(i, State.new(self.__pat_len, #self.__seq_atoms))
    end
end

-- {{{2 Propagation

function Propagator:__propagate_sequence_lit(state, control, sid, lit)
    --print ("    propagate_lit: " .. tostring(sid) .. "/" .. tostring(lit))
    state.seq_active[sid] = false
    table.append(state.stack, -sid)
    if not control.assignment:is_true(lit) then
        local klaus = {lit}
        for i = 1, self.__pat_len do
            local pat = state.pat[i]
            if pat ~= nil then
                table.append(klaus, -pat[1])
            end
        end
        --print ("      add_clause: " .. table.tostring(klaus))
        return control:add_clause{klaus} and control:propagate()
    end
    return true
end

function Propagator:__propagate_sequence(state, control, sid, lit, items)
    --print ("  propagate sequence: " .. tostring(sid) .. "/" .. tostring(lit) .. "/" .. table.tostring(items))
    if state.pat_assigned < self.__pat_len and control.assignment:is_false(lit) then
        return true
    end
    local iid = 1
    for i = 1, self.__pat_len do
        pat = state.pat[i]
        while true do
            if iid > #items then
                return self:__propagate_sequence_lit(state, control, sid, -lit)
            end
            iid = iid + 1
            if pat == nil or items[iid-1] == pat[2] then
                break
            end
        end
    end
    return state.pat_assigned < self.__pat_len or self:__propagate_sequence_lit(state, control, sid, lit)
end

function Propagator:propagate(control, changes, state)
    --print ("propagate: " .. table.tostring(changes))
    if #state.trail == 0 or state.trail[#state.trail][1] < control.assignment.decision_level then
        table.append(state.trail, {control.assignment.decision_level, #state.stack + 1})
    end
    for _, lit in ipairs(changes) do
        for _, pat in ipairs(self.__pat_atoms[lit]) do
            if state.pat[pat[1]] ~= nil then
                assert(false)
                -- this case should not occur if the pattern is generated properly
                local old = state.pat[pat[1]][1]
                assert(control.assignment:is_true(old))
                if not control:add_clause{{-lit, -old}} or not control:propagate() then
                    return
                end
            else
                table.append(state.stack, pat[1])
                state.pat_assigned = state.pat_assigned + 1
                state.pat[pat[1]] = {lit, pat[2]}
                for _, sid in ipairs(self.__occ_list[pat[2]]) do
                    local atom = self.__seq_atoms[sid]
                    if state.seq_active[sid] and not self:__propagate_sequence(state, control, sid, atom[1], atom[2]) then
                        return
                    end
                end
            end
        end
    end
end

function Propagator:undo(id, assign, changes, state)
    --print ("undo: " .. table.tostring(changes))
    sid = state.trail[#state.trail][2]
    for i = sid, #state.stack do
        psid = state.stack[i]
        if psid >= 1 then
            state.pat[psid] = nil
            state.pat_assigned = state.pat_assigned - 1
        else
            state.seq_active[-psid] = true
        end
        state.stack[i] = nil
    end
    state.trail[#state.trail] = nil
end

-- {{{1 Main

function main(c)
    c.configuration.solve.opt_mode = "optN"
    c:ground({ {"base", {}} })
    c:register_propagator(Propagator.new())
    backend = c:backend()
    -- :- not sup(U), seq(U,_,_), n == 0.
    if c:get_const("n").number == 0 then
        local grouped = {}
        for atom in c.symbolic_atoms:by_signature("seq", 3) do
            table.append(table.setdefault(grouped, atom.symbol.arguments[1].number, {}), atom.literal)
        end
        local keys = table.keys(grouped)
        table.sort(keys)
        for _, key in ipairs(keys) do
            local a = backend:add_atom()
            for _, l in ipairs(grouped[key]) do
                backend:add_rule{{a}, {l}}
            end
            backend:add_rule{{}, {-c.symbolic_atoms:lookup(clingo.Function("sup", {key})).literal, a}}
        end
    end
    -- :- sup(U), pat(_,I), not seq(U,_,I).
    local grouped_pat, grouped_seq = {}, {}
    for atom in c.symbolic_atoms:by_signature("pat", 2) do
        table.append(table.setdefault(grouped_pat, tostring(atom.symbol.arguments[2]), {}), atom.literal)
    end
    for atom in c.symbolic_atoms:by_signature("seq", 3) do
        local a, b = atom.symbol.arguments[1], atom.symbol.arguments[3]
        grouped_seq[tostring(a) .. "," .. tostring(b)] = true
    end
    local projected_pat, projected_seq = {}, {}
    local keys = table.keys(grouped_pat)
    table.sort(keys)
    for _, key in ipairs(keys) do
        local a = backend:add_atom()
        for _, l in ipairs(grouped_pat[key]) do
            backend:add_rule{{a}, {l}}
        end
        projected_pat[key] = a
    end
    for atom in c.symbolic_atoms:by_signature("sup", 1) do
        for _, key in ipairs(keys) do
            local a, b = atom.symbol.arguments[1], key
            if not grouped_seq[tostring(a) .. "," .. tostring(b)] then
                backend:add_rule{{}, {atom.literal, projected_pat[key]}}
            end
        end
    end
    backend:close()
    c:solve()
end

-- }}}1

#end.

% {{{1 ASP

#show pat/2.

#theory seq {
    term { };
    &seq/1 : term, body;
    &pat/0 : term, directive
}.

#const n = 5.

1 { pat(P,I) : seq(_,_,I) } 1 :- P = 0..(n-1).

&pat { P,I : pat(P,I) }.
sup(U) :- &seq(U) { P,I : seq(U,P,I) }, seq(U,_,_).

#maximize { 1,U : sup(U) }.

% abaca
seq(0,0,a).
seq(0,1,b).
seq(0,2,a).
seq(0,3,c).
seq(0,4,a).

% abdca
seq(1,0,a).
seq(1,1,b).
seq(1,2,d).
seq(1,3,c).
seq(1,4,a).

% aedca
seq(2,0,a).
seq(2,1,e).
seq(2,2,d).
seq(2,3,c).
seq(2,4,a).
