#script (lua)

function table.append(table, value)
    table[#table + 1] = value
    return table
end

function table.filter(f, t)
  local ret = {}
  for k, v in pairs(t) do
    if f(v) then
        ret[#ret+1] = v
    end
  end
  return ret
end

function table.shallow_copy(t)
    return table.filter(function() return true end, t)
end

function main(prg)
    prg:ground({{"base", {}}})

    -- extract relevant theory atoms and minimize the number of messages
    local backend = prg:backend()
    local lits = {}
    local atoms = {}
    for atom in prg.theory_atoms do
        if atom.term.name == "cannot" and #atom.term.arguments == 0 then
            local a = {}
            a.literal = atom.literal
            a.elements = {}
            for _, elem in ipairs(atom.elements) do
                local e = {}

                -- format message
                local t = elem.terms
                e.message = string.sub(t[1].name, 2, -2)
                for i = 2, #t do
                    e.message = string.gsub(e.message, "%%", tostring(t[i]), 1)
                end

                -- create condition to minimize
                local condition = table.shallow_copy(elem.condition)
                table.append(condition, atom.literal)
                if #condition > 1 then
                    atom = backend:add_atom()
                    backend:add_rule{{atom}, condition}
                    condition[1] = atom
                end
                table.append(lits, {condition[1], 1})
                e.literal = condition[1]

                table.append(a.elements, e)
            end
            table.append(atoms, a)
        end
    end
    backend:add_minimize{0, lits}
    backend:close()

    -- solve and print debug messages
    local it = prg:solve{yield=true}
    local res, ret = pcall(function()
        for m in it:iter() do
            local invalid = false
            local isTrue = function(x)
                return m:is_true(x.literal)
            end
            for _, atom in ipairs(table.filter(isTrue, atoms)) do
                if not invalid then
                    print("ERROR: invalid answer set")
                    invalid = true
                end
                for _, elem in ipairs(table.filter(isTrue, atom.elements)) do
                    print ("- cannot " .. elem.message)
                end
            end
        end
    end)
    it:close()
    if not res then
        error(ret)
    end
end

#end.

#theory cannot {
    t { };
    &cannot/0: t, head
}.
