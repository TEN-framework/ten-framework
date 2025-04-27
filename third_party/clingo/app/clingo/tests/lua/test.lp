#script (lua)

function map(func, array)
  for i,v in ipairs(array) do
    array[i] = func(v)
  end
  return array
end

function sorted(t)
    table.sort(t)
    return t
end

function on_model(x)
    print("Answer: 42")
    print(
           "hasA(" ..        tostring(x:contains(clingo.Function("a"))) .. ") "
        .. "hasVolatile(" .. tostring(x:contains(clingo.Function("volatile", {9}))) .. ") "
        .. "model(" ..       table.concat(sorted(map(tostring, x:symbols{shown=true})), ",") .. ") ")
    io.flush()
end

function main (prg)
    n = prg:get_const("n").number
    parts = {}
    parts[#parts+1] = {"base", {}}
    prg:ground(parts)
    prg:solve()
    parts = {}
    prg:add("test", {"x"}, "test(x).")
    parts[#parts+1] = {"test", {clingo.Function("f", {1,2})}}
    for i=1,n do
        parts[#parts+1] = {"cumulative", {i}}
        prg:ground(parts)
        parts = {}
        ret = prg:solve{on_model=on_model}
        print("Answer: 42")
        print(tostring(ret))
        io.flush()
    end
end

#end.

#const n = 10.

#program base.

a.

#program cumulative(k).

1 { c(k) } 1.
q(k) :- c(k).
r(k,X) :- test(X).

#external volatile(k).
 :- not c(5), not volatile(k).
volatile(k-1).

