#script (lua)

function print_conf(conf, ident)
    for _, key in ipairs(conf.keys) do
        local subconf = conf[key]
        if type(subconf) == type(conf) then
            label = key
            if #subconf >= 0 then
                label = label .. "[0.." .. tostring(#subconf) .. "]"
            end
            print (ident .. label .. " - " .. conf:description(key))
            print_conf(subconf, "  " .. ident .. label .. ".")
        else
            print (ident .. key .. "[=" .. tostring(subconf) .. "] - " .. conf:description(key))
        end
    end
end

function main(prg)
    prg:ground({{"base", {}}})
    
    print_conf(prg.configuration, "")

    print ("The heuristics of the solvers in the 'many' portfolio:")
    prg.configuration.configuration = "many"
    prg.configuration.solve.parallel_mode = 3 -- just use the first 3 solvers
    for x in prg.configuration.solver:iter() do
        print ("  " .. x.heuristic)
    end

    prg.configuration.solve.models = 0
    print ("==================== All Models ===================")
    prg:solve()

    prg.configuration.solve.models = 1
    print ("===================== One Model ===================")
    prg:solve()

    prg.configuration.solve.models = 0
    prg.configuration.solve.enum_mode = "cautious"
    print ("=============== Cautious Consequences =============")
    prg:solve()

    prg.configuration.solve.models = 0
    prg.configuration.solve.enum_mode = "brave"
    print ("================ Brave Consequences ===============")
    prg:solve()
end

#end.

{ a; b; c }.
