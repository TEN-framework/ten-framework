#script (lua)

clingo = require("clingo")

symbols = {clingo.Number(17)}

function compare(a, b)
    if #a ~= #b then
        return false
    end
    for i=1, #a do
        if a[i] ~= b[i] then
            return false
        end
    end
    return true
end

function on_model(m)
    m:extend(symbols)
    assert(compare(m:symbols{theory=true}, symbols))
end

function main(prg)
    prg:ground({{"base", {}}})
    prg:solve{on_model=on_model}
end

#end.

1{a;b;c}1.
