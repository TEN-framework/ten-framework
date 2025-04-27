#script (python)

import clingo
from clingo import Function, String, Model
import sys

models = []

def getModels():
    return models

def split(model):
    return [String(x) for x in sorted(str(model).split(" "))]

def fun(name, model):
    return Function(name, model) if len(model) > 0 else Function(name)

def on_model(model):
    models.append(fun("model", split(model)))
    models.append(fun("shown", sorted(model.symbols(shown=True))))
    models.append(fun("atoms", sorted(model.symbols(atoms=True))))
    models.append(fun("terms", sorted(model.symbols(terms=True))))

def main(prg):
    prg.ground([("base", [])])
    prg.solve(on_model=on_model)
    prg.ground([("result", [])])
    prg.solve()

#end.

{ b }.
c.
#show c/0.
#show a : b.

#program result.
res(X) :- X = @getModels(). 
#show res/1.

