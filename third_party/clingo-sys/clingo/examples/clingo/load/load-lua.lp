#script(lua)

function main(prg)
    print(string.format("#const y=%s", tostring(prg:get_const("y"))))
    prg:load("program.lp")
    print(string.format("#const y=%s", tostring(prg:get_const("y"))))
    print(string.format("#const x=%s", tostring(prg:get_const("x"))))
    prg:add("base", {}, "#const x=10. b.")
    print(string.format("#const x=%s", tostring(prg:get_const("x"))))
    prg:ground({{"base", {}}})
    prg:solve()
end

#end.

c.
