#script (lua)

function solve(handle)
    local core
    local ret = pcall(function()
        for m in handle:iter() do
        end
        handle:get()
        core = handle:core()
    end)
    handle:close()
    if not ret then
        error("solving failed")
    end
    return core
end

function main(prg)
    prg:ground({{"base", {}}})
    local a = prg.symbolic_atoms:lookup(clingo.Function("a")).literal
    local b = prg.symbolic_atoms:lookup(clingo.Function("b")).literal
    local core = solve(prg:solve{assumptions={a, b}, yield=true})
    local backend = prg:backend()
    local ret = pcall(function()
        for _, lit in ipairs(core) do
            backend:add_rule{{}, {-lit}}
        end
    end)
    if not ret then
        error("adding rules failed")
    end
    backend:close()
    local core = solve(prg:solve{yield=true})
    assert(#core == 0)
end

#end.

{a; b}.
:- a, b.
