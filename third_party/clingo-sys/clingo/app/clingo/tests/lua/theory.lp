#script (lua)
clingo = require("clingo")

t = {}

function get()
    return t
end

function convert_term(t)
    if t.type == clingo.TheoryTermType.Function then
        a = {}
        for i, x in ipairs(t.arguments) do
            a[#a+1] = convert_term(x)
        end
        return clingo.Tuple({t.name, clingo.Tuple(a)})
    elseif t.type == clingo.TheoryTermType.Number then
        return t.number
    elseif t.type == clingo.TheoryTermType.Symbol then
        return t.name
    else
        return clingo.Function("unimplemented")
    end

end

function main(c)
    c:ground({{"base",{}}})
    for x in c.theory_atoms do
        r = {}
        r[#r+1] = convert_term(x.term)
        e = {}
        for i,y in ipairs(x.elements) do
            cond = {}
            for i, z in ipairs(y.condition) do
                cond[#cond+1] = "n/a"
            end
            Tuple = {}
            for i, z in ipairs(y.terms) do
                Tuple[#Tuple+1] = convert_term(z)
            end
            e[#e+1] = clingo.Tuple({clingo.Tuple(Tuple), clingo.Tuple(cond)})
        end
        table.sort(e)
        r[#r+1] = clingo.Tuple(e)
        if x.guard ~= nil then
            r[#r+1] = clingo.Tuple({x.guard[1], convert_term(x.guard[2])})
        end
        t[#t+1] = clingo.Tuple(r)
    end
    c:ground({{"theory",{}}})
    c:solve()
end

#end.

#theory t {
  t {
    + : 1, binary, left;
    - : 1, binary, left
  };
  &a/0: t, any;
  &b/0: t, {=}, t, any
}.

2 { p; q } 2.

&a{ a+b : p,q; 1,c-d}.
&a{ 1,c-d}.
&b{ } = a+b.

#program theory.
p(@get()).
