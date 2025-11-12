#script (python)

import clingo

def inf():
    return clingo.Infimum

def sup():
    return clingo.Supremum

#end.

inf(@inf()).
sup(@sup()).
