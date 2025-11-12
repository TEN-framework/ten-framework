#script (lua)

function main(prg)
    prg:ground({{"base", {}}, {"p", {42}}})
    prg:solve()
end

#end.
#program p(t).

a(t).

#include "include.lp".
