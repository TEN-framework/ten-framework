#script (lua) 

function get(val, default)
    if val ~= nil then 
        return val 
    else 
        return default 
    end
end

function main(prg)
    local imin   = get(prg:get_const("imin"), clingo.Number(0))
    local imax   = prg:get_const("imax")
    local istop  = get(prg:get_const("istop"), clingo.String("SAT"))

    local step, ret = 0, nil
    while (imax == nil or step < imax.number) and
          (step == 0 or step < imin.number or (
              (istop.string == "SAT"     and not ret.satisfiable) or
              (istop.string == "UNSAT"   and not ret.unsatisfiable) or 
              (istop.string == "UNKNOWN" and not ret.unknown))) do
        local parts = {}
        table.insert(parts, {"check", {step}})
        if step > 0 then
            prg:release_external(clingo.Function("query", {step-1}))
            table.insert(parts, {"step", {step}})
        else
            table.insert(parts, {"base", {}})
        end
        prg:ground(parts)
        prg:assign_external(clingo.Function("query", {step}), true)
        ret, step = prg:solve(), step+1
    end
end

#end.

#program check(t).
#external query(t).
