#script (lua)

function table.shallow_copy(t)
  local t2 = {}
  for k,v in pairs(t) do
    t2[k] = v
  end
  return t2
end

function table.append(t, val)
    t[#t + 1] = val
end

function table.get(t, key, def)
    local val = t[key]
    if val ~= nil then
        return val
    else
        return def
    end
end

function table.sort_unique(t)
    local set = {}
    for k, v in ipairs(t) do
        set[tostring(v)] = v
    end
    local ret = {}
    for k, v in pairs(set) do
        table.append(ret, v)
    end
    table.sort(ret)
    return ret
end

function table.map(t, f)
    for k, v in ipairs(t) do
        t[k] = f(v)
    end
    return t
end

function string.startswith(str, pre)
   return string.sub(str, 1, string.len(pre)) == pre
end

function bool_to_num(var)
  return var and 1 or 0
end

Function = clingo.Function
Tuple = clingo.Tuple

local Context = { }
function Context:__index(key)
    return Context[key]
end

function Context.new(reified)
    local self = setmetatable({}, Context)
    self.__reified = table.shallow_copy(reified)
    return self
end

function Context:get()
    return self.__reified
end

local Observer = { }
Observer.__index = Observer

function Observer.new()
    local self = setmetatable({}, Observer)
    self.__delayed = {}
    self.__symbols = {}
    self.__reified = {}
    self.__terms   = {}
    self.__elems   = {}
    return self
end

function Observer:__index(name)
    f = Observer[name]
    if f == nil then
        assert(not string.startswith(name, "_"))
        return function (...)
            table.append(self.__delayed, {name, {...}})
        end
    else
        return f
    end
end

function Observer:__map(lit)
    local sign = false
    if lit < 0 then
        sign = true
        lit = -lit
    end
    local ret = table.get(self.__symbols, lit, Function("__aux"))
    if sign then
        ret = Function("neg", {ret})
    end
    return ret
end

function Observer:__map_lits(t)
    for k, v in ipairs(t) do
        t[k] = self:__map(v)
    end
    return t
end

function Observer:__map_wlits(t)
    for k, v in ipairs(t) do
        t[k] = Tuple({self:__map(v[1]), v[2]})
    end
    return t
end

function Observer:init_program(incremental)
    table.append(self.__reified, Function("init_program", {bool_to_num(incremental)}))
end

function Observer:begin_step()
    table.append(self.__reified, Function("begin_step", {}))
end

function Observer:_rule(choice, head, body)
    head = table.sort_unique(self:__map_lits(head))
    body = table.sort_unique(self:__map_lits(body))
    table.append(self.__reified, Function("rule", {bool_to_num(choice), Tuple(head), Tuple(body)}))
end

function Observer:_weight_rule(choice, head, lower_bound, body)
    head = table.sort_unique(self:__map_lits(head))
    body = table.sort_unique(self:__map_wlits(body))
    table.append(self.__reified, Function("weight_rule", {bool_to_num(choice), Tuple(head), lower_bound, Tuple(body)}))
end

function Observer:_minimize(priority, literals)
    literals = table.sort_unique(self:__map_wlits(literals))
    table.append(self.__reified, Function("minimize", {priority, Tuple(literals)}))
end

function Observer:_project(atoms)
    atoms = table.sort_unique(self:__map_lits(atoms))
    table.append(self.__reified, Function("project", {Tuple(atoms)}))
end

function Observer:output_atom(symbol, atom)
    self.__symbols[atom] = symbol
    table.append(self.__reified, Function("output_atom", {symbol}))
end

function Observer:_output_term(symbol, condition)
    condition = table.sort_unique(self:__map_lits(condition))
    table.append(self.__reified, Function("output_term", {symbol, Tuple(condition)}))
end

function Observer:_output_csp(symbol, value, condition)
    condition = table.sort_unique(self:__map_lits(condition))
    table.append(self.__reified, Function("output_csp", {symbol, value, Tuple(condition)}))
end

function Observer:_external(atom, value)
    table.append(self.__reified, Function("external", {self:__map(atom), tostring(value)}))
end

function Observer:_assume(literals)
    literals = table.sort_unique(self:__map_lits(literals))
    table.append(self.__reified, Function("assume", {Tuple(literals)}))
end

function Observer:_heuristic(atom, type, bias, priority, condition)
    condition = table.sort_unique(self:__map_lits(condition))
    table.append(self.__reified, Function("heuristic", {self:__map(atom), tostring(type), bias, Tuple(condition)}))
end

function Observer:_acyc_edge(node_u, node_v, condition)
    condition = table.sort_unique(self:__map_lits(condition))
    table.append(self.__reified, Function("acyc_edge", {node_u, node_v, Tuple(condition)}))
end

function Observer:theory_term_number(term_id, number)
    self.__terms[term_id] = function() return number end
end

function Observer:theory_term_string(term_id, name)
    self.__terms[term_id] = function() return Function(name) end
end

function Observer:theory_term_compound(term_id, name_id_or_type, arguments)
    self.__terms[term_id] = function ()
        return Function(self.__terms[name_id_or_type]().name, table.map(arguments, function (i)
            return self.__terms[i]()
        end))
    end
end

function Observer:theory_element(element_id, terms, condition)
    self.__elems[element_id] = function () 
        condition = table.sort_unique(self:__map_lits(condition))
        return Function("elem", {Tuple(table.map(terms, function (i)
            return self.__terms[i]() 
        end)), Tuple(condition)})
    end
end

function Observer:_theory_atom(atom_id_or_zero, term_id, elements)
    elements = table.sort_unique(table.map(elements, function (e)
        return self.__elems[e]()
    end))
    self.__symbols[atom_id_or_zero] = Function("theory", {self.__terms[term_id](), Tuple(elements)});
end

function Observer:_theory_atom_with_guard(atom_id_or_zero, term_id, elements, operator_id, right_hand_side_id)
    elements = table.sort_unique(table.map(elements, function (e)
        return self.__elems[e]()
    end))
    self.__symbols[atom_id_or_zero] = Function("theory", {self.__terms[term_id](), Tuple(elements), self.__terms[operator_id](), self.__terms[right_hand_side_id]()});
end

function Observer:end_step()
    table.append(self.__reified, Function("end_step", {}))
end

function Observer:finalize()
    if unpack == nil then
        unpack = table.unpack
    end
    for _, v in ipairs(self.__delayed) do
        local name, args
        name, args = v[1], v[2]
        if string.startswith(name, "theory_atom") then
            self["_" .. name](unpack(args))
        end
    end
    for _, v in ipairs(self.__delayed) do
        local name, args
        name, args = v[1], v[2]
        if not string.startswith(name, "theory_atom") then
            self["_" .. name](unpack(args))
        end
    end
    return Context.new(self.__reified)
end

function main(prg)
    f = Function("f", {1})
    g = Function("g")
    obs = Observer.new()
    prg:register_observer(obs)
    prg:ground({{"base", {}}})
    prg:solve{assumptions={{Function("b"), false}}}
    ctx = obs:finalize()
    prg:add("q", {}, "q(@get()).")
    prg:ground({{"q", {}}}, ctx)
    prg:solve()
end

#end.

1 {a; b}.
#minimize {1:a; 2:b}.
#project a.
#show x : a, b.
#external a.
#heuristic a : b. [1@2,sign]
#edge (a,b) : a, b.

#theory t {
  term   { + : 1, binary, left };
  &a/0 : term, any;
  &b/1 : term, {=}, term, any
}.
a :- &a { 1+2,"test": a, b }.
b :- &b(3) { } = 17.
