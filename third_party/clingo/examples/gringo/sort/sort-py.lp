#script (python)

from clingo import Number, Tuple_

values = []

def gather(a):
    values.append(a)
    return Number(1)

def sort():
    return [Tuple_((Number(i), val)) for i, val in enumerate(sorted(values))]

#end.
