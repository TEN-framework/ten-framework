#script (python)

import clingo
import sys

symbols = [clingo.Number(17)]

def on_model(m):
    m.extend(symbols)

def main(prg):
    prg.ground([("base", [])])
    with prg.solve(on_model=on_model, yield_=True) as handle:
        for m in handle:
            assert list(m.symbols(theory=True)) == symbols
        handle.get()

#end.

1{a;b;c}1.
