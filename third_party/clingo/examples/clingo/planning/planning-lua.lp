#script (lua) 
function main(prg)
    local step   = 0
    local check  = false
    while true do
        parts = {}
        if step > 0 then
            parts[#parts+1] = {"trans", {step}}
        else
            parts[#parts+1] = {"base", {}}
        end
        parts[#parts+1] = {"state", {step}}
        if check then
            parts[#parts+1] = {"check", {step}}
        end
        prg:ground(parts)
        prg:release_external(clingo.Function("vol", {step-1}))
        prg:assign_external(clingo.Function("vol", {step}), true)
        local ret = prg:solve()
        if ret.satisfiable then
            if not check and prg:get_const("nocheck") == nil then
                check = true
                parts = {}
                parts[#parts+1] = {"check", {step}}
                prg:ground(parts)
                ret = prg:solve()
                if ret.satisfiable then
                    break
                end
            else 
                break
            end
        end
        step = step+1
    end
end
#end.
