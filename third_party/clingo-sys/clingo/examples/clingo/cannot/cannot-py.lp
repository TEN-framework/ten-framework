#script (python)

import clingo

class Thing:
    def __init__(self):
        pass

def main(prg):
    prg.ground([("base", [])])

    # extract relevant theory atoms and minimize the number of messages
    with prg.backend() as backend:
        lits = []
        atoms = []
        for atom in prg.theory_atoms:
            if atom.term.name == "cannot" and len(atom.term.arguments) == 0:
                a = Thing()
                a.literal = atom.literal
                a.elements = []
                for elem in atom.elements:
                    e = Thing()

                    # format message
                    t = elem.terms
                    e.message = t[0].name[1:-1]
                    for term in t[1:]:
                        e.message = e.message.replace("%", str(term), 1)

                    # create condition to minimize
                    condition = elem.condition[:]
                    condition.append(atom.literal)
                    if len(condition) > 1:
                        atom = backend.add_atom()
                        backend.add_rule([atom], condition)
                        condition[0] = atom
                    lits.append((condition[0], 1))
                    e.literal = condition[0]

                    a.elements.append(e)
                atoms.append(a)
        backend.add_minimize(0, lits)

    # solve and print debug messages
    with prg.solve(yield_=True) as it:
        for m in it:
            isTrue = lambda x: m.is_true(x.literal)
            invalid = False
            for atom in filter(isTrue, atoms):
                if not invalid:
                    print("ERROR: invalid answer set")
                    invalid = True
                for elem in filter(isTrue, atom.elements):
                    print ("- cannot {}".format(elem.message))

#end.

#theory cannot {
    t { };
    &cannot/0: t, head
}.
