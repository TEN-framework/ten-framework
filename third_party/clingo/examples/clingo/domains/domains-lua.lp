#script (lua)

clingo = require("clingo")

function main(prg)
    prg:ground({{"base", {}}})
    print ("universe:" .. tostring(#prg.symbolic_atoms))
    for x in prg.symbolic_atoms:iter() do
        print (x.symbol, x.is_fact, x.is_external)
    end
    print ("p(2) is in domain: " .. tostring(prg.symbolic_atoms:lookup(clingo.Function("p", {3})) ~= nil))
    print ("p(4) is in domain: " .. tostring(prg.symbolic_atoms:lookup(clingo.Function("p", {4})) ~= nil))
    print ("domain of p/1:")
    for x in prg.symbolic_atoms:by_signature("p", 1) do
        print (x.symbol, x.is_fact, x.is_external)
    end
    print ("signatures:")
    for i,x in ipairs(prg.symbolic_atoms.signatures) do
        if x[3] then s = "" else s = "~" end
        print (s .. x[1] .. "/" .. tostring(x[2]))
    end
end

#end. 
