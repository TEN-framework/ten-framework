#script (lua)

require("clingo")

function p(n)
    return clingo.Function("p", {n})
end

function main(prg)
    local bot = 100
    local lit = function (sym) 
        return prg.symbolic_atoms:lookup(sym).literal
    end
    prg:ground({{"base", {}}})
    prg:solve{assumptions={{p(3), false}, lit(p(4))}}
    prg:solve{assumptions={{p(3), true}, -lit(p(4))}}

    prg:assign_external(p(1), true)
    prg:solve{assumptions={-lit(p(3)), -lit(p(4))}}
    prg:assign_external(-lit(p(1)), true)
    prg:solve{assumptions={-lit(p(3)), -lit(p(4))}}
    prg:assign_external(lit(p(1)), true)
    prg:solve{assumptions={-lit(p(3)), -lit(p(4))}}

    prg:solve{assumptions={-lit(p(3)), -lit(p(4)), bot}}
    prg:solve{assumptions={-lit(p(3)), -lit(p(4)), -bot}}

    -- note: this actually creates the external
    --       hence, the subsequent call with the assumption is unsatisfiable
    prg:assign_external(bot, true)
    prg:solve{assumptions={-lit(p(3)), -lit(p(4)), -bot}}
end

#end.

#external p(1..2).
{p(3..4)}.
