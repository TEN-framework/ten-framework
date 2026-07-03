#script (lua)

c = clingo.Control()

function get()
    return { clingo.parse_term("1"), clingo.parse_term("p(1+2)"), clingo.parse_term("-p"), clingo.parse_term("-p(1)") }
end

c:ground({{"base", {}}})
c:solve()

#end.

p(@get()).
