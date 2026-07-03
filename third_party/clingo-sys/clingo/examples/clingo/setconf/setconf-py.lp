#script (python)

import clingo

def print_conf(conf, ident):
    for key in conf.keys:
        subconf = getattr(conf, key)
        if isinstance(subconf, clingo.Configuration):
            label = key
            if subconf.is_array:
                label += "[0.." + str(len(subconf)) + "]"
            print ("{0}{1} - {2}".format(ident, label, conf.description(key)))
            print_conf(subconf, "  " + ident + label + ".")
        else:
            print ("{0}{1}[={2}] - {3}".format(ident, key, subconf, conf.description(key)))

def main(prg):
    prg.ground([("base", [])])
    blub = prg.configuration.solver

    print_conf(prg.configuration, "")

    print ("The heuristics of the solvers in the 'many' portfolio:")
    prg.configuration.configuration = "many"
    prg.configuration.solve.parallel_mode = 3 # just use the first 3 solvers
    for x in prg.configuration.solver:
        print ("  " + x.heuristic)

    prg.configuration.solve.models = 0
    print ("==================== All Models ===================")
    prg.solve()

    prg.configuration.solve.models = 1
    print ("===================== One Model ===================")
    prg.solve()

    prg.configuration.solve.models = 0
    prg.configuration.solve.enum_mode = "cautious"
    print ("=============== Cautious Consequences =============")
    prg.solve()

    prg.configuration.solve.models = 0
    prg.configuration.solve.enum_mode = "brave"
    print ("================ Brave Consequences ===============")
    prg.solve()

#end.

{ a; b; c }.
