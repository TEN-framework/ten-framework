#script (lua)

function main(ctl)
    ctl:ground({{"base",{}}})
    ctl:add("rules", {}, "r :- q(_), p.")
    ctl:ground({{"rules", {}}})
    ctl:solve()
end

#end.

p.
q(1).
