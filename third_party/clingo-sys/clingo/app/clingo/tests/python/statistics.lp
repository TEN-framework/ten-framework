#script (python)

import clingo

def add_statistics(step, accu):
    accu.update({
        "test": 10,
        "x": [1, 2, 3],
        "y": {
            "a": 2,
            "b": [1,2,3]
        }
    })
    accu["test"] = 20
    accu["x"] = [99, 9]
    accu["x"].extend([4,5,6])
    for i in range(1, 10):
        accu["test"] += i
    accu.update({
        "y": {
            "a": lambda x: x+40,
            "c": 99,
            "b": [0]
        }
    })

def tosymbol(x):
    if isinstance(x, dict):
        ret = []
        for key in sorted(x):
            ret.append(clingo.Tuple_([clingo.String(key), tosymbol(x[key])]))
        return clingo.Function("dict", ret)
    elif isinstance(x, list):
        ret = []
        for y in x:
            ret.append(tosymbol(y))
        return clingo.Function("list", ret)
    else:
        return clingo.Number(int(x))

def tomodel():
    return tosymbol(user)

def main(prg):
    prg.ground([("base", [])])
    prg.solve(on_statistics=add_statistics)
    global user
    user = prg.statistics["user_accu"]
    prg.ground([("two", [])])
    prg.solve()

#end.

#program two.
p(@tomodel()).
