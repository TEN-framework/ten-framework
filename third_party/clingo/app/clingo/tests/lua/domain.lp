p(1).
{ p(3) }. 
#external p(1..3).

q(X) :- p(X).

#script (lua)



clingo = require("clingo")

function main(prg)
    prg:ground({{"base", {}}})
    print "Solving..."
    print "Answer: 1"
    out = "universe(" .. tostring(#prg.symbolic_atoms) .. ")"
    for x in prg.symbolic_atoms:iter() do
        out = out .. " domain(" .. tostring(x.symbol) .. "," .. tostring(x.is_fact) .. "," .. tostring(x.is_external) .. ")"
    end
    out = out .. " in_domain(p(2)," .. tostring(prg.symbolic_atoms:lookup(clingo.Function("p", {3})) ~= nil) .. ")"
    out = out .. " in_domain(p(4)," .. tostring(prg.symbolic_atoms:lookup(clingo.Function("p", {4})) ~= nil) .. ")"
    for x in prg.symbolic_atoms:by_signature("p", 1) do
        out = out .. " domain_of_p(" .. tostring(x.symbol) .. "," .. tostring(x.is_fact) .. "," .. tostring(x.is_external) .. ")"
    end
    for i,x in ipairs(prg.symbolic_atoms.signatures) do
        out = out .. " sig(" .. x[1] .. "," .. tostring(x[2]) .. ")"
    end
    print (out)
    io.flush()
end

#end. 
