#script (lua)

function tostringTable(t) 
    ret   = ""
    comma = False
    for _, elem in pairs(t) do
        if comma then ret = ret .. ", "
        else          comma = true 
        end
        ret = ret .. tostring(elem)
    end
    return ret
end

function main(prg)
    on_model = function (m)
        print("shown")
        print("  positive: " .. tostringTable(m:symbols{shown=true}))
        print("  negative: " .. tostringTable(m:symbols{shown=true, complement=true}))
        print("atoms")
        print("  positive: " .. tostringTable(m:symbols{atoms=true}))
        print("  negative: " .. tostringTable(m:symbols{atoms=true, complement=true}))
        print("terms")
        print("  positive: " .. tostringTable(m:symbols{terms=true}))
        print("  negative: " .. tostringTable(m:symbols{terms=true, complement=true}))
    end
    prg:ground({{"base", {}}})
    prg:solve{on_model=on_model}
end

#end.

{a}.
b :- a.
#show c : a.
#show b/0.
