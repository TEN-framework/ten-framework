#script (lua)

clingo = require("clingo")

values = {}

function gather(a)
    values[#values+1] = a
    return 1
end

function sort()
    table.sort(values)
    ret = {}
    for k, v in ipairs(values) do
        ret[#ret+1] = clingo.Tuple({k, v})
    end
    return ret
end

#end.
